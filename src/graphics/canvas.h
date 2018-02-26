#include "graphics/texture.h"
#include "util.h"

#pragma once

typedef struct {
  Texture texture;
  GLuint framebuffer;
  GLuint resolveFramebuffer;
  GLuint depthStencilBuffer;
  GLuint msaaTexture;
  int msaa;
} Canvas;

bool lovrCanvasSupportsFormat(TextureFormat format);

Canvas* lovrCanvasCreate(int width, int height, TextureFormat format, int msaa, bool depth, bool stencil);
void lovrCanvasDestroy(void* ref);
TextureFormat lovrCanvasGetFormat(Canvas* canvas);
int lovrCanvasGetMSAA(Canvas* canvas);
