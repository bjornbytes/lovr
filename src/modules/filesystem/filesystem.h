#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define LOVR_PATH_MAX 1024

typedef struct Archive Archive;
typedef struct File File;

typedef enum {
  FILE_CREATE,
  FILE_DELETE,
  FILE_MODIFY,
  FILE_RENAME
} FileAction;

bool lovrFilesystemInit(void);
void lovrFilesystemDestroy(void);
bool lovrFilesystemSetSource(const char* source);
const char* lovrFilesystemGetSource(void);
bool lovrFilesystemIsFused(void);
void lovrFilesystemWatch(void);
void lovrFilesystemUnwatch(void);
bool lovrFilesystemMount(const char* path, const char* mountpoint, bool append, const char *root);
bool lovrFilesystemUnmount(const char* path);
const char* lovrFilesystemGetRealDirectory(const char* path);
bool lovrFilesystemIsFile(const char* path);
bool lovrFilesystemIsDirectory(const char* path);
bool lovrFilesystemGetSize(const char* path, uint64_t* size);
bool lovrFilesystemGetLastModified(const char* path, uint64_t* modtime);
void* lovrFilesystemRead(const char* path, size_t* size);
void lovrFilesystemGetDirectoryItems(const char* path, void (*callback)(void* context, const char* path), void* context);
const char* lovrFilesystemGetIdentity(void);
bool lovrFilesystemSetIdentity(const char* identity, bool precedence);
const char* lovrFilesystemGetSaveDirectory(void);
bool lovrFilesystemCreateDirectory(const char* path);
bool lovrFilesystemRemove(const char* path);
bool lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append);
size_t lovrFilesystemGetAppdataDirectory(char* buffer, size_t size);
size_t lovrFilesystemGetBundlePath(char* buffer, size_t size, const char** root);
size_t lovrFilesystemGetExecutablePath(char* buffer, size_t size);
size_t lovrFilesystemGetUserDirectory(char* buffer, size_t size);
size_t lovrFilesystemGetWorkingDirectory(char* buffer, size_t size);
const char* lovrFilesystemGetRequirePath(void);
void lovrFilesystemSetRequirePath(const char* requirePath);

// Archive

Archive* lovrArchiveCreate(const char* path, const char* mountpoint, const char* root);
void lovrArchiveDestroy(void* ref);

// File

typedef enum {
  OPEN_READ,
  OPEN_WRITE,
  OPEN_APPEND
} OpenMode;

File* lovrFileCreate(const char* path, OpenMode mode);
void lovrFileDestroy(void* ref);
const char* lovrFileGetPath(File* file);
OpenMode lovrFileGetMode(File* file);
bool lovrFileGetSize(File* file, uint64_t* size);
bool lovrFileRead(File* file, void* data, size_t size, size_t* count);
bool lovrFileWrite(File* file, const void* data, size_t size, size_t* count);
bool lovrFileSeek(File* file, uint64_t offset);
uint64_t lovrFileTell(File* file);
