#include "graphics/texture.h"
#include "data/textureData.h"
#include <stdbool.h>

#pragma once

typedef struct {
  int msaa;
  bool depth;
  bool stencil;
  bool mipmaps;
} CanvasFlags;

typedef struct Canvas Canvas;

struct Canvas {
  Texture texture;
  GLuint framebuffer;
  GLuint resolveFramebuffer;
  GLuint depthStencilBuffer;
  GLuint msaaTexture;
  CanvasFlags flags;
};

bool lovrCanvasSupportsFormat(TextureFormat format);

Canvas* lovrCanvasCreate(int width, int height, TextureFormat format, CanvasFlags flags);
void lovrCanvasDestroy(void* ref);
uint32_t lovrCanvasGetId(Canvas* canvas); // FIXME temporary
void lovrCanvasResolve(Canvas* canvas);
TextureFormat lovrCanvasGetFormat(Canvas* canvas);
int lovrCanvasGetMSAA(Canvas* canvas);
TextureData* lovrCanvasNewTextureData(Canvas* canvas);
