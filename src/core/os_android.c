#include "os.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>

// This is probably bad, but makes things easier to build
#include <android_native_app_glue.c>

static struct {
  struct android_app* app;
  JNIEnv* jni;
  fn_event* callback;
} state;

static void onAppCmd(struct android_app* app, int32_t cmd) {
  if (cmd == APP_CMD_DESTROY && state.callback) {
    state.callback(OS_EVENT_QUIT, (os_event_data) { 0 });
  }
}

static int32_t onInputEvent(struct android_app* app, AInputEvent* input) {
  if (AInputEvent_getType(input) != AINPUT_EVENT_TYPE_KEY || !state.callback) {
    return 0;
  }

  os_event_type type;
  switch (AKeyEvent_getAction(input)) {
    case AKEY_EVENT_ACTION_DOWN: type = OS_EVENT_KEY_PRESSED; break;
    case AKEY_EVENT_ACTION_UP: type = OS_EVENT_KEY_RELEASED; break;
    default: return 0;
  }

  os_event_data data = { 0 };
  switch (AKeyEvent_getKeyCode(input)) {
    case AKEYCODE_A: data.key.code = KEY_A; break;
    case AKEYCODE_B: data.key.code = KEY_B; break;
    case AKEYCODE_C: data.key.code = KEY_C; break;
    case AKEYCODE_D: data.key.code = KEY_D; break;
    case AKEYCODE_E: data.key.code = KEY_E; break;
    case AKEYCODE_F: data.key.code = KEY_F; break;
    case AKEYCODE_G: data.key.code = KEY_G; break;
    case AKEYCODE_H: data.key.code = KEY_H; break;
    case AKEYCODE_I: data.key.code = KEY_I; break;
    case AKEYCODE_J: data.key.code = KEY_J; break;
    case AKEYCODE_K: data.key.code = KEY_K; break;
    case AKEYCODE_L: data.key.code = KEY_L; break;
    case AKEYCODE_M: data.key.code = KEY_M; break;
    case AKEYCODE_N: data.key.code = KEY_N; break;
    case AKEYCODE_O: data.key.code = KEY_O; break;
    case AKEYCODE_P: data.key.code = KEY_P; break;
    case AKEYCODE_Q: data.key.code = KEY_Q; break;
    case AKEYCODE_R: data.key.code = KEY_R; break;
    case AKEYCODE_S: data.key.code = KEY_S; break;
    case AKEYCODE_T: data.key.code = KEY_T; break;
    case AKEYCODE_U: data.key.code = KEY_U; break;
    case AKEYCODE_V: data.key.code = KEY_V; break;
    case AKEYCODE_W: data.key.code = KEY_W; break;
    case AKEYCODE_X: data.key.code = KEY_X; break;
    case AKEYCODE_Y: data.key.code = KEY_Y; break;
    case AKEYCODE_Z: data.key.code = KEY_Z; break;
    case AKEYCODE_0: data.key.code = KEY_0; break;
    case AKEYCODE_1: data.key.code = KEY_1; break;
    case AKEYCODE_2: data.key.code = KEY_2; break;
    case AKEYCODE_3: data.key.code = KEY_3; break;
    case AKEYCODE_4: data.key.code = KEY_4; break;
    case AKEYCODE_5: data.key.code = KEY_5; break;
    case AKEYCODE_6: data.key.code = KEY_6; break;
    case AKEYCODE_7: data.key.code = KEY_7; break;
    case AKEYCODE_8: data.key.code = KEY_8; break;
    case AKEYCODE_9: data.key.code = KEY_9; break;

    case AKEYCODE_SPACE: data.key.code = KEY_SPACE; break;
    case AKEYCODE_ENTER: data.key.code = KEY_ENTER; break;
    case AKEYCODE_TAB: data.key.code = KEY_TAB; break;
    case AKEYCODE_ESCAPE: data.key.code = KEY_ESCAPE; break;
    case AKEYCODE_DEL: data.key.code = KEY_BACKSPACE; break;
    case AKEYCODE_DPAD_UP: data.key.code = KEY_UP; break;
    case AKEYCODE_DPAD_DOWN: data.key.code = KEY_DOWN; break;
    case AKEYCODE_DPAD_LEFT: data.key.code = KEY_LEFT; break;
    case AKEYCODE_DPAD_RIGHT: data.key.code = KEY_RIGHT; break;
    case AKEYCODE_MOVE_HOME: data.key.code = KEY_HOME; break;
    case AKEYCODE_MOVE_END: data.key.code = KEY_END; break;
    case AKEYCODE_PAGE_UP: data.key.code = KEY_PAGE_UP; break;
    case AKEYCODE_PAGE_DOWN: data.key.code = KEY_PAGE_DOWN; break;
    case AKEYCODE_INSERT: data.key.code = KEY_INSERT; break;
    case AKEYCODE_FORWARD_DEL: data.key.code = KEY_DELETE; break;
    case AKEYCODE_F1: data.key.code = KEY_F1; break;
    case AKEYCODE_F2: data.key.code = KEY_F2; break;
    case AKEYCODE_F3: data.key.code = KEY_F3; break;
    case AKEYCODE_F4: data.key.code = KEY_F4; break;
    case AKEYCODE_F5: data.key.code = KEY_F5; break;
    case AKEYCODE_F6: data.key.code = KEY_F6; break;
    case AKEYCODE_F7: data.key.code = KEY_F7; break;
    case AKEYCODE_F8: data.key.code = KEY_F8; break;
    case AKEYCODE_F9: data.key.code = KEY_F9; break;
    case AKEYCODE_F10: data.key.code = KEY_F10; break;
    case AKEYCODE_F11: data.key.code = KEY_F11; break;
    case AKEYCODE_F12: data.key.code = KEY_F12; break;

    case AKEYCODE_GRAVE: data.key.code = KEY_BACKTICK; break;
    case AKEYCODE_MINUS: data.key.code = KEY_MINUS; break;
    case AKEYCODE_EQUALS: data.key.code = KEY_EQUALS; break;
    case AKEYCODE_LEFT_BRACKET: data.key.code = KEY_LEFT_BRACKET; break;
    case AKEYCODE_RIGHT_BRACKET: data.key.code = KEY_RIGHT_BRACKET; break;
    case AKEYCODE_BACKSLASH: data.key.code = KEY_BACKSLASH; break;
    case AKEYCODE_SEMICOLON: data.key.code = KEY_SEMICOLON; break;
    case AKEYCODE_APOSTROPHE: data.key.code = KEY_APOSTROPHE; break;
    case AKEYCODE_COMMA: data.key.code = KEY_COMMA; break;
    case AKEYCODE_PERIOD: data.key.code = KEY_PERIOD; break;
    case AKEYCODE_SLASH: data.key.code = KEY_SLASH; break;

    case AKEYCODE_CTRL_LEFT: data.key.code = KEY_LEFT_CONTROL; break;
    case AKEYCODE_SHIFT_LEFT: data.key.code = KEY_LEFT_SHIFT; break;
    case AKEYCODE_ALT_LEFT: data.key.code = KEY_LEFT_ALT; break;
    case AKEYCODE_META_LEFT: data.key.code = KEY_LEFT_OS; break;
    case AKEYCODE_CTRL_RIGHT: data.key.code = KEY_RIGHT_CONTROL; break;
    case AKEYCODE_SHIFT_RIGHT: data.key.code = KEY_RIGHT_SHIFT; break;
    case AKEYCODE_ALT_RIGHT: data.key.code = KEY_RIGHT_ALT; break;
    case AKEYCODE_META_RIGHT: data.key.code = KEY_RIGHT_OS; break;

    case AKEYCODE_CAPS_LOCK: data.key.code = KEY_CAPS_LOCK; break;
    case AKEYCODE_SCROLL_LOCK: data.key.code = KEY_SCROLL_LOCK; break;
    case AKEYCODE_NUM_LOCK: data.key.code = KEY_NUM_LOCK; break;

    default: return 0;
  }

  data.key.scancode = AKeyEvent_getScanCode(input);
  data.key.repeat = AKeyEvent_getRepeatCount(input) > 0;

  state.callback(type, data);

  // Text event
  if (action == BUTTON_PRESSED) {
    jclass jKeyEvent = (*state.jni)->FindClass(state.jni, "android/view/KeyEvent");
    jmethodID jgetUnicodeChar = (*state.jni)->GetMethodID(state.jni, jKeyEvent, "getUnicodeChar", "(I)I");
    jmethodID jKeyEventInit = (*state.jni)->GetMethodID(state.jni, jKeyEvent, "<init>", "(II)V");
    jobject jevent = (*state.jni)->NewObject(state.jni, jKeyEvent, jKeyEventInit, AKEY_EVENT_ACTION_DOWN, AKeyEvent_getKeyCode(input));
    uint32_t codepoint = (*state.jni)->CallIntMethod(state.jni, jevent, jgetUnicodeChar, AKeyEvent_getMetaState(input));
    if (codepoint > 0) {
      state.callback(OS_EVENT_TEXT_INPUT, (os_event_data) { .text.codepoint = codepoint });
    }
    (*state.jni)->DeleteLocalRef(state.jni, jevent);
    (*state.jni)->DeleteLocalRef(state.jni, jKeyEvent);
  }

  return 1;
}

