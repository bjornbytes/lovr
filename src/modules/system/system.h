#include <stdbool.h>

#pragma once

typedef enum {
  PERMISSION_AUDIO_CAPTURE
} Permission;

bool lovrSystemInit(void);
void lovrSystemDestroy(void);
const char* lovrSystemGetOS(void);
void lovrSystemRequestPermission(Permission permission);
