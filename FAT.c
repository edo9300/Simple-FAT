#include "FAT.h"
#include <stddef.h> /*size_t, NULL*/
#include <stdint.h> /*int_t types*/
#include <fcntl.h> /*open*/
#include <unistd.h> /*close, ftruncate*/
#include <sys/mman.h> /*mmap, munmap, msync*/
#include <string.h> /*memcpy, strncpy, strncmp*/
#include <errno.h> /*errno*/
#include <malloc.h> /*malloc*/
#include <assert.h> /*assert*/

#define TOTAL_BLOCKS 1024
#define BLOCK_BUFFER_SIZE 512
#define DIRECTORY_ENTRY_MAX_NAME 256
#define TOTAL_DIR_ENTRIES 256
#define MAX_DIR_CHILDREN 64

#define DELETED_CHILD_ENTRY UINT16_MAX
#define FREE_CHILD_ENTRY 0

#define UNUSED_FAT_ENTRY UINT32_MAX
#define LAST_FAT_ENTRY (UINT32_MAX - 1)

#define ROOT_WORKING_DIRECTORY 0

typedef struct DirectoryEntry {
	char filename[DIRECTORY_ENTRY_MAX_NAME];
	uint8_t file_type;
	uint8_t num_children;
	uint16_t parent_directory;
	uint32_t size;
	uint32_t first_fat_entry;
	uint16_t children[MAX_DIR_CHILDREN];
} DirectoryEntry;

typedef struct FileBlock {
	char buffer[BLOCK_BUFFER_SIZE];
} FileBlock;

typedef struct FATTable {
	uint32_t entries[TOTAL_BLOCKS];
} FATTable;

typedef struct DirectoryTable {
	DirectoryEntry entries[TOTAL_DIR_ENTRIES];
} DirectoryTable;

typedef struct Disk {
	FATTable fat;
	DirectoryTable directories;
	FileBlock blocks[TOTAL_BLOCKS];
} Disk;

typedef struct FATBackingDisk {
	size_t currently_mapped_size;
	Disk* mmapped_disk;
	int mmapped_file_descriptor;
	uint16_t current_working_directory;
} FATBackingDisk;

typedef struct FileHandle {
	uint32_t current_pos;
	uint32_t current_block_index;
	uint32_t directory_entry;
	FAT backing_disk;
} FileHandle;

static void setupRootDir(FATBackingDisk* disk);

FAT initFAT(const char* diskname, int anew) {
	int prev_errno;
	int descriptor;
	int flags = O_CREAT | O_RDWR;
	FATBackingDisk* backing_disk = (FATBackingDisk*)malloc(sizeof(FATBackingDisk));
	if(backing_disk == NULL)
		goto error;
	if(anew)
		flags |= O_TRUNC;
	backing_disk->mmapped_file_descriptor = open(diskname, flags, 0666);
	if(backing_disk->mmapped_file_descriptor == -1)
		goto error;
	if(anew) {
		if(ftruncate(backing_disk->mmapped_file_descriptor, sizeof(Disk)) != 0)
			goto error;
	}
	backing_disk->mmapped_disk = (Disk*)mmap(NULL,
												sizeof(Disk),
												PROT_READ | PROT_WRITE,
												MAP_SHARED,
												backing_disk->mmapped_file_descriptor,
												0);
	if(backing_disk->mmapped_disk == MAP_FAILED)
		goto error;
	if(anew) {
		memset(&(backing_disk->mmapped_disk->fat), 0xff, sizeof(FATTable));
		setupRootDir(backing_disk);
	}
	backing_disk->currently_mapped_size = sizeof(FATTable) + sizeof(FileBlock) * TOTAL_BLOCKS;
	backing_disk->current_working_directory = ROOT_WORKING_DIRECTORY;
	return backing_disk;
error:
	if(backing_disk == NULL) /* Nothing to free */
		return NULL;
	descriptor = backing_disk->mmapped_file_descriptor;
	free(backing_disk);
	if(descriptor == -1)
		return NULL;
	prev_errno = errno;
	close(descriptor);
	errno = prev_errno;
	return NULL;
}

