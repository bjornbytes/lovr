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
#include <AVFoundation/AVFoundation.h>

#define cls(T) ((id) objc_getClass(#T))
#define msg(ret, obj, fn) ((ret(*)(id, SEL)) objc_msgSend)(obj, sel_getUid(fn))
#define msg1(ret, obj, fn, T1, A1) ((ret(*)(id, SEL, T1)) objc_msgSend)(obj, sel_getUid(fn), A1)
#define msg2(ret, obj, fn, T1, A1, T2, A2) ((ret(*)(id, SEL, T1, T2)) objc_msgSend)(obj, sel_getUid(fn), A1, A2)
#define msg3(ret, obj, fn, T1, A1, T2, A2, T3, A3) ((ret(*)(id, SEL, T1, T2, T3)) objc_msgSend)(obj, sel_getUid(fn), A1, A2, A3)

#include "os_glfw.h"

static struct {
  uint64_t frequency;
  fn_permission* onPermissionEvent;
} state;

bool os_init(void) {
  mach_timebase_info_data_t info;
  mach_timebase_info(&info);
  state.frequency = (info.denom * 1e9) / info.numer;
  return true;
}

void os_destroy(void) {
  glfwTerminate();
}

const char* os_get_name(void) {
  return "macOS";
}

uint32_t os_get_core_count(void) {
  uint32_t count;
  size_t size = sizeof(count);
  sysctlbyname("hw.logicalcpu", &count, &size, NULL, 0);
  return count;
}

void os_open_console(void) {
  //
}

double os_get_time(void) {
  return mach_absolute_time() / (double) state.frequency;
}

void os_sleep(double seconds) {
  seconds += .5e-9;
  struct timespec t;
  t.tv_sec = seconds;
  t.tv_nsec = (seconds - t.tv_sec) * 1e9;
  while (nanosleep(&t, &t));
}

typedef void (^PermissionHandler)(BOOL granted);

void os_request_permission(os_permission permission) {
  if (permission == OS_PERMISSION_AUDIO_CAPTURE) {
    id AVCaptureDevice = cls(AVCaptureDevice);

    // If on old OS without permissions check, permission is implicitly granted
    if (!class_respondsToSelector(AVCaptureDevice, sel_getUid("authorizationStatusForMediaType:"))) {
      if (state.onPermissionEvent) state.onPermissionEvent(permission, true);
      return;
    }

#if TARGET_OS_IOS || (defined(MAC_OS_X_VERSION_10_14) && __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_14)
    __block fn_permission* callback = state.onPermissionEvent;
    PermissionHandler handler = ^(BOOL granted) {
      dispatch_async(dispatch_get_main_queue(), ^{
        if (callback) callback(OS_PERMISSION_AUDIO_CAPTURE, granted);
      });
    };

    switch (msg1(AVAuthorizationStatus, AVCaptureDevice, "authorizationStatusForMediaType:", AVMediaType, AVMediaTypeAudio)) {
      case AVAuthorizationStatusAuthorized:
        if (state.onPermissionEvent) state.onPermissionEvent(permission, true);
        break;
      case AVAuthorizationStatusNotDetermined:
        msg2(void, AVCaptureDevice, "requestAccessForMediaType:completionHandler:", AVMediaType, AVMediaTypeAudio, PermissionHandler, handler);
        break;
      case AVAuthorizationStatusDenied:
        if (state.onPermissionEvent) state.onPermissionEvent(permission, false);
        break;
      case AVAuthorizationStatusRestricted:
        if (state.onPermissionEvent) state.onPermissionEvent(permission, false);
        break;
    }
#else
#error "Unsupported macOS target"
#endif
  }
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

void os_on_permission(fn_permission* callback) {
  state.onPermissionEvent = callback;
}

void os_thread_attach(void) {
  //
}

void os_thread_detach(void) {
  //
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
  id extension = msg1(id, cls(NSString), "stringWithUTF8String:", char*, "lovr");
  id bundle = msg(id, cls(NSBundle), "mainBundle");
  id path = msg2(id, bundle, "pathForResource:ofType:", id, nil, id, extension);
  if (path == nil) {
    *root = NULL;
    return os_get_executable_path(buffer, size);
  }

  const char* cpath = msg(const char*, path, "UTF8String");
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
