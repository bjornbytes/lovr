#include "graphics/texture.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

static void lovrTextureCreateStorage(Texture* texture) {
  TextureData* textureData = texture->slices[0];

  if (textureData->format.compressed || texture->type == TEXTURE_CUBE) {
    return;
  }

  int w = textureData->width;
  int h = textureData->height;
  int mipmapCount = log2(MAX(w, h)) + 1;
  GLenum internalFormat;
  if (lovrGraphicsIsGammaCorrect() && texture->srgb) {
    internalFormat = textureData->format.glInternalFormat.srgb;
  } else {
    internalFormat = textureData->format.glInternalFormat.linear;
  }
  GLenum format = textureData->format.glFormat;
#ifndef EMSCRIPTEN
  if (GLAD_GL_ARB_texture_storage) {
#endif
    glTexStorage2D(GL_TEXTURE_2D, mipmapCount, internalFormat, w, h);
#ifndef EMSCRIPTEN
  } else {
    for (int i = 0; i < mipmapCount; i++) {
      glTexImage2D(GL_TEXTURE_2D, i, internalFormat, w, h, 0, format, GL_UNSIGNED_BYTE, NULL);
      w = MAX(w >> 1, 1);
      h = MAX(h >> 1, 1);
    }
  }
#endif
}

static void validateSlices(TextureType type, TextureData* slices[6], int sliceCount) {
  if (type == TEXTURE_CUBE) {
    lovrAssert(sliceCount == 6, "Cube textures must have 6 images");
    int width = slices[0]->width;
    int height = slices[0]->height;
    lovrAssert(width == height, "Cube textures must be square");
    for (int i = 1; i < sliceCount; i++) {
      int hasSameDimensions = slices[i]->width == width && slices[i]->height == height;
      lovrAssert(hasSameDimensions, "All textures in a cube texture must have the same dimensions");
    }
  } else if (type == TEXTURE_2D) {
    lovrAssert(sliceCount == 1, "2D textures can only contain a single image");
  } else {
    lovrThrow("Unknown texture type");
  }
}

Texture* lovrTextureCreate(TextureType type, TextureData* slices[6], int sliceCount, bool srgb) {
  Texture* texture = lovrAlloc(sizeof(Texture), lovrTextureDestroy);
  if (!texture) return NULL;

  texture->type = type;
  validateSlices(type, slices, sliceCount);
  texture->sliceCount = sliceCount;
  memcpy(texture->slices, slices, sliceCount * sizeof(TextureData*));
  texture->framebuffer = 0;
  texture->depthBuffer = 0;
  texture->srgb = srgb;
  glGenTextures(1, &texture->id);
  lovrGraphicsBindTexture(texture, type, 0);
  lovrTextureCreateStorage(texture);
  lovrTextureRefresh(texture);
  lovrTextureSetFilter(texture, lovrGraphicsGetDefaultFilter());
  WrapMode wrapMode = (type == TEXTURE_CUBE) ? WRAP_CLAMP : WRAP_REPEAT;
  lovrTextureSetWrap(texture, (TextureWrap) { .s = wrapMode, .t = wrapMode, .r = wrapMode });

  return texture;
}

Texture* lovrTextureCreateWithFramebuffer(TextureData* textureData, TextureProjection projection, int msaa) {
  Texture* texture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true);
  if (!texture) return NULL;

  int width = texture->width;
  int height = texture->height;
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

  lovrAssert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Error creating texture");
  lovrGraphicsClear(true, true);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return texture;
}

void lovrTextureDestroy(const Ref* ref) {
  Texture* texture = containerof(ref, Texture);
  for (int i = 0; i < texture->sliceCount; i++) {
    lovrTextureDataDestroy(texture->slices[i]);
  }
  if (texture->framebuffer) {
    glDeleteFramebuffers(1, &texture->framebuffer);
  }
  glDeleteTextures(1, &texture->id);
  free(texture);
}

