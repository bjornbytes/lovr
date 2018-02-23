#include <stdio.h>
#include <stdbool.h>

#pragma once

#define LOVR_PATH_MAX 1024

typedef int getDirectoryItemsCallback(void* userdata, const char* dir, const char* file);

typedef struct {
  bool initialized;
  char* source;
  const char* identity;
  char* savePathRelative;
  char* savePathFull;
  bool isFused;
} FilesystemState;

void lovrFilesystemInit(const char* arg0, const char* arg1);
void lovrFilesystemDestroy();
int lovrFilesystemCreateDirectory(const char* path);
int lovrFilesystemGetAppdataDirectory(char* dest, unsigned int size);
void lovrFilesystemGetDirectoryItems(const char* path, getDirectoryItemsCallback callback, void* userdata);
int lovrFilesystemGetExecutablePath(char* dest, unsigned int size);
const char* lovrFilesystemGetIdentity();
long lovrFilesystemGetLastModified(const char* path);
const char* lovrFilesystemGetRealDirectory(const char* path);
const char* lovrFilesystemGetSaveDirectory();
size_t lovrFilesystemGetSize(const char* path);
const char* lovrFilesystemGetSource();
const char* lovrFilesystemGetUserDirectory();
bool lovrFilesystemIsDirectory(const char* path);
bool lovrFilesystemIsFile(const char* path);
bool lovrFilesystemIsFused();
int lovrFilesystemMount(const char* path, const char* mountpoint, bool append);
void* lovrFilesystemRead(const char* path, size_t* bytesRead);
int lovrFilesystemRemove(const char* path);
int lovrFilesystemSetIdentity(const char* identity);
int lovrFilesystemSetSource(const char* source);
int lovrFilesystemUnmount(const char* path);
size_t lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append);
