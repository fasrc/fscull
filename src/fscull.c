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
static int data_root_l = 0;  //its strlen (not including the null)

//absolute path to the trash directory
static char *trash_root = NULL;
static int trash_root_l = 0;  //its strlen (not including the null)
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


//--- helpers

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

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		return -1;
	}

	return check_directory_fd(fd, path);
}

static int open_directory_at(int dirfd, const char *path) {
	int fd;

	fd = openat(dirfd, path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		return -1;
	}

	return check_directory_fd(fd, path);
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
	for (i==0; i<exempt_paths_l; i++) {
		int l = strlen(exempt_paths[i]);
		if (strncmp(fpath, exempt_paths[i], l) == 0 &&
		    (fpath[l] == '\0' || fpath[l] == '/')) {
			verbosity>=3 && fprintf(stdout, "exempt file: %s\n", fpath);
			return 1;
		}
	}
	verbosity>=3 && fprintf(stdout, "non-exempt file: %s\n", fpath);
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
		verbosity>=3 && fprintf(stdout, "cullable file: %s\n", fpath);
		return 1;
	}
	verbosity>=3 && fprintf(stdout, "non-cullable file: %s\n", fpath);
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
	size_t fpath_l;
	size_t parent_rel_l;
	int src_parent_fd = -1;
	int dst_parent_fd = -1;
	int rc = -1;

	fpath_l = strlen(fpath);


	//--- compute the destination path in trash (tpath) -- substitute leading data_path with trash_path

	if (fpath_l < (size_t)data_root_l || strncmp(fpath, data_root, data_root_l) != 0) {
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
		verbosity>=3 && fprintf(stdout, "pretend mode: skipping directory creation under: %s/%s\n", trash_root, parent_rel);
	}


	//--- move the file

	if (!pretend) {
		if (renameat(src_parent_fd, leafname, dst_parent_fd, leafname)) {
			perror(fpath);
			goto cleanup;
		}
	} else {
		verbosity>=3 && fprintf(stdout, "pretend mode: skipping file move: %s -> %s/%s\n", fpath, trash_root, relative_path);
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
		case FTW_DNR:
			//fpath is a directory which can't be read
			fprintf(stderr, "*** ERROR *** unreadable directory: %s\n", fpath);
			exit_status = EXIT_FAILURE;
			return -1;
		case FTW_NS:
			//the stat(2) call failed on fpath, which is not a symbolic link
			fprintf(stderr, "*** ERROR *** unstatable file: %s\n", fpath);
			exit_status = EXIT_FAILURE;
			return -1;
		default: {
			//(FTW_F)
			//typically want to ignore symlinks
			if (!S_ISLNK(sb->st_mode)) {
				rc = cullable(sb, fpath);
				if (rc > 0) {
					if (cull(fpath)) {
						exit_status = EXIT_FAILURE;
					} else {
						verbosity>=1 && fprintf(stdout, "culled file: %s\n", fpath);
					}
				} else if (rc == 0) {
					verbosity>=2 && fprintf(stdout, "did not cull file: %s\n", fpath);
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
}


int main(int argc, char **argv) {
	//--- option and argument parsing

	exempt_paths = (char **)malloc(MAX_EXEMPT_PATHS * sizeof(char *));

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
				data_root = optarg;
				if ( access(data_root, R_OK) ) {
					fprintf(stderr, "*** ERROR *** unable to access data root: %s: errno %d: ", data_root, errno);
					perror(NULL);
					exit(EXIT_FAILURE);
				}
				data_root = realpath(data_root, NULL);
				data_root_l = strlen(data_root);
				break;
			case 't':
				trash_root = optarg;
				if ( access(trash_root, X_OK) ) {
					fprintf(stderr, "*** ERROR *** unable to access trash root: %s: errno %d: ", trash_root, errno);
					perror(NULL);
					exit(EXIT_FAILURE);
				}
				trash_root = realpath(trash_root, NULL);
				trash_root_l = strlen(trash_root);
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
				if ( exempt_paths_l >= MAX_EXEMPT_PATHS ) {
					fprintf(stderr, "*** ERROR *** hit MAX_EXEMPT_PATHS\n");
					exit(EXIT_FAILURE);
				} else {
					if ( access(optarg, F_OK) ) {
						fprintf(stderr, "*** ERROR *** exempt paths must currently exist: %s: errno %d: ", optarg, errno);
						perror(NULL);
						exit(EXIT_FAILURE);
					}
					exempt_paths[exempt_paths_l] = realpath(optarg, NULL);
					exempt_paths_l++;
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

	//reset argc/argv, forgetting about options above
	argv[optind-1] = argv[0];
	argv += (optind - 1);
	argc -= (optind - 1);

	//add the trash to the exempt directories (if applicable)
	if ( exempt_paths_l >= MAX_EXEMPT_PATHS ) {
		fprintf(stderr, "*** ERROR *** hit MAX_EXEMPT_PATHS\n");
		exit(EXIT_FAILURE);
	} else {
		if ( strncmp(trash_root, data_root, data_root_l) == 0 ) {
			verbosity>=3 && fprintf(stdout, "trash_root is a subdirectory of data_root, exempting\n");
			exempt_paths[exempt_paths_l] = trash_root;
			exempt_paths_l++;
		}
	}

	//check that required options have been given
	if ( data_root == NULL || trash_root == NULL || (retention_window <=0 || retention_window == INT_MAX )) {
		fprintf(stderr, "usage: %s --data-root DATA_ROOT --trash-root TRASH_ROOT --retention-window SECONDS...\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	data_root_fd = open_directory(data_root);
	if (data_root_fd < 0) {
		exit(EXIT_FAILURE);
	}

	trash_root_fd = open_directory(trash_root);
	if (trash_root_fd < 0) {
		close(data_root_fd);
		exit(EXIT_FAILURE);
	}


	//---

	//repeat the basic parameters
	if (verbosity>=0) {
		fprintf(stdout, "running with:\n");
		fprintf(stdout, "    --data-root: %s\n", data_root);
		fprintf(stdout, "    --data-root: %s\n", data_root);
		fprintf(stdout, "    --data-root: %s\n", data_root);
		fprintf(stdout, "    --trash-root: %s\n", trash_root);
		int i = 0;
		for (i==0; i<exempt_paths_l; i++) {
			fprintf(stdout, "    an --exempt-path: %s\n", exempt_paths[i]);
		}
		fprintf(stdout, "    --retention-window: %lld\n", (long long)retention_window);
		fprintf(stdout, "    verbosity: %d\n", verbosity);
	}

	//get the current time, for calculating file age
	//set this once at the beginning, so all ages are computed using the same standard and the window doesn't roll
	//(though each rank is computing this separately)
	time(&t_now);
	if (t_now <= 0) {
		fprintf(stderr, "*** ERROR *** time() failed: errno %d\n", errno);
		exit(errno);
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
