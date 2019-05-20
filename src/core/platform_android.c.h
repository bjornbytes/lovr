#include "platform.h"
#include <stdio.h>
#include <unistd.h>

bool lovrPlatformInit() {
  return true;
}

void lovrPlatformDestroy() {
  //
}

void lovrPlatformPollEvents() {
  //
}

/* Temporarily implemented elsewhere
double lovrPlatformGetTime() {

}

void lovrPlatformSetTime(double t) {

}
*/

bool lovrPlatformCreateWindow(WindowFlags* flags) {
  return true;
}

void lovrPlatformGetWindowSize(int* width, int* height) {
  *width = *height = 0;
}

/* Temporarily implemented elsewhere
void lovrPlatformGetFramebufferSize(int* width, int* height) {
  *width = *height = 0;
}
*/

void lovrPlatformSwapBuffers() {
  //
}

void lovrPlatformOnWindowClose(windowCloseCallback callback) {
  //
}

void lovrPlatformOnWindowResize(windowResizeCallback callback) {
  //
}

void lovrPlatformOnMouseButton(mouseButtonCallback callback) {
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

int lovrPlatformGetExecutablePath(char* dest, uint32_t size) {
  return 1;
}

sds lovrPlatformGetApplicationId() {
  pid_t pid = getpid();
  sds procPath = sdscatfmt(sdsempty(), "/proc/%i/cmdline", (int)pid);
  FILE *procFile = fopen(procPath, "r");
  sdsfree(procPath);
  if (procFile) {
    sds procData = sdsempty();
    char data[64];
    int read;
    while ((read = fread(data, 1, sizeof(data), procFile))) {
      procData = sdscatlen(procData, data, read);
    }
    return procData;
  } else {
    return NULL;
  }
}


#include <EGL/egl.h>
#include <EGL/eglext.h>
getProcAddressProc lovrGetProcAddress = eglGetProcAddress;
