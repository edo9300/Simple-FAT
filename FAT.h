#ifndef FAT_H
#define FAT_H
#include <stddef.h> /*size_t, NULL*/
#include <stdint.h> /*int_t types*/

typedef struct FileHandle {
	uint32_t current_pos;
	uint32_t current_block_index;
	uint32_t directory_entry;
} FileHandle;

typedef enum SeekWhence {
	FAT_SEEK_SET,
	FAT_SEEK_CUR,
	FAT_SEEK_END
} SeekWhence;

/*
* Creates/opens the "virtual disk" used to store the files
* this will then be the disk used for the lifetime of the program
* returns 0 on success, otherwise error
*/
int initFAT(const char* diskname, int anew);

/*
* Frees all the resources and flushes pending changes to the backing
* file
*/
int terminateFAT(void);

FileHandle* createFileFAT(const char* filename, FileHandle* handle);
int eraseFileFAT(FileHandle* file);
int writeFAT(FileHandle* to, const void* in, size_t size);
int readFAT(FileHandle* from, void* out, size_t size);
int seekFAT(FileHandle* file, int32_t offset, SeekWhence whence);
int createDirFAT(const char* dirname);
int eraseDirFAT(const char* dirname);
int changeDirFAT(const char* new_dirname);
int listDirFAT(const char* dirname);

#endif /*FAT_H*/
