#include <stdint.h>
#include <stdbool.h>

#pragma once

#ifdef __ANDROID__
#include <android/log.h>
#define lovrLog(...) __android_log_print(ANDROID_LOG_DEBUG, "LOVR", __VA_ARGS__)
#define lovrLogv(...) __android_log_vprint(ANDROID_LOG_DEBUG, "LOVR", __VA_ARGS__)
#define lovrWarn(...) __android_log_print(ANDROID_LOG_WARN, "LOVR", __VA_ARGS__)
#define lovrWarnv(...) __android_log_vprint(ANDROID_LOG_WARN, "LOVR", __VA_ARGS__)
#else
#include <stdio.h>
#define lovrLog(...) printf(__VA_ARGS__)
#define lovrLogv(...) vprintf(__VA_ARGS__)
#define lovrWarn(...) fprintf(stderr, __VA_ARGS__)
#define lovrWarnv(...) vfprintf(stderr, __VA_ARGS__)
#endif

typedef struct {
  uint32_t width;
  uint32_t height;
  bool fullscreen;
  bool srgb;
  int vsync;
  int msaa;
  const char* title;
  struct {
    void* data;
    uint32_t width;
    uint32_t height;
  } icon;
} WindowFlags;

typedef void (*windowCloseCallback)();
typedef void (*windowResizeCallback)(uint32_t width, uint32_t height);

typedef void (*gpuProc)(void);
typedef gpuProc (*getProcAddressProc)(const char*);
extern getProcAddressProc lovrGetProcAddress;

void lovrPlatformPollEvents();
double lovrPlatformGetTime();
void lovrPlatformSetTime(double t);
bool lovrPlatformSetWindow(WindowFlags* flags);
void lovrPlatformGetWindowSize(uint32_t* width, uint32_t* height);
void lovrPlatformOnWindowClose(windowCloseCallback callback);
void lovrPlatformOnWindowResize(windowResizeCallback callback);
void lovrPlatformSwapBuffers();
void lovrSleep(double seconds);
int lovrGetExecutablePath(char* dest, uint32_t size);
