#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define LOVR_PATH_MAX 1024

typedef struct Archive Archive;
typedef struct File File;

typedef enum {
  MOUNT_READ,
  MOUNT_READWRITE
} MountMode;

bool lovrFilesystemInit(void);
void lovrFilesystemDestroy(void);
void lovrFilesystemSetSource(const char* source);
const char* lovrFilesystemGetSource(void);
bool lovrFilesystemIsFused(void);
bool lovrFilesystemMount(const char* path, const char* mountpoint, MountMode mode, bool append, const char *root);
bool lovrFilesystemUnmount(const char* path);
const char* lovrFilesystemGetRealDirectory(const char* path);
bool lovrFilesystemIsFile(const char* path);
bool lovrFilesystemIsDirectory(const char* path);
uint64_t lovrFilesystemGetSize(const char* path);
uint64_t lovrFilesystemGetLastModified(const char* path);
void* lovrFilesystemRead(const char* path, size_t* size);
bool lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append);
void lovrFilesystemGetDirectoryItems(const char* path, void (*callback)(void* context, const char* path), void* context);
bool lovrFilesystemCreateDirectory(const char* path);
bool lovrFilesystemRemove(const char* path);
const char* lovrFilesystemGetIdentity(void);
bool lovrFilesystemSetIdentity(const char* identity, bool precedence);
const char* lovrFilesystemGetSaveDirectory(void);
size_t lovrFilesystemGetAppdataDirectory(char* buffer, size_t size);
size_t lovrFilesystemGetBundlePath(char* buffer, size_t size, const char** root);
size_t lovrFilesystemGetExecutablePath(char* buffer, size_t size);
size_t lovrFilesystemGetUserDirectory(char* buffer, size_t size);
size_t lovrFilesystemGetWorkingDirectory(char* buffer, size_t size);
const char* lovrFilesystemGetRequirePath(void);
void lovrFilesystemSetRequirePath(const char* requirePath);

// Archive

Archive* lovrArchiveCreate(const char* path, const char* mountpoint, MountMode mode, const char* root);
void lovrArchiveDestroy(void* ref);

// File

typedef enum {
  OPEN_READ,
  OPEN_WRITE,
  OPEN_APPEND
} OpenMode;

File* lovrFileCreate(const char* path, OpenMode mode, const char** error);
void lovrFileDestroy(void* ref);
const char* lovrFileGetPath(File* file);
OpenMode lovrFileGetMode(File* file);
uint64_t lovrFileGetSize(File* file);
bool lovrFileRead(File* file, void* data, size_t size, size_t* count);
bool lovrFileWrite(File* file, const void* data, size_t size, size_t* count);
bool lovrFileSeek(File* file, uint64_t offset);
uint64_t lovrFileTell(File* file);
