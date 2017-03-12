#include "glfw.h"
#include "util.h"

#pragma once

typedef enum {
  SKYBOX_CUBE,
  SKYBOX_PANORAMA
} SkyboxType;

typedef struct {
  Ref ref;
  SkyboxType type;
  GLuint texture;
} Skybox;

Skybox* lovrSkyboxCreate(void** data, size_t* size, int count);
void lovrSkyboxDestroy(const Ref* ref);