int main(int argc, char** argv);

void android_main(struct android_app* app) {
  state.app = app;
  (*app->activity->vm)->AttachCurrentThread(app->activity->vm, &state.jni, NULL);
  os_open_console();
  app->onAppCmd = onAppCmd;
  app->onInputEvent = onInputEvent;
  main(0, NULL);
  (*app->activity->vm)->DetachCurrentThread(app->activity->vm);
}

bool os_init(void) {
  return true;
}

void os_destroy(void) {
  // If main exits, tell Android to finish the activity and wait for destruction.
  if (state.app->activityState == APP_CMD_RESUME) {
    state.callback = NULL;
    ANativeActivity_finish(state.app->activity);
    while (!state.app->destroyRequested) {
      os_poll_events();
    }
  }
  memset(&state, 0, sizeof(state));
}

void os_thread_attach(void) {
  JNIEnv* jni;
  (*state.app->activity->vm)->AttachCurrentThread(state.app->activity->vm, &jni, NULL);
}

void os_thread_detach(void) {
  (*state.app->activity->vm)->DetachCurrentThread(state.app->activity->vm);
}

const char* os_get_name(void) {
  return "Android";
}

uint32_t os_get_core_count(void) {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

// Redirect stdout/stderr to __android_log_write with a pipe
static int logfd[2];
static void* log_main(void* data) {
  int* fd = logfd;
  pipe(fd);
  dup2(fd[1], STDOUT_FILENO);
  dup2(fd[1], STDERR_FILENO);
  setvbuf(stdout, 0, _IOLBF, 0);
  setvbuf(stderr, 0, _IONBF, 0);
  ssize_t length;
  char buffer[1024];
  while ((length = read(fd[0], buffer, sizeof(buffer) - 1)) > 0) {
    buffer[length] = '\0';
    __android_log_write(ANDROID_LOG_DEBUG, "LOVR", buffer);
  }
  return 0;
}

void os_open_console(void) {
  pthread_t thread;
  pthread_create(&thread, NULL, log_main, NULL);
  pthread_detach(thread);
}

double os_get_time(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double) t.tv_sec + (t.tv_nsec / 1e9);
}

