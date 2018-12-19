#include "graphics/texture.h"
#include "graphics/opengl.h"

#pragma once

#define MAX_CANVAS_ATTACHMENTS 4

typedef struct {
  Texture* texture;
  int slice;
  int level;
} Attachment;

typedef struct {
  struct {
    bool enabled;
    bool readable;
    TextureFormat format;
  } depth;
  bool stereo;
  int msaa;
  bool mipmaps;
} CanvasFlags;

typedef struct {
  Ref ref;
  int width;
  int height;
  CanvasFlags flags;
  Attachment attachments[MAX_CANVAS_ATTACHMENTS];
  Attachment depth;
  int attachmentCount;
  bool needsAttach;
  bool needsResolve;
  GPU_CANVAS_FIELDS
} Canvas;

Canvas* lovrCanvasCreate(int width, int height, CanvasFlags flags);
Canvas* lovrCanvasCreateFromHandle(int width, int height, CanvasFlags flags, uint32_t framebuffer, uint32_t depthBuffer, uint32_t resolveBuffer, int attachmentCount, bool immortal);
void lovrCanvasDestroy(void* ref);
const Attachment* lovrCanvasGetAttachments(Canvas* canvas, int* count);
void lovrCanvasSetAttachments(Canvas* canvas, Attachment* attachments, int count);
void lovrCanvasBind(Canvas* canvas, bool willDraw);
bool lovrCanvasIsDirty(Canvas* canvas);
void lovrCanvasResolve(Canvas* canvas);
bool lovrCanvasIsStereo(Canvas* canvas);
int lovrCanvasGetWidth(Canvas* canvas);
int lovrCanvasGetHeight(Canvas* canvas);
int lovrCanvasGetMSAA(Canvas* canvas);
Texture* lovrCanvasGetDepthTexture(Canvas* canvas);
TextureData* lovrCanvasNewTextureData(Canvas* canvas, int index);
