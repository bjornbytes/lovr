#include "graphics/texture.h"
#include "graphics/buffer.h"
#include "util.h"
#include <stdlib.h>

Texture* lovrTextureCreateFromData(TextureData* textureData) {
  Texture* texture = lovrAlloc(sizeof(Texture), lovrTextureDestroy);
  if (!texture) return NULL;

  texture->textureData = textureData;
  texture->type = TEXTURE_IMAGE;
  texture->buffer = NULL;
  glGenTextures(1, &texture->id);

  if (textureData) {
    lovrTextureBind(texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureData->width, textureData->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData->data);
    lovrTextureSetFilter(texture, FILTER_LINEAR, FILTER_LINEAR);
    lovrTextureSetWrap(texture, WRAP_REPEAT, WRAP_REPEAT);
  }

  return texture;
}

Texture* lovrTextureCreateFromBuffer(Buffer* buffer) {
  if (!buffer) {
    return NULL;
  }

  Texture* texture = lovrAlloc(sizeof(Texture), lovrTextureDestroy);
  if (!texture) return NULL;

  glGenTextures(1, &texture->id);
  texture->textureData = NULL;
  texture->type = TEXTURE_BUFFER;
  texture->buffer = buffer;
  lovrTextureBind(texture);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, buffer->vbo);
  lovrRetain(&buffer->ref);

  return texture;
}

void lovrTextureDestroy(const Ref* ref) {
  Texture* texture = containerof(ref, Texture);
  if (texture->buffer) {
    lovrRelease(&texture->buffer->ref);
  }
  lovrTextureDataDestroy(texture->textureData);
  glDeleteTextures(1, &texture->id);
  free(texture);
}

void lovrTextureDataDestroy(TextureData* textureData) {
  free(textureData->data);
  free(textureData);
}

void lovrTextureBind(Texture* texture) {
  glBindTexture(texture->type, texture->id);
}

void lovrTextureRefresh(Texture* texture) {
  if (!texture->buffer) {
    return;
  }

  lovrTextureBind(texture);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, texture->buffer->vbo);
}

int lovrTextureGetHeight(Texture* texture) {
  return texture->height;
}

int lovrTextureGetWidth(Texture* texture) {
  return texture->width;
}

void lovrTextureGetFilter(Texture* texture, FilterMode* min, FilterMode* mag) {
  *min = texture->filterMin;
  *mag = texture->filterMag;
}

void lovrTextureSetFilter(Texture* texture, FilterMode min, FilterMode mag) {
  texture->filterMin = min;
  texture->filterMag = mag;
  lovrTextureBind(texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
}

void lovrTextureGetWrap(Texture* texture, WrapMode* horizontal, WrapMode* vertical) {
  *horizontal = texture->wrapHorizontal;
  *vertical = texture->wrapVertical;
}

void lovrTextureSetWrap(Texture* texture, WrapMode horizontal, WrapMode vertical) {
  texture->wrapHorizontal = horizontal;
  texture->wrapVertical = vertical;
  lovrTextureBind(texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, horizontal);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, vertical);
}
