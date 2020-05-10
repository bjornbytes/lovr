#include <stdbool.h>
#include <stdint.h>

#pragma once

typedef struct {
  bool (*init)(void);
  void (*apply)(const float* input, float* output, uint32_t frames, float* transform);
} Spatializer;

extern Spatializer lovrSteamAudioSpatializer;
