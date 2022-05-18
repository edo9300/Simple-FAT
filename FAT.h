#ifndef FAT_H
#define FAT_H
#include <stddef.h> /*size_t*/
typedef void* FileHandle;

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