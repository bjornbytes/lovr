#include "os.h"
#include <Windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>
#include <stdio.h>

#include "os_glfw.h"

static uint64_t epoch;
static uint64_t frequency;

static uint64_t getTime() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

bool lovrPlatformInit() {
  LARGE_INTEGER f;
  QueryPerformanceFrequency(&f);
  frequency = f.QuadPart;
  epoch = getTime();
  return true;
}

void lovrPlatformDestroy() {
  glfwTerminate();
}

const char* lovrPlatformGetName() {
  return "Windows";
}

double lovrPlatformGetTime() {
  return (getTime() - epoch) / (double) frequency;
}

void lovrPlatformSetTime(double t) {
  epoch = getTime() - (uint64_t) (t * frequency + .5);
}

void lovrPlatformSleep(double seconds) {
  Sleep((unsigned int) (seconds * 1000));
}

void lovrPlatformOpenConsole() {
  if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
    if (GetLastError() != ERROR_ACCESS_DENIED) {
      if (!AllocConsole()) {
        return;
      }
    }
  }

  freopen("CONOUT$", "w", stdout);
  freopen("CONIN$", "r", stdin);
  freopen("CONOUT$", "w", stderr);
}

size_t lovrPlatformGetHomeDirectory(char* buffer, size_t size) {
  PWSTR wpath = NULL;
  if (SHGetKnownFolderPath(&FOLDERID_Profile, 0, NULL, &wpath) == S_OK) {
    size_t bytes = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buffer, (int) size, NULL, NULL) - 1;
    CoTaskMemFree(wpath);
    return bytes;
  }
  return 0;
}

size_t lovrPlatformGetDataDirectory(char* buffer, size_t size) {
  PWSTR wpath = NULL;
  if (SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &wpath) == S_OK) {
    size_t bytes = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buffer, (int) size, NULL, NULL) - 1;
    CoTaskMemFree(wpath);
    return bytes;
  }
  return 0;
}

size_t lovrPlatformGetWorkingDirectory(char* buffer, size_t size) {
  WCHAR wpath[1024];
  int length = GetCurrentDirectoryW((int) size, wpath);
  if (length) {
    return WideCharToMultiByte(CP_UTF8, 0, wpath, length + 1, buffer, (int) size, NULL, NULL) - 1;
  }
  return 0;
}

size_t lovrPlatformGetExecutablePath(char* buffer, size_t size) {
  WCHAR wpath[1024];
  DWORD length = GetModuleFileNameW(NULL, wpath, 1024);
  if (length < 1024) {
    return WideCharToMultiByte(CP_UTF8, 0, wpath, length + 1, buffer, (int) size, NULL, NULL) - 1;
  }
  return 0;
}

size_t lovrPlatformGetBundlePath(char* buffer, size_t size, const char** root) {
  *root = NULL;
  return lovrPlatformGetExecutablePath(buffer, size);
}
