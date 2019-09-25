#include "fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>

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

bool fs_write(fs_handle file, const void* buffer, size_t* bytes) {
  ssize_t result = write(file, buffer, *bytes);
  if (result < 0) {
    *bytes = 0;
    return false;
  } else {
    *bytes = (size_t) result;
    return true;
  }
}

bool fs_seek(fs_handle file, int64_t offset, SeekMode origin) {
  int whence;
  switch (origin) {
    case FROM_START: whence = SEEK_SET; break;
    case FROM_CURRENT: whence = SEEK_CUR; break;
    case FROM_END: whence = SEEK_END; break;
    default: return false;
  }
  return lseek(file, offset, whence) >= 0;
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

bool fs_mkdir(const char* path) {
  return mkdir(path, S_IRWXU) == 0;
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

static size_t copy(char* buffer, size_t size, const char* string, size_t length) {
  if (length < size - 1) {
    memcpy(buffer, string, length);
    buffer[length + 1] = '\0';
    return length;
  }
  return 0;
}

size_t fs_getUserDirectory(char* buffer, size_t size) {
  const char* home = getenv("HOME");

  if (!home) {
    struct passwd* entry = getpwuid(getuid());
    if (!entry) {
      return 0;
    }
    home = entry->pw_dir;
  }

  return copy(buffer, size, home, strlen(home));
}

size_t fs_getDataDirectory(char* buffer, size_t size) {
#if __APPLE__
  size_t cursor = fs_getUserDirectory(buffer, size);

  if (cursor > 0) {
    const char* suffix = "/Library/Application Support";
    return copy(buffer + cursor, size - cursor, suffix, strlen(suffix));
  }

  return 0;
#else
  const char* xdg = getenv("XDG_DATA_HOME");

  if (xdg) {
    return copy(buffer, size, xdg, strlen(xdg));
  } else {
    size_t cursor = fs_getUserDirectory(buffer, size);
    if (cursor > 0) {
      const char* suffix = "/.local/share";
      return copy(buffer + cursor, size - cursor, suffix, strlen(suffix));
    }
  }

  return 0;
#endif
}

size_t fs_getWorkingDirectory(char* buffer, size_t size) {
  return getcwd(buffer, size) ? strlen(buffer) : 0;
}

size_t fs_getExecutablePath(char* buffer, size_t size) {
#if __APPLE_
  return _NSGetExecutablePath(buffer, &size) ? 0 : size;
#else
  ssize_t length = readlink("/proc/self/exe", buffer, size);
  return (length < 0) ? 0 : (size_t) length;
#endif
}
