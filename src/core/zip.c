#include "zip.h"

bool zip_open(zip* self, const char* path) {
  if (!fs_open(path, OPEN_READ, &self->file)) {
    return false;
  }

  return true;
}

bool zip_close(zip* self) {
  return fs_close(self->file);
}

bool zip_read(zip* self, const char* path, void* buffer, size_t* bytes) {
  return false;
}

bool zip_stat(zip* self, const char* path, FileInfo* info) {
  return false;
}

bool zip_list(zip* self, const char* path, zip_list_cb* callback) {
  return false;
}
