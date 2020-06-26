#include "os.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

// This is probably bad, but makes things easier to build
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-pedantic"
#include <android_native_app_glue.c>
#pragma clang diagnostic pop

// The activity is considered ready if it's resumed and there's an active window.  This is just an
// artifact of how Oculus' app model works and could be the wrong abstraction, feel free to change.
typedef void (*activeCallback)(bool active);

#ifndef LOVR_USE_OCULUS_MOBILE
static struct {
  struct android_app* app;
  ANativeWindow* window;
  bool resumed;
  JNIEnv* jni;
  EGLDisplay display;
  EGLContext context;
  EGLSurface surface;
  activeCallback onActive;
  quitCallback onQuit;
  pthread_t log;
} state;

// To make regular printing work, a thread makes a pipe and redirects stdout and stderr to the write
// end of the pipe.  The read end of the pipe is forwarded to __android_log_write.
static void* log_main(void* data) {
  int* fd = data;
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

static void onAppCmd(struct android_app* app, int32_t cmd) {
  bool wasActive = state.window && state.resumed;

  switch (cmd) {
    case APP_CMD_RESUME: state.resumed = true; break;
    case APP_CMD_PAUSE: state.resumed = false; break;
    case APP_CMD_INIT_WINDOW: state.window = app->window; break;
    case APP_CMD_TERM_WINDOW: state.window = NULL; break;
    default: break;
  }

  bool active = state.window && state.resumed;
  if (state.onActive && wasActive != active) {
    state.onActive(active);
  }
}

int main(int argc, char** argv);

void android_main(struct android_app* app) {
  state.app = app;
  (*app->activity->vm)->AttachCurrentThread(app->activity->vm, &state.jni, NULL);
  int fd[2];
  pthread_create(&state.log, NULL, log_main, fd);
  pthread_detach(state.log);
  app->onAppCmd = onAppCmd;
  main(0, NULL);
  (*app->activity->vm)->DetachCurrentThread(app->activity->vm);
}
#endif

bool lovrPlatformInit() {
  return true;
}

void lovrPlatformDestroy() {
#ifndef LOVR_USE_OCULUS_MOBILE
  if (state.display) eglMakeCurrent(state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  if (state.surface) eglDestroySurface(state.display, state.surface);
  if (state.context) eglDestroyContext(state.display, state.context);
  if (state.display) eglTerminate(state.display);
#endif
  memset(&state, 0, sizeof(state));
}

const char* lovrPlatformGetName() {
  return "Android";
}

// lovr-oculus-mobile provides its own implementation of the timing functions
#ifndef LOVR_USE_OCULUS_MOBILE
static uint64_t epoch;
#define NS_PER_SEC 1000000000ULL

static uint64_t getTime() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t) t.tv_sec * NS_PER_SEC + (uint64_t) t.tv_nsec;
}

double lovrPlatformGetTime() {
  return (getTime() - epoch) / (double) NS_PER_SEC;
}

void lovrPlatformSetTime(double time) {
  epoch = getTime() - (uint64_t) (time * NS_PER_SEC + .5);
}

void lovrPlatformSleep(double seconds) {
  seconds += .5e-9;
  struct timespec t;
  t.tv_sec = seconds;
  t.tv_nsec = (seconds - t.tv_sec) * NS_PER_SEC;
  while (nanosleep(&t, &t));
}
#endif

void lovrPlatformPollEvents() {
#ifndef LOVR_USE_OCULUS_MOBILE
  int events;
  struct android_poll_source* source;
  bool active = state.window && state.resumed;
  while (ALooper_pollAll(active ? 0 : 0, NULL, &events, (void**) &source) >= 0) {
    if (source) {
      source->process(state.app, source);
    }
  }
#endif
}

void lovrPlatformOpenConsole() {
  // TODO
}

size_t lovrPlatformGetHomeDirectory(char* buffer, size_t size) {
  return 0;
}

