#include "lovr.h"
#include "audio/audio.h"
#include "event/event.h"
#include "filesystem/filesystem.h"
#include "graphics/graphics.h"
#include "math/math.h"
#include "physics/physics.h"
#include "timer/timer.h"

void lovrDestroy() {
  lovrAudioDestroy();
  lovrEventDestroy();
  lovrFilesystemDestroy();
  lovrGraphicsDestroy();
  lovrHeadsetDestroy();
  lovrMathDestroy();
  lovrPhysicsDestroy();
  lovrTimerDestroy();
}

const char* lovrGetOS() {
#ifdef _WIN32
  return "Windows";
#elif __APPLE__
  return "macOS";
#elif EMSCRIPTEN
  return "Web";
#elif __linux__
  return "Linux";
#else
  return NULL;
#endif
}

void lovrGetVersion(int* major, int* minor, int* patch) {
  *major = LOVR_VERSION_MAJOR;
  *minor = LOVR_VERSION_MINOR;
  *patch = LOVR_VERSION_PATCH;
}
