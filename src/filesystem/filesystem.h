#ifndef LOVR_FILESYSTEM_TYPES
#define LOVR_FILESYSTEM_TYPES

#define LOVR_PATH_MAX 1024

typedef struct {
  const char* gameSource;
  const char* identity;
  char* savePathRelative;
  char* savePathFull;
} FilesystemState;

#endif

void lovrFilesystemInit(const char* arg0);
void lovrFilesystemDestroy();
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
