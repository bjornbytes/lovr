#include "graphics/canvas.h"
#include "graphics/graphics.h"
#include "core/ref.h"
#include <stdlib.h>

const Attachment* lovrCanvasGetAttachments(Canvas* canvas, uint32_t* count) {
  if (count) *count = canvas->attachmentCount;
  return canvas->attachments;
}

void lovrCanvasSetAttachments(Canvas* canvas, Attachment* attachments, uint32_t count) {
  lovrAssert(count > 0, "A Canvas must have at least one attached Texture");
  lovrAssert(count <= MAX_CANVAS_ATTACHMENTS, "Only %d textures can be attached to a Canvas, got %d\n", MAX_CANVAS_ATTACHMENTS, count);

  if (!canvas->needsAttach && count == canvas->attachmentCount && !memcmp(canvas->attachments, attachments, count * sizeof(Attachment))) {
    return;
  }

  lovrGraphicsFlushCanvas(canvas);

  for (uint32_t i = 0; i < count; i++) {
    Texture* texture = attachments[i].texture;
    uint32_t slice = attachments[i].slice;
    uint32_t level = attachments[i].level;
    uint32_t width = lovrTextureGetWidth(texture, level);
    uint32_t height = lovrTextureGetHeight(texture, level);
    uint32_t depth = lovrTextureGetDepth(texture, level);
    uint32_t mipmaps = lovrTextureGetMipmapCount(texture);
    bool hasDepthBuffer = canvas->flags.depth.enabled;
    lovrAssert(slice < depth, "Invalid attachment slice (Texture has %d, got %d)", depth, slice + 1);
    lovrAssert(level < mipmaps, "Invalid attachment mipmap level (Texture has %d, got %d)", mipmaps, level + 1);
    lovrAssert(!hasDepthBuffer || width == canvas->width, "Texture width of %d does not match Canvas width (%d)", width, canvas->width);
    lovrAssert(!hasDepthBuffer || height == canvas->height, "Texture height of %d does not match Canvas height (%d)", height, canvas->height);
#ifndef __ANDROID__ // On multiview canvases, the multisample settings can be different
    lovrAssert(texture->msaa == canvas->flags.msaa, "Texture MSAA does not match Canvas MSAA");
#endif
    lovrRetain(texture);
  }

  for (uint32_t i = 0; i < canvas->attachmentCount; i++) {
    lovrRelease(Texture, canvas->attachments[i].texture);
  }

  memcpy(canvas->attachments, attachments, count * sizeof(Attachment));
  canvas->attachmentCount = count;
  canvas->needsAttach = true;
}

bool lovrCanvasIsStereo(Canvas* canvas) {
  return canvas->flags.stereo;
}

uint32_t lovrCanvasGetWidth(Canvas* canvas) {
  return canvas->width;
}

uint32_t lovrCanvasGetHeight(Canvas* canvas) {
  return canvas->height;
}

uint32_t lovrCanvasGetMSAA(Canvas* canvas) {
  return canvas->flags.msaa;
}

Texture* lovrCanvasGetDepthTexture(Canvas* canvas) {
  return canvas->depth.texture;
}
