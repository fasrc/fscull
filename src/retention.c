/*

This code uses fsmr to scan a file system looking for files that haven't been accessed in the past set amount of time.
It will will then move those files to a .trash directory for safe keeping.
It also excludes specific paths from the retention enforcement.

Copyright (c) 2014, Paul P. Edmon
All rights reserved.

Copyright (c) 2013, John A. Brunelle
All rights reserved.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <libdftw.h>
#include <cmapreduce.h>
#include <fsmr.h>

//the map function
//see ftw(3) man page (dftw is basically identical)
static int map(const char *fpath, const struct stat *sb, int tflag, void *kv) {
	off_t size;
	uid_t uid;
	time_t mtime;
	time_t ctime;  //current time (not change time)
	time_t subtime;
	time_t age;
	char filesystem_root[1024];
	char trash_root[1024];
	char trash_path[1024];
	char relative_path[1024];
	char excluded_dir[1024];
	char fpath_exclude[1024];
	char *file;
	char trash_dir[1024];
	int len_fsroot;
	int len_exclude;
	int result;
	struct stat st = {0};

	time(&ctime);
	age = 60;//90*24*60*60;
	strcpy(filesystem_root,"/n/regal/rc_admin/pedmon/test/");
	strcpy(trash_root,"/n/regal/rc_admin/pedmon/test/.trash/");
	strcpy(excluded_dir,"/n/regal/rc_admin/pedmon/test/excluded/");
	len_fsroot = strlen(filesystem_root);

	//If fpath includes the .trash directory move on
	len_exclude = strlen(trash_root);
	strcpy(fpath_exclude,fpath);
	fpath_exclude[len_exclude]='\0';
	if (strcmp(fpath_exclude,trash_root) == 0) {
		printf("Excluded dir: %s\n", fpath);
		return 0;
	}

	//If fpath includes an excluded directory move on
	len_exclude = strlen(excluded_dir);
	strcpy(fpath_exclude,fpath);
	fpath_exclude[len_exclude]='\0';
	if (strcmp(fpath_exclude,excluded_dir) == 0) {
		printf("Excluded dir: %s\n", fpath);
		return 0;
	}

	switch (tflag) {
		case FTW_D:
			//fpath is a directory
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
			//ignore symlinks
			if (!S_ISLNK(sb->st_mode)) {
				//get size, uid, and mtime of file
				size = sb->st_size;
				uid  = sb->st_uid;
				mtime = sb->st_mtime;

				//Figure out the difference between the current time and mtime
				subtime = ctime-mtime;

				//If subtime is larger than our age limit then we need to do something
				if ( subtime > age ) {
					printf("file, uid, size, subtime, age: %s %i %i %i %i\n", fpath, uid, size, subtime, age);
					printf("length: %i\n", len_fsroot);
					//Figure out the relative path
					strcpy(relative_path,fpath+len_fsroot);
					printf("relative_path: %s\n", relative_path);
					//Figure out what the new location will be in the trash directory
					strcpy(trash_path, trash_root);
					strcat(trash_path, relative_path);
					printf("trash_path: %s\n", trash_path);
					//Figure out what the base directory is with out the file we are moving.
					file = strrchr(trash_path,'/');
					strcpy(trash_dir, trash_path);
					trash_dir[strlen(trash_dir)-strlen(file)]='\0';
					printf("trash_dir: %s\n",trash_dir);
					//If the trash directory does not exist make it.
					if (stat(trash_dir, &st) == -1) {
    					mkdir(trash_dir, 00755);
					}

					//Move the file to the trash.
					result = rename(fpath, trash_path);
		
  					if ( result == 0 )
    					puts ( "File successfully renamed" );
  					else
    					perror( "Error renaming file" );
				}
			}
			return 0;
		}
	}
}

//the reduce function
//see http://mapreduce.sandia.gov/doc/reduce.html
//No reduction needed for this one.
static void reduce(char *key, int keybytes, char *multivalue, int nvalues, int *valuebytes) {
	return 0;
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
