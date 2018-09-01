#include "graphics/texture.h"

#pragma once

#define MAX_CANVAS_ATTACHMENTS 4

typedef enum {
  DEPTH_D16,
  DEPTH_D32F,
  DEPTH_D24S8,
  DEPTH_NONE
} DepthFormat;

typedef struct {
  Texture* texture;
  int slice;
  int level;
} Attachment;

typedef struct {
  DepthFormat depth;
  bool stereo;
  int msaa;
  bool mipmaps;
} CanvasFlags;

typedef struct Canvas Canvas;

Canvas* lovrCanvasCreate(int width, int height, CanvasFlags flags);
void lovrCanvasDestroy(void* ref);
const Attachment* lovrCanvasGetAttachments(Canvas* canvas, int* count);
void lovrCanvasSetAttachments(Canvas* canvas, Attachment* attachments, int count);
void lovrCanvasBind(Canvas* canvas, bool willDraw);
void lovrCanvasResolve(Canvas* canvas);
bool lovrCanvasIsStereo(Canvas* canvas);
int lovrCanvasGetWidth(Canvas* canvas);
int lovrCanvasGetHeight(Canvas* canvas);
int lovrCanvasGetMSAA(Canvas* canvas);
DepthFormat lovrCanvasGetDepthFormat(Canvas* canvas);
TextureData* lovrCanvasNewTextureData(Canvas* canvas, int index);
