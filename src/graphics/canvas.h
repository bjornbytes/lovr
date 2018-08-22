#include "graphics/texture.h"

#pragma once

#define MAX_COLOR_ATTACHMENTS 4

typedef enum {
  ATTACHMENT_COLOR,
  ATTACHMENT_DEPTH
} AttachmentType;

typedef struct {
  Texture* texture;
  int slice;
  int level;
} Attachment;

typedef struct Canvas Canvas;

Canvas* lovrCanvasCreate();
void lovrCanvasDestroy(void* ref);
