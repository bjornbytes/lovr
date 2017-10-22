#include <stdio.h>

#pragma once

#define LOVR_PATH_MAX 1024

typedef void getDirectoryItemsCallback(void* userdata, const char* dir, const char* file);

typedef struct {
  char* source;
  const char* identity;
  char* savePathRelative;
  char* savePathFull;
  int isFused;
} FilesystemState;

void lovrFilesystemInit(const char* arg0, const char* arg1);
void lovrFilesystemDestroy();
int lovrFilesystemCreateDirectory(const char* path);
int lovrFilesystemExists(const char* path);
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
int lovrFilesystemIsDirectory(const char* path);
int lovrFilesystemIsFile(const char* path);
int lovrFilesystemIsFused();
int lovrFilesystemMount(const char* path, const char* mountpoint, int append);
void* lovrFilesystemRead(const char* path, size_t* bytesRead);
int lovrFilesystemRemove(const char* path);
int lovrFilesystemSetIdentity(const char* identity);
int lovrFilesystemSetSource(const char* source);
int lovrFilesystemUnmount(const char* path);
size_t lovrFilesystemWrite(const char* path, const char* content, size_t size, int append);
