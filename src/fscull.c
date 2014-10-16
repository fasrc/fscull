#include <stdio.h>
#include <stdlib.h>

#include <libdftw.h>
#include <cmapreduce.h>
#include <fsmr.h>


//the map function
//see ftw(3) man page (dftw is basically identical)
static int map(const char *fpath, const struct stat *sb, int tflag, void *kv) {
	switch (tflag) {
		case FTW_D:
			//fpath is a directory
			//typically don't do anything with it
			return 0;
		case FTW_DNR:
			//fpath is a directory which can't be read
			fprintf(stderr, "unreadable directory: %s\n", fpath);
			return 1;
		case FTW_NS:
			//the stat(2) call failed on fpath, which is not a symbolic link
			fprintf(stderr, "unstatable file: %s\n", fpath);
			return 1;
		default: {
			//(FTW_F)
			//typically want to ignore symlinks
			if (!S_ISLNK(sb->st_mode)) {
				/*

				...
				the man map computation
				see MR_kv_add to emit a key/value pair
				...

				*/
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
	if(argc!=2) {
		fprintf(stderr, "usage: %s DIRECTORY\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	fprintf(stdout, "recursing directory: %s\n", argv[1]);

	if (fsmr(argv[1], map, reduce)) {
		fprintf(stderr, "*** ERROR *** %s failed\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
