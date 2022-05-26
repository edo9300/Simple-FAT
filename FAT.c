#include "FAT.h"
#include <stddef.h> /*size_t, NULL*/
#include <stdint.h> /*int_t types*/
#include <fcntl.h> /*open*/
#include <unistd.h> /*close, ftruncate*/
#include <sys/mman.h> /*mmap, munmap, msync*/
#include <string.h> /*memcpy, strncpy, strncmp*/
#include <errno.h> /*errno*/
#include <malloc.h> /*malloc*/

#define TOTAL_BLOCKS 1024
#define BLOCK_BUFFER_SIZE 511
#define DIRECTORY_ENTRY_MAX_NAME 256
#define TOTAL_FAT_ENTRIES 256
#define MAX_DIR_CHILDREN 64

#define DELETED_CHILD_ENTRY UINT16_MAX
#define FREE_CHILD_ENTRY 0

#define ROOT_WORKING_DIRECTORY UINT16_MAX

typedef struct DirectoryEntry {
	char filename[DIRECTORY_ENTRY_MAX_NAME];
	uint8_t file_type;
	uint8_t num_children;
	uint16_t parent_directory;
	uint32_t size;
	uint32_t first_block;
	uint16_t children[MAX_DIR_CHILDREN];
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
	uint16_t current_working_directory;
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
	backing_disk.current_working_directory = ROOT_WORKING_DIRECTORY;
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

#define getEntryFromIndex(index) (&backing_disk.mapped_FAT->entries[index])
#define getBlockFromIndex(index) (&backing_disk.mapped_Blocks[index])

static int findDirEntry(const char* filename, int* free, DirectoryEntryType file_type) {
	DirectoryEntry* cur_entry;
	int i;
	int found_free = -1;
	for(i = 0; i < TOTAL_FAT_ENTRIES; ++i) {
		cur_entry = getEntryFromIndex(i);
		if(cur_entry->filename[0] == 0) {
			if(found_free == -1)
				found_free = i;
			continue;
		}
		if(cur_entry->parent_directory == backing_disk.current_working_directory &&
		   strncmp(filename, cur_entry->filename, sizeof(cur_entry->filename)) == 0) {
			if(cur_entry->file_type != file_type) {
				found_free = -1;
				break;
			}
			return i;
		}
	}
	if(backing_disk.current_working_directory != ROOT_WORKING_DIRECTORY
	   && getEntryFromIndex(backing_disk.current_working_directory)->num_children >= MAX_DIR_CHILDREN)
		found_free = -1;
	if(free)
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

static void addChildToFolder(DirectoryEntry* parent, uint16_t child) {
	int i;
	uint16_t* cur_child;
	for(i = 0; i < MAX_DIR_CHILDREN; ++i) {
		cur_child = &(parent->children[i]);
		if(*cur_child == FREE_CHILD_ENTRY || *cur_child == DELETED_CHILD_ENTRY) {
			*cur_child = child;
			break;
		}
	}
	++(parent->num_children);
}

static int initializeDirEntry(int entry_id, const char* filename, DirectoryEntryType file_type) {
	int new_block = 0;
	DirectoryEntry* entry;
	if(file_type != FAT_DIRECTORY) {
		new_block = findFreeBlock();
		if(new_block == -1)
			return -1;
		backing_disk.mapped_Blocks[new_block].type = LAST;
	}
	entry = getEntryFromIndex(entry_id);
	strncpy(&entry->filename[0], filename, sizeof(entry->filename));
	entry->first_block = new_block;
	entry->size = 0;
	entry->file_type = file_type;
	entry->parent_directory = backing_disk.current_working_directory;
	if(entry->parent_directory != ROOT_WORKING_DIRECTORY)
		addChildToFolder(getEntryFromIndex(entry->parent_directory), entry_id);
	if(file_type == FAT_DIRECTORY) {
		entry->num_children = 0;
		memset(entry->children, 0, sizeof(entry->children));
	}
	return 0;
}

FileHandle* createFileFAT(const char* filename, FileHandle* handle) {
	int free_entry;
	int used_entry;
	used_entry = findDirEntry(filename, &free_entry, FAT_FILE);
	if(used_entry == -1 && free_entry == -1)
		return NULL;
	if(used_entry != -1) {
		handle->current_pos = 0;
		handle->current_block_index = 0;
		handle->directory_entry = used_entry;
	} else {
		if(initializeDirEntry(free_entry, filename, FAT_FILE) == -1)
			return NULL;
		handle->current_pos = 0;
		handle->current_block_index = 0;
		handle->directory_entry = free_entry;
	}
	return handle;
}

#define getDirectoryEntryFromHandle(handle) getEntryFromIndex(handle->directory_entry)
#define getFirstBlockFromDirectoryEntry(entry) getBlockFromIndex(entry->first_block)
#define getNextBlockFromBlock(block) getBlockFromIndex(block->next_block)

static void removeChildFromFolder(DirectoryEntry* parent, uint16_t child) {
	int i;
	uint16_t* cur_child;
	for(i = 0; i < MAX_DIR_CHILDREN; ++i) {
		cur_child = &(parent->children[i]);
		if(*cur_child == child) {
			*cur_child = DELETED_CHILD_ENTRY;
			break;
		}
		if(*cur_child == FREE_CHILD_ENTRY)
			break;
	}
}


int eraseFileFAT(FileHandle* file) {
	DirectoryEntry* entry = getDirectoryEntryFromHandle(file);
	FileBlock* current_block = getFirstBlockFromDirectoryEntry(entry);
	while(current_block->type != LAST) {
		current_block->type = FREE;
		current_block = getNextBlockFromBlock(current_block);
	}
	current_block->type = FREE;
	if(entry->parent_directory != ROOT_WORKING_DIRECTORY)
		removeChildFromFolder(getEntryFromIndex(entry->parent_directory), file->directory_entry);
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
	new_block = getBlockFromIndex(new_block_index);
	memset(new_block, 0, sizeof(new_block->buffer));
	new_block->type = LAST;
	return new_block;
}

#define getAbsolutePosFromHandle(handle) ((handle->current_block_index * BLOCK_BUFFER_SIZE) + handle->current_pos)
#define getTotalSizeFromHandle(handle) (getDirectoryEntryFromHandle(handle)->size)

#define updateFileHandlePositionFromAbsolutePosition(handle, absolute_pos)\
do {\
	handle->current_block_index = absolute_pos / BLOCK_BUFFER_SIZE;\
	handle->current_pos = absolute_pos % BLOCK_BUFFER_SIZE;\
} while(0)

int writeFAT(FileHandle* to, const void* in, size_t size) {
	size_t written = 0;
	uint32_t pos = to->current_pos;
	uint32_t absolute_pos = getAbsolutePosFromHandle(to);
	FileBlock* block = getCurrentBlockFromHandle(to);
	char* cur = (char*)in;
	uint32_t to_write;
	uint32_t iterated_blocks = 0;
	while(written < size) {
		to_write = BLOCK_BUFFER_SIZE - pos;
		if(to_write > (size - written))
			to_write = size - written;
		memcpy(block->buffer + pos, cur, to_write);
		pos += to_write;
		absolute_pos += to_write;
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
	if(absolute_pos > getTotalSizeFromHandle(to))
		getTotalSizeFromHandle(to) = absolute_pos;
	return written;
}

int readFAT(FileHandle* from, void* out, size_t size) {
	uint32_t absolute_pos = getAbsolutePosFromHandle(from);
	uint32_t file_size = getTotalSizeFromHandle(from);
	uint32_t total_read = 0;
	FileBlock* block = getCurrentBlockFromHandle(from);
	char* cur = (char*)out;
	uint32_t to_read;
	uint32_t iterated_blocks = 0;
	uint32_t pos = from->current_pos;
	if((absolute_pos + size) > file_size)
		size = file_size - absolute_pos;
	while(total_read < size) {
		to_read = BLOCK_BUFFER_SIZE - pos;
		if(to_read > (size - total_read))
			to_read = size - total_read;
		memcpy(cur, block->buffer + pos, to_read);
		pos += to_read;
		absolute_pos += to_read;
		cur += to_read;
		total_read += to_read;
		if(pos >= BLOCK_BUFFER_SIZE) {
			pos %= BLOCK_BUFFER_SIZE;
			if((block = getOrAllocateNewBlock(block)) == NULL)
				break;
			++iterated_blocks;
		}
	}
	from->current_pos = pos;
	from->current_block_index += iterated_blocks;
	return total_read;
}

int seekFAT(FileHandle* file, int32_t offset, SeekWhence whence) {
	uint32_t new_pos;
	if(whence > FAT_SEEK_END)
		return -1;
	switch(whence) {
		case FAT_SEEK_SET:
			if(offset < 0)
				return -1;
			new_pos = offset;
			break;
		case FAT_SEEK_CUR: {
			new_pos = getAbsolutePosFromHandle(file) + offset;
			if(new_pos > getTotalSizeFromHandle(file))
				return -1;
			break;
		}
		case FAT_SEEK_END: {
			if(offset > 0)
				return -1;
			new_pos = getTotalSizeFromHandle(file);
			new_pos += offset;
			/*underflow*/
			if(new_pos > getTotalSizeFromHandle(file))
				return -1;
			break;
		}
	}
	updateFileHandlePositionFromAbsolutePosition(file, new_pos);
	return 0;
}

int createDirFAT(const char* dirname) {
	int free_entry;
	int used_entry;
	used_entry = findDirEntry(dirname, &free_entry, FAT_DIRECTORY);
	if(free_entry == -1 && used_entry == -1)
		return -1;
	if(used_entry != -1)
		return 0;
	return initializeDirEntry(free_entry, dirname, FAT_DIRECTORY);
}

int eraseDirFAT(const char* dirname) {
	int entry_id = findDirEntry(dirname, NULL, FAT_DIRECTORY);
	DirectoryEntry* entry;
	if(entry_id == -1)
		return -1;
	entry = getEntryFromIndex(entry_id);
	if(entry->num_children > 0)
		return -1;
	if(entry->parent_directory != ROOT_WORKING_DIRECTORY)
		removeChildFromFolder(getEntryFromIndex(entry->parent_directory), entry_id);
	memset(entry, 0, sizeof(DirectoryEntry));
	return 0;
}

int changeDirFAT(const char* new_dirname) {
	int entry_id;
	/* Go up 1 folder */
	if(new_dirname[0] == '.' && new_dirname[1] == '.' && new_dirname[2] == '\0') {
		if(backing_disk.current_working_directory == ROOT_WORKING_DIRECTORY)
			return -1;
		backing_disk.current_working_directory = getEntryFromIndex(backing_disk.current_working_directory)->parent_directory;
		return 0;
	}
	/* Set working directory to root */
	if((new_dirname[0] == '/' || new_dirname[0] == '\\') && new_dirname[1] == '\0') {
		backing_disk.current_working_directory = ROOT_WORKING_DIRECTORY;
		return 0;
	}
	entry_id = findDirEntry(new_dirname, NULL, FAT_DIRECTORY);
	if(entry_id == -1)
		return -1;
	backing_disk.current_working_directory = entry_id;
	return 0;
}

static DirectoryElement tmp_folders[MAX_DIR_CHILDREN + 1];

static DirectoryElement* copyBufferToHeapAllocatedArray(const DirectoryElement* folders, size_t size) {
	DirectoryElement* arr = (DirectoryElement*)malloc(size * sizeof(DirectoryElement));
	memcpy(arr, folders, size * sizeof(DirectoryElement));
	return arr;
}

static DirectoryElement* getFoldersFromRoot() {
	DirectoryEntry* cur_entry;
	int i;
	int populated = 0;
	for(i = 0; i < TOTAL_FAT_ENTRIES && populated < MAX_DIR_CHILDREN; ++i) {
		cur_entry = getEntryFromIndex(i);
		if(cur_entry->filename[0] == 0)
			continue;
		if(cur_entry->parent_directory != ROOT_WORKING_DIRECTORY)
			continue;
		tmp_folders[populated].filename = cur_entry->filename;
		tmp_folders[populated].file_type = (DirectoryEntryType)cur_entry->file_type;
		++populated;
	}
	tmp_folders[populated].filename = NULL;
	return copyBufferToHeapAllocatedArray(tmp_folders, populated + 1);
}

DirectoryElement* listDirFAT() {
	int i;
	DirectoryEntry* current_directory;
	DirectoryEntry* current_child_entry;
	if(backing_disk.current_working_directory == ROOT_WORKING_DIRECTORY)
		return getFoldersFromRoot();
	current_directory = getEntryFromIndex(backing_disk.current_working_directory);
	for(i = 0; i < current_directory->num_children;) {
		if(current_directory->children[i] != DELETED_CHILD_ENTRY) {
			current_child_entry = getEntryFromIndex(current_directory->children[i]);
			tmp_folders[i].filename = current_child_entry->filename;
			tmp_folders[i].file_type = (DirectoryEntryType)current_child_entry->file_type;
		}
		++i;
	}
	tmp_folders[i].filename = NULL;
	return copyBufferToHeapAllocatedArray(tmp_folders, i + 1);
}

void freeDirList(DirectoryElement* list) {
	free(list);
}

