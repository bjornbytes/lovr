#include "../glfw.h"

#ifndef LOVR_TEXTURE_TYPES
#define LOVR_TEXTURE_TYPES

typedef struct {
  GLuint id;
  int width;
  int height;
} Texture;

#endif

Texture* lovrTextureCreate(void* data, int size);
void lovrTextureDestroy(Texture* texture);
int lovrTextureGetHeight(Texture* texture);
int lovrTextureGetWidth(Texture* texture);
