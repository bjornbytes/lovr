#include "lovr.h"

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
