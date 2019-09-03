#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define LOVR_PATH_MAX 1024

extern const char lovrDirSep;

typedef int (*getDirectoryItemsCallback)(void* userdata, const char* dir, const char* file);

bool lovrFilesystemInit(const char* argExe, const char* argGame, const char* argRoot);
void lovrFilesystemDestroy(void);
bool lovrFilesystemCreateDirectory(const char* path);
bool lovrFilesystemGetAppdataDirectory(char* dest, unsigned int size);
bool lovrFilesystemGetApplicationId(char* dest, size_t size);
void lovrFilesystemGetDirectoryItems(const char* path, getDirectoryItemsCallback callback, void* userdata);
int lovrFilesystemGetExecutablePath(char* path, uint32_t size);
const char* lovrFilesystemGetIdentity(void);
long lovrFilesystemGetLastModified(const char* path);
const char* lovrFilesystemGetRealDirectory(const char* path);
const char* lovrFilesystemGetRequirePath();
const char* lovrFilesystemGetCRequirePath();
const char* lovrFilesystemGetSaveDirectory(void);
size_t lovrFilesystemGetSize(const char* path);
const char* lovrFilesystemGetSource(void);
bool lovrFilesystemGetUserDirectory(char* buffer, size_t size);
bool lovrFilesystemGetWorkingDirectory(char* dest, unsigned int size);
bool lovrFilesystemIsDirectory(const char* path);
bool lovrFilesystemIsFile(const char* path);
bool lovrFilesystemIsFused(void);
bool lovrFilesystemMount(const char* path, const char* mountpoint, bool append, const char *root);
void* lovrFilesystemRead(const char* path, size_t bytes, size_t* bytesRead);
bool lovrFilesystemRemove(const char* path);
bool lovrFilesystemSetIdentity(const char* identity);
void lovrFilesystemSetRequirePath(const char* requirePath);
void lovrFilesystemSetCRequirePath(const char* requirePath);
bool lovrFilesystemUnmount(const char* path);
size_t lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append);
