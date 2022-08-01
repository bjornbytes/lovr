#include <stdbool.h>
#include <stdint.h>
#include "core/os.h"

#pragma once

typedef enum {
  PERMISSION_AUDIO_CAPTURE
} Permission;

bool lovrSystemInit(void);
void lovrSystemDestroy(void);
const char* lovrSystemGetOS(void);
uint32_t lovrSystemGetCoreCount(void);
void lovrSystemRequestPermission(Permission permission);
bool lovrSystemIsKeyDown(os_key key);
