#include "FAT.h"
#include <stdio.h>
#include <assert.h>

int main(int argc, char** argv) {
	int err;
	if(argc < 2) {
		puts("the filename paramter for the disk is required");
		return 1;
	}
	err = initFAT(argv[1]);
	if(err < 0) {
		puts("failed to initialize FAT");
		return 1;
	}

	err = terminateFAT();
	assert((err == 0) && "failed to free the resources");
	return 0;
}