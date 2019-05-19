#include "graphics/canvas.h"
#include "graphics/graphics.h"

const Attachment* lovrCanvasGetAttachments(Canvas* canvas, int* count) {
  if (count) *count = canvas->attachmentCount;
  return canvas->attachments;
}

void lovrCanvasSetAttachments(Canvas* canvas, Attachment* attachments, int count) {
  lovrAssert(count > 0, "A Canvas must have at least one attached Texture");
  lovrAssert(count <= MAX_CANVAS_ATTACHMENTS, "Only %d textures can be attached to a Canvas, got %d\n", MAX_CANVAS_ATTACHMENTS, count);

  if (!canvas->needsAttach && count == canvas->attachmentCount && !memcmp(canvas->attachments, attachments, count * sizeof(Attachment))) {
    return;
  }

  lovrGraphicsFlushCanvas(canvas);

  for (int i = 0; i < count; i++) {
    Texture* texture = attachments[i].texture;
    int slice = attachments[i].slice;
    int level = attachments[i].level;
    int width = lovrTextureGetWidth(texture, level);
    int height = lovrTextureGetHeight(texture, level);
    int depth = lovrTextureGetDepth(texture, level);
    int mipmaps = lovrTextureGetMipmapCount(texture);
    bool hasDepthBuffer = canvas->flags.depth.enabled;
    lovrAssert(slice >= 0 && slice < depth, "Invalid attachment slice (Texture has %d, got %d)", depth, slice + 1);
    lovrAssert(level >= 0 && level < mipmaps, "Invalid attachment mipmap level (Texture has %d, got %d)", mipmaps, level + 1);
    lovrAssert(!hasDepthBuffer || width == canvas->width, "Texture width of %d does not match Canvas width (%d)", width, canvas->width);
    lovrAssert(!hasDepthBuffer || height == canvas->height, "Texture height of %d does not match Canvas height (%d)", height, canvas->height);
    lovrAssert(texture->msaa == canvas->flags.msaa, "Texture MSAA does not match Canvas MSAA");
    lovrRetain(texture);
  }

  for (int i = 0; i < canvas->attachmentCount; i++) {
    lovrRelease(Texture, canvas->attachments[i].texture);
  }

  memcpy(canvas->attachments, attachments, count * sizeof(Attachment));
  canvas->attachmentCount = count;
  canvas->needsAttach = true;
}

bool lovrCanvasIsStereo(Canvas* canvas) {
  return canvas->flags.stereo;
}

int lovrCanvasGetWidth(Canvas* canvas) {
  return canvas->width;
}

int lovrCanvasGetHeight(Canvas* canvas) {
  return canvas->height;
}

int lovrCanvasGetMSAA(Canvas* canvas) {
  return canvas->flags.msaa;
}

Texture* lovrCanvasGetDepthTexture(Canvas* canvas) {
  return canvas->depth.texture;
}
