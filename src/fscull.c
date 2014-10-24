/*

Copyright (c) 2014, Harvard FAS Research Computing
All rights reserved.

*/


#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
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

//the time against which ages are calculated (set at startup, and doesn't change)
static time_t t_now;

//the retention window in seconds; files older than this are culled
static time_t retention_window = INT_MAX;

static int exit_status = 0;

char verbosity = 0;


//--- exemptions

static int MAX_EXEMPT_PATHS = 4096;
static char **exempt_paths = NULL;
static int exempt_paths_l = 0;


//--- helpers

int mkdir_p(char *path, mode_t mode) {
	/*
	 * 	make all components of the given path, like `mkdir -p`
	 * 	this is recursive
	 *
	 * 	as with dirname(3), this may modify path (and I *think* this is okay even with the recursion)
	 *
	 * 	returns 0 for success, <0 for failure (and writes an error to stderr)
	 */

	if (mkdir(path, mode)) {
		if (errno == EEXIST) {
			return 0;
		} else if (errno == ENOENT) {
			return mkdir_p(dirname(path), mode);
		} else {
			fprintf(stderr, "*** ERROR *** mkdir failed: %s: errno %d: ", path, errno);
			perror(NULL);
			return -1;
		}
	} else {
		verbosity>=3 && fprintf(stdout, "created directory: %s\n", path);
	}
	return 0;
}


//--- main funcationality

static char exempt(const struct stat *sb, const char *fpath) {
	/*
	 * test whether or not the file is exempt
	 * returns 0 if not, >0 if so, and <0 if error
	 */

	int i = 0;
	for (i==0; i<exempt_paths_l; i++) {
		int l = strlen(exempt_paths[i]);
		if ( strncmp(fpath, exempt_paths[i], l) == 0 ) {
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
	 */

	if ( (t_now - sb->st_atime) > retention_window && exempt(sb, fpath)==0) {
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
	 */

	//the absolute path of the file's new location in trash
	static char tpath[PATH_MAX];
	static int tpath_l;

	//length of the given path (to save recomputing it many times)
	static int fpath_l;
	fpath_l = strlen(fpath);

	//a temporary buffer
	static char tpath_copy[PATH_MAX];


	//--- compute the destination path in trash (tpath) -- substitute leading data_path with trash_path

	//check that tpath buffer is large enough
	if ( sizeof(tpath) < (strlen(trash_root) + strlen(fpath) - strlen(data_root) + 1) ) {
		fprintf(stderr, "*** ERROR *** internal error: attempt to create trash path > PATH_MAX for: %s\n", fpath);
		return -1;
	}

	//start with the trash_root
	memcpy(
		tpath,
		trash_root,
		trash_root_l
	);
	//add the stuff in fpath after the data_root
	memcpy(
		tpath + trash_root_l,
		fpath + data_root_l,
		fpath_l - data_root_l + 1
	);

	tpath_l = strlen(tpath);


	//--- make the directory for it in the trash

	//dirname modified its arg; make a copy
	memcpy(tpath_copy, tpath, tpath_l+1);
	if (mkdir_p(dirname(tpath_copy), 0700)) {
		fprintf(stderr, "*** ERROR *** unable to make directory in trash: %s: errno %d: ", tpath_copy, errno);
		perror(NULL);
		return -1;
	}


	//--- move the file

	if (rename(fpath, tpath)) {
		fprintf(stderr, "*** ERROR *** unable to move file to trash: %s -> %s: errno %d: ", fpath, tpath, errno);
		perror(NULL);
		return -1;
	}


	return 0;
}


//---

//the map function
//see ftw(3) man page (dftw is basically identical)
static int map(const char *fpath, const struct stat *sb, int tflag, void *kv) {
	off_t size;
	uid_t uid;
	int rc;

	switch (tflag) {
		case FTW_D:
			//fpath is a directory
			//typically don't do anything with it

			////skipping a directory by returning non-zero does not work
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
				size = sb->st_size;
				uid  = sb->st_uid;

				rc = cullable(sb, fpath);
				if (rc > 0) {
					if (cull(fpath)) {
						fprintf(stderr, "*** ERROR *** failed to cull file: %s\n", fpath);
						exit_status = EXIT_FAILURE;
						return -1;
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

			{"verbose", no_argument, NULL, 'v'},

			{"help", no_argument, NULL, 'h'},
			{0, 0, 0, 0}
		};

		int c = 0;
		int *indexptr = 0;

		c = getopt_long(argc, argv, "d:t:w:e:vh", longopts, indexptr);
		if (c == -1) break;
		switch (c) {
			case 'd':
				data_root = realpath(optarg, NULL);
				data_root_l = strlen(data_root);
				break;
			case 't':
				trash_root = realpath(optarg, NULL);
				trash_root_l = strlen(trash_root);
				break;
			case 'w':
				if ( sscanf(optarg, "%d", &retention_window) <=0 ) {
					fprintf(stderr, "*** ERROR *** invalid --retention-window: %s\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
			case 'e':
				if ( exempt_paths_l >= MAX_EXEMPT_PATHS ) {
					fprintf(stderr, "*** ERROR *** hit MAX_EXEMPT_PATHS\n");
					exit(EXIT_FAILURE);
				} else {
					exempt_paths[exempt_paths_l] = realpath(optarg, NULL);
					exempt_paths_l++;
				}
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
	if ( data_root == NULL || trash_root == NULL || (retention_window <=0 ||  retention_window == INT_MAX )) {
		fprintf(stderr, "usage: %s --data-root DATA_ROOT --trash-root TRASH_ROOT --retention-window SECONDS...\n", argv[0]);
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
		fprintf(stdout, "    --retention-window: %d\n", retention_window);
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
		exit(EXIT_FAILURE);
	}

	////FIXME do this properly
	//exit(exit_status);
	exit(EXIT_SUCCESS);
}
