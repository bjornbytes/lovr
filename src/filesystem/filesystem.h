#include <stdio.h>

#pragma once

#define LOVR_PATH_MAX 1024

typedef void getDirectoryItemsCallback(void* userdata, const char* dir, const char* file);

typedef enum {
  OPEN_READ,
  OPEN_WRITE,
  OPEN_APPEND
} FileMode;

typedef void File;

typedef struct {
  char* source;
  const char* identity;
  char* savePathRelative;
  char* savePathFull;
  int isFused;
} FilesystemState;

void lovrFilesystemInit(const char* arg0, const char* arg1);
void lovrFilesystemDestroy();
void lovrFilesystemClose(File* file);
int lovrFilesystemCreateDirectory(const char* path);
int lovrFilesystemExists(const char* path);
void lovrFilesystemFileRead(File* file, void* data, size_t size, size_t count, size_t* bytesRead);
int lovrFilesystemGetAppdataDirectory(char* dest, unsigned int size);
void lovrFilesystemGetDirectoryItems(const char* path, getDirectoryItemsCallback callback, void* userdata);
int lovrFilesystemGetExecutablePath(char* dest, unsigned int size);
const char* lovrFilesystemGetIdentity();
long lovrFilesystemGetLastModified(const char* path);
const char* lovrFilesystemGetRealDirectory(const char* path);
const char* lovrFilesystemGetSaveDirectory();
size_t lovrFilesystemGetSize(File* file);
const char* lovrFilesystemGetSource();
const char* lovrFilesystemGetUserDirectory();
int lovrFilesystemIsDirectory(const char* path);
int lovrFilesystemIsFile(const char* path);
int lovrFilesystemIsFused();
int lovrFilesystemMount(const char* path, const char* mountpoint, int append);
File* lovrFilesystemOpen(const char* path, FileMode mode);
void* lovrFilesystemRead(const char* path, size_t* bytesRead);
int lovrFilesystemRemove(const char* path);
int lovrFilesystemSeek(File* file, size_t position);
int lovrFilesystemSetIdentity(const char* identity);
int lovrFilesystemSetSource(const char* source);
size_t lovrFilesystemTell(File* file);
int lovrFilesystemUnmount(const char* path);
int lovrFilesystemWrite(const char* path, const char* content, size_t size, int append);
