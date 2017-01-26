#pragma once

#define LOVR_PATH_MAX 1024

typedef struct {
  const char* gameSource;
  const char* identity;
  char* savePathRelative;
  char* savePathFull;
} FilesystemState;

void lovrFilesystemInit(const char* arg0);
void lovrFilesystemDestroy();
int lovrFilesystemAppend(const char* path, const char* content, int size);
int lovrFilesystemExists(const char* path);
int lovrFilesystemGetExecutablePath(char* dest, unsigned int size);
const char* lovrFilesystemGetIdentity();
const char* lovrFilesystemGetRealDirectory(const char* path);
const char* lovrFilesystemGetSource();
const char* lovrFilesystemGetUserDirectory();
int lovrFilesystemIsDirectory(const char* path);
int lovrFilesystemIsFile(const char* path);
void* lovrFilesystemRead(const char* path, int* bytesRead);
int lovrFilesystemSetIdentity(const char* identity);
int lovrFilesystemSetSource(const char* source);
int lovrFilesystemWrite(const char* path, const char* content, int size);
