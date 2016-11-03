#ifndef LOVR_FILE_TYPES
#define LOVR_FILE_TYPES

typedef enum {
  MODE_CLOSED,
  MODE_READ,
  MODE_WRITE,
  MODE_APPEND
} FileMode;

typedef struct {
  const char* filename;
  PHYSFS_file* handle;
  FileMode mode;
} File;

#endif

File* lovrFileCreate(const char* filename);
void lovrFileDestroy(File* file);
