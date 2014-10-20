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

#include <libdftw.h>
#include <cmapreduce.h>
#include <fsmr.h>

static int exit_status = 0;

char DEBUG = 1;

static char *data_root = NULL;
static char *trash_root = NULL;
static time_t t_now;

static time_t retention_window = 60 * 60 * 24;  //24 hours


static char cullable(const struct stat *sb) {
	/*
	 * test whether or not file is cullable
	 * returns 0 if no, >0 if yes, and <0 if error (though that's not used yet)
	 */
	
	if ( (t_now - sb->st_atime) > retention_window ) return 1;
	return 0;
}

static int cull(const char *fpath) {
/*
 * do the actual culling
 * this assumes the file is cullable; it does not double-check!
 */
	static char p[PATH_MAX];

	char *foo = "foo";

	if ( sizeof(p) < strlen(fpath)+strlen(foo)+1 ) {
		fprintf(stderr, "internal error: attempt to create path > PATH_MAX for: %s\n", fpath);
		return 1;
	}
	memcpy(p, fpath, strlen(fpath)+1);
	memcpy(p + strlen(fpath), foo, strlen(foo)+1);

	struct stat sb;

	if (stat(p, &sb)) {
		fprintf(stderr, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx: %s: errno %d: ", p, errno);
		perror(NULL);
		return 1;
	}

	//do the actual culling
	
	return 0;
}


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
			fprintf(stderr, "unreadable directory: %s\n", fpath);
			exit_status = EXIT_FAILURE;
			return 1;
		case FTW_NS:
			//the stat(2) call failed on fpath, which is not a symbolic link
			fprintf(stderr, "unstatable file: %s\n", fpath);
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
	data_root = argv[1];
	trash_root = argv[2];

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
