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
  int vsync;
  int msaa;
  const char* title;
  struct {
    void* data;
    uint32_t width;
    uint32_t height;
  } icon;
} WindowFlags;

typedef enum {
  MOUSE_LEFT,
  MOUSE_RIGHT
} MouseButton;

typedef enum {
  MOUSE_MODE_NORMAL,
  MOUSE_MODE_GRABBED
} MouseMode;

typedef enum {
  KEY_W,
  KEY_A,
  KEY_S,
  KEY_D,
  KEY_Q,
  KEY_E,
  KEY_UP,
  KEY_DOWN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_ESCAPE,
  KEY_F5
} KeyCode;

typedef enum {
  BUTTON_PRESSED,
  BUTTON_RELEASED
} ButtonAction;

typedef void (*windowCloseCallback)(void);
typedef void (*windowResizeCallback)(int width, int height);
typedef void (*mouseButtonCallback)(MouseButton button, ButtonAction action);
typedef void (*keyboardCallback)(KeyCode key, ButtonAction action);

typedef void (*gpuProc)(void);
typedef gpuProc (*getProcAddressProc)(const char*);
extern getProcAddressProc lovrGetProcAddress;

bool lovrPlatformInit(void);
void lovrPlatformDestroy(void);
const char* lovrPlatformGetName();
void lovrPlatformPollEvents(void);
double lovrPlatformGetTime(void);
void lovrPlatformSetTime(double t);
void lovrPlatformOpenConsole();
bool lovrPlatformCreateWindow(WindowFlags* flags);
bool lovrPlatformHasWindow(void);
void lovrPlatformGetWindowSize(int* width, int* height);
void lovrPlatformGetFramebufferSize(int* width, int* height);
void lovrPlatformSetSwapInterval(int interval);
void lovrPlatformSwapBuffers(void);
void lovrPlatformOnWindowClose(windowCloseCallback callback);
void lovrPlatformOnWindowResize(windowResizeCallback callback);
void lovrPlatformOnMouseButton(mouseButtonCallback callback);
void lovrPlatformOnKeyboardEvent(keyboardCallback callback);
void lovrPlatformGetMousePosition(double* x, double* y);
void lovrPlatformSetMouseMode(MouseMode mode);
bool lovrPlatformIsMouseDown(MouseButton button);
bool lovrPlatformIsKeyDown(KeyCode key);
void lovrPlatformSleep(double seconds);
int lovrPlatformGetExecutablePath(char* dest, uint32_t size);
#ifdef _WIN32
#include <windows.h>
HANDLE lovrPlatformGetWindow(void);
HGLRC lovrPlatformGetContext(void);
#endif
