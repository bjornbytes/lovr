#include "platform.h"
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

bool lovrPlatformSetWindow(WindowFlags* flags) {
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

void lovrSleep(double seconds) {
  usleep((unsigned int) (seconds * 1000000));
}

int lovrGetExecutablePath(char* dest, uint32_t size) {
  return 1;
}

#include <EGL/egl.h>
#include <EGL/eglext.h>
getProcAddressProc lovrGetProcAddress = eglGetProcAddress;