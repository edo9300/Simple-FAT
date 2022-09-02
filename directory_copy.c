#include "FAT.h"
#include <stdio.h>
#include <assert.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h> /*open*/

static FAT fat;

static int insertFile(char* name) {
	Handle handle;
	int fd;
	char buf[512];
	int err = 0;
	ssize_t nread;
	int written;
	if((handle = createFileFAT(fat, name)) == NULL) {
		printf("failed to create file: %s, error: %s, aborting\n", name, strerror(errno));
		return -1;
	}
	fd = open(name, O_RDONLY);
	if(fd == -1) {
		perror("failed to open file");
		freeHandle(handle);
		return -1;
	}
	while((nread = read(fd, buf, sizeof(buf))) > 0) {
		written = writeFAT(handle, buf, (size_t)nread);
		if(written != (int)nread) {
			printf("Didn't manage to fully copy the file: %s\n", name);
			err = 1;
			break;
		}
	}
	close(fd);
	freeHandle(handle);
	return err;
}

static int insertDirectory(char* name) {
	DIR* directory;
	struct dirent* cur_dir;
	struct stat current_file_stat;
	int err = 0;
	if(chdir(name) != 0) {
		fprintf(stderr, "failed to change directory to %s: %s\n", name, strerror(errno));
		return -1;
	}
	directory = opendir(".");
	if(directory == NULL) {
		if(chdir("..") != 0) {
			assert(0 && "failed to change to parent directory");
		}
		perror("failed to open directory");
		return -1;
	}
	while((cur_dir = readdir(directory)) != NULL) {
		if(cur_dir->d_name[0] == '.' && ((cur_dir->d_name[1] == '\0') || (cur_dir->d_name[1] == '.' && cur_dir->d_name[2] == '\0')))
			continue;
		stat(cur_dir->d_name, &current_file_stat);
		if(S_ISDIR(current_file_stat.st_mode)) {
			err = createDirFAT(fat, cur_dir->d_name);
			if(err == -1) {
				printf("failed to create folder: %s\n", cur_dir->d_name);
				break;
			}
			err = changeDirFAT(fat, cur_dir->d_name);
			if(err == -1) {
				printf("failed to change to folder in the FAT: %s\n", cur_dir->d_name);
				break;
			}
			err = insertDirectory(cur_dir->d_name);
			changeDirFAT(fat, "..");
		}
		else if(S_ISREG(current_file_stat.st_mode))
			err = insertFile(cur_dir->d_name);
		if(err != 0)
			break;
	}
	if(chdir("..") != 0) {
		assert(0 && "failed to change to parent directory");
	}
	closedir(directory);
	return err;
}

int main(int argc, char** argv) {
	int err;
	if(argc < 2) {
		puts("the first argument must be the folder to put in a \"virtual disk\" and the second must be the name for the disk");
		return 1;
	}
	fat = initFAT(argv[2], 1);
	if(fat == NULL) {
		perror("failed to initialize FAT");
		return 1;
	}
	err = insertDirectory(argv[1]);
	if(terminateFAT(fat) != 0) {
		assert(0 || (char*)"failed to free the resources");
	}
	return err;
}
