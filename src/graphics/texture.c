#include "graphics/texture.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

static void lovrTextureCreateStorage(Texture* texture) {
  TextureData* textureData = texture->textureData;

  if (textureData->format.compressed || !textureData->mipmaps.generated) {
    return;
  }

  int w = textureData->width;
  int h = textureData->height;
  int mipmapCount = log2(MAX(w, h)) + 1;
  GLenum internalFormat = textureData->format.glInternalFormat;
  GLenum format = textureData->format.glFormat;
  if (GLAD_GL_ARB_texture_storage) {
    glTexStorage2D(GL_TEXTURE_2D, mipmapCount, internalFormat, w, h);
  } else {
    for (int i = 0; i < mipmapCount; i++) {
      glTexImage2D(GL_TEXTURE_2D, i, internalFormat, w, h, 0, format, GL_UNSIGNED_BYTE, NULL);
      w = MAX(w >> 1, 1);
      h = MAX(h >> 1, 1);
    }
  }
}

Texture* lovrTextureCreate(TextureData* textureData) {
  Texture* texture = lovrAlloc(sizeof(Texture), lovrTextureDestroy);
  if (!texture) return NULL;

  texture->framebuffer = 0;
  texture->depthBuffer = 0;
  texture->textureData = textureData;
  glGenTextures(1, &texture->id);
  lovrGraphicsBindTexture(texture);
  lovrTextureCreateStorage(texture);
  lovrTextureRefresh(texture);
  lovrTextureSetFilter(texture, FILTER_LINEAR, FILTER_LINEAR);
  lovrTextureSetWrap(texture, WRAP_REPEAT, WRAP_REPEAT);

  return texture;
}

Texture* lovrTextureCreateWithFramebuffer(TextureData* textureData, TextureProjection projection, int msaa) {
  Texture* texture = lovrTextureCreate(textureData);
  if (!texture) return NULL;

  int width = texture->textureData->width;
  int height = texture->textureData->height;
  texture->projection = projection;
  texture->msaa = msaa;

  // Framebuffer
  glGenFramebuffers(1, &texture->framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, texture->framebuffer);

  // Color attachment
  if (msaa) {
    glGenRenderbuffers(1, &texture->msaaId);
    glBindRenderbuffer(GL_RENDERBUFFER, texture->msaaId);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, texture->msaaId);
  } else {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);
  }

  // Depth attachment
  if (projection == PROJECTION_PERSPECTIVE) {
    glGenRenderbuffers(1, &texture->depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, texture->depthBuffer);
    if (msaa) {
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, GL_DEPTH_COMPONENT, width, height);
    } else {
      glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, texture->depthBuffer);
  }

  // Resolve framebuffer
  if (msaa) {
    glGenFramebuffers(1, &texture->resolveFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, texture->resolveFramebuffer);
    glBindTexture(GL_TEXTURE_2D, texture->id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, texture->framebuffer);
  }

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    error("Error creating texture\n");
  } else {
    lovrGraphicsClear(1, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  return texture;
}

void lovrTextureDestroy(const Ref* ref) {
  Texture* texture = containerof(ref, Texture);
  lovrTextureDataDestroy(texture->textureData);
  if (texture->framebuffer) {
    glDeleteFramebuffers(1, &texture->framebuffer);
  }
  glDeleteTextures(1, &texture->id);
  free(texture);
}

void lovrTextureBindFramebuffer(Texture* texture) {
  if (!texture->framebuffer) {
    error("Texture cannot be used as a canvas");
  }

  int w = texture->textureData->width;
  int h = texture->textureData->height;

  lovrGraphicsBindFramebuffer(texture->framebuffer);
  lovrGraphicsSetViewport(0, 0, w, h);

  if (texture->projection == PROJECTION_ORTHOGRAPHIC) {
    float projection[16];
    mat4_orthographic(projection, 0, w, 0, h, -1, 1);
    lovrGraphicsSetProjection(projection);
  } else if (texture->projection == PROJECTION_PERSPECTIVE) {
    mat4 projection = lovrGraphicsGetProjection();
    float b = projection[5];
    float c = projection[10];
    float d = projection[14];
    float aspect = (float) w / h;
    float k = (c - 1.f) / (c + 1.f);
    float near = (d * (1.f - k)) / (2.f * k);
    float far = k * near;
    float fov = 2.f * atan(1.f / b);
    float newProjection[16];
    mat4_perspective(newProjection, near, far, fov, aspect);
    lovrGraphicsSetProjection(newProjection);
  }
}

void lovrTextureResolveMSAA(Texture* texture) {
  if (!texture->msaa) {
    return;
  }

  int w = texture->textureData->width;
  int h = texture->textureData->height;

  glBindFramebuffer(GL_READ_FRAMEBUFFER, texture->framebuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, texture->resolveFramebuffer);
  glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void lovrTextureRefresh(Texture* texture) {
  TextureData* textureData = texture->textureData;
  GLenum glInternalFormat = textureData->format.glInternalFormat;
  GLenum glFormat = textureData->format.glFormat;
  lovrGraphicsBindTexture(texture);

  if (textureData->format.compressed) {
    Mipmap m; int i;
    vec_foreach(&textureData->mipmaps.list, m, i) {
      glCompressedTexImage2D(GL_TEXTURE_2D, i, glInternalFormat, m.width, m.height, 0, m.size, m.data);
    }
  } else {
    int w = textureData->width;
    int h = textureData->height;
    glTexImage2D(GL_TEXTURE_2D, 0, glInternalFormat, w, h, 0, glFormat, GL_UNSIGNED_BYTE, textureData->data);
    if (textureData->mipmaps.generated) {
      glGenerateMipmap(GL_TEXTURE_2D);
    }
  }
}

int lovrTextureGetHeight(Texture* texture) {
  return texture->textureData->height;
}

int lovrTextureGetWidth(Texture* texture) {
  return texture->textureData->width;
}

void lovrTextureGetFilter(Texture* texture, FilterMode* min, FilterMode* mag) {
  *min = texture->filterMin;
  *mag = texture->filterMag;
}

void lovrTextureSetFilter(Texture* texture, FilterMode min, FilterMode mag) {
  texture->filterMin = min;
  texture->filterMag = mag;
  lovrGraphicsBindTexture(texture);
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
  lovrGraphicsBindTexture(texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, horizontal);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, vertical);
}
