#include "graphics/texture.h"
#include "graphics/graphics.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

GLenum lovrTextureFormatGetGLFormat(TextureFormat format) {
  switch (format) {
    case FORMAT_RGB: return GL_RGB;
    case FORMAT_RGBA: return GL_RGBA;
    case FORMAT_RGBA16F: return GL_RGBA;
    case FORMAT_RGBA32F: return GL_RGBA;
    case FORMAT_RG11B10F: return GL_RGB;
    case FORMAT_DXT1: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    case FORMAT_DXT3: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case FORMAT_DXT5: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
  }
}

GLenum lovrTextureFormatGetGLInternalFormat(TextureFormat format, bool srgb) {
  switch (format) {
    case FORMAT_RGB: return srgb ? GL_SRGB8 : GL_RGB8;
    case FORMAT_RGBA: return srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    case FORMAT_RGBA16F: return GL_RGBA16F;
    case FORMAT_RGBA32F: return GL_RGBA32F;
    case FORMAT_RG11B10F: return GL_R11F_G11F_B10F;
    case FORMAT_DXT1: return srgb ? GL_COMPRESSED_SRGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    case FORMAT_DXT3: return srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case FORMAT_DXT5: return srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
  }
}

bool lovrTextureFormatIsCompressed(TextureFormat format) {
  switch (format) {
    case FORMAT_DXT1:
    case FORMAT_DXT3:
    case FORMAT_DXT5:
      return true;
    default:
      return false;
  }
}

static void lovrTextureUpload(Texture* texture) {
  TextureData* textureData = texture->slices[0];
  texture->width = textureData->width;
  texture->height = textureData->height;
  bool srgb = lovrGraphicsIsGammaCorrect() && texture->srgb;

  // Allocate storage
  if (!lovrTextureFormatIsCompressed(textureData->format) && texture->type != TEXTURE_CUBE) {
    int w = textureData->width;
    int h = textureData->height;
    int mipmapCount = log2(MAX(w, h)) + 1;
    GLenum glFormat = lovrTextureFormatGetGLFormat(textureData->format);
    GLenum internalFormat = lovrTextureFormatGetGLInternalFormat(textureData->format, srgb);
#ifndef EMSCRIPTEN
    if (GLAD_GL_ARB_texture_storage) {
#endif
      glTexStorage2D(GL_TEXTURE_2D, mipmapCount, internalFormat, w, h);
#ifndef EMSCRIPTEN
    } else {
      for (int i = 0; i < mipmapCount; i++) {
        glTexImage2D(GL_TEXTURE_2D, i, internalFormat, w, h, 0, glFormat, GL_UNSIGNED_BYTE, NULL);
        w = MAX(w >> 1, 1);
        h = MAX(h >> 1, 1);
      }
    }
#endif
  }

  // Upload data
  for (int i = 0; i < texture->sliceCount; i++) {
    TextureData* textureData = texture->slices[i];
    GLenum glFormat = lovrTextureFormatGetGLFormat(textureData->format);
    GLenum glInternalFormat = lovrTextureFormatGetGLInternalFormat(textureData->format, srgb);
    GLenum binding = (texture->type == TEXTURE_CUBE) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + i : GL_TEXTURE_2D;

    if (lovrTextureFormatIsCompressed(textureData->format)) {
      Mipmap m; int i;
      vec_foreach(&textureData->mipmaps.list, m, i) {
        glCompressedTexImage2D(binding, i, glInternalFormat, m.width, m.height, 0, m.size, m.data);
      }
    } else {
      int w = textureData->width;
      int h = textureData->height;

      if (texture->type == TEXTURE_CUBE) {
        glTexImage2D(binding, 0, glInternalFormat, w, h, 0, glFormat, GL_UNSIGNED_BYTE, textureData->data);
      } else if (textureData->data) {
        glTexSubImage2D(binding, 0, 0, 0, w, h, glFormat, GL_UNSIGNED_BYTE, textureData->data);
      }

      if (textureData->mipmaps.generated) {
        glGenerateMipmap(texture->type);
      }
    }
  }
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
  texture->srgb = srgb;
  glGenTextures(1, &texture->id);
  lovrGraphicsBindTexture(texture, type, 0);
  lovrTextureUpload(texture);
  lovrTextureSetFilter(texture, lovrGraphicsGetDefaultFilter());
  WrapMode wrapMode = (type == TEXTURE_CUBE) ? WRAP_CLAMP : WRAP_REPEAT;
  lovrTextureSetWrap(texture, (TextureWrap) { .s = wrapMode, .t = wrapMode, .r = wrapMode });

  return texture;
}

void lovrTextureDestroy(const Ref* ref) {
  Texture* texture = containerof(ref, Texture);
  for (int i = 0; i < texture->sliceCount; i++) {
    lovrTextureDataDestroy(texture->slices[i]);
  }
  glDeleteTextures(1, &texture->id);
  free(texture);
}

TextureFilter lovrTextureGetFilter(Texture* texture) {
  return texture->filter;
}

void lovrTextureSetFilter(Texture* texture, TextureFilter filter) {
  bool hasMipmaps = lovrTextureFormatIsCompressed(texture->slices[0]->format) || texture->slices[0]->mipmaps.generated;
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
