#ifndef FAT_H
#define FAT_H
#include <stddef.h> /*size_t*/
typedef void* FileHandle;

/*
* Creates/opens the "virtual disk" used to store the files
* this will then be the disk used for the lifetime of the program
* returns 0 on success, otherwise error
*/
int initFAT(const char* diskname);

FileHandle createFile(const char* filename);
int eraseFile(FileHandle file);
int write(FileHandle to, const void* in, size_t size);
int read(FileHandle from, void* out, size_t size);
int seek(FileHandle file, size_t offset, int whence);
int createDir(const char* dirname);
int eraseDir(const char* dirname);
int changeDir(const char* new_dirname);
int listDir(const char* dirname);

#endif /*FAT_H*/