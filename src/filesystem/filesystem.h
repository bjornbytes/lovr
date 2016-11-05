void lovrFilesystemInit(const char* arg0);
void lovrFilesystemDestroy();
int lovrFilesystemExists(const char* path);
const char* lovrFilesystemGetExecutablePath();
int lovrFilesystemIsDirectory(const char* path);
int lovrFilesystemIsFile(const char* path);
void* lovrFilesystemRead(const char* path, int* bytesRead);
int lovrFilesystemSetSource(const char* source);
int lovrFilesystemWrite(const char* path, const char* contents, int size);