void os_sleep(double seconds) {
  seconds += .5e-9;
  struct timespec t;
  t.tv_sec = seconds;
  t.tv_nsec = (seconds - t.tv_sec) * 1e9;
  while (nanosleep(&t, &t));
}

#define _JNI(PKG, X) Java_ ## PKG ## _ ## X
#define JNI(PKG, X) _JNI(PKG, X)

JNIEXPORT void JNICALL JNI(LOVR_JAVA_PACKAGE, Activity_lovrPermissionEvent)(JNIEnv* jni, jobject activity, jint permission, jboolean granted) {
  if (state.callback) {
    state.callback(OS_EVENT_PERMISSION, (os_event_data) { .permission = { permission, granted } });
  }
}

void os_request_permission(os_permission permission) {
  if (permission == OS_PERMISSION_AUDIO_CAPTURE) {
    jobject activity = state.app->activity->clazz;
    jclass class = (*state.jni)->GetObjectClass(state.jni, activity);
    jmethodID requestAudioCapturePermission = (*state.jni)->GetMethodID(state.jni, class, "requestAudioCapturePermission", "()V");
    if (!requestAudioCapturePermission) {
      (*state.jni)->DeleteLocalRef(state.jni, class);
      if (state.callback) {
        state.callback(OS_EVENT_PERMISSION, (os_event_data) { .permission = { permission, false } });
      }
      return;
    }

    (*state.jni)->CallVoidMethod(state.jni, activity, requestAudioCapturePermission);
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

bool os_vm_release(void* p, size_t size) {
  return !madvise(p, size, MADV_DONTNEED);
}

size_t os_get_home_directory(char* buffer, size_t size) {
  return 0;
}

size_t os_get_data_directory(char* buffer, size_t size) {
  const char* path = state.app->activity->externalDataPath;
  size_t length = strlen(path);
  if (length >= size) return 0;
  memcpy(buffer, path, length);
  buffer[length] = '\0';
  return length;
}

size_t os_get_working_directory(char* buffer, size_t size) {
  return getcwd(buffer, size) ? strlen(buffer) : 0;
}

size_t os_get_executable_path(char* buffer, size_t size) {
  ssize_t length = readlink("/proc/self/exe", buffer, size - 1);
  if (length >= 0) {
    buffer[length] = '\0';
    return length;
  } else {
    return 0;
  }
}

size_t os_get_bundle_path(char* buffer, size_t size, const char** root) {
  jobject activity = state.app->activity->clazz;
  jclass class = (*state.jni)->GetObjectClass(state.jni, activity);
  jmethodID getPackageCodePath = (*state.jni)->GetMethodID(state.jni, class, "getPackageCodePath", "()Ljava/lang/String;");
  if (!getPackageCodePath) {
    (*state.jni)->DeleteLocalRef(state.jni, class);
    return 0;
  }

  jstring jpath = (*state.jni)->CallObjectMethod(state.jni, activity, getPackageCodePath);
  (*state.jni)->DeleteLocalRef(state.jni, class);
  if ((*state.jni)->ExceptionOccurred(state.jni)) {
    (*state.jni)->ExceptionClear(state.jni);
    return 0;
  }

  size_t length = (*state.jni)->GetStringUTFLength(state.jni, jpath);
  if (length >= size) return 0;

  const char* path = (*state.jni)->GetStringUTFChars(state.jni, jpath, NULL);
  memcpy(buffer, path, length);
  buffer[length] = '\0';
  (*state.jni)->ReleaseStringUTFChars(state.jni, jpath, path);
  *root = "/assets";
  return length;
}

void os_on_event(fn_event* callback) {
  state.callback = callback;
}

// Notes about polling:
// - Stop polling if a destroy is requested to give the application a chance to shut down.
//   Otherwise this loop would still wait for an event and the app would seem unresponsive.
// - Block if the app is paused or no window is present
// - If the app was active and becomes inactive after an event, break instead of waiting for
//   another event.  This gives the main loop a chance to respond (e.g. exit VR mode).
void os_poll_events(void) {
  while (!state.app->destroyRequested) {
    int events;
    struct android_poll_source* source;
    int timeout = (state.app->window && state.app->activityState == APP_CMD_RESUME) ? 0 : -1;
    if (ALooper_pollAll(timeout, NULL, &events, (void**) &source) >= 0) {
      if (source) {
        source->process(state.app, source);
      }

      if (timeout == 0 && (!state.app->window || state.app->activityState != APP_CMD_RESUME)) {
        break;
      }
    } else {
      break;
    }
  }
}

bool os_window_open(const os_window_config* config) {
  return true;
}

bool os_window_is_open(void) {
  return false;
}

void os_window_get_size(uint32_t* width, uint32_t* height) {
  *width = *height = 0;
}

float os_window_get_pixel_density(void) {
  return 0.f;
}

void os_get_mouse_position(double* x, double* y) {
  *x = *y = 0.;
}

void os_set_mouse_relative(bool relative) {
  //
}

bool os_is_mouse_down(int button) {
  return false;
}

bool os_is_key_down(os_key key) {
  return false;
}

// Private, must be forward declared

void* os_get_java_vm(void) {
  return state.app->activity->vm;
}

void* os_get_jni_context(void) {
  return state.app->activity->clazz;
}