size_t lovrPlatformGetDataDirectory(char* buffer, size_t size) {
  const char* path = state.app->activity->externalDataPath;
  size_t length = strlen(path);
  if (length >= size) return 0;
  memcpy(buffer, path, length);
  buffer[length] = '\0';
  return length;
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

size_t lovrPlatformGetBundlePath(char* buffer, size_t size) {
  jobject activity = state.app->activity->clazz;
  jclass class = (*state.jni)->GetObjectClass(state.jni, activity);
  jmethodID getPackageCodePath = (*state.jni)->GetMethodID(state.jni, class, "getPackageCodePath", "()Ljava/lang/String;");
  if (!getPackageCodePath) {
    return 0;
  }

  jstring jpath = (*state.jni)->CallObjectMethod(state.jni, activity, getPackageCodePath);
  if ((*state.jni)->ExceptionOccurred(state.jni)) {
    (*state.jni)->ExceptionClear(state.jni);
    return 0;
  }

  const char* path = (*state.jni)->GetStringUTFChars(state.jni, jpath, NULL);
  size_t length = strlen(path);
  if (length >= size) return 0;
  memcpy(buffer, path, length);
  buffer[length] = '\0';
  return length;
}

bool lovrPlatformCreateWindow(WindowFlags* flags) {
#ifndef LOVR_USE_OCULUS_MOBILE // lovr-oculus-mobile creates its own EGL context
  if (state.display) {
    return true;
  }

  if ((state.display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
    return false;
  }

  if (eglInitialize(state.display, NULL, NULL) == EGL_FALSE) {
    return false;
  }

  EGLConfig configs[1024];
  EGLint configCount;
  if (eglGetConfigs(state.display, configs, sizeof(configs) / sizeof(configs[0]), &configCount) == EGL_FALSE) {
    return false;
  }

  const EGLint attributes[] = {
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 0,
    EGL_STENCIL_SIZE, 0,
    EGL_SAMPLES, 0,
    EGL_NONE
  };

  EGLConfig config = 0;
  for (EGLint i = 0; i < configCount && !config; i++) {
    EGLint value, mask;

    mask = EGL_OPENGL_ES3_BIT_KHR;
    if (!eglGetConfigAttrib(state.display, configs[i], EGL_RENDERABLE_TYPE, &value) || (value & mask) != mask) {
      continue;
    }

    mask = EGL_PBUFFER_BIT | EGL_WINDOW_BIT;
    if (!eglGetConfigAttrib(state.display, configs[i], EGL_SURFACE_TYPE, &value) || (value & mask) != mask) {
      continue;
    }

    for (size_t a = 0; a < sizeof(attributes) / sizeof(attributes[0]); a += 2) {
      if (attributes[a] == EGL_NONE) {
        config = configs[i];
        break;
      }

      if (!eglGetConfigAttrib(state.display, configs[i], attributes[a], &value) || value != attributes[a + 1]) {
        break;
      }
    }
  }

  EGLint contextAttributes[] = {
    EGL_CONTEXT_CLIENT_VERSION, 3,
    EGL_NONE
  };

  if ((state.context = eglCreateContext(state.display, config, EGL_NO_CONTEXT, contextAttributes)) == EGL_NO_CONTEXT) {
    return false;
  }

  EGLint surfaceAttributes[] = {
    EGL_WIDTH, 16,
    EGL_HEIGHT, 16,
    EGL_NONE
  };

  if ((state.surface = eglCreatePbufferSurface(state.display, config, surfaceAttributes)) == EGL_NO_SURFACE) {
    eglDestroyContext(state.display, state.context);
    return false;
  }

  if (eglMakeCurrent(state.display, state.surface, state.surface, state.context) == EGL_FALSE) {
    eglDestroySurface(state.display, state.surface);
    eglDestroyContext(state.display, state.context);
  }
#endif
  return true;
}

#ifndef LOVR_USE_OCULUS_MOBILE
bool lovrPlatformHasWindow() {
  return false;
}
#endif

void lovrPlatformGetWindowSize(int* width, int* height) {
  if (width) *width = 0;
  if (height) *height = 0;
}

#ifndef LOVR_USE_OCULUS_MOBILE
void lovrPlatformGetFramebufferSize(int* width, int* height) {
  *width = 0;
  *height = 0;
}
#endif

void lovrPlatformSwapBuffers() {
  //
}

void* lovrPlatformGetProcAddress(const char* function) {
  return (void*) eglGetProcAddress(function);
}

void lovrPlatformOnQuitRequest(quitCallback callback) {
  state.onQuit = callback;
}

void lovrPlatformOnWindowFocus(windowFocusCallback callback) {
  //
}

void lovrPlatformOnWindowResize(windowResizeCallback callback) {
  //
}

void lovrPlatformOnMouseButton(mouseButtonCallback callback) {
  //
}

void lovrPlatformOnKeyboardEvent(keyboardCallback callback) {
  //
}

void lovrPlatformGetMousePosition(double* x, double* y) {
  *x = *y = 0.;
}

void lovrPlatformSetMouseMode(MouseMode mode) {
  //
}

bool lovrPlatformIsMouseDown(MouseButton button) {
  return false;
}

bool lovrPlatformIsKeyDown(KeyCode key) {
  return false;
}

// Private, must be declared manually to use

void lovrPlatformOnActive(activeCallback callback) {
  state.onActive = callback;
}

struct ANativeActivity* lovrPlatformGetActivity() {
  return state.app->activity;
}

ANativeWindow* lovrPlatformGetNativeWindow() {
  return state.window;
}

JNIEnv* lovrPlatformGetJNI() {
  return state.jni;
}

EGLDisplay lovrPlatformGetEGLDisplay() {
  return state.display;
}

EGLContext lovrPlatformGetEGLContext() {
  return state.context;
}

EGLSurface lovrPlatformGetEGLSurface() {
  return state.surface;
}
