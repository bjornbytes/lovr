#include "graphics/texture.h"
#include "data/textureData.h"
#include "util.h"
#include <stdbool.h>

#pragma once

typedef struct {
  int msaa;
  bool depth;
  bool stencil;
  bool stereo;
  bool mipmaps;
} CanvasFlags;

typedef struct {
  Texture texture;
  GLuint framebuffer;
  GLuint resolveFramebuffer;
  GLuint depthStencilBuffer;
  GLuint msaaTexture;
  CanvasFlags flags;
} Canvas;

bool lovrCanvasSupportsFormat(TextureFormat format);

Canvas* lovrCanvasCreate(int width, int height, TextureFormat format, CanvasFlags flags);
void lovrCanvasDestroy(void* ref);
void lovrCanvasResolve(Canvas* canvas);
TextureFormat lovrCanvasGetFormat(Canvas* canvas);
int lovrCanvasGetMSAA(Canvas* canvas);
TextureData* lovrCanvasNewTextureData(Canvas* canvas);
