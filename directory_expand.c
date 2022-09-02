#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS 
#define _CRT_DECLARE_NONSTDC_NAMES 1
#include <direct.h>
#include <io.h>
#include <sys/types.h>
#define mkdir(name,mode) mkdir(name)
typedef int ssize_t;
#else
#include <unistd.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif
#endif
#include "FAT.h"
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>

static FAT fat;

static int extractFile(const char* name) {
	Handle handle;
	int fd;
	char buf[512];
	char* out_ptr;
	int err = 0;
	int nread;
	ssize_t nwritten;
	if((handle = createFileFAT(fat, name)) == NULL) {
		printf("failed to open file in FAT: %s\n", name);
		return -1;
	}
	if((fd = open(name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666)) == -1) {
		fprintf(stderr, "failed to create output file %s: %s\n", name, strerror(errno));
		freeHandle(handle);
		return -1;
	}
	while((nread = readFAT(handle, buf, sizeof(buf))) > 0) {
		out_ptr = buf;
		do {
			nwritten = write(fd, out_ptr, (size_t)nread);

			if(nwritten >= 0) {
				nread -= nwritten;
				out_ptr += nwritten;
			} else if(errno != EINTR) {
				fprintf(stderr, "failed to write output file %s: %s\n", name, strerror(errno));
				err = 1;
				goto on_error;
			}
		} while(nread > 0);
	}
	on_error:
	close(fd);
	freeHandle(handle);
	return err;
}

static int extractDirectory(const char* name) {
	DirectoryElement* contents;
	DirectoryElement* cur_element;
	int err = 0;
	if(mkdir(name, 0770) != 0) {
		fprintf(stderr, "failed to create directory %s: %s\n", name, strerror(errno));
		return -1;
	}
	if(chdir(name) != 0) {
		fprintf(stderr, "failed to change directory to %s: %s\n", name, strerror(errno));
		return -1;
	}

	contents = listDirFAT(fat);
	for(cur_element = contents; cur_element->filename != NULL; ++cur_element) {
		printf("CWD: %s, Got: %s, type is: %s\n", name, cur_element->filename, cur_element->file_type == FAT_DIRECTORY ? "folder" : "file");
		if(cur_element->file_type == FAT_DIRECTORY) {
			changeDirFAT(fat, cur_element->filename);
			err = extractDirectory(cur_element->filename);
			changeDirFAT(fat, "..");
		} else
			err = extractFile(cur_element->filename);
	}
	freeDirList(contents);

	if(chdir("..") != 0) {
		assert(0 && "failed to change to parent directory");
	}
	return err;
}

int main(int argc, char** argv) {
	int err;
	if(argc < 2) {
		puts("the first argument must be the \"virtual disk\" to expand and the second must be the name of the folder where this disk will be extracted to");
		return 1;
	}
	fat = initFAT(argv[1], 0);
	if(fat == NULL) {
		perror("failed to initialize FAT");
		return 1;
	}

	err = extractDirectory(argv[2]);
	if(terminateFAT(fat) != 0) {
		assert(0 || (char*)"failed to free the resources");
	}
	return err;
}
