#include "os.h"
#include <stdio.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

static struct {
  EGLDisplay display;
  EGLContext context;
  EGLSurface surface;
} state;

// Currently, the Android entrypoint is in lovr-oculus-mobile, so this one is not enabled.
#if 0
#include <android_native_app_glue.h>
#include <android/log.h>

static JavaVM* lovrJavaVM;
static JNIEnv* lovrJNIEnv;

int main(int argc, char** argv);

static void onAppCmd(struct android_app* app, int32_t cmd) {
  // pause, resume, events, etc.
}

void android_main(struct android_app* app) {
  lovrJavaVM = app->activity->vm;
  lovrJavaVM->AttachCurrentThread(&lovrJNIEnv, NULL);
  app->onAppCmd = onAppCmd;
  main(0, NULL);
  lovrJavaVM->DetachCurrentThread();
}
#endif

bool lovrPlatformInit() {
  return true;
}

void lovrPlatformDestroy() {
#if 0
  if (state.display) eglMakeCurrent(state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  if (state.surface) eglDestroySurface(state.display, state.surface);
  if (state.context) eglDestroyContext(state.display, state.context);
  if (state.display) eglTerminate(state.display);
  memset(&state, 0, sizeof(state));
#endif
}

const char* lovrPlatformGetName() {
  return "Android";
}

void lovrPlatformPollEvents() {
  // TODO
}

void lovrPlatformOpenConsole() {
  // TODO
}

bool lovrPlatformCreateWindow(WindowFlags* flags) {
#if 0 // lovr-oculus-mobile creates the EGL context right now
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

    for (int a = 0; a < sizeof(attributes) / sizeof(attributes[0]); a += 2) {
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

void lovrPlatformGetWindowSize(int* width, int* height) {
  if (width) *width = 0;
  if (height) *height = 0;
}

void lovrPlatformSwapBuffers() {
  //
}

void* lovrPlatformGetProcAddress(const char* function) {
  return (void*) eglGetProcAddress(function);
}

void lovrPlatformOnWindowClose(windowCloseCallback callback) {
  //
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

void lovrPlatformSleep(double seconds) {
  usleep((unsigned int) (seconds * 1000000));
}
