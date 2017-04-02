#include "filesystem/blob.h"
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

Skybox* lovrSkyboxCreate(Blob** blobs, SkyboxType type);
void lovrSkyboxDestroy(const Ref* ref);
