#include "graphics/texture.h"
#include "data/textureData.h"
#include <stdbool.h>

#pragma once

#define MAX_CANVASES 4

typedef struct {
  int msaa;
  bool depth;
  bool stencil;
  bool mipmaps;
} CanvasFlags;

typedef struct Canvas Canvas;

bool lovrCanvasSupportsFormat(TextureFormat format);

Canvas* lovrCanvasCreate(int width, int height, TextureFormat format, CanvasFlags flags);
void lovrCanvasDestroy(void* ref);
void lovrCanvasResolve(Canvas* canvas);
TextureFormat lovrCanvasGetFormat(Canvas* canvas);
int lovrCanvasGetMSAA(Canvas* canvas);
TextureData* lovrCanvasNewTextureData(Canvas* canvas);
