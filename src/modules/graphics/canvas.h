#include "graphics/texture.h"
#include "graphics/opengl.h"

#pragma once

#define MAX_CANVAS_ATTACHMENTS 4

struct Texture;
struct TextureData;

typedef struct Attachment {
  struct Texture* texture;
  uint32_t slice;
  uint32_t level;
} Attachment;

typedef struct {
  struct {
    bool enabled;
    bool readable;
    TextureFormat format;
  } depth;
  uint32_t msaa;
  bool stereo;
  bool mipmaps;
} CanvasFlags;

typedef struct Canvas {
  uint32_t width;
  uint32_t height;
  CanvasFlags flags;
  Attachment attachments[MAX_CANVAS_ATTACHMENTS];
  Attachment depth;
  uint32_t attachmentCount;
  bool needsAttach;
  bool needsResolve;
  GPU_CANVAS_FIELDS
} Canvas;

Canvas* lovrCanvasInit(Canvas* canvas, uint32_t width, uint32_t height, CanvasFlags flags);
Canvas* lovrCanvasInitFromHandle(Canvas* canvas, uint32_t width, uint32_t height, CanvasFlags flags, uint32_t framebuffer, uint32_t depthBuffer, uint32_t resolveBuffer, uint32_t attachmentCount, bool immortal);
#define lovrCanvasCreate(...) lovrCanvasInit(lovrAlloc(Canvas), __VA_ARGS__)
#define lovrCanvasCreateFromHandle(...) lovrCanvasInitFromHandle(lovrAlloc(Canvas), __VA_ARGS__)
void lovrCanvasDestroy(void* ref);
const Attachment* lovrCanvasGetAttachments(Canvas* canvas, uint32_t* count);
void lovrCanvasSetAttachments(Canvas* canvas, Attachment* attachments, uint32_t count);
void lovrCanvasResolve(Canvas* canvas);
bool lovrCanvasIsStereo(Canvas* canvas);
uint32_t lovrCanvasGetWidth(Canvas* canvas);
uint32_t lovrCanvasGetHeight(Canvas* canvas);
uint32_t lovrCanvasGetMSAA(Canvas* canvas);
struct Texture* lovrCanvasGetDepthTexture(Canvas* canvas);
struct TextureData* lovrCanvasNewTextureData(Canvas* canvas, uint32_t index);
