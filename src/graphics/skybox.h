#include "glfw.h"
#include "util.h"

#pragma once

typedef struct {
  Ref ref;
  GLuint texture;
} Skybox;

Skybox* lovrSkyboxCreate(void** data, int* size);
void lovrSkyboxDestroy(const Ref* ref);
