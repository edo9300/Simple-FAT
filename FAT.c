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
} DirectoryEntry;

int initFAT(const char* diskname) {
	(void)diskname;
	return -1;
}

FileHandle createFile(const char* filename) {
	(void)filename;
	return NULL;
}

int eraseFile(FileHandle file) {
	(void)file;
	return -1;
}

int write(FileHandle to, const void* in, size_t size) {
	(void)to;
	(void)in;
	(void)size;
	return -1;
}

int read(FileHandle from, void* out, size_t size) {
	(void)from;
	(void)out;
	(void)size;
	return -1;
}

int seek(FileHandle file, size_t offset, int whence) {
	(void)file;
	(void)offset;
	(void)whence;
	return -1;
}

int createDir(const char* dirname) {
	(void)dirname;
	return -1;
}

int eraseDir(const char* dirname) {
	(void)dirname;
	return -1;
}

int changeDir(const char* new_dirname) {
	(void)new_dirname;
	return -1;
}

int listDir(const char* dirname) {
	(void)dirname;
	return -1;
}
