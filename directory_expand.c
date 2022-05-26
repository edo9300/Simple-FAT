#include "FAT.h"
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int extractFile(const char* name) {
	FileHandle handle;
	int fd;
	char buf[512];
	char* out_ptr;
	int err = 0;
	int nread;
	ssize_t nwritten;
	if(createFileFAT(name, &handle) == NULL) {
		printf("failed to open file in FAT: %s\n", name);
		return -1;
	}
	if((fd = creat(name, 0660)) == -1) {
		fprintf(stderr, "failed to create output file %s: %s\n", name, strerror(errno));
		return -1;
	}
	while(nread = readFAT(&handle, buf, sizeof(buf)), nread > 0) {
		out_ptr = buf;
		do {
			nwritten = write(fd, out_ptr, nread);

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
	return err;
}

static int extractDirectory(const char* name) {
	DirectoryElement* contents;
	DirectoryElement* cur_element;
	int err;
	if(mkdir(name, 0660) != 0) {
		fprintf(stderr, "failed to create directory %s: %s\n", name, strerror(errno));
		return -1;
	}
	if(chdir(name) != 0) {
		fprintf(stderr, "failed to change directory to %s: %s\n", name, strerror(errno));
		return -1;
	}

	contents = listDirFAT();
	for(cur_element = contents; cur_element->filename != NULL; ++cur_element) {
		printf("CWD: %s, Got: %s, type is: %s\n", name, cur_element->filename, cur_element->file_type == FAT_DIRECTORY ? "folder" : "file");
		if(cur_element->file_type == FAT_DIRECTORY) {
			changeDirFAT(cur_element->filename);
			err = extractDirectory(cur_element->filename);
			changeDirFAT("..");
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
	err = initFAT(argv[1], 0);
	if(err < 0) {
		perror("failed to initialize FAT");
		return 1;
	}

	err = extractDirectory(argv[2]);
	if(terminateFAT() != 0) {
		assert(0 || "failed to free the resources");
	}
	return err;
}