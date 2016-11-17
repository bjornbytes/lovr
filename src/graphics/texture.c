#include "texture.h"
#include "buffer.h"
#include "../util.h"
#include <stdlib.h>

Texture* lovrTextureCreate(void* data, int size) {
  Texture* texture = malloc(sizeof(Texture));
  if (!texture) return NULL;

  texture->buffer = 0;
  glGenTextures(1, &texture->id);

  if (data) {
    int channels;
    unsigned char* image = loadImage(data, size, &texture->width, &texture->height, &channels, 3);
    if (!image) {
      lovrTextureDestroy(texture);
      return NULL;
    }

    lovrTextureBind(texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texture->width, texture->height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    lovrTextureSetFilter(texture, FILTER_LINEAR, FILTER_LINEAR);
    lovrTextureSetWrap(texture, WRAP_REPEAT, WRAP_REPEAT);
    free(image);
  }

  return texture;
}

Texture* lovrTextureCreateFromBuffer(Buffer* buffer) {
  if (!buffer) {
    return NULL;
  }

  Texture* texture = malloc(sizeof(Texture));
  if (!texture) return NULL;

  glGenTextures(1, &texture->id);
  texture->buffer = buffer->vbo;
  lovrTextureBind(texture);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, buffer->vbo);

  return texture;
}

void lovrTextureDestroy(Texture* texture) {
  glDeleteTextures(1, &texture->id);
  free(texture);
}

void lovrTextureBind(Texture* texture) {
  glBindTexture(texture->buffer ? GL_TEXTURE_BUFFER : GL_TEXTURE_2D, texture->id);
}

void lovrTextureRefresh(Texture* texture) {
  if (!texture->buffer) {
    return;
  }

  lovrTextureBind(texture);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, texture->buffer);
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
