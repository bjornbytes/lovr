#include "os.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <objc/objc-runtime.h>
#include <mach-o/dyld.h>
#include <mach/mach_time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <sys/mman.h>
#import <AVFoundation/AVFoundation.h>

#include "os_glfw.h"
#include "util.h"

static struct {
  uint64_t frequency;
  fn_permission* onPermissionEvent;
} state;

bool os_init() {
  mach_timebase_info_data_t info;
  mach_timebase_info(&info);
  state.frequency = (info.denom * 1e9) / info.numer;
  return true;
}

void os_destroy() {
  glfwTerminate();
  memset(&state, 0, sizeof(state));
}

const char* os_get_name() {
  return "macOS";
}

uint32_t os_get_core_count() {
  uint32_t count;
  size_t size = sizeof(count);
  sysctlbyname("hw.logicalcpu", &count, &size, NULL, 0);
  return count;
}

void os_open_console() {
  //
}

double os_get_time() {
  return mach_absolute_time() / (double) state.frequency;
}

void* os_vm_init(size_t size) {
  return mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

bool os_vm_free(void* p, size_t size) {
  return !munmap(p, size);
}

bool os_vm_commit(void* p, size_t size) {
  return !mprotect(p, size, PROT_READ | PROT_WRITE);
}

void os_sleep(double seconds) {
  seconds += .5e-9;
  struct timespec t;
  t.tv_sec = seconds;
  t.tv_nsec = (seconds - t.tv_sec) * 1e9;
  while (nanosleep(&t, &t));
}

void os_request_permission(os_permission permission) {
  if (permission == OS_PERMISSION_AUDIO_CAPTURE) {
    // If on old OS without permissions check, permission is implicitly granted
    if (![AVCaptureDevice respondsToSelector:@selector(authorizationStatusForMediaType:)]) {
      if (state.onPermissionEvent) state.onPermissionEvent(permission, true);
      return;
    }

#if TARGET_OS_IOS || (defined(MAC_OS_X_VERSION_10_14) && __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_14)
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]) {
      case AVAuthorizationStatusAuthorized:
        if (state.onPermissionEvent) state.onPermissionEvent(permission, true);
        break;
      case AVAuthorizationStatusNotDetermined:
        [AVCaptureDevice
          requestAccessForMediaType:AVMediaTypeAudio
          completionHandler:^(BOOL granted) {
            dispatch_async(dispatch_get_main_queue(), ^{
              if (state.onPermissionEvent) state.onPermissionEvent(permission, granted);
            });
          }
        ];
        break;
      case AVAuthorizationStatusDenied:
        if (state.onPermissionEvent) state.onPermissionEvent(permission, false);
        break;
      case AVAuthorizationStatusRestricted:
        if (state.onPermissionEvent) state.onPermissionEvent(permission, false);
        break;
    }
#else
    lovrThrow("Unreachable");
#endif
  }
}

void os_on_permission(fn_permission* callback) {
  state.onPermissionEvent = callback;
}

size_t os_get_home_directory(char* buffer, size_t size) {
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

size_t os_get_data_directory(char* buffer, size_t size) {
  size_t cursor = os_get_home_directory(buffer, size);

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

size_t os_get_working_directory(char* buffer, size_t size) {
  return getcwd(buffer, size) ? strlen(buffer) : 0;
}

size_t os_get_executable_path(char* buffer, size_t size) {
  uint32_t size32 = size;
  return _NSGetExecutablePath(buffer, &size32) ? 0 : size32;
}

size_t os_get_bundle_path(char* buffer, size_t size, const char** root) {
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
