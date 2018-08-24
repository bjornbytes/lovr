#include "graphics/texture.h"

#pragma once

#define MAX_CANVAS_ATTACHMENTS 4

typedef struct {
  Texture* texture;
  int slice;
  int level;
} Attachment;

typedef struct Canvas Canvas;

Canvas* lovrCanvasCreate();
void lovrCanvasDestroy(void* ref);
const Attachment* lovrCanvasGetAttachments(Canvas* canvas, int* count);
void lovrCanvasSetAttachments(Canvas* canvas, Attachment* attachments, int count);
void lovrCanvasBind(Canvas* canvas);
