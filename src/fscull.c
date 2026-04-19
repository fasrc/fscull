#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

/*

Copyright (c) 2014, Harvard FAS Research Computing
All rights reserved.

*/


#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <limits.h>
#include <getopt.h>
#include <errno.h>

#include <libdftw.h>
#include <cmapreduce.h>
#include <fsmr.h>


static char *helpstr = \
"NAME\n"
"    fscull - distributed filesystem data retention policy enforcement\n"
"\n"
"SYNOPSIS\n"
"    fscull --data-root PATH --trash-root PATH --retention-window SECONDS...\n"
"\n"
"See man page for more information.\n"
;


//--- basic parameters

//absolute path to the data directory
static char *data_root = NULL;
static size_t data_root_l = 0;  //its strlen (not including the null)

//absolute path to the trash directory
static char *trash_root = NULL;
static size_t trash_root_l = 0;  //its strlen (not including the null)
static int data_root_fd = -1;
static int trash_root_fd = -1;

//the time against which ages are calculated (set at startup, and doesn't change)
static time_t t_now;

//the retention window in seconds; files older than this are culled
static time_t retention_window = INT_MAX;

static int exit_status = EXIT_SUCCESS;

char pretend = 0;
char verbosity = 0;


//--- exemptions

static int MAX_EXEMPT_PATHS = 4096;
static char **exempt_paths = NULL;
static int exempt_paths_l = 0;

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1U << 0)
#endif


//--- helpers

static char path_is_same_or_descendant(const char *path, const char *root) {
	size_t root_len;

	if (path == NULL || root == NULL) {
		return 0;
	}

	root_len = strlen(root);
	if (root_len == 1 && root[0] == '/') {
		return path[0] == '/';
	}

	return strncmp(path, root, root_len) == 0 &&
	       (path[root_len] == '\0' || path[root_len] == '/');
}

static int check_directory_fd(int fd, const char *path) {
	struct stat st;

	if (fstat(fd, &st) != 0) {
		perror(path);
		close(fd);
		return -1;
	}

	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		perror(path);
		close(fd);
		return -1;
	}

	return fd;
}

static int open_directory(const char *path) {
	int fd;

	fd = open(path, O_RDONLY | O_NOFOLLOW);
	if (fd < 0) {
		perror(path);
		return -1;
	}

	return check_directory_fd(fd, path);
}

static int open_directory_at(int dirfd, const char *path) {
	int fd;

	fd = openat(dirfd, path, O_RDONLY | O_NOFOLLOW);
	if (fd < 0) {
		perror(path);
		return -1;
	}

	return check_directory_fd(fd, path);
}

static int init_root_directory(const char *input_path, char **resolved_path, size_t *resolved_len, int *dirfd_out, struct stat *st_out) {
	char *resolved;
	int fd;

	resolved = realpath(input_path, NULL);
	if (resolved == NULL) {
		perror(input_path);
		return -1;
	}

	fd = open_directory(resolved);
	if (fd < 0) {
		free(resolved);
		return -1;
	}

	if (fstat(fd, st_out) != 0) {
		perror(input_path);
		close(fd);
		free(resolved);
		return -1;
	}

	*resolved_path = resolved;
	*resolved_len = strlen(resolved);
	*dirfd_out = fd;
	return 0;
}

static int move_file_noreplace(int src_parent_fd, const char *leafname, int dst_parent_fd) {
#if defined(__linux__) && defined(SYS_renameat2)
	if (syscall(SYS_renameat2, src_parent_fd, leafname, dst_parent_fd, leafname, RENAME_NOREPLACE) == 0) {
		return 0;
	}
	if (errno != ENOSYS && errno != EINVAL) {
		return -1;
	}
#endif

	/*
	 * linkat() + unlinkat() preserves the no-overwrite guarantee on older
	 * kernels, though a source-unlink failure can leave the file linked in
	 * both locations.
	 */
	if (linkat(src_parent_fd, leafname, dst_parent_fd, leafname, 0) != 0) {
		return -1;
	}
	if (unlinkat(src_parent_fd, leafname, 0) != 0) {
		return -1;
	}
	return 0;
}

