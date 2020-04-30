#ifdef _WIN32

#include "fs.h"
#include <windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>

#define FS_PATH_MAX 1024

bool fs_open(const char* path, OpenMode mode, fs_handle* file) {
  WCHAR wpath[FS_PATH_MAX];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, FS_PATH_MAX)) {
    return false;
  }

  DWORD access;
  DWORD creation;
  switch (mode) {
    case OPEN_READ: access = GENERIC_READ; creation = OPEN_EXISTING; break;
    case OPEN_WRITE: access = GENERIC_WRITE; creation = CREATE_ALWAYS; break;
    case OPEN_APPEND: access = GENERIC_WRITE; creation = OPEN_ALWAYS; break;
    default: return false;
  }

  DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE;
  file->handle = CreateFileW(wpath, access, share, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file->handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  if (mode == OPEN_APPEND && SetFilePointer(file->handle, 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER) {
    CloseHandle(file->handle);
    return false;
  }

  return true;
}

bool fs_close(fs_handle file) {
  return CloseHandle(file.handle);
}

bool fs_read(fs_handle file, void* buffer, size_t* bytes) {
  DWORD bytes32 = *bytes > UINT32_MAX ? UINT32_MAX : (DWORD) *bytes;
  bool success = ReadFile(file.handle, buffer, bytes32, &bytes32, NULL);
  *bytes = bytes32;
  return success;
}

bool fs_write(fs_handle file, const void* buffer, size_t* bytes) {
  DWORD bytes32 = *bytes > UINT32_MAX ? UINT32_MAX : (DWORD) *bytes;
  bool success = WriteFile(file.handle, buffer, bytes32, &bytes32, NULL);
  *bytes = bytes32;
  return success;
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

size_t fs_getHomeDir(char* buffer, size_t size) {
  PWSTR wpath = NULL;
  if (SHGetKnownFolderPath(&FOLDERID_Profile, 0, NULL, &wpath) == S_OK) {
    size_t bytes = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buffer, (int) size, NULL, NULL) - 1;
    CoTaskMemFree(wpath);
    return bytes;
  }
  return 0;
}

size_t fs_getDataDir(char* buffer, size_t size) {
  PWSTR wpath = NULL;
  if (SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &wpath) == S_OK) {
    size_t bytes = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buffer, (int) size, NULL, NULL) - 1;
    CoTaskMemFree(wpath);
    return bytes;
  }
  return 0;
}

size_t fs_getWorkDir(char* buffer, size_t size) {
  WCHAR wpath[FS_PATH_MAX];
  int length = GetCurrentDirectoryW((int) size, wpath);
  if (length) {
    return WideCharToMultiByte(CP_UTF8, 0, wpath, length + 1, buffer, (int) size, NULL, NULL) - 1;
  }
  return 0;
}

size_t fs_getExecutablePath(char* buffer, size_t size) {
  WCHAR wpath[FS_PATH_MAX];
  DWORD length = GetModuleFileNameW(NULL, wpath, FS_PATH_MAX);
  if (length < FS_PATH_MAX) {
    return WideCharToMultiByte(CP_UTF8, 0, wpath, length + 1, buffer, (int) size, NULL, NULL) - 1;
  }
  return 0;
}

size_t fs_getBundlePath(char* buffer, size_t size) {
  return fs_getExecutablePath(buffer, size);
}

size_t fs_getBundleId(char* buffer, size_t size) {
  return 0;
}

#else // !_WIN32

#include "fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pwd.h>

bool fs_open(const char* path, OpenMode mode, fs_handle* file) {
  int flags;
  switch (mode) {
    case OPEN_READ: flags = O_RDONLY; break;
    case OPEN_WRITE: flags = O_WRONLY | O_CREAT | O_TRUNC; break;
    case OPEN_APPEND: flags = O_WRONLY | O_CREAT; break;
    default: return false;
  }
  file->fd = open(path, flags, S_IRUSR | S_IWUSR);
  return file->fd >= 0;
}

bool fs_close(fs_handle file) {
  return close(file.fd) == 0;
}

bool fs_read(fs_handle file, void* buffer, size_t* bytes) {
  ssize_t result = read(file.fd, buffer, *bytes);
  if (result < 0 || result > SSIZE_MAX) {
    *bytes = 0;
    return false;
  } else {
    *bytes = (uint32_t) result;
    return true;
  }
}

