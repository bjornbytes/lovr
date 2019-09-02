#include "fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
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

size_t fs_getUserDirectory(char* buffer, size_t size) {
  const char* home = getenv("HOME");

  if (!home) {
    struct passwd* entry = getpwuid(getuid());
    if (!entry) {
      buffer[0] = '\0';
      return 0;
    }
    home = entry->pw_dir;
  }

  // Untested
  size_t length = 0;
  while (home[length] && length < size) {
    buffer[length] = home[length];
    length++;
  }
  buffer[length] = '\0';
  return length;
}

size_t fs_getDataDirectory(char* buffer, size_t size) {
#ifdef __linux__
  const char* xdg = getenv("XDG_DATA_HOME");

  if (xdg) {
    // String copy
  } else {
    size_t length = fs_getUserDirectory(buffer, size);
    const char* suffix = "/.config/home";
  }

#endif
}

size_t fs_getWorkingDirectory(char buffer[static FS_PATH_MAX]) {

}

size_t fs_getExecutablePath(char buffer[static FS_PATH_MAX]) {

}
