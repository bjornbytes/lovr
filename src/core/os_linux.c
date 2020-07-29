#include "os.h"
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>

#include "os_glfw.h"

static uint64_t epoch;
#define NS_PER_SEC 1000000000ULL

static uint64_t getTime() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t) t.tv_sec * NS_PER_SEC + (uint64_t) t.tv_nsec;
}

bool lovrPlatformInit() {
  epoch = getTime();
  return true;
}

void lovrPlatformDestroy() {
  glfwTerminate();
}

const char* lovrPlatformGetName() {
  return "Linux";
}

double lovrPlatformGetTime() {
  return (getTime() - epoch) / (double) NS_PER_SEC;
}

void lovrPlatformSetTime(double t) {
  epoch = getTime() - (uint64_t) (t * NS_PER_SEC + .5);
}

void lovrPlatformSleep(double seconds) {
  seconds += .5e-9;
  struct timespec t;
  t.tv_sec = seconds;
  t.tv_nsec = (seconds - t.tv_sec) * NS_PER_SEC;
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
  const char* xdg = getenv("XDG_DATA_HOME");

  if (xdg) {
    size_t length = strlen(xdg);
    if (length < size) {
      memcpy(buffer, xdg, length);
      buffer[length] = '\0';
      return length;
    }
  } else {
    size_t cursor = lovrPlatformGetHomeDirectory(buffer, size);
    if (cursor > 0) {
      buffer += cursor;
      size -= cursor;
      const char* suffix = "/.local/share";
      size_t length = strlen(suffix);
      if (length < size) {
        memcpy(buffer, suffix, length);
        buffer[length] = '\0';
        return cursor + length;
      }
    }
  }

  return 0;
}

size_t lovrPlatformGetWorkingDirectory(char* buffer, size_t size) {
  return getcwd(buffer, size) ? strlen(buffer) : 0;
}

size_t lovrPlatformGetExecutablePath(char* buffer, size_t size) {
  ssize_t length = readlink("/proc/self/exe", buffer, size - 1);
  if (length >= 0) {
    buffer[length] = '\0';
    return length;
  } else {
    return 0;
  }
}

size_t lovrPlatformGetBundlePath(char* buffer, size_t size, const char** root) {
  *root = NULL;
  return lovrPlatformGetExecutablePath(buffer, size);
}
