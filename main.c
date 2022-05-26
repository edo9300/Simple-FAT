#include "FAT.h"
#include <stdio.h>
#include <assert.h>

static void printFolderContents(const DirectoryElement* contents) {
	const DirectoryElement* cur_element;
	for(cur_element = contents; cur_element->filename != NULL; ++cur_element) {
		if(cur_element->file_type == FAT_DIRECTORY)
			printf("[%s]\n", cur_element->filename);
		else
			printf("%s\n", cur_element->filename);
	}
}

int main(int argc, char** argv) {
	int err;
	int return_code = 0;
	char a[] = "Test string to write to the file";
	char b[] = "Content of file with name of bbb";
	char read_string[512] = { 0 };
	FileHandle handle;
	FileHandle handle2;
	DirectoryElement* contents;
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

	if(createFileFAT("bbb", &handle2) == NULL) {
		return_code = 1;
		puts("failed to create file");
		goto cleanup;
	}


	written = writeFAT(&handle, a, 20);
	printf("total written: %d, to write were: %d\n", written, 20);

	written = writeFAT(&handle2, b, sizeof(b));
	printf("total written to b: %d, to write were: %ld\n", written, sizeof(b));

	written = writeFAT(&handle, a + 20, sizeof(a) - 20);
	printf("total written: %d, to write were: %ld\n", written, sizeof(a) - 20);

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

	if(createDirFAT("this is a folder") == -1) {
		return_code = 1;
		puts("failed to create folder");
		goto cleanup;
	}

	contents = listDirFAT();
	printFolderContents(contents);
	freeDirList(contents);

	changeDirFAT("this is a folder");

	if(createDirFAT("aaa") == -1) {
		return_code = 1;
		puts("failed to create folder");
		goto cleanup;
	}

	if(eraseFileFAT(&handle) == -1) {
		return_code = 1;
		puts("failed to delete file");
		goto cleanup;
	}

	contents = listDirFAT();
	printFolderContents(contents);
	freeDirList(contents);
	
cleanup:
	err = terminateFAT();
	assert((err == 0) && "failed to free the resources");
	return return_code;
}
