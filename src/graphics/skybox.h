#include "../glfw.h"

#ifndef LOVR_SKYBOX_TYPES
#define LOVR_SKYBOX_TYPES
typedef struct {
  GLuint texture;
} Skybox;
#endif

Skybox* lovrSkyboxCreate();
void lovrSkyboxDestroy(Skybox* skybox);
void lovrSkyboxDraw(Skybox* skybox, float angle, float ax, float ay, float az);
