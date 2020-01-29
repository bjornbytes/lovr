#include "os.h"
#include <stdio.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

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
  //
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
