#ifndef FAT_H
#define FAT_H
#include <stddef.h> /*size_t, NULL*/
#include <stdint.h> /*int_t types*/

typedef void* FAT;
typedef void* Handle;

typedef enum SeekWhence {
	FAT_SEEK_SET,
	FAT_SEEK_CUR,
	FAT_SEEK_END
} SeekWhence;

typedef enum DirectoryEntryType {
	FAT_FILE,
	FAT_DIRECTORY
} DirectoryEntryType;

typedef struct DirectoryElement {
	const char* filename;
	DirectoryEntryType file_type;
} DirectoryElement;

/*
* Creates/opens the "virtual disk" used to store the files
* this will then be the disk used for the lifetime of the program
* returns 0 on success, otherwise error
*/
FAT initFAT(const char* diskname, int anew);

/*
* Frees all the resources and flushes pending changes to the backing
* file
*/
int terminateFAT(FAT fat);

Handle createFileFAT(FAT fat, const char* filename);
void freeHandle(Handle handle);
int eraseFileFAT(Handle file);
int writeFAT(Handle to, const void* in, size_t size);
int readFAT(Handle from, void* out, size_t size);
int seekFAT(Handle file, int32_t offset, SeekWhence whence);
int createDirFAT(FAT fat, const char* dirname);
int eraseDirFAT(FAT fat, const char* dirname);
int changeDirFAT(FAT fat, const char* new_dirname);
DirectoryElement* listDirFAT(FAT fat);
void freeDirList(DirectoryElement* list);

#endif /*FAT_H*/
