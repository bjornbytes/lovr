#include "fs.h"
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

bool fs_open(const char* path, FileMode mode, fs_handle* file) {
  int flags;
  switch (mode) {
    case OPEN_READ: flags = O_RDONLY; break;
    case OPEN_WRITE: flags = O_WRONLY | O_CREAT | O_TRUNC; break;
    case OPEN_APPEND: flags = O_WRONLY | O_CREAT; break;
    default: return false;
  }
  int fd = open(path, flags, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    return false;
  }
  *file = fd;
  return true;
}

bool fs_close(fs_handle handle) {
  return close(handle) == 0;
}

bool fs_read(fs_handle file, void* buffer, size_t* bytes) {
  ssize_t result = read(file, buffer, *bytes);
  if (result < 0) {
    *bytes = 0;
    return false;
  } else {
    *bytes = (size_t) result;
    return true;
  }
}

bool fs_write(fs_handle file, void* buffer, size_t* bytes) {
  ssize_t result = write(file, buffer, *bytes);
  if (result < 0) {
    *bytes = 0;
    return false;
  } else {
    *bytes = (size_t) result;
    return true;
  }
}

bool fs_stat(const char* path, FileInfo* info) {
  struct stat stats;
  if (stat(path, &stats)) {
    return false;
  }

  if (info) {
    info->size = (uint64_t) stats.st_size;
    info->lastModified = (uint64_t) stats.st_mtimespec.tv_sec;
    info->type = (stats.st_mode & S_IFDIR) ? FILE_DIRECTORY : FILE_REGULAR;
  }

  return true;
}

bool fs_remove(const char* path) {
  return unlink(path) == 0 || rmdir(path) == 0;
}

// Cannot be used with string literals
// TODO access / statat instead of fs_stat
bool fs_mkdir(const char* path) {
  const char* cursor = path;
  bool making = false;

  while (1) {
    char* slash = strchr(cursor, '/');
    if (slash) {
      *slash = '\0';
    }

    if (!making) {
      making = !fs_stat(path, NULL);
    }

    if (making) {
      if (mkdir(path, S_IRWXU)) {
        return false;
      }
    }

    if (slash) {
      *slash = '/';
      cursor = slash + 1;
    } else {
      break;
    }
  }

  return true;
}

bool fs_list(const char* path, fs_list_cb* callback, void* context) {
  DIR* dir = opendir(path);
  if (!dir) {
    return false;
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    callback(context, entry->d_name);
  }

  closedir(dir);
  return true;
}
