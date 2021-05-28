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
void lovrSystemRequestPermission(Permission permission);
void lovrSystemOpenWindow(struct os_window_config* window);
bool lovrSystemIsWindowOpen(void);
uint32_t lovrSystemGetWindowWidth(void);
uint32_t lovrSystemGetWindowHeight(void);
float lovrSystemGetWindowDensity(void);
