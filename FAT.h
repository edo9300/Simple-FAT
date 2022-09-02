#ifndef FAT_H
#define FAT_H
#include <stddef.h> /*size_t, NULL*/
#include <stdint.h> /*int_t types*/

/*
* Handle representing a virtual disk that was opened by the program.
*/
typedef void* FAT;
/*
* Handle representing a file that was opened within a FAT.
*/
typedef void* Handle;
/*
* Arguments to pass to seekFAT.
*/
typedef enum SeekWhence {
	/*
	* Seeks starting from the beginning of the file
	*/
	FAT_SEEK_SET,
	/*
	* Seeks starting from the current position in the file
	* of the file Handle
	*/
	FAT_SEEK_CUR,
	/*
	* Seeks starting from the end of the file (positive values go "back" in the file)
	*/
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
* Creates or opens a virtual disk at the provided path.
* If anew is a nonzero value and a file with the passed name already exists,
* OR no file with that name exists,
* a new disk is created (overwritting the already existing file in case).
* If a file with that name already exists, and anew is 0, that file is opened as a disk
* Returns a FAT handle to the opened disk on success
* NULL on error.
*/
FAT initFAT(const char* diskname, int anew);

/*
* Frees all the resources and flushes pending changes for the passed FAT
* handle.
* No file Handle or DirectoryElement array is freed by this function, they
* MUST be manually freed under any circumstance.
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
* Frees the memory associated with the passed file Handle.
*/
void freeHandle(Handle handle);

/*
* Erases a file in the current working directory corresponding to the passed
* name.
* Returns 0 on success.
* Returns -1 on error.
*/
int eraseFileFAT(FAT fat, const char* filename);

/*
* Erases the file associated to the provided handle.
* (name inspired by the posix functions working on file handles)
* The handle object still has to be freed afterwards.
*/
int eraseFileFATAt(Handle file);

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
* This object has to be freed with freeDirList.
*/
DirectoryElement* listDirFAT(FAT fat);

/*
* Frees a DirectoryElement array returned by listDirFAT.
*/
void freeDirList(DirectoryElement* list);

#endif /*FAT_H*/
