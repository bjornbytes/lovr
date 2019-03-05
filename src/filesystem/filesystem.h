#include "lib/vec/vec.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#pragma once

#define LOVR_PATH_MAX 1024

extern const char lovrDirSep;

typedef int getDirectoryItemsCallback(void* userdata, const char* dir, const char* file);

typedef struct {
  bool initialized;
  char* source;
  const char* identity;
  char* savePathRelative;
  char* savePathFull;
  bool isFused;
  char* requirePath[2];
  vec_str_t requirePattern[2];
} FilesystemState;

bool lovrFilesystemInit(const char* argExe, const char* argGame, const char* argRoot);
void lovrFilesystemDestroy(void);
int lovrFilesystemCreateDirectory(const char* path);
int lovrFilesystemGetAppdataDirectory(char* dest, unsigned int size);
void lovrFilesystemGetDirectoryItems(const char* path, getDirectoryItemsCallback callback, void* userdata);
int lovrFilesystemGetExecutablePath(char* path, uint32_t size);
const char* lovrFilesystemGetIdentity(void);
long lovrFilesystemGetLastModified(const char* path);
const char* lovrFilesystemGetRealDirectory(const char* path);
vec_str_t* lovrFilesystemGetRequirePath(void);
vec_str_t* lovrFilesystemGetCRequirePath(void);
const char* lovrFilesystemGetSaveDirectory(void);
size_t lovrFilesystemGetSize(const char* path);
const char* lovrFilesystemGetSource(void);
const char* lovrFilesystemGetUserDirectory(void);
int lovrFilesystemGetWorkingDirectory(char* dest, unsigned int size);
bool lovrFilesystemIsDirectory(const char* path);
bool lovrFilesystemIsFile(const char* path);
bool lovrFilesystemIsFused(void);
int lovrFilesystemMount(const char* path, const char* mountpoint, bool append, const char *root);
void* lovrFilesystemRead(const char* path, size_t bytes, size_t* bytesRead);
int lovrFilesystemRemove(const char* path);
int lovrFilesystemSetIdentity(const char* identity);
void lovrFilesystemSetRequirePath(const char* requirePath);
void lovrFilesystemSetCRequirePath(const char* requirePath);
int lovrFilesystemUnmount(const char* path);
size_t lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append);