int terminateFAT(FAT fat) {
	int has_err;
	int err;
	FATBackingDisk* backing_disk = (FATBackingDisk*)fat;
	has_err = err = msync(backing_disk->mmapped_disk, backing_disk->currently_mapped_size, MS_SYNC);
	err = munmap(backing_disk->mmapped_disk, backing_disk->currently_mapped_size);
	if(err != 0)
		has_err = err;
	err = close(backing_disk->mmapped_file_descriptor);
	if(err != 0)
		has_err = err;
	free(fat);
	return has_err;
}

#define getEntryFromIndex(index) (&(backing_disk->mmapped_disk->directories.entries[index]))
#define getBlockFromIndex(index) (&(backing_disk->mmapped_disk->blocks[index]))
#define getNextFatEntry(entry) (backing_disk->mmapped_disk->fat.entries[entry])
#define setNextFatEntry(entry,to) do { backing_disk->mmapped_disk->fat.entries[entry] = (uint32_t)to; } while(0)

static void setupRootDir(FATBackingDisk* backing_disk) {
	DirectoryEntry* entry = getEntryFromIndex(ROOT_WORKING_DIRECTORY);
	entry->filename[0] = '/';
	entry->filename[1] = '\0';
}

static int findDirEntry(FATBackingDisk* backing_disk, const char* filename, int* free, DirectoryEntryType file_type) {
	DirectoryEntry* cur_entry;
	int i;
	int found_free = -1;
	/*
	* We skip the root directory as it should not be touchable from the various functions
	*/
	for(i = 1; i < TOTAL_DIR_ENTRIES; ++i) {
		cur_entry = getEntryFromIndex(i);
		if(cur_entry->filename[0] == 0) {
			if(found_free == -1)
				found_free = i;
			continue;
		}
		if(cur_entry->parent_directory == backing_disk->current_working_directory &&
		   strncmp(filename, cur_entry->filename, sizeof(cur_entry->filename)) == 0) {
			if(cur_entry->file_type != file_type) {
				found_free = -1;
				break;
			}
			return i;
		}
	}
	if(getEntryFromIndex(backing_disk->current_working_directory)->num_children >= MAX_DIR_CHILDREN)
		found_free = -1;
	if(free)
		*free = found_free;
	return -1;
}

