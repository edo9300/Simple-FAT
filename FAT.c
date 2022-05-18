#include "FAT.h"
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h> /*open*/
#include <unistd.h> /*close*/
#include <sys/mman.h> /*mmap*/
#include <errno.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct DirectoryEntry {
	char filename[256];
	uint32_t size;
	uint32_t first_block;
	uint32_t next_entry;
} DirectoryEntry;

typedef enum BLOCK_TYPE {
	FREE,
	USED,
	LAST
} BLOCK_TYPE;

typedef struct FATBlock {
	char buffer[511];
	BLOCK_TYPE type;
} FATBlock;

typedef struct FATTable {
	DirectoryEntry entries[256];
} FATTable;

typedef struct FATBackingDisk {
	int mmapped_file_descriptor;
	char* mmapped_file_memory;
	int currently_mapped_size;
} FATBackingDisk;

static FATBackingDisk backing_disk;

int initFAT(const char* diskname, int anew) {
	int prev_errno;
	int flags = O_CREAT | O_RDWR;
	if(anew)
		flags |= O_TRUNC;
	backing_disk.mmapped_file_descriptor = open(diskname, flags, 0666);
	if(backing_disk.mmapped_file_descriptor == -1)
		return -1;
	if(anew) {
		if(ftruncate(backing_disk.mmapped_file_descriptor, sizeof(FATTable)) != 0)
			return -1;
	}
	backing_disk.mmapped_file_memory = (char*)mmap(NULL,
												sizeof(FATTable),
												PROT_READ | PROT_WRITE,
												MAP_PRIVATE | MAP_POPULATE,
												backing_disk.mmapped_file_descriptor,
												0);
	if(backing_disk.mmapped_file_memory == MAP_FAILED) {
		prev_errno = errno;
		close(backing_disk.mmapped_file_descriptor);
		errno = prev_errno;
		return -1;
	}
	backing_disk.currently_mapped_size = sizeof(FATTable);
	return 0;
}

int terminateFAT(void) {
	int has_err;
	int err;
	has_err = err = msync(backing_disk.mmapped_file_memory, backing_disk.currently_mapped_size, MS_SYNC);
	err = munmap(backing_disk.mmapped_file_memory, backing_disk.currently_mapped_size);
	if(err != 0)
		has_err = err;
	err = close(backing_disk.mmapped_file_descriptor);
	if(err != 0)
		has_err = err;
	return has_err;
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
