#ifdef _WIN32

#include "fs.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define FS_PATH_MAX 1024

static fs_error error(void) {
  switch (GetLastError()) {
    case ERROR_SUCCESS: return FS_OK;
    case ERROR_FILE_NOT_FOUND: return FS_NOT_FOUND;
    case ERROR_PATH_NOT_FOUND: return FS_NOT_FOUND;
    case ERROR_ACCESS_DENIED: return FS_PERMISSION;
    case ERROR_INVALID_DRIVE: return FS_NOT_FOUND;
    case ERROR_CURRENT_DIRECTORY: return FS_BUSY;
    case ERROR_INSUFFICIENT_BUFFER: return FS_TOO_LONG;
    case ERROR_WRITE_PROTECT: return FS_READ_ONLY;
    case ERROR_NOT_READY: return FS_IO;
    case ERROR_SEEK: return FS_IO;
    case ERROR_SECTOR_NOT_FOUND: return FS_IO;
    case ERROR_WRITE_FAULT: return FS_IO;
    case ERROR_READ_FAULT: return FS_IO;
    case ERROR_GEN_FAILURE: return FS_IO;
    case ERROR_SHARING_VIOLATION: return FS_BUSY;
    case ERROR_LOCK_VIOLATION: return FS_BUSY;
    case ERROR_HANDLE_DISK_FULL: return FS_FULL;
    case ERROR_NETWORK_ACCESS_DENIED: return FS_PERMISSION;
    default: return FS_UNKNOWN_ERROR;
  }
}

fs_error fs_open(const char* path, char mode, fs_handle* file) {
  WCHAR wpath[FS_PATH_MAX];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX)) {
    return error();
  }

  DWORD access;
  DWORD creation;
  switch (mode) {
    case 'r': access = GENERIC_READ; creation = OPEN_EXISTING; break;
    case 'w': access = GENERIC_WRITE; creation = CREATE_ALWAYS; break;
    case 'a': access = GENERIC_WRITE; creation = OPEN_ALWAYS; break;
    default: return FS_UNKNOWN_ERROR;
  }

  DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE;
  file->handle = CreateFileW(wpath, access, share, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file->handle == INVALID_HANDLE_VALUE) {
    return error();
  }

  if (mode == 'a' && SetFilePointer(file->handle, 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER) {
    int err = error();
    CloseHandle(file->handle);
    return err;
  }

  return FS_OK;
}

fs_error fs_close(fs_handle file) {
  return CloseHandle(file.handle) ? FS_OK : error();
}

fs_error fs_read(fs_handle file, void* data, size_t size, size_t* count) {
  DWORD bytes32 = size > UINT32_MAX ? UINT32_MAX : (DWORD) size;
  bool success = ReadFile(file.handle, data, bytes32, &bytes32, NULL);
  *count = bytes32;
  return success ? FS_OK : error();
}

fs_error fs_write(fs_handle file, const void* data, size_t size, size_t* count) {
  DWORD bytes32 = size > UINT32_MAX ? UINT32_MAX : (DWORD) size;
  bool success = WriteFile(file.handle, data, bytes32, &bytes32, NULL);
  *count = bytes32;
  return success ? FS_OK : error();
}

fs_error fs_seek(fs_handle file, uint64_t offset) {
  LARGE_INTEGER n = { .QuadPart = offset };
  return SetFilePointerEx(file.handle, n, NULL, FILE_BEGIN) ? FS_OK : error();
}

fs_error fs_fstat(fs_handle file, fs_info* info) {
  LARGE_INTEGER size;
  if (!GetFileSizeEx(file.handle, &size)) {
    return error();
  }
  info->size = size.QuadPart;
  info->lastModified = 0;
  info->type = FILE_REGULAR;
  return FS_OK;
}

fs_error fs_map(const char* path, void** pointer, size_t* size) {
  WCHAR wpath[FS_PATH_MAX];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX)) {
    return error();
  }

  fs_handle file;
  file.handle = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file.handle == INVALID_HANDLE_VALUE) {
    return error();
  }

  DWORD hi;
  DWORD lo = GetFileSize(file.handle, &hi);
  if (lo == INVALID_FILE_SIZE) {
    int err = error();
    CloseHandle(file.handle);
    return err;
  }

  if (SIZE_MAX > UINT32_MAX) {
    *size = ((size_t) hi << 32) | lo;
  } else if (hi > 0) {
    CloseHandle(file.handle);
    return FS_UNKNOWN_ERROR;
  } else {
    *size = lo;
  }

  HANDLE mapping = CreateFileMappingA(file.handle, NULL, PAGE_READONLY, hi, lo, NULL);
  if (mapping == NULL) {
    int err = error();
    CloseHandle(file.handle);
    return err;
  }

  *pointer = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, *size);
  int err = error();

  CloseHandle(mapping);
  CloseHandle(file.handle);
  return err;
}

fs_error fs_unmap(void* data, size_t size) {
  return UnmapViewOfFile(data) ? FS_OK : error();
}

fs_error fs_stat(const char* path, fs_info* info) {
  WCHAR wpath[FS_PATH_MAX];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX)) {
    return error();
  }

  WIN32_FILE_ATTRIBUTE_DATA attributes;
  if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &attributes)) {
    return error();
  }

  FILETIME lastModified = attributes.ftLastWriteTime;
  info->type = (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? FILE_DIRECTORY : FILE_REGULAR;
  info->lastModified = ((uint64_t) lastModified.dwHighDateTime << 32) | lastModified.dwLowDateTime;
  info->lastModified /= 10000000ULL; // Convert windows 100ns ticks to seconds
  info->lastModified -= 11644473600ULL; // Convert windows epoch (1601) to POSIX epoch (1970)
  info->size = ((uint64_t) attributes.nFileSizeHigh << 32) | attributes.nFileSizeLow;
  return FS_OK;
}