static int open_dir_components(int root_fd, const char *relative_path, int create, mode_t mode) {
	char *path_copy = NULL;
	char *component = NULL;
	char *saveptr = NULL;
	int current_fd = -1;
	int next_fd = -1;

	current_fd = dup(root_fd);
	if (current_fd < 0) {
		perror("dup");
		return -1;
	}

	if (relative_path == NULL || relative_path[0] == '\0') {
		return current_fd;
	}

	path_copy = strdup(relative_path);
	if (path_copy == NULL) {
		perror("strdup");
		close(current_fd);
		return -1;
	}

	component = strtok_r(path_copy, "/", &saveptr);
	while (component != NULL) {
		if (create) {
			if (mkdirat(current_fd, component, mode) != 0 && errno != EEXIST) {
				perror(component);
				goto cleanup;
			}
		}

		next_fd = open_directory_at(current_fd, component);
		if (next_fd < 0) {
			goto cleanup;
		}

		close(current_fd);
		current_fd = next_fd;
		next_fd = -1;
		component = strtok_r(NULL, "/", &saveptr);
	}

	free(path_copy);
	return current_fd;

cleanup:
	if (next_fd >= 0) {
		close(next_fd);
	}
	if (current_fd >= 0) {
		close(current_fd);
	}
	free(path_copy);
	return -1;
}


//--- main funcationality

static char exempt(const struct stat *sb, const char *fpath) {
	/*
	 * test whether or not the file is exempt
	 *
	 * returns 0 if not, >0 if so, and <0 if error
	 *
	 * assumes path sb and fpath are not NULL
	 */

	int i = 0;
	(void)sb;
	for (i = 0; i < exempt_paths_l; i++) {
		if (path_is_same_or_descendant(fpath, exempt_paths[i])) {
			if (verbosity >= 3) {
				fprintf(stdout, "exempt file: %s\n", fpath);
			}
			return 1;
		}
	}
	if (verbosity >= 3) {
		fprintf(stdout, "non-exempt file: %s\n", fpath);
	}
	return 0;
}

static char cullable(const struct stat *sb, const char *fpath) {
	/*
	 * test whether or not the file is cullable
	 *
	 * returns 0 if no, >0 if yes, and <0 if error (though that's not used yet)
	 *
	 * assumes path sb and fpath are not NULL
	 */

	if ( (t_now - sb->st_mtime) > retention_window && exempt(sb, fpath)==0 ) {
		if (verbosity >= 3) {
			fprintf(stdout, "cullable file: %s\n", fpath);
		}
		return 1;
	}
	if (verbosity >= 3) {
		fprintf(stdout, "non-cullable file: %s\n", fpath);
	}
	return 0;
}

static int cull(const char *fpath) {
	/*
	 * do the actual culling
	 * this assumes the file is cullable -- it does not double-check!
	 *
	 * return 0 for success, <0 for failure (and writes an error to stderr)
	 *
	 * assumes fpath is not NULL
	 */

	char *parent_rel = NULL;
	const char *relative_path;
	const char *leafname;
	const char *last_sep;
	size_t parent_rel_l;
	int src_parent_fd = -1;
	int dst_parent_fd = -1;
	int rc = -1;

	//--- compute the destination path in trash (tpath) -- substitute leading data_path with trash_path

	if (!path_is_same_or_descendant(fpath, data_root)) {
		errno = EINVAL;
		perror(fpath);
		return -1;
	}
	if (data_root_fd < 0 || trash_root_fd < 0) {
		errno = EBADF;
		perror("cull");
		return -1;
	}

	relative_path = fpath + data_root_l;
	while (*relative_path == '/') {
		relative_path++;
	}
	if (*relative_path == '\0') {
		errno = EINVAL;
		perror(fpath);
		return -1;
	}

	last_sep = strrchr(relative_path, '/');
	if (last_sep == NULL) {
		leafname = relative_path;
		parent_rel = strdup("");
		if (parent_rel == NULL) {
			perror("strdup");
			return -1;
		}
	} else {
		leafname = last_sep + 1;
		parent_rel_l = (size_t)(last_sep - relative_path);
		parent_rel = (char *)malloc(parent_rel_l + 1);
		if (parent_rel == NULL) {
			perror("malloc");
			return -1;
		}
		memcpy(parent_rel, relative_path, parent_rel_l);
		parent_rel[parent_rel_l] = '\0';
	}


	//--- make the directory for it in the trash

	if (!pretend) {
		src_parent_fd = open_dir_components(data_root_fd, parent_rel, 0, 0);
		if (src_parent_fd < 0) {
			goto cleanup;
		}

		dst_parent_fd = open_dir_components(trash_root_fd, parent_rel, 1, 0700);
		if (dst_parent_fd < 0) {
			goto cleanup;
		}
	} else {
		if (verbosity >= 3) {
			fprintf(stdout, "pretend mode: skipping directory creation under: %s/%s\n", trash_root, parent_rel);
		}
	}


	//--- move the file

	if (!pretend) {
		if (move_file_noreplace(src_parent_fd, leafname, dst_parent_fd) != 0) {
			perror(fpath);
			goto cleanup;
		}
	} else {
		if (verbosity >= 3) {
			fprintf(stdout, "pretend mode: skipping file move: %s -> %s/%s\n", fpath, trash_root, relative_path);
		}
	}

	rc = 0;

cleanup:
	if (src_parent_fd >= 0) {
		close(src_parent_fd);
	}
	if (dst_parent_fd >= 0) {
		close(dst_parent_fd);
	}
	free(parent_rel);
	return rc;
}


