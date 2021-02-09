#include "data/image.h"

#pragma once

#define MAX_CANVAS_ATTACHMENTS 4

struct Image;
struct Texture;

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

typedef struct Canvas Canvas;
Canvas* lovrCanvasCreate(uint32_t width, uint32_t height, CanvasFlags flags);
Canvas* lovrCanvasCreateFromHandle(uint32_t width, uint32_t height, CanvasFlags flags, uint32_t framebuffer, uint32_t depthBuffer, uint32_t resolveBuffer, uint32_t attachmentCount, bool immortal);
void lovrCanvasDestroy(void* ref);
const Attachment* lovrCanvasGetAttachments(Canvas* canvas, uint32_t* count);
void lovrCanvasSetAttachments(Canvas* canvas, Attachment* attachments, uint32_t count);
void lovrCanvasResolve(Canvas* canvas);
bool lovrCanvasIsStereo(Canvas* canvas);
void lovrCanvasSetStereo(Canvas* canvas, bool stereo);
uint32_t lovrCanvasGetWidth(Canvas* canvas);
uint32_t lovrCanvasGetHeight(Canvas* canvas);
void lovrCanvasSetWidth(Canvas* canvas, uint32_t width);
void lovrCanvasSetHeight(Canvas* canvas, uint32_t height);
uint32_t lovrCanvasGetMSAA(Canvas* canvas);
struct Texture* lovrCanvasGetDepthTexture(Canvas* canvas);
struct Image* lovrCanvasNewImage(Canvas* canvas, uint32_t index);
