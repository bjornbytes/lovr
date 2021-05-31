#include "os.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <KnownFolders.h>
#include <ShlObj.h>
#include <stdio.h>

#include "os_glfw.h"

static uint64_t frequency;

#ifndef LOVR_BUILDING_SHARED

int main(int argc, char** argv);

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR args, int show) {
  int argc;
  char** argv;

  LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!wargv) {
    return EXIT_FAILURE;
  }

  argv = calloc(argc + 1, sizeof(char*));
  if (!argv) {
    return EXIT_FAILURE;
  }

  for (int i = 0; i < argc; i++) {
    int size = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);

    argv[i] = malloc(size);
    if (!argv[i]) {
      return EXIT_FAILURE;
    }

    if (!WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], size, NULL, NULL)) {
      return EXIT_FAILURE;
    }
  }

  int status = main(argc, argv);

  for (int i = 0; i < argc; i++) {
    free(argv[i]);
  }
  free(argv);

  return status;
}

#endif

bool os_init() {
  LARGE_INTEGER f;
  QueryPerformanceFrequency(&f);
  frequency = f.QuadPart;
  return true;
}

void os_destroy() {
  glfwTerminate();
}

const char* os_get_name() {
  return "Windows";
}

uint32_t os_get_core_count() {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return info.dwNumberOfProcessors;
}

void os_open_console() {
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

double os_get_time() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart / (double) frequency;
}

void os_sleep(double seconds) {
  Sleep((unsigned int) (seconds * 1000));
}

void os_request_permission(os_permission permission) {
  //
}

void os_on_permission(fn_permission* callback) {
  //
}

size_t os_get_home_directory(char* buffer, size_t size) {
  PWSTR wpath = NULL;
  if (SHGetKnownFolderPath(&FOLDERID_Profile, 0, NULL, &wpath) == S_OK) {
    size_t bytes = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buffer, (int) size, NULL, NULL) - 1;
    CoTaskMemFree(wpath);
    return bytes;
  }
  return 0;
}

size_t os_get_data_directory(char* buffer, size_t size) {
  PWSTR wpath = NULL;
  if (SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &wpath) == S_OK) {
    size_t bytes = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buffer, (int) size, NULL, NULL) - 1;
    CoTaskMemFree(wpath);
    return bytes;
  }
  return 0;
}

size_t os_get_working_directory(char* buffer, size_t size) {
  WCHAR wpath[1024];
  int length = GetCurrentDirectoryW((int) size, wpath);
  if (length) {
    return WideCharToMultiByte(CP_UTF8, 0, wpath, length + 1, buffer, (int) size, NULL, NULL) - 1;
  }
  return 0;
}

size_t os_get_executable_path(char* buffer, size_t size) {
  WCHAR wpath[1024];
  DWORD length = GetModuleFileNameW(NULL, wpath, 1024);
  if (length < 1024) {
    return WideCharToMultiByte(CP_UTF8, 0, wpath, length + 1, buffer, (int) size, NULL, NULL) - 1;
  }
  return 0;
}

size_t os_get_bundle_path(char* buffer, size_t size, const char** root) {
  *root = NULL;
  return os_get_executable_path(buffer, size);
}