static int findFreeBlock(FATBackingDisk* backing_disk) {
	int i;
	for(i = 0; i < TOTAL_BLOCKS; ++i) {
		if(backing_disk->mmapped_disk->fat.entries[i] == UNUSED_FAT_ENTRY)
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
	assert(i < MAX_DIR_CHILDREN);
	++(parent->num_children);
}

static int initializeDirEntry(FATBackingDisk* backing_disk, int entry_id, const char* filename, DirectoryEntryType file_type) {
	int new_fat_entry = 0;
	DirectoryEntry* entry;
	if(file_type != FAT_DIRECTORY) {
		new_fat_entry = findFreeBlock(backing_disk);
		if(new_fat_entry == -1)
			return -1;
		setNextFatEntry(new_fat_entry, LAST_FAT_ENTRY);
	}
	entry = getEntryFromIndex(entry_id);
	strncpy(&entry->filename[0], filename, sizeof(entry->filename));
	entry->first_fat_entry = (uint32_t)new_fat_entry;
	entry->size = 0;
	entry->file_type = (uint8_t)file_type;
	entry->parent_directory = backing_disk->current_working_directory;
	addChildToFolder(getEntryFromIndex(entry->parent_directory), (uint16_t)entry_id);
	if(file_type == FAT_DIRECTORY) {
		entry->num_children = 0;
		memset(entry->children, 0, sizeof(entry->children));
	}
	return 0;
}

Handle createFileFAT(FAT fat, const char* filename) {
	int free_entry;
	int used_entry;
	FATBackingDisk* backing_disk = (FATBackingDisk*)fat;
	FileHandle* handle;
	used_entry = findDirEntry(backing_disk, filename, &free_entry, FAT_FILE);
	if(used_entry == -1 && free_entry == -1)
		return NULL;
	handle = (FileHandle*)malloc(sizeof(FileHandle));
	if(used_entry != -1) {
		handle->current_pos = 0;
		handle->current_block_index = 0;
		handle->directory_entry = (uint32_t)used_entry;
	} else {
		if(initializeDirEntry(backing_disk, free_entry, filename, FAT_FILE) == -1) {
			free(handle);
			return NULL;
		}
		handle->current_pos = 0;
		handle->current_block_index = 0;
		handle->directory_entry = (uint32_t)free_entry;
	}
	handle->backing_disk = fat;
	return handle;
}

void freeHandle(Handle handle) {
	free(handle);
}

#define getBackingDiskFromHandle(handle) ((FATBackingDisk*)handle->backing_disk)
#define getDirectoryEntryFromHandle(handle) getEntryFromIndex(handle->directory_entry)
#define getFirstFatEntryFromDirectoryEntry(entry) (entry->first_fat_entry)

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

int eraseFileFAT(FAT fat, const char* filename) {
	DirectoryEntry* entry;
	uint32_t current_fat_entry;
	uint32_t new_fat_entry;
	FATBackingDisk* backing_disk = (FATBackingDisk*)fat;
	int entry_id = findDirEntry(backing_disk, filename, NULL, FAT_FILE);
	if(entry_id == -1)
		return -1;
	entry = getEntryFromIndex(entry_id);
	current_fat_entry = getFirstFatEntryFromDirectoryEntry(entry);
	while(current_fat_entry != LAST_FAT_ENTRY) {
		assert(current_fat_entry != UNUSED_FAT_ENTRY);
		new_fat_entry = getNextFatEntry(current_fat_entry);
		setNextFatEntry(current_fat_entry, UNUSED_FAT_ENTRY);
		current_fat_entry = new_fat_entry;
	}
	if(entry->parent_directory != ROOT_WORKING_DIRECTORY)
		removeChildFromFolder(getEntryFromIndex(entry->parent_directory), (uint16_t)entry_id);
	memset(entry, 0, sizeof(DirectoryEntry));
	return 0;
}

int eraseFileFATAt(Handle file) {
	uint32_t current_fat_entry;
	uint32_t new_fat_entry;
	FileHandle* handle = (FileHandle*)file;
	FATBackingDisk* backing_disk = getBackingDiskFromHandle(handle);
	DirectoryEntry* entry = getDirectoryEntryFromHandle(handle);
	current_fat_entry = getFirstFatEntryFromDirectoryEntry(entry);
	while(current_fat_entry != LAST_FAT_ENTRY) {
		assert(current_fat_entry != UNUSED_FAT_ENTRY);
		new_fat_entry = getNextFatEntry(current_fat_entry);
		setNextFatEntry(current_fat_entry, UNUSED_FAT_ENTRY);
		current_fat_entry = new_fat_entry;
	}
	if(entry->parent_directory != ROOT_WORKING_DIRECTORY)
		removeChildFromFolder(getEntryFromIndex(entry->parent_directory), (uint16_t)handle->directory_entry);
	memset(entry, 0, sizeof(DirectoryEntry));
	return 0;
}

static FileBlock* getCurrentBlockFromHandle(FileHandle* handle, uint32_t* return_fat_entry) {
	uint32_t i;
	FATBackingDisk* backing_disk = getBackingDiskFromHandle(handle);
	DirectoryEntry* entry = getDirectoryEntryFromHandle(handle);
	uint32_t current_fat_entry = getFirstFatEntryFromDirectoryEntry(entry);
	for(i = 0; i < handle->current_block_index; i++) {
		assert(current_fat_entry != UNUSED_FAT_ENTRY);
		if(current_fat_entry == LAST_FAT_ENTRY)
			return NULL;
		current_fat_entry = getNextFatEntry(current_fat_entry);
	}
	*return_fat_entry = current_fat_entry;
	return getBlockFromIndex(current_fat_entry);
}

static FileBlock* getOrAllocateNewBlock(FATBackingDisk* backing_disk, uint32_t* current_fat_entry) {
	FileBlock* new_block;
	int new_block_index;
	uint32_t next_fat_entry = getNextFatEntry(*current_fat_entry);
	if(next_fat_entry != LAST_FAT_ENTRY) {
		*current_fat_entry = next_fat_entry;
		return getBlockFromIndex(next_fat_entry);
	}
	new_block_index = findFreeBlock(backing_disk);
	if(new_block_index == -1)
		return NULL;
	setNextFatEntry(*current_fat_entry, new_block_index);
	*current_fat_entry = (uint32_t)new_block_index;
	setNextFatEntry(new_block_index, LAST_FAT_ENTRY);
	new_block = getBlockFromIndex(new_block_index);
	memset(new_block, 0, sizeof(new_block->buffer));
	return new_block;
}

#define getAbsolutePosFromHandle(handle) ((handle->current_block_index * BLOCK_BUFFER_SIZE) + handle->current_pos)
#define getTotalSizeFromHandle(handle) (getDirectoryEntryFromHandle(handle)->size)

#define updateFileHandlePositionFromAbsolutePosition(handle, absolute_pos)\
do {\
	handle->current_block_index = absolute_pos / BLOCK_BUFFER_SIZE;\
	handle->current_pos = absolute_pos % BLOCK_BUFFER_SIZE;\
} while(0)

int writeFAT(Handle to, const void* in, size_t size) {
	uint32_t current_fat_entry;
	FileHandle* handle = (FileHandle*)to;
	size_t written = 0;
	uint32_t pos = handle->current_pos;
	FATBackingDisk* backing_disk = getBackingDiskFromHandle(handle);
	uint32_t absolute_pos = getAbsolutePosFromHandle(handle);
	FileBlock* block = getCurrentBlockFromHandle(handle, &current_fat_entry);
	const char* cur = (const char*)in;
	size_t to_write;
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
			if((block = getOrAllocateNewBlock(backing_disk, &current_fat_entry)) == NULL)
				break;
			++iterated_blocks;
		}
	}
	handle->current_pos = pos;
	handle->current_block_index += iterated_blocks;
	if(absolute_pos > getTotalSizeFromHandle(handle))
		getTotalSizeFromHandle(handle) = absolute_pos;
	return (int)written;
}

