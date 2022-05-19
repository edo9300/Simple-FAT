#include "FAT.h"
#include <stdio.h>
#include <assert.h>
#include <errno.h>

int main(int argc, char** argv) {
	int err;
	int return_code = 0;
	FileHandle handle;
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
	
cleanup:
	err = terminateFAT();
	assert((err == 0) && "failed to free the resources");
	return return_code;
}
