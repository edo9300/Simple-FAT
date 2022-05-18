#include "FAT.h"
#include <stddef.h>
#include <stdint.h>
#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct DirectoryEntry {
	char filename[256];
	uint32_t size;
	uint32_t first_block;
	struct DirectoryEntry* next;
} DirectoryEntry;

typedef enum BLOCK_TYPE {
	NORMAL,
	LAST
} BLOCK_TYPE;

typedef struct FATBlock {
	char buffer[511];
	BLOCK_TYPE type;
} FATBlock;

typedef struct FATTable {
	DirectoryEntry entries[256];
} FATTable;

struct FATBackingDisk {
	int mmapped_file_descriptor;
	char* mmapped_file_memory;
	int currently_mapped_size;
} FATBackingDisk;

int initFAT(const char* diskname, int anew) {
	(void)diskname;
	(void)anew;
	return -1;
}

int terminateFAT(void) {
	return -1;
}

FileHandle createFileFAT(const char* filename) {
	(void)filename;
	return NULL;
}

int eraseFileFAT(FileHandle file) {
	(void)file;
	return -1;
}

int writeFAT(FileHandle to, const void* in, size_t size) {
	(void)to;
	(void)in;
	(void)size;
	return -1;
}

int readFAT(FileHandle from, void* out, size_t size) {
	(void)from;
	(void)out;
	(void)size;
	return -1;
}

int seekFAT(FileHandle file, size_t offset, int whence) {
	(void)file;
	(void)offset;
	(void)whence;
	return -1;
}

int createDirFAT(const char* dirname) {
	(void)dirname;
	return -1;
}

int eraseDirFAT(const char* dirname) {
	(void)dirname;
	return -1;
}

int changeDirFAT(const char* new_dirname) {
	(void)new_dirname;
	return -1;
}

int listDirFAT(const char* dirname) {
	(void)dirname;
	return -1;
}
