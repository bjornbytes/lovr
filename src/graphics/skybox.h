#include "glfw.h"
#include "util.h"

#ifndef LOVR_SKYBOX_TYPES
#define LOVR_SKYBOX_TYPES
typedef struct {
  Ref ref;
  GLuint texture;
} Skybox;
#endif

Skybox* lovrSkyboxCreate(void** data, int* size);
void lovrSkyboxDestroy(const Ref* ref);