int readFAT(Handle from, void* out, size_t size) {
	uint32_t current_fat_entry;
	FileHandle* handle = (FileHandle*)from;
	FATBackingDisk* backing_disk = getBackingDiskFromHandle(handle);
	uint32_t absolute_pos = getAbsolutePosFromHandle(handle);
	uint32_t file_size = getTotalSizeFromHandle(handle);
	uint32_t total_read = 0;
	FileBlock* block = getCurrentBlockFromHandle(handle, &current_fat_entry);
	char* cur = (char*)out;
	size_t to_read;
	uint32_t iterated_blocks = 0;
	uint32_t pos = handle->current_pos;
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
			if((block = getOrAllocateNewBlock(backing_disk, &current_fat_entry)) == NULL)
				break;
			++iterated_blocks;
		}
	}
	handle->current_pos = pos;
	handle->current_block_index += iterated_blocks;
	return (int)total_read;
}

int seekFAT(Handle file, int32_t offset, SeekWhence whence) {
	int64_t new_pos;
	FileHandle* handle = (FileHandle*)file;
	FATBackingDisk* backing_disk = getBackingDiskFromHandle(handle);
	if(whence > FAT_SEEK_END)
		return -1;
	switch(whence) {
		case FAT_SEEK_SET:
			if(offset < 0)
				return -1;
			new_pos = offset;
			break;
		case FAT_SEEK_CUR: {
			new_pos = getAbsolutePosFromHandle(handle) + (int64_t)offset;
			if(new_pos > getTotalSizeFromHandle(handle))
				return -1;
			break;
		}
		case FAT_SEEK_END: {
			if(offset > 0)
				return -1;
			new_pos = getTotalSizeFromHandle(handle);
			new_pos += offset;
			/*underflow*/
			if(new_pos > getTotalSizeFromHandle(handle))
				return -1;
			break;
		}
	}
	updateFileHandlePositionFromAbsolutePosition(handle, (uint32_t)new_pos);
	return 0;
}

