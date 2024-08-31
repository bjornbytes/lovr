#include <stdbool.h>
#include <stdint.h>

#pragma once

struct os_window_config;

typedef enum {
  PERMISSION_AUDIO_CAPTURE
} Permission;

bool lovrSystemInit(void);
void lovrSystemDestroy(void);
const char* lovrSystemGetOS(void);
uint32_t lovrSystemGetCoreCount(void);
void lovrSystemOpenConsole(void);
void lovrSystemRequestPermission(Permission permission);
bool lovrSystemOpenWindow(struct os_window_config* config);
bool lovrSystemIsWindowOpen(void);
void lovrSystemGetWindowSize(uint32_t* width, uint32_t* height);
float lovrSystemGetWindowDensity(void);
void lovrSystemPollEvents(void);
bool lovrSystemIsKeyDown(int keycode);
bool lovrSystemWasKeyPressed(int keycode);
bool lovrSystemWasKeyReleased(int keycode);
bool lovrSystemHasKeyRepeat(void);
void lovrSystemSetKeyRepeat(bool repeat);
void lovrSystemGetMousePosition(double* x, double* y);
bool lovrSystemIsMouseDown(int button);
bool lovrSystemWasMousePressed(int button);
bool lovrSystemWasMouseReleased(int button);
float lovrSystemGetScrollDelta(void);
const char* lovrSystemGetClipboardText(void);
void lovrSystemSetClipboardText(const char* text);
