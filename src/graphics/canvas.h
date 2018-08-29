#include "graphics/texture.h"

#pragma once

#define MAX_CANVAS_ATTACHMENTS 4

typedef enum {
  DEPTH_D16,
  DEPTH_D32,
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
} CanvasFlags;

typedef struct Canvas Canvas;

Canvas* lovrCanvasCreate(int width, int height, CanvasFlags flags);
void lovrCanvasDestroy(void* ref);
const Attachment* lovrCanvasGetAttachments(Canvas* canvas, int* count);
void lovrCanvasSetAttachments(Canvas* canvas, Attachment* attachments, int count);
void lovrCanvasBind(Canvas* canvas);
bool lovrCanvasIsStereo(Canvas* canvas);
uint32_t lovrCanvasGetWidth(Canvas* canvas);
uint32_t lovrCanvasGetHeight(Canvas* canvas);