fs_error fs_remove(const char* path) {
  WCHAR wpath[FS_PATH_MAX];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX)) {
    return error();
  }
  return (DeleteFileW(wpath) || RemoveDirectoryW(wpath)) ? FS_OK : error();
}

fs_error fs_mkdir(const char* path) {
  WCHAR wpath[FS_PATH_MAX];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX)) {
    return error();
  }
  return CreateDirectoryW(wpath, NULL) ? FS_OK : error();
}

fs_error fs_list(const char* path, fs_list_cb* callback, void* context) {
  WCHAR wpath[FS_PATH_MAX];

  int length = MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX);

  if (length == 0 || length + 3 >= FS_PATH_MAX) {
    return error();
  } else {
    wcscat(wpath, L"/*");
  }

  WIN32_FIND_DATAW findData;
  HANDLE handle = FindFirstFileW(wpath, &findData);
  if (handle == INVALID_HANDLE_VALUE) {
    return error();
  }

  char filename[FS_PATH_MAX];
  do {
    if (!WideCharToMultiByte(CP_UTF8, 0, findData.cFileName, -1, filename, FS_PATH_MAX, NULL, NULL)) {
      int err = error();
      FindClose(handle);
      return err;
    }

    callback(context, filename);
  } while (FindNextFileW(handle, &findData));

  FindClose(handle);
  return FS_OK;
}

#else // !_WIN32

#include "fs.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>

static int check(int result) {
  if (result < 0) {
    switch (errno) {
      case EACCES: return FS_PERMISSION;
      case EPERM: return FS_PERMISSION;
      case EROFS: return FS_READ_ONLY;
      case EEXIST: return FS_EXISTS;
      case ENOENT: return FS_NOT_FOUND;
      case EDQUOT: return FS_FULL;
      case ENOSPC: return FS_FULL;
      case ENOTDIR: return FS_NOT_DIR;
      case EISDIR: return FS_IS_DIR;
      case ENOTEMPTY: return FS_NOT_EMPTY;
      case ELOOP: return FS_LOOP;
      case ETXTBSY: return FS_BUSY;
      case EIO: return FS_IO;
      default: return FS_UNKNOWN_ERROR;
    }
  } else {
    return FS_OK;
  }
}

fs_error fs_open(const char* path, char mode, fs_handle* file) {
  int flags;
  switch (mode) {
    case 'r': flags = O_RDONLY; break;
    case 'w': flags = O_WRONLY | O_CREAT | O_TRUNC; break;
    case 'a': flags = O_APPEND | O_WRONLY | O_CREAT; break;
    default: return FS_UNKNOWN_ERROR;
  }

  file->fd = open(path, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
  if (file->fd < 0) return check(file->fd);

  int result;
  struct stat stats;
  if ((result = fstat(file->fd, &stats)) < 0 || S_ISDIR(stats.st_mode)) {
    close(file->fd);
    return result < 0 ? check(result) : FS_IS_DIR;
  }

  return FS_OK;
}

fs_error fs_close(fs_handle file) {
  return check(close(file.fd));
}

fs_error fs_read(fs_handle file, void* data, size_t size, size_t* count) {
  ssize_t result = read(file.fd, data, size);
  if (result < 0) {
    *count = 0;
    return check(result);
  } else {
    *count = result;
    return FS_OK;
  }
}

fs_error fs_write(fs_handle file, const void* data, size_t size, size_t* count) {
  ssize_t result = write(file.fd, data, size);
  if (result < 0) {
    *count = 0;
    return check(result);
  } else {
    *count = result;
    return FS_OK;
  }
}

fs_error fs_seek(fs_handle file, uint64_t offset) {
  return check(lseek(file.fd, (off_t) offset, SEEK_SET));
}

fs_error fs_fstat(fs_handle file, fs_info* info) {
  struct stat stats;
  int result = fstat(file.fd, &stats);
  info->size = (uint64_t) stats.st_size;
  info->lastModified = (uint64_t) stats.st_mtime;
  info->type = S_ISDIR(stats.st_mode) ? FILE_DIRECTORY : FILE_REGULAR;
  return check(result);
}

fs_error fs_map(const char* path, void** pointer, size_t* size) {
  int error;

  fs_info info;
  if ((error = fs_stat(path, &info)) != FS_OK) {
    return error;
  }

  fs_handle file;
  if ((error = fs_open(path, 'r', &file)) != FS_OK) {
    return error;
  }

  *pointer = mmap(NULL, info.size, PROT_READ, MAP_PRIVATE, file.fd, 0);
  *size = info.size;
  fs_close(file);

  if (*pointer == MAP_FAILED) {
    return check(-1);
  }

  return FS_OK;
}

fs_error fs_unmap(void* data, size_t size) {
  return check(munmap(data, size));
}

fs_error fs_stat(const char* path, fs_info* info) {
  struct stat stats;
  int result = stat(path, &stats);
  info->size = (uint64_t) stats.st_size;
  info->lastModified = (uint64_t) stats.st_mtime;
  info->type = S_ISDIR(stats.st_mode) ? FILE_DIRECTORY : FILE_REGULAR;
  return check(result);
}

fs_error fs_remove(const char* path) {
  if (unlink(path)) {
    if (errno == EISDIR || errno == EPERM) {
      return check(rmdir(path));
    } else {
      return check(-1);
    }
  }
  return FS_OK;
}

fs_error fs_mkdir(const char* path) {
  return check(mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH));
}

fs_error fs_list(const char* path, fs_list_cb* callback, void* context) {
  DIR* dir = opendir(path);
  if (!dir) return check(-1);

  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    callback(context, entry->d_name);
  }

  closedir(dir);
  return FS_OK;
}

#endif
