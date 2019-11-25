#include "platform.h"
#include <stdio.h>
#include <unistd.h>

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
  //
}

/* Temporarily implemented elsewhere
double lovrPlatformGetTime() {

}

void lovrPlatformSetTime(double t) {

}
*/

void lovrPlatformOpenConsole() {
  //
}

bool lovrPlatformCreateWindow(WindowFlags* flags) {
  return true;
}

void lovrPlatformGetWindowSize(int* width, int* height) {
  if (width) *width = 0;
  if (height) *height = 0;
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

#include <EGL/egl.h>
#include <EGL/eglext.h>
getProcAddressProc lovrGetProcAddress = eglGetProcAddress;
