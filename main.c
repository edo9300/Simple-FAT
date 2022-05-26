#include "FAT.h"
#include <stdio.h>
#include <assert.h>
#include <errno.h>

int main(int argc, char** argv) {
	int err;
	int return_code = 0;
	char a[] = "Test string to write to the file";
	char read_string[512] = { 0 };
	FileHandle handle;
	int written;
	int read;
	if(argc < 2) {
		puts("the filename paramter for the disk is required");
		return 1;
	}
	err = initFAT(argv[1], 1);
	if(err < 0) {
		perror("failed to initialize FAT");
		return 1;
	}
	
	if(createFileFAT("aaa", &handle) == NULL) {
		return_code = 1;
		puts("failed to create file");
		goto cleanup;
	}
	written = writeFAT(&handle, a, sizeof(a));
	printf("total written: %d, to write were: %ld\n", written, sizeof(a));

	if(createFileFAT("aaa", &handle) == NULL) {
		return_code = 1;
		puts("failed to reopen file");
		goto cleanup;
	}

	read = readFAT(&handle, read_string, sizeof(read_string));
	read_string[read] = 0;
	printf("total read: %d, to read were: %ld, read content: \"%s\"\n", read, sizeof(read_string), read_string);

	if(seekFAT(&handle, 1, FAT_SEEK_SET) == -1) {
		return_code = 1;
		puts("failed to seek file");
		goto cleanup;
	}

	read = readFAT(&handle, read_string, sizeof(read_string));
	read_string[read] = 0;
	printf("total read after seeking with set: %d, to read were: %ld, read content: \"%s\"\n", read, sizeof(read_string), read_string);

	if(seekFAT(&handle, 1, FAT_SEEK_SET) == -1 || seekFAT(&handle, 5, FAT_SEEK_CUR) == -1) {
		return_code = 1;
		puts("failed to seek file");
		goto cleanup;
	}

	read = readFAT(&handle, read_string, sizeof(read_string));
	read_string[read] = 0;
	printf("total read after seeking with cur: %d, to read were: %ld, read content: \"%s\"\n", read, sizeof(read_string), read_string);

	if(seekFAT(&handle, -25, FAT_SEEK_END) == -1) {
		return_code = 1;
		puts("failed to seek file");
		goto cleanup;
	}

	read = readFAT(&handle, read_string, sizeof(read_string));
	read_string[read] = 0;
	printf("total read after seeking with end: %d, to read were: %ld, read content: \"%s\"\n", read, sizeof(read_string), read_string);
	
cleanup:
	err = terminateFAT();
	assert((err == 0) && "failed to free the resources");
	return return_code;
}
