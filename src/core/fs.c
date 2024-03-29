#ifdef _WIN32

#include "fs.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define FS_PATH_MAX 1024

bool fs_open(const char* path, char mode, fs_handle* file) {
  WCHAR wpath[FS_PATH_MAX];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX)) {
    return false;
  }

  DWORD access;
  DWORD creation;
  switch (mode) {
    case 'r': access = GENERIC_READ; creation = OPEN_EXISTING; break;
    case 'w': access = GENERIC_WRITE; creation = CREATE_ALWAYS; break;
    case 'a': access = GENERIC_WRITE; creation = OPEN_ALWAYS; break;
    default: return false;
  }

  DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE;
  file->handle = CreateFileW(wpath, access, share, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file->handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  if (mode == 'a' && SetFilePointer(file->handle, 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER) {
    CloseHandle(file->handle);
    return false;
  }

  return true;
}

bool fs_close(fs_handle file) {
  return CloseHandle(file.handle);
}

bool fs_read(fs_handle file, void* data, size_t size, size_t* count) {
  DWORD bytes32 = size > UINT32_MAX ? UINT32_MAX : (DWORD) size;
  bool success = ReadFile(file.handle, data, bytes32, &bytes32, NULL);
  *count = bytes32;
  return success;
}

bool fs_write(fs_handle file, const void* data, size_t size, size_t* count) {
  DWORD bytes32 = size > UINT32_MAX ? UINT32_MAX : (DWORD) size;
  bool success = WriteFile(file.handle, data, bytes32, &bytes32, NULL);
  *count = bytes32;
  return success;
}

bool fs_seek(fs_handle file, uint64_t offset) {
  LARGE_INTEGER n = { .QuadPart = offset };
  return SetFilePointerEx(file.handle, n, NULL, FILE_BEGIN);
}

bool fs_fstat(fs_handle file, FileInfo* info) {
  LARGE_INTEGER size;
  if (!GetFileSizeEx(file.handle, &size)) {
    return false;
  }
  info->size = size.QuadPart;
  info->lastModified = 0;
  info->type = FILE_REGULAR;
  return true;
}

void* fs_map(const char* path, size_t* size) {
  WCHAR wpath[FS_PATH_MAX];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX)) {
    return false;
  }

  fs_handle file;
  file.handle = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file.handle == INVALID_HANDLE_VALUE) {
    return NULL;
  }

  DWORD hi;
  DWORD lo = GetFileSize(file.handle, &hi);
  if (lo == INVALID_FILE_SIZE) {
    CloseHandle(file.handle);
    return NULL;
  }

  if (SIZE_MAX > UINT32_MAX) {
    *size = ((size_t) hi << 32) | lo;
  } else if (hi > 0) {
    CloseHandle(file.handle);
    return NULL;
  } else {
    *size = lo;
  }

  HANDLE mapping = CreateFileMappingA(file.handle, NULL, PAGE_READONLY, hi, lo, NULL);
  if (mapping == NULL) {
    CloseHandle(file.handle);
    return NULL;
  }

  void* data = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, *size);

  CloseHandle(mapping);
  CloseHandle(file.handle);
  return data;
}

bool fs_unmap(void* data, size_t size) {
  return UnmapViewOfFile(data);
}

bool fs_stat(const char* path, FileInfo* info) {
  WCHAR wpath[FS_PATH_MAX];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX)) {
    return false;
  }

  WIN32_FILE_ATTRIBUTE_DATA attributes;
  if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &attributes)) {
    return false;
  }

  FILETIME lastModified = attributes.ftLastWriteTime;
  info->type = (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? FILE_DIRECTORY : FILE_REGULAR;
  info->lastModified = ((uint64_t) lastModified.dwHighDateTime << 32) | lastModified.dwLowDateTime;
  info->lastModified /= 10000000ULL; // Convert windows 100ns ticks to seconds
  info->lastModified -= 11644473600ULL; // Convert windows epoch (1601) to POSIX epoch (1970)
  info->size = ((uint64_t) attributes.nFileSizeHigh << 32) | attributes.nFileSizeLow;
  return true;
}