bool fs_write(fs_handle file, const void* buffer, size_t* bytes) {
  ssize_t result = write(file.fd, buffer, *bytes);
  if (result < 0 || result > SSIZE_MAX) {
    *bytes = 0;
    return false;
  } else {
    *bytes = (uint32_t) result;
    return true;
  }
}

void* fs_map(const char* path, size_t* size) {
  FileInfo info;
  fs_handle file;
  if (!fs_stat(path, &info) || !fs_open(path, OPEN_READ, &file)) {
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

  if (info) {
    info->size = (uint64_t) stats.st_size;
    info->lastModified = (uint64_t) stats.st_mtime;
    info->type = S_ISDIR(stats.st_mode) ? FILE_DIRECTORY : FILE_REGULAR;
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

#ifdef __APPLE__
#include <objc/objc-runtime.h>
#include <mach-o/dyld.h>
#elif __ANDROID__
extern const char* lovrOculusMobileWritablePath; // TODO
#endif

static size_t copy(char* buffer, size_t size, const char* string, size_t length) {
  if (length >= size) { return 0; }
  memcpy(buffer, string, length);
  buffer[length + 1] = '\0';
  return length;
}

size_t fs_getHomeDir(char* buffer, size_t size) {
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

size_t fs_getDataDir(char* buffer, size_t size) {
#if __APPLE__
  size_t cursor = fs_getHomeDir(buffer, size);

  if (cursor > 0) {
    const char* suffix = "/Library/Application Support";
    return cursor + copy(buffer + cursor, size - cursor, suffix, strlen(suffix));
  }

  return 0;
#elif EMSCRIPTEN
  const char* path = "/home/web_user";
  return copy(buffer, size, path, strlen(path));
#elif __ANDROID__
  return copy(buffer, size, lovrOculusMobileWritablePath, strlen(lovrOculusMobileWritablePath));
#else
  const char* xdg = getenv("XDG_DATA_HOME");

  if (xdg) {
    return copy(buffer, size, xdg, strlen(xdg));
  } else {
    size_t cursor = fs_getHomeDir(buffer, size);
    if (cursor > 0) {
      const char* suffix = "/.local/share";
      return cursor + copy(buffer + cursor, size - cursor, suffix, strlen(suffix));
    }
  }

  return 0;
#endif
}

size_t fs_getWorkDir(char* buffer, size_t size) {
  return getcwd(buffer, size) ? strlen(buffer) : 0;
}

size_t fs_getExecutablePath(char* buffer, size_t size) {
#if __APPLE__
  uint32_t size32 = size;
  return _NSGetExecutablePath(buffer, &size32) ? 0 : size32;
#elif EMSCRIPTEN
  return 0;
#else
  ssize_t length = readlink("/proc/self/exe", buffer, size);
  return (length < 0) ? 0 : (size_t) length;
#endif
}

size_t fs_getBundlePath(char* buffer, size_t size) {
#ifdef __APPLE__
  id extension = ((id(*)(Class, SEL, char*)) objc_msgSend)(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), "lovr");
  id bundle = ((id(*)(Class, SEL)) objc_msgSend)(objc_getClass("NSBundle"), sel_registerName("mainBundle"));
  id path = ((id(*)(id, SEL, char*, id)) objc_msgSend)(bundle, sel_registerName("pathForResource:ofType:"), nil, extension);
  if (path == nil) {
    return 0;
  }

  const char* cpath = ((const char*(*)(id, SEL)) objc_msgSend)(path, sel_registerName("UTF8String"));
  if (!cpath) {
    return 0;
  }

  size_t length = strlen(cpath);
  if (length >= size) {
    return 0;
  }

  memcpy(buffer, cpath, length);
  buffer[length] = '\0';
  return length;
#else
  return fs_getExecutablePath(buffer, size);
#endif
}

size_t fs_getBundleId(char* buffer, size_t size) {
#ifdef __ANDROID__
  // TODO
  pid_t pid = getpid();
  char path[32];
  snprintf(path, 32, "/proc/%i/cmdline", (int) pid);
  FILE* file = fopen(path, "r");
  if (file) {
    size_t read = fread(buffer, 1, size, file);
    fclose(file);
    return true;
  }
  return 0;
#else
  buffer[0] = '\0';
  return 0;
#endif
}

#endif