void lovrTextureBindFramebuffer(Texture* texture) {
  lovrAssert(texture->framebuffer, "Texture cannot be used as a canvas");
  lovrGraphicsBindFramebuffer(texture->framebuffer);
  lovrGraphicsSetViewport(0, 0, texture->width, texture->height);

  if (texture->projection == PROJECTION_ORTHOGRAPHIC) {
    float projection[16];
    mat4_orthographic(projection, 0, texture->width, 0, texture->height, -1, 1);
    lovrGraphicsSetProjection(projection);
  } else if (texture->projection == PROJECTION_PERSPECTIVE) {
    mat4 projection = lovrGraphicsGetProjection();
    float b = projection[5];
    float c = projection[10];
    float d = projection[14];
    float aspect = (float) texture->width / texture->height;
    float k = (c - 1.f) / (c + 1.f);
    float near = (d * (1.f - k)) / (2.f * k);
    float far = k * near;
    float fov = -2.f * atan(1.f / b);
    float newProjection[16];
    mat4_perspective(newProjection, near, far, fov, aspect);
    lovrGraphicsSetProjection(newProjection);
  }
}

void lovrTextureResolveMSAA(Texture* texture) {
  if (!texture->msaa) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return;
  }

  int width = texture->width;
  int height = texture->height;
  glBindFramebuffer(GL_READ_FRAMEBUFFER, texture->framebuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, texture->resolveFramebuffer);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void lovrTextureRefresh(Texture* texture) {
  lovrGraphicsBindTexture(texture, texture->type, 0);

  validateSlices(texture->type, texture->slices, texture->sliceCount);
  texture->width = texture->slices[0]->width;
  texture->height = texture->slices[0]->height;

  for (int i = 0; i < texture->sliceCount; i++) {
    TextureData* textureData = texture->slices[i];
    GLenum glInternalFormat;
    if (lovrGraphicsIsGammaCorrect() && texture->srgb) {
      glInternalFormat = textureData->format.glInternalFormat.srgb;
    } else {
      glInternalFormat = textureData->format.glInternalFormat.linear;
    }
    GLenum glFormat = textureData->format.glFormat;
    GLenum binding = (texture->type == TEXTURE_CUBE) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + i : GL_TEXTURE_2D;

    if (textureData->format.compressed) {
      Mipmap m; int i;
      vec_foreach(&textureData->mipmaps.list, m, i) {
        glCompressedTexImage2D(binding, i, glInternalFormat, m.width, m.height, 0, m.size, m.data);
      }
    } else {
      int w = textureData->width;
      int h = textureData->height;

      if (textureData->data) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, glFormat, GL_UNSIGNED_BYTE, textureData->data);
      }

      if (textureData->mipmaps.generated) {
        glGenerateMipmap(GL_TEXTURE_2D); // TODO
      }
    }
  }
}

TextureFilter lovrTextureGetFilter(Texture* texture) {
  return texture->filter;
}

void lovrTextureSetFilter(Texture* texture, TextureFilter filter) {
  bool hasMipmaps = texture->slices[0]->format.compressed || texture->slices[0]->mipmaps.generated;
  float anisotropy = filter.mode == FILTER_ANISOTROPIC ? MAX(filter.anisotropy, 1.) : 1.;
  lovrGraphicsBindTexture(texture, texture->type, 0);
  texture->filter = filter;

  switch (filter.mode) {
    case FILTER_NEAREST:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      break;

    case FILTER_BILINEAR:
      if (hasMipmaps) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      }
      break;

    case FILTER_TRILINEAR:
    case FILTER_ANISOTROPIC:
      if (hasMipmaps) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      }
      break;
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
}

TextureWrap lovrTextureGetWrap(Texture* texture) {
  return texture->wrap;
}

void lovrTextureSetWrap(Texture* texture, TextureWrap wrap) {
  texture->wrap = wrap;
  lovrGraphicsBindTexture(texture, texture->type, 0);
  glTexParameteri(texture->type, GL_TEXTURE_WRAP_S, wrap.s);
  glTexParameteri(texture->type, GL_TEXTURE_WRAP_T, wrap.t);
  if (texture->type == TEXTURE_CUBE) {
    glTexParameteri(texture->type, GL_TEXTURE_WRAP_R, wrap.r);
  }
}
