#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#pragma once

typedef struct {
  uint32_t width;
  uint32_t height;
  bool fullscreen;
  bool resizable;
  bool debug;
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
  KEY_A,
  KEY_B,
  KEY_C,
  KEY_D,
  KEY_E,
  KEY_F,
  KEY_G,
  KEY_H,
  KEY_I,
  KEY_J,
  KEY_K,
  KEY_L,
  KEY_M,
  KEY_N,
  KEY_O,
  KEY_P,
  KEY_Q,
  KEY_R,
  KEY_S,
  KEY_T,
  KEY_U,
  KEY_V,
  KEY_W,
  KEY_X,
  KEY_Y,
  KEY_Z,
  KEY_0,
  KEY_1,
  KEY_2,
  KEY_3,
  KEY_4,
  KEY_5,
  KEY_6,
  KEY_7,
  KEY_8,
  KEY_9,

  KEY_SPACE,
  KEY_ENTER,
  KEY_TAB,
  KEY_ESCAPE,
  KEY_BACKSPACE,
  KEY_UP,
  KEY_DOWN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_HOME,
  KEY_END,
  KEY_PAGE_UP,
  KEY_PAGE_DOWN,
  KEY_INSERT,
  KEY_DELETE,
  KEY_F1,
  KEY_F2,
  KEY_F3,
  KEY_F4,
  KEY_F5,
  KEY_F6,
  KEY_F7,
  KEY_F8,
  KEY_F9,
  KEY_F10,
  KEY_F11,
  KEY_F12,

  KEY_BACKTICK,
  KEY_MINUS,
  KEY_EQUALS,
  KEY_LEFT_BRACKET,
  KEY_RIGHT_BRACKET,
  KEY_BACKSLASH,
  KEY_SEMICOLON,
  KEY_APOSTROPHE,
  KEY_COMMA,
  KEY_PERIOD,
  KEY_SLASH,

  KEY_LEFT_CONTROL,
  KEY_LEFT_SHIFT,
  KEY_LEFT_ALT,
  KEY_LEFT_OS,
  KEY_RIGHT_CONTROL,
  KEY_RIGHT_SHIFT,
  KEY_RIGHT_ALT,
  KEY_RIGHT_OS,

  KEY_CAPS_LOCK,
  KEY_SCROLL_LOCK,
  KEY_NUM_LOCK,

  KEY_COUNT
} KeyboardKey;

typedef enum {
  BUTTON_PRESSED,
  BUTTON_RELEASED
} ButtonAction;

typedef void (*quitCallback)(void);
typedef void (*windowFocusCallback)(bool focused);
typedef void (*windowResizeCallback)(int width, int height);
typedef void (*mouseButtonCallback)(MouseButton button, ButtonAction action);
typedef void (*keyboardCallback)(ButtonAction action, KeyboardKey key, uint32_t scancode, bool repeat);
typedef void (*textCallback)(uint32_t codepoint);

bool lovrPlatformInit(void);
void lovrPlatformDestroy(void);
const char* lovrPlatformGetName(void);
double lovrPlatformGetTime(void);
void lovrPlatformSetTime(double t);
void lovrPlatformSleep(double seconds);
void lovrPlatformOpenConsole(void);
void lovrPlatformPollEvents(void);
size_t lovrPlatformGetHomeDirectory(char* buffer, size_t size);
size_t lovrPlatformGetDataDirectory(char* buffer, size_t size);
size_t lovrPlatformGetWorkingDirectory(char* buffer, size_t size);
size_t lovrPlatformGetExecutablePath(char* buffer, size_t size);
size_t lovrPlatformGetBundlePath(char* buffer, size_t size, const char** root);
bool lovrPlatformCreateWindow(const WindowFlags* flags);
bool lovrPlatformHasWindow(void);
void lovrPlatformGetWindowSize(int* width, int* height);
void lovrPlatformGetFramebufferSize(int* width, int* height);
void lovrPlatformSetSwapInterval(int interval);
void lovrPlatformSwapBuffers(void);
void* lovrPlatformGetProcAddress(const char* function);
void lovrPlatformOnQuitRequest(quitCallback callback);
void lovrPlatformOnWindowFocus(windowFocusCallback callback);
void lovrPlatformOnWindowResize(windowResizeCallback callback);
void lovrPlatformOnMouseButton(mouseButtonCallback callback);
void lovrPlatformOnKeyboardEvent(keyboardCallback callback);
void lovrPlatformOnTextEvent(textCallback callback);
void lovrPlatformGetMousePosition(double* x, double* y);
void lovrPlatformSetMouseMode(MouseMode mode);
bool lovrPlatformIsMouseDown(MouseButton button);
bool lovrPlatformIsKeyDown(KeyboardKey key);