//---

//the map function
//see ftw(3) man page (dftw is basically identical)
static int map(const char *fpath, const struct stat *sb, int tflag, void *kv) {
	int rc = 0;
	(void)kv;

	switch (tflag) {
		case FTW_D:
			//fpath is a directory
			//typically don't do anything with it

			////FIXME skipping a directory by returning non-zero does not work
			////it would be much better to patch dftw to not even enter --exempt-path directories
			////as-is, this still processes every single file, comparing to the exempt path just to end up ignoring it
			//if ( strcmp(fpath, exempt_dir) == 0 ) {
			//	verbosity>=3 && fprintf(stdout, "exempt path: %s\n", fpath);
			//	return -1;
			//} else {
				return 0;
			//}
		default: {
			//(FTW_F)
			//typically want to ignore symlinks
			if (!S_ISLNK(sb->st_mode)) {
				rc = cullable(sb, fpath);
				if (rc > 0) {
					if (cull(fpath)) {
						exit_status = EXIT_FAILURE;
					} else {
						if (verbosity >= 1) {
							fprintf(stdout, "culled file: %s\n", fpath);
						}
					}
				} else if (rc == 0) {
					if (verbosity >= 2) {
						fprintf(stdout, "did not cull file: %s\n", fpath);
					}
				} else if (rc < 0) {
					fprintf(stderr, "*** ERROR *** failed to determine if file is cullable: %s\n", fpath);
					exit_status = EXIT_FAILURE;
					return -1;
				}

				////emit a {uid:size} key/value
				//MR_kv_add(kv, (char*)&uid, sizeof(uid_t), (char*)&size, sizeof(off_t));
			}
			return 0;
		}
	}
}


//the reduce function
//see http://mapreduce.sandia.gov/doc/reduce.html
static void reduce(char *key, int keybytes, char *multivalue, int nvalues, int *valuebytes) {
	(void)key;
	(void)keybytes;
	(void)multivalue;
	(void)nvalues;
	(void)valuebytes;
}


