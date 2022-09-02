#include "FAT.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

static void printFolderContents(const DirectoryElement* contents) {
	const DirectoryElement* cur_element;
	for(cur_element = contents; cur_element->filename != NULL; ++cur_element) {
		if(cur_element->file_type == FAT_DIRECTORY)
			printf("[%s]\n", cur_element->filename);
		else
			printf("%s\n", cur_element->filename);
	}
}

static void printCurrentFolderContents(FAT fat) {
	DirectoryElement* contents = listDirFAT(fat);
	printFolderContents(contents);
	freeDirList(contents);
}

static char *rand_string(char *str, size_t size) {
	size_t n;
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK";
    if (size) {
        --size;
        for (n = 0; n < size; n++) {
            int key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}

static void createTooManyChildren(FAT fat, const char* dirname) {
	int i;
	char name[20];
	Handle handle;
	changeDirFAT(fat, dirname);
	
	for(i = 0; i < 66; ++i) {
		handle = createFileFAT(fat, rand_string(name, 20));
		if(handle == NULL) {
			printf("failed to create file number %d\n", i);
			goto error;
		}
		freeHandle(handle);
	}
	printf("succesfully created all the files\n");
	error:
	printf("total files in the current directory\n");

	printCurrentFolderContents(fat);
}

int main(int argc, char** argv) {
	int err;
	int return_code = 0;
	char a[] = "Test string to write to the file";
	char b[] = "Content of file with name of bbb";
	char read_string[512] = { 0 };
	Handle handle;
	Handle handle2;
	int written;
	int read;
	FAT fat;
	if(argc < 2) {
		puts("the filename paramter for the disk is required");
		return 1;
	}
	fat = initFAT(argv[1], 1);
	if(fat == NULL) {
		perror("failed to initialize FAT");
		return 1;
	}
	
	if((handle = createFileFAT(fat, "aaa")) == NULL) {
		return_code = 1;
		puts("failed to create file");
		goto cleanup;
	}

	if((handle2 = createFileFAT(fat, "bbb")) == NULL) {
		return_code = 1;
		puts("failed to create file");
		goto cleanup;
	}


	written = writeFAT(handle, a, 20);
	printf("total written: %d, to write were: %d\n", written, 20);

	written = writeFAT(handle2, b, (int)sizeof(b));
	printf("total written to b: %d, to write were: %d\n", written, (int)sizeof(b));

	freeHandle(handle2);
	handle2 = NULL;

	written = writeFAT(handle, a + 20, (int)sizeof(a) - 20);
	printf("total written: %d, to write were: %d\n", written, (int)sizeof(a) - 20);

	freeHandle(handle);

	if((handle = createFileFAT(fat, "aaa")) == NULL) {
		return_code = 1;
		puts("failed to reopen file");
		goto cleanup;
	}

	read = readFAT(handle, read_string, (int)sizeof(read_string));
	read_string[read] = 0;
	printf("total read: %d, to read were: %d, read content: \"%s\"\n", read, (int)sizeof(read_string), read_string);

	if(seekFAT(handle, 1, FAT_SEEK_SET) == -1) {
		return_code = 1;
		puts("failed to seek file");
		goto cleanup;
	}

	read = readFAT(handle, read_string, (int)sizeof(read_string));
	read_string[read] = 0;
	printf("total read after seeking with set: %d, to read were: %d, read content: \"%s\"\n", read, (int)sizeof(read_string), read_string);

	if(seekFAT(handle, 1, FAT_SEEK_SET) == -1 || seekFAT(handle, 5, FAT_SEEK_CUR) == -1) {
		return_code = 1;
		puts("failed to seek file");
		goto cleanup;
	}

	read = readFAT(handle, read_string, (int)sizeof(read_string));
	read_string[read] = 0;
	printf("total read after seeking with cur: %d, to read were: %d, read content: \"%s\"\n", read, (int)sizeof(read_string), read_string);

	if(seekFAT(handle, -25, FAT_SEEK_END) == -1) {
		return_code = 1;
		puts("failed to seek file");
		goto cleanup;
	}

	read = readFAT(handle, read_string, (int)sizeof(read_string));
	read_string[read] = 0;
	printf("total read after seeking with end: %d, to read were: %d, read content: \"%s\"\n", read, (int)sizeof(read_string), read_string);

	if(createDirFAT(fat, "this is a folder") == -1) {
		return_code = 1;
		puts("failed to create folder");
		goto cleanup;
	}

	printCurrentFolderContents(fat);

	changeDirFAT(fat, "this is a folder");

	if(createDirFAT(fat, "aaa") == -1) {
		return_code = 1;
		puts("failed to create folder");
		goto cleanup;
	}

	if(eraseFileFATAt(handle) == -1) {
		return_code = 1;
		puts("failed to delete file");
		goto cleanup;
	}

	freeHandle(handle);
	handle = NULL;

	printCurrentFolderContents(fat);
	
	createTooManyChildren(fat, "/");
	
cleanup:
	if(handle)
		freeHandle(handle);
	if(handle2)
		freeHandle(handle2);
	err = terminateFAT(fat);
	assert((err == 0) && "failed to free the resources");
	return return_code;
}
