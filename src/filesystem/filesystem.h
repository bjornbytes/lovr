void lovrFilesystemInit(const char* arg0);
void lovrFilesystemDestroy();
int lovrFilesystemExists(const char* path);
const char* lovrFilesystemGetExecutablePath();
int lovrFilesystemIsDirectory(const char* path);
int lovrFilesystemIsFile(const char* path);
int lovrFilesystemSetSource(const char* source);