int main(int argc, char **argv) {
	//--- option and argument parsing
	const char *data_root_arg = NULL;
	const char *trash_root_arg = NULL;
	struct stat data_root_st;
	struct stat trash_root_st;

	exempt_paths = (char **)malloc(MAX_EXEMPT_PATHS * sizeof(char *));
	if (exempt_paths == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	while (1) {
		static struct option longopts[] = {
			{"data-root"       , required_argument, NULL, 'd'},
			{"trash-root"      , required_argument, NULL, 't'},
			{"retention-window", required_argument, NULL, 'w'},
			{"exempt-path"     , required_argument, NULL, 'e'},

			{"pretend", no_argument, NULL, 'p'},
			{"verbose", no_argument, NULL, 'v'},

			{"help", no_argument, NULL, 'h'},
			{0, 0, 0, 0}
		};

		int c = 0;
		int *indexptr = 0;
		char *endptr = NULL;
		long long parsed_retention_window;

		c = getopt_long(argc, argv, "d:t:w:e:pvh", longopts, indexptr);
		if (c == -1) break;
		switch (c) {
			case 'd':
				data_root_arg = optarg;
				break;
			case 't':
				trash_root_arg = optarg;
				break;
			case 'w':
				errno = 0;
				parsed_retention_window = strtoll(optarg, &endptr, 10);
				if (errno != 0 || endptr == optarg || *endptr != '\0') {
					fprintf(stderr, "*** ERROR *** invalid --retention-window: %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				retention_window = (time_t)parsed_retention_window;
				if ((long long)retention_window != parsed_retention_window) {
					fprintf(stderr, "*** ERROR *** invalid --retention-window: %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
			case 'e':
				{
					char *resolved_exempt_path;

				if ( exempt_paths_l >= MAX_EXEMPT_PATHS ) {
					fprintf(stderr, "*** ERROR *** hit MAX_EXEMPT_PATHS\n");
					exit(EXIT_FAILURE);
				} else {
					resolved_exempt_path = realpath(optarg, NULL);
					if (resolved_exempt_path == NULL) {
						perror(optarg);
						exit(EXIT_FAILURE);
					}
					exempt_paths[exempt_paths_l] = resolved_exempt_path;
					exempt_paths_l++;
				}
				}
				break;

			case 'p':
				pretend = 1;
				break;
			case 'v':
				verbosity++;
				break;

			case 'h':
				fputs(helpstr, stdout);
				exit(EXIT_SUCCESS);

			case '?':
				//(getopt_long will have written the error message)
				exit(EXIT_FAILURE);
			default:
				fprintf(stderr, "*** ERROR *** unable to parse command line options\n");
				exit(EXIT_FAILURE);
		}
	}

	//check that required options have been given
	if ( data_root_arg == NULL || trash_root_arg == NULL || (retention_window <=0 || retention_window == INT_MAX )) {
		fprintf(stderr, "usage: %s --data-root DATA_ROOT --trash-root TRASH_ROOT --retention-window SECONDS...\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (init_root_directory(data_root_arg, &data_root, &data_root_l, &data_root_fd, &data_root_st) != 0) {
		exit(EXIT_FAILURE);
	}

	if (init_root_directory(trash_root_arg, &trash_root, &trash_root_l, &trash_root_fd, &trash_root_st) != 0) {
		close(data_root_fd);
		exit(EXIT_FAILURE);
	}

	if (data_root_st.st_dev != trash_root_st.st_dev) {
		fprintf(stderr, "*** ERROR *** data-root and trash-root must be on the same filesystem\n");
		close(data_root_fd);
		close(trash_root_fd);
		exit(EXIT_FAILURE);
	}

	{
		int i = 0;
		for (i = 0; i < exempt_paths_l; i++) {
			if (!path_is_same_or_descendant(exempt_paths[i], data_root)) {
				fprintf(stderr, "*** ERROR *** --exempt-path must resolve within --data-root: %s\n", exempt_paths[i]);
				close(data_root_fd);
				close(trash_root_fd);
				exit(EXIT_FAILURE);
			}
		}
	}

	//reset argc/argv, forgetting about options above
	argv[optind-1] = argv[0];
	argv += (optind - 1);
	argc -= (optind - 1);

	//add the trash to the exempt directories (if applicable)
	if ( exempt_paths_l >= MAX_EXEMPT_PATHS ) {
		fprintf(stderr, "*** ERROR *** hit MAX_EXEMPT_PATHS\n");
		exit(EXIT_FAILURE);
	} else {
		if ( path_is_same_or_descendant(trash_root, data_root) ) {
			if (verbosity >= 3) {
				fprintf(stdout, "trash_root is a subdirectory of data_root, exempting\n");
			}
			exempt_paths[exempt_paths_l] = trash_root;
			exempt_paths_l++;
		}
	}


	//---

	//repeat the basic parameters
	fprintf(stdout, "running with:\n");
	fprintf(stdout, "    --data-root: %s\n", data_root);
	fprintf(stdout, "    --trash-root: %s\n", trash_root);
	{
		int i = 0;
		for (i = 0; i < exempt_paths_l; i++) {
			fprintf(stdout, "    an --exempt-path: %s\n", exempt_paths[i]);
		}
	}
	fprintf(stdout, "    --retention-window: %lld\n", (long long)retention_window);
	fprintf(stdout, "    verbosity: %d\n", verbosity);

	//get the current time, for calculating file age
	//set this once at the beginning, so all ages are computed using the same standard and the window doesn't roll
	//(though each rank is computing this separately)
	time(&t_now);
	if (t_now <= 0) {
		fprintf(stderr, "*** ERROR *** time() failed: errno %d\n", errno);
		exit(EXIT_FAILURE);
	}

	if (fsmr(data_root, map, reduce)) {
		fprintf(stderr, "*** ERROR *** %s failed\n", argv[0]);
		close(data_root_fd);
		close(trash_root_fd);
		exit(EXIT_FAILURE);
	}

	close(data_root_fd);
	close(trash_root_fd);

	exit(exit_status);
}
