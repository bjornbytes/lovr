#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

#define LOVR_PATH_MAX 1024

#ifdef _WIN32
#define LOVR_PATH_SEP '\\'
#else
#define LOVR_PATH_SEP '/'
#endif

bool lovrFilesystemInit(const char* archive);
void lovrFilesystemDestroy(void);
const char* lovrFilesystemGetSource(void);
bool lovrFilesystemIsFused(void);
bool lovrFilesystemMount(const char* path, const char* mountpoint, bool append, const char *root);
bool lovrFilesystemUnmount(const char* path);
const char* lovrFilesystemGetRealDirectory(const char* path);
bool lovrFilesystemIsFile(const char* path);
bool lovrFilesystemIsDirectory(const char* path);
uint64_t lovrFilesystemGetSize(const char* path);
uint64_t lovrFilesystemGetLastModified(const char* path);
void* lovrFilesystemRead(const char* path, size_t bytes, size_t* bytesRead);
void lovrFilesystemGetDirectoryItems(const char* path, void (*callback)(void* context, const char* path), void* context);
const char* lovrFilesystemGetIdentity(void);
bool lovrFilesystemSetIdentity(const char* identity, bool precedence);
const char* lovrFilesystemGetSaveDirectory(void);
bool lovrFilesystemCreateDirectory(const char* path);
bool lovrFilesystemRemove(const char* path);
size_t lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append);
size_t lovrFilesystemGetAppdataDirectory(char* buffer, size_t size);
size_t lovrFilesystemGetExecutablePath(char* buffer, size_t size);
size_t lovrFilesystemGetUserDirectory(char* buffer, size_t size);
size_t lovrFilesystemGetWorkingDirectory(char* buffer, size_t size);
const char* lovrFilesystemGetRequirePath(void);
void lovrFilesystemSetRequirePath(const char* requirePath);
