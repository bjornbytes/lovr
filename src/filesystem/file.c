#include "file.h"

File* lovrFileCreate(const char* filename) {
  File* file = malloc(sizeof(File));
  file->filename = filename;
  file->mode = MODE_CLOSED;
  return file;
}

void lovrFileDestroy(File* file) {
  //
}

int lovrFileOpen(File* file, FileMode mode) {
  switch (mode) {
    case MODE_READ:
      if (!lovrFilesystemExists(file->filename)) {
        error(
      }

      file->handle = PHYSFS_openRead(file->filename);
      break;

    case MODE_WRITE:
      file->handle = PHYSFS_openWrite(file->filename);
      break;

    case MODE_APPEND:
      file->handle = PHYSFS_openAppend(file->filename);
      break;

    case MODE_CLOSED:
      break;
  }
}
