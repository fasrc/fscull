/*

Copyright (c) 2014, Harvard FAS Research Computing
All rights reserved.

*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

#include <libdftw.h>
#include <cmapreduce.h>
#include <fsmr.h>

static int exit_status = 0;

char DEBUG = 1;


//--- parameters

//absolute path to the data directory
static char *data_root = NULL;
int data_root_l = 0;  //its strlen (not including the null)

//absolute path to the trash directory
static char *trash_root = NULL;
int trash_root_l = 0;  //its strlen (not including the null)

//the time against which ages are calculated (set at startup, and doesn't change)
static time_t t_now;

//the retention window; files older than this are culled
static time_t retention_window = 60 * 60 * 24;  //24 hours


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
		if (DEBUG) fprintf(stdout, "created directory: %s\n", path);
	}
	return 0;
}


//--- main funcationality

static char cullable(const struct stat *sb) {
	/*
	 * test whether or not the file is cullable
	 *
	 * returns 0 if no, >0 if yes, and <0 if error (though that's not used yet)
	 */
	
	if ( (t_now - sb->st_atime) > retention_window ) return 1;
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


	//--- compute tpath -- substitute leading data_path with trash_path
	
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

	switch (tflag) {
		case FTW_D:
			//fpath is a directory
			//typically don't do anything with it
			return 0;
		case FTW_DNR:
			//fpath is a directory which can't be read
			fprintf(stderr, "*** ERROR *** unreadable directory: %s\n", fpath);
			exit_status = EXIT_FAILURE;
			return 1;
		case FTW_NS:
			//the stat(2) call failed on fpath, which is not a symbolic link
			fprintf(stderr, "*** ERROR *** unstatable file: %s\n", fpath);
			exit_status = EXIT_FAILURE;
			return 1;
		default: {
			//(FTW_F)
			//typically want to ignore symlinks
			if (!S_ISLNK(sb->st_mode)) {
				size = sb->st_size;
				uid  = sb->st_uid;

				if (cullable(sb) > 0) {
					if (DEBUG) fprintf(stdout, "cullable file: %s\n", fpath);
					if (cull(fpath)) {
						fprintf(stderr, "*** ERROR *** failed to cull file: %s\n", fpath);
						exit_status = EXIT_FAILURE;
					}
				} else {
					if (DEBUG) fprintf(stdout, "non-cullable file: %s\n", fpath);
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
	//get the directories, from the command line
	if(argc!=3) {
		fprintf(stderr, "usage: %s DATA_ROOT TRASH_ROOT\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	//get these directories as absolute paths
	data_root = realpath(argv[1], NULL);
	data_root_l = strlen(data_root);
	trash_root = realpath(argv[2], NULL);
	trash_root_l = strlen(trash_root);

	fprintf(stdout, "running with data_root: %s\n", data_root);
	fprintf(stdout, "running with trash_root: %s\n", trash_root);

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
