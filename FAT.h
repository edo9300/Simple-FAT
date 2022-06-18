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
* Creates or opens the "virtual disk" used to store the files
* this will then be the disk used for the lifetime of the program.
* Returns an opaque pointer to the created disk on success
* NULL on error.
*/
FAT initFAT(const char* diskname, int anew);

/*
* Frees all the resources and flushes pending changes to the passed
* disk file, also frees the object.
*/
int terminateFAT(FAT fat);

/*
* Creates or open a file in the given fat with the passed name.
* The file is located in the current working directory set by changeDirFAT.
* Returns an opaque pointer to the file handle on success, NULL on error.
* This object has to be freed by freeHandle.
*/
Handle createFileFAT(FAT fat, const char* filename);

/*
* Frees the memory associated with the passed handle.
*/
void freeHandle(Handle handle);

/*
* Erases the file associated to the provided handle.
* The handle object still has to be freed afterwards.
*/
int eraseFileFAT(Handle file);

/*
* Writes *size* bytes from *in* to the passed file handle.
* Returns the number of written bytes.
*/
int writeFAT(Handle to, const void* in, size_t size);

/*
* Reads at most *size* bytes from the passed file handle 
* and writes them in the *out* buffer.
* Returns the number of read bytes.
*/
int readFAT(Handle from, void* out, size_t size);

/*
* Change the position of the cursor in the passed file handle.
*/
int seekFAT(Handle file, int32_t offset, SeekWhence whence);

/*
* Creates a directory in the given fat with the passed name.
* The folder is located in the current working directory set by changeDirFAT.
* Returns 0 if the directory is succesfully created or it already exists.
* Returns -1 on error.
*/
int createDirFAT(FAT fat, const char* dirname);

/*
* Erases a directory in the current working directory corresponding to the passed
* name.
* Returns 0 on success.
* Returns -1 if the folder currently has children or no folder with such name exists.
*/
int eraseDirFAT(FAT fat, const char* dirname);

/*
* Changes the current working directory to the provided one.
* Valid directory names are directories that are children of the current working directory,
* the .. directory corresponding to the parent of the current directory (if any),
* and / or \\ corresponding to the root of the filesystem.
* Returns 0 on success,
* Returns -1 on error.
*/
int changeDirFAT(FAT fat, const char* new_dirname);

/*
* Returns an array of DirectoryElement(s) containing the filename and the type of the element
* in the current directory.
* This array is terminated by an element whose filename field is NULL.
* This array has to be freed with freeDirList.
*/
DirectoryElement* listDirFAT(FAT fat);

/*
* Frees a DirectoryElement array returned by listDirFAT.
*/
void freeDirList(DirectoryElement* list);

#endif /*FAT_H*/
