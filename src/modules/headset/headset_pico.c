#include "headset/headset.h"
#include "core/os.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>

// Platform

bool lovrPlatformInit() {
  lovrPlatformOpenConsole();
  return true;
}

void lovrPlatformDestroy() {
  //
}

const char* lovrPlatformGetName() {
  return "Android";
}

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

void lovrPlatformPollEvents() {
  //
}

// To make regular printing work, a thread makes a pipe and redirects stdout and stderr to the write
// end of the pipe.  The read end of the pipe is forwarded to __android_log_write.
static struct {
  int handles[2];
  pthread_t thread;
} logState;

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

void lovrPlatformOpenConsole() {
  pthread_create(&logState.thread, NULL, log_main, logState.handles);
  pthread_detach(logState.thread);
}

size_t lovrPlatformGetHomeDirectory(char* buffer, size_t size) {
  return 0;
}

size_t lovrPlatformGetDataDirectory(char* buffer, size_t size) {
  buffer[0] = '\0';
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

static char apkPath[1024];
size_t lovrPlatformGetBundlePath(char* buffer, size_t size) {
  size_t length = strlen(apkPath);
  if (length >= size) return 0;
  memcpy(buffer, apkPath, length);
  buffer[length] = '\0';
  return length;
}

bool lovrPlatformCreateWindow(WindowFlags* flags) {
  return true;
}

bool lovrPlatformHasWindow() {
  return false;
}

void lovrPlatformGetWindowSize(int* width, int* height) {
  if (width) *width = 0;
  if (height) *height = 0;
}

void lovrPlatformGetFramebufferSize(int* width, int* height) {
  *width = 0;
  *height = 0;
}

void lovrPlatformSwapBuffers() {
  //
}

void* lovrPlatformGetProcAddress(const char* function) {
  return (void*) eglGetProcAddress(function);
}

void lovrPlatformOnQuitRequest(quitCallback callback) {
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

// Headset backend

static struct {
  float offset;
} state;

static bool pico_init(float offset, uint32_t msaa) {
  state.offset = offset;
  return true;
}

static void pico_destroy(void) {
  memset(&state, 0, sizeof(state));
}

static bool pico_getName(char* name, size_t length) {
  return false;
}

static HeadsetOrigin pico_getOriginType(void) {
  return ORIGIN_HEAD;
}

static double pico_getDisplayTime(void) {
  return 0.;
}

static void pico_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  *width = 0;
  *height = 0;
}

static const float* pico_getDisplayMask(uint32_t* count) {
  *count = 0;
  return NULL;
}

static uint32_t pico_getViewCount(void) {
  return 2;
}

static bool pico_getViewPose(uint32_t view, float* position, float* orientation) {
  return false;
}

static bool pico_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  return false;
}

static void pico_getClipDistance(float* clipNear, float* clipFar) {
  *clipNear = 0.f;
  *clipFar = 0.f;
}

static void pico_setClipDistance(float clipNear, float clipFar) {
  //
}

static void pico_getBoundsDimensions(float* width, float* depth) {
  *width = *depth = 0.f;
}

static const float* pico_getBoundsGeometry(uint32_t* count) {
  *count = 0;
  return NULL;
}

static bool pico_getPose(Device device, float* position, float* orientation) {
  return false;
}

static bool pico_getVelocity(Device device, float* velocity, float* angularVelocity) {
  return false;
}

static bool pico_isDown(Device device, DeviceButton button, bool* down, bool* changed) {
  return false;
}

static bool pico_isTouched(Device device, DeviceButton button, bool* touched) {
  return false;
}

static bool pico_getAxis(Device device, DeviceAxis axis, float* value) {
  return false;
}

static bool pico_vibrate(Device device, float strength, float duration, float frequency) {
  return false;
}

static struct ModelData* pico_newModelData(Device device) {
  return NULL;
}

static void pico_renderTo(void (*callback)(void*), void* userdata) {
  //
}

static void pico_update(float dt) {
  //
}

HeadsetInterface lovrHeadsetPicoDriver = {
  .driverType = DRIVER_PICO,
  .init = pico_init,
  .destroy = pico_destroy,
  .getName = pico_getName,
  .getOriginType = pico_getOriginType,
  .getDisplayTime = pico_getDisplayTime,
  .getDisplayDimensions = pico_getDisplayDimensions,
  .getDisplayMask = pico_getDisplayMask,
  .getViewCount = pico_getViewCount,
  .getViewPose = pico_getViewPose,
  .getViewAngles = pico_getViewAngles,
  .getClipDistance = pico_getClipDistance,
  .setClipDistance = pico_setClipDistance,
  .getBoundsDimensions = pico_getBoundsDimensions,
  .getBoundsGeometry = pico_getBoundsGeometry,
  .getPose = pico_getPose,
  .getVelocity = pico_getVelocity,
  .isDown = pico_isDown,
  .isTouched = pico_isTouched,
  .getAxis = pico_getAxis,
  .vibrate = pico_vibrate,
  .newModelData = pico_newModelData,
  .renderTo = pico_renderTo,
  .update = pico_update
};
