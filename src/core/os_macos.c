#include "os.h"
#include <objc/objc-runtime.h>
#include <mach-o/dyld.h>
#include <mach/mach_time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>

#include "os_glfw.h"

static uint64_t epoch;
static uint64_t frequency;

static uint64_t getTime() {
  return mach_absolute_time();
}

bool lovrPlatformInit() {
  mach_timebase_info_data_t info;
  mach_timebase_info(&info);
  frequency = (info.denom * 1e9) / info.numer;
  epoch = getTime();
  return true;
}

void lovrPlatformDestroy() {
  glfwTerminate();
}

const char* lovrPlatformGetName() {
  return "macOS";
}

double lovrPlatformGetTime() {
  return (getTime() - epoch) / (double) frequency;
}

void lovrPlatformSetTime(double t) {
  epoch = getTime() - (uint64_t) (t * frequency + .5);
}

void lovrPlatformSleep(double seconds) {
  seconds += .5e-9;
  struct timespec t;
  t.tv_sec = seconds;
  t.tv_nsec = (seconds - t.tv_sec) * 1e9;
  while (nanosleep(&t, &t));
}

void lovrPlatformOpenConsole() {
  //
}

size_t lovrPlatformGetHomeDirectory(char* buffer, size_t size) {
  const char* path = getenv("HOME");

  if (!path) {
    struct passwd* entry = getpwuid(getuid());
    if (!entry) {
      return 0;
    }
    path = entry->pw_dir;
  }

  size_t length = strlen(path);
  if (length >= size) { return 0; }
  memcpy(buffer, path, length);
  buffer[length] = '\0';
  return length;
}

size_t lovrPlatformGetDataDirectory(char* buffer, size_t size) {
  size_t cursor = lovrPlatformGetHomeDirectory(buffer, size);

  if (cursor > 0) {
    buffer += cursor;
    size -= cursor;
    const char* suffix = "/Library/Application Support";
    size_t length = strlen(suffix);
    if (length < size) {
      memcpy(buffer, suffix, length);
      buffer[length] = '\0';
      return cursor + length;
    }
  }

  return 0;
}

size_t lovrPlatformGetWorkingDirectory(char* buffer, size_t size) {
  return getcwd(buffer, size) ? strlen(buffer) : 0;
}

size_t lovrPlatformGetExecutablePath(char* buffer, size_t size) {
  uint32_t size32 = size;
  return _NSGetExecutablePath(buffer, &size32) ? 0 : size32;
}

size_t lovrPlatformGetBundlePath(char* buffer, size_t size, const char** root) {
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
  *root = NULL;
  return length;
}