int createDirFAT(FAT fat, const char* dirname) {
	int free_entry;
	int used_entry;
	FATBackingDisk* backing_disk = (FATBackingDisk*)fat;
	used_entry = findDirEntry(backing_disk, dirname, &free_entry, FAT_DIRECTORY);
	if(free_entry == -1 && used_entry == -1)
		return -1;
	if(used_entry != -1)
		return 0;
	return initializeDirEntry(backing_disk, free_entry, dirname, FAT_DIRECTORY);
}

int eraseDirFAT(FAT fat, const char* dirname) {
	DirectoryEntry* entry;
	FATBackingDisk* backing_disk = (FATBackingDisk*)fat;
	int entry_id = findDirEntry(backing_disk, dirname, NULL, FAT_DIRECTORY);
	if(entry_id == -1)
		return -1;
	entry = getEntryFromIndex(entry_id);
	if(entry->num_children > 0)
		return -1;
	if(entry->parent_directory != ROOT_WORKING_DIRECTORY)
		removeChildFromFolder(getEntryFromIndex(entry->parent_directory), (uint16_t)entry_id);
	memset(entry, 0, sizeof(DirectoryEntry));
	return 0;
}

int changeDirFAT(FAT fat, const char* new_dirname) {
	int entry_id;
	FATBackingDisk* backing_disk = (FATBackingDisk*)fat;
	/* Go up 1 folder */
	if(new_dirname[0] == '.' && new_dirname[1] == '.' && new_dirname[2] == '\0') {
		if(backing_disk->current_working_directory == ROOT_WORKING_DIRECTORY)
			return -1;
		backing_disk->current_working_directory = getEntryFromIndex(backing_disk->current_working_directory)->parent_directory;
		return 0;
	}
	/* Set working directory to root */
	if((new_dirname[0] == '/' || new_dirname[0] == '\\') && new_dirname[1] == '\0') {
		backing_disk->current_working_directory = ROOT_WORKING_DIRECTORY;
		return 0;
	}
	entry_id = findDirEntry(backing_disk, new_dirname, NULL, FAT_DIRECTORY);
	if(entry_id == -1)
		return -1;
	backing_disk->current_working_directory = (uint16_t)entry_id;
	return 0;
}

static DirectoryElement tmp_folders[MAX_DIR_CHILDREN + 1];

static DirectoryElement* copyBufferToHeapAllocatedArray(const DirectoryElement* folders, size_t size) {
	DirectoryElement* arr = (DirectoryElement*)malloc(size * sizeof(DirectoryElement));
	memcpy(arr, folders, size * sizeof(DirectoryElement));
	return arr;
}

DirectoryElement* listDirFAT(FAT fat) {
	size_t i;
	DirectoryEntry* current_directory;
	DirectoryEntry* current_child_entry;
	FATBackingDisk* backing_disk = (FATBackingDisk*)fat;
	current_directory = getEntryFromIndex(backing_disk->current_working_directory);
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
	if(list)
		free(list);
}