bool fs_remove(const char* path) {
  WCHAR wpath[FS_PATH_MAX];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX)) {
    return false;
  }
  return DeleteFileW(wpath) || RemoveDirectoryW(wpath);
}

bool fs_mkdir(const char* path) {
  WCHAR wpath[FS_PATH_MAX];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX)) {
    return false;
  }
  return CreateDirectoryW(wpath, NULL);
}

bool fs_list(const char* path, fs_list_cb* callback, void* context) {
  WCHAR wpath[FS_PATH_MAX];

  int length = MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX);

  if (length == 0 || length + 3 >= FS_PATH_MAX) {
    return false;
  } else {
    wcscat(wpath, L"/*");
  }

  WIN32_FIND_DATAW findData;
  HANDLE handle = FindFirstFileW(wpath, &findData);
  if (handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  char filename[FS_PATH_MAX];
  do {
    if (!WideCharToMultiByte(CP_UTF8, 0, findData.cFileName, -1, filename, FS_PATH_MAX, NULL, NULL)) {
      FindClose(handle);
      return false;
    }

    callback(context, filename);
  } while (FindNextFileW(handle, &findData));

  FindClose(handle);
  return true;
}

#else // !_WIN32

#include "fs.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>

bool fs_open(const char* path, char mode, fs_handle* file) {
  int flags;
  switch (mode) {
    case 'r': flags = O_RDONLY; break;
    case 'w': flags = O_WRONLY | O_CREAT | O_TRUNC; break;
    case 'a': flags = O_APPEND | O_WRONLY | O_CREAT; break;
    default: return false;
  }
  struct stat stats;
  file->fd = open(path, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
  return file->fd >= 0 && !fstat(file->fd, &stats) && !S_ISDIR(stats.st_mode);
}

bool fs_close(fs_handle file) {
  return close(file.fd) == 0;
}

bool fs_read(fs_handle file, void* data, size_t size, size_t* count) {
  ssize_t result = read(file.fd, data, size);
  if (result < 0 || result > SSIZE_MAX) {
    *count = 0;
    return false;
  } else {
    *count = result;
    return true;
  }
}

bool fs_write(fs_handle file, const void* data, size_t size, size_t* count) {
  ssize_t result = write(file.fd, data, size);
  if (result < 0 || result > SSIZE_MAX) {
    *count = 0;
    return false;
  } else {
    *count = (size_t) result;
    return true;
  }
}

bool fs_seek(fs_handle file, uint64_t offset) {
  return lseek(file.fd, (off_t) offset, SEEK_SET) != (off_t) -1;
}

bool fs_fstat(fs_handle file, FileInfo* info) {
  struct stat stats;
  if (fstat(file.fd, &stats)) {
    return false;
  }

  info->size = (uint64_t) stats.st_size;
  info->lastModified = (uint64_t) stats.st_mtime;
  info->type = S_ISDIR(stats.st_mode) ? FILE_DIRECTORY : FILE_REGULAR;
  return true;
}

void* fs_map(const char* path, size_t* size) {
  FileInfo info;
  fs_handle file;
  if (!fs_stat(path, &info) || !fs_open(path, 'r', &file)) {
    return NULL;
  }
  *size = info.size;
  void* data = mmap(NULL, *size, PROT_READ, MAP_PRIVATE, file.fd, 0);
  fs_close(file);
  return data;
}

bool fs_unmap(void* data, size_t size) {
  return munmap(data, size) == 0;
}

bool fs_stat(const char* path, FileInfo* info) {
  struct stat stats;
  if (stat(path, &stats)) {
    return false;
  }

  info->size = (uint64_t) stats.st_size;
  info->lastModified = (uint64_t) stats.st_mtime;
  info->type = S_ISDIR(stats.st_mode) ? FILE_DIRECTORY : FILE_REGULAR;
  return true;
}

bool fs_remove(const char* path) {
  return unlink(path) == 0 || rmdir(path) == 0;
}

bool fs_mkdir(const char* path) {
  return mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0;
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

#endif
