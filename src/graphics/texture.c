#include "graphics/texture.h"
#include "util.h"
#include <stdlib.h>

Texture* lovrTextureCreate(TextureData* textureData, int hasFramebuffer) {
  Texture* texture = lovrAlloc(sizeof(Texture), lovrTextureDestroy);
  if (!texture) return NULL;

  texture->textureData = textureData;
  glGenTextures(1, &texture->id);

  if (textureData) {
    lovrTextureBind(texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureData->width, textureData->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData->data);
    lovrTextureSetFilter(texture, FILTER_LINEAR, FILTER_LINEAR);
    lovrTextureSetWrap(texture, WRAP_REPEAT, WRAP_REPEAT);
  }

  if (hasFramebuffer) {
    glGenFramebuffers(1, &texture->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, texture->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  } else {
    texture->fbo = 0;
  }

  return texture;
}

void lovrTextureDestroy(const Ref* ref) {
  Texture* texture = containerof(ref, Texture);
  lovrTextureDataDestroy(texture->textureData);
  if (texture->fbo) {
    glDeleteFramebuffers(1, &texture->fbo);
  }
  glDeleteTextures(1, &texture->id);
  free(texture);
}

void lovrTextureDataDestroy(TextureData* textureData) {
  free(textureData->data);
  free(textureData);
}

void lovrTextureBind(Texture* texture) {
  glBindTexture(GL_TEXTURE_2D, texture->id);
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

void lovrTextureRenderTo(Texture* texture, textureRenderCallback callback, void* userdata) {
  if (!texture->fbo) {
    error("Texture does not have a framebuffer");
  }

  int oldViewport[4];
  glGetIntegerv(GL_VIEWPORT, oldViewport);
  glViewport(0, 0, texture->textureData->width, texture->textureData->height);
  glBindFramebuffer(GL_FRAMEBUFFER, texture->fbo);
  callback(userdata);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
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
