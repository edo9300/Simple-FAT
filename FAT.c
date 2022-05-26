#include "FAT.h"
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h> /*open*/
#include <unistd.h> /*close*/
#include <sys/mman.h> /*mmap*/
#include <string.h>
#include <errno.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#define TOTAL_BLOCKS 2 /*1024*/
#define BLOCK_BUFFER_SIZE 20 /*511*/
#define DIRECTORY_ENTRY_MAX_NAME 5 /*256*/
#define TOTAL_FAT_ENTRIES 2 /*256*/

typedef struct DirectoryEntry {
	char filename[DIRECTORY_ENTRY_MAX_NAME];
	uint32_t size;
	uint32_t first_block;
} DirectoryEntry;

typedef enum BLOCK_TYPE {
	FREE,
	USED,
	LAST
} BLOCK_TYPE;

typedef struct FileBlock {
	char buffer[BLOCK_BUFFER_SIZE];
	uint8_t type;
	uint32_t next_block;
} FileBlock;

typedef struct FATTable {
	DirectoryEntry entries[TOTAL_FAT_ENTRIES];
} FATTable;

typedef struct FATBackingDisk {
	int mmapped_file_descriptor;
	char* mmapped_file_memory;
	FATTable* mapped_FAT;
	FileBlock* mapped_Blocks;
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
		if(ftruncate(backing_disk.mmapped_file_descriptor, sizeof(FATTable) + sizeof(FileBlock) * TOTAL_BLOCKS) != 0)
			return -1;
	}
	backing_disk.mmapped_file_memory = (char*)mmap(NULL,
												sizeof(FATTable) + sizeof(FileBlock) * TOTAL_BLOCKS,
												PROT_READ | PROT_WRITE,
												MAP_SHARED,
												backing_disk.mmapped_file_descriptor,
												0);
	if(backing_disk.mmapped_file_memory == MAP_FAILED) {
		prev_errno = errno;
		close(backing_disk.mmapped_file_descriptor);
		errno = prev_errno;
		return -1;
	}
	backing_disk.mapped_FAT = (FATTable*)backing_disk.mmapped_file_memory;
	backing_disk.mapped_Blocks = (FileBlock*)(backing_disk.mmapped_file_memory + sizeof(FATTable));
	backing_disk.currently_mapped_size = sizeof(FATTable) + sizeof(FileBlock) * TOTAL_BLOCKS;
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

static int findDirEntry(const char* filename, int* free) {
	int i;
	int found_free = -1;
	for(i = 0; i < TOTAL_FAT_ENTRIES; ++i) {
		if(*(backing_disk.mapped_FAT->entries[i].filename) == 0) {
			if(found_free == -1)
				found_free = i;
			continue;
		}
		if(strncmp(filename, backing_disk.mapped_FAT->entries[i].filename, sizeof(backing_disk.mapped_FAT->entries[i].filename)) == 0){
			return i;
		}
	}
	*free = found_free;
	return -1;
}

static int findFreeBlock() {
	int i;
	for(i = 0; i < TOTAL_BLOCKS; ++i) {
		if(backing_disk.mapped_Blocks[i].type == FREE)
			return i;
	}
	return -1;
}

static int initializeDirEntry(int entry_id, const char* filename) {
	int new_block;
	DirectoryEntry* entry;
	new_block = findFreeBlock();
	if(new_block == -1)
		return -1;
	entry = &backing_disk.mapped_FAT->entries[entry_id];
	strncpy(&entry->filename[0], filename, sizeof(entry->filename));
	entry->first_block = new_block;
	backing_disk.mapped_Blocks[new_block].type = LAST;
	return 0;
}

FileHandle* createFileFAT(const char* filename, FileHandle* handle) {
	int free_entry;
	int used_entry;
	used_entry = findDirEntry(filename, &free_entry);
	if(free_entry == -1 && used_entry == -1)
		return NULL;
	if(used_entry != -1) {
		handle->current_pos = 0;
		handle->current_block_index = 0;
		handle->directory_entry = used_entry;
	} else {
		if(initializeDirEntry(free_entry, filename) == -1)
			return NULL;
		handle->current_pos = 0;
		handle->current_block_index = 0;
		handle->directory_entry = free_entry;
	}
	return handle;
}

#define getDirectoryEntryFromHandle(handle) &backing_disk.mapped_FAT->entries[handle->directory_entry]
#define getFirstBlockFromDirectoryEntry(entry) &backing_disk.mapped_Blocks[entry->first_block]
#define getNextBlockFromBlock(block) &backing_disk.mapped_Blocks[block->next_block]

int eraseFileFAT(FileHandle* file) {
	DirectoryEntry* entry = getDirectoryEntryFromHandle(file);
	FileBlock* current_block = getFirstBlockFromDirectoryEntry(entry);
	while(current_block->type != LAST) {
		current_block->type = FREE;
		current_block = getNextBlockFromBlock(current_block);
	}
	current_block->type = FREE;
	memset(entry, 0, sizeof(DirectoryEntry));
	return 0;
}

static FileBlock* getCurrentBlockFromHandle(FileHandle* handle) {
	uint32_t i;
	DirectoryEntry* entry = getDirectoryEntryFromHandle(handle);
	FileBlock* matching_block = getFirstBlockFromDirectoryEntry(entry);
	for(i = 0; i < handle->current_block_index; i++) {
		if(matching_block->type == LAST)
			return NULL;
		matching_block = getNextBlockFromBlock(matching_block);
	}
	return matching_block;
}

static FileBlock* getOrAllocateNewBlock(FileBlock* cur) {
	FileBlock* new_block;
	int new_block_index;
	if(cur->type != LAST)
		return getNextBlockFromBlock(cur);
	new_block_index = findFreeBlock();
	if(new_block_index == -1)
		return NULL;
	cur->type = USED;
	cur->next_block = new_block_index;
	new_block = &backing_disk.mapped_Blocks[new_block_index];
	new_block->type = LAST;
	return new_block;
}

int writeFAT(FileHandle* to, const void* in, size_t size) {
	size_t written = 0;
	uint32_t pos = to->current_pos;
	FileBlock* block = getCurrentBlockFromHandle(to);
	char* cur = (char*)in;
	uint32_t to_write;
	uint32_t iterated_blocks = 0;
	while(written < size) {
		to_write = BLOCK_BUFFER_SIZE - pos;
		if(to_write > (size - written))
			to_write = size - written;
		memcpy(block->buffer, cur, to_write);
		pos += to_write;
		cur += to_write;
		written += to_write;
		if(pos >= BLOCK_BUFFER_SIZE) {
			pos %= BLOCK_BUFFER_SIZE;
			if((block = getOrAllocateNewBlock(block)) == NULL)
				break;
			++iterated_blocks;
		}
	}
	to->current_pos = pos;
	to->current_block_index += iterated_blocks;
	return written;
}

int readFAT(FileHandle* from, void* out, size_t size) {
	(void)from;
	(void)out;
	(void)size;
	return -1;
}

int seekFAT(FileHandle* file, size_t offset, int whence) {
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
