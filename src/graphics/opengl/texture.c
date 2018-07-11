#include "graphics/opengl/opengl.h"
#include "graphics/graphics.h"
#include "lib/vec/vec.h"
#include "math/math.h"
#include <math.h>

GLenum lovrConvertWrapMode(WrapMode mode) {
  switch (mode) {
    case WRAP_CLAMP: return GL_CLAMP_TO_EDGE;
    case WRAP_REPEAT: return GL_REPEAT;
    case WRAP_MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
  }
}

GLenum lovrConvertTextureFormat(TextureFormat format) {
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

GLenum lovrConvertTextureFormatInternal(TextureFormat format, bool srgb) {
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

bool lovrIsTextureFormatCompressed(TextureFormat format) {
  switch (format) {
    case FORMAT_DXT1:
    case FORMAT_DXT3:
    case FORMAT_DXT5:
      return true;
    default:
      return false;
  }
}

static void lovrTextureAllocate(Texture* texture, TextureData* textureData) {
  texture->allocated = true;
  texture->width = textureData->width;
  texture->height = textureData->height;

  if (lovrIsTextureFormatCompressed(textureData->format)) {
    return;
  }

  int w = textureData->width;
  int h = textureData->height;
  int mipmapCount = log2(MAX(w, h)) + 1;
  bool srgb = lovrGraphicsIsGammaCorrect() && texture->srgb;
  GLenum glFormat = lovrConvertTextureFormat(textureData->format);
  GLenum internalFormat = lovrConvertTextureFormatInternal(textureData->format, srgb);
#ifndef EMSCRIPTEN
  if (GLAD_GL_ARB_texture_storage) {
#endif
  if (texture->type == TEXTURE_ARRAY) {
    glTexStorage3D(texture->glType, mipmapCount, internalFormat, w, h, texture->depth);
  } else {
    glTexStorage2D(texture->glType, mipmapCount, internalFormat, w, h);
  }
#ifndef EMSCRIPTEN
  } else {
    for (int i = 0; i < mipmapCount; i++) {
      switch (texture->type) {
        case TEXTURE_2D:
          glTexImage2D(texture->glType, i, internalFormat, w, h, 0, glFormat, GL_UNSIGNED_BYTE, NULL);
          break;

        case TEXTURE_CUBE:
          for (int face = 0; face < 6; face++) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, i, internalFormat, w, h, 0, glFormat, GL_UNSIGNED_BYTE, NULL);
          }
          break;

        case TEXTURE_ARRAY:
        case TEXTURE_VOLUME:
          glTexImage3D(texture->glType, i, internalFormat, w, h, texture->depth, 0, glFormat, GL_UNSIGNED_BYTE, NULL);
          break;
      }
      w = MAX(w >> 1, 1);
      h = MAX(h >> 1, 1);
    }
  }
#endif
}

Texture* lovrTextureCreate(TextureType type, TextureData** slices, int depth, bool srgb, bool mipmaps) {
  Texture* texture = lovrAlloc(sizeof(Texture), lovrTextureDestroy);
  if (!texture) return NULL;

  texture->type = type;
  switch (type) {
    case TEXTURE_2D: texture->glType = GL_TEXTURE_2D; break;
    case TEXTURE_ARRAY: texture->glType = GL_TEXTURE_2D_ARRAY; break;
    case TEXTURE_CUBE: texture->glType = GL_TEXTURE_CUBE_MAP; break;
    case TEXTURE_VOLUME: texture->glType = GL_TEXTURE_3D; break;
  }

  texture->slices = calloc(depth, sizeof(TextureData**));
  texture->depth = depth;
  texture->srgb = srgb;
  texture->mipmaps = mipmaps;

  WrapMode wrap = type == TEXTURE_CUBE ? WRAP_CLAMP : WRAP_REPEAT;
  glGenTextures(1, &texture->id);
  gpuBindTexture(texture, 0);
  lovrTextureSetFilter(texture, lovrGraphicsGetDefaultFilter());
  lovrTextureSetWrap(texture, (TextureWrap) { .s = wrap, .t = wrap, .r = wrap });

  lovrAssert(type != TEXTURE_CUBE || depth == 6, "6 images are required for a cube texture\n");
  lovrAssert(type != TEXTURE_2D || depth == 1, "2D textures can only contain a single image");

  if (slices) {
    for (int i = 0; i < depth; i++) {
      lovrTextureReplacePixels(texture, slices[i], i);
    }
  }

  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  for (int i = 0; i < texture->depth; i++) {
    lovrRelease(texture->slices[i]);
  }
  glDeleteTextures(1, &texture->id);
  free(texture->slices);
  free(texture);
}

GLuint lovrTextureGetId(Texture* texture) {
  return texture->id;
}

int lovrTextureGetWidth(Texture* texture) {
  return texture->width;
}

int lovrTextureGetHeight(Texture* texture) {
  return texture->height;
}

int lovrTextureGetDepth(Texture* texture) {
  return texture->depth;
}

TextureType lovrTextureGetType(Texture* texture) {
  return texture->type;
}

void lovrTextureReplacePixels(Texture* texture, TextureData* textureData, int slice) {
  lovrRetain(textureData);
  lovrRelease(texture->slices[slice]);
  texture->slices[slice] = textureData;

  if (!texture->allocated) {
    lovrAssert(texture->type != TEXTURE_CUBE || textureData->width == textureData->height, "Cubemap images must be square");
    lovrTextureAllocate(texture, textureData);
  } else {
    lovrAssert(textureData->width == texture->width && textureData->height == texture->height, "All texture slices must have the same dimensions");
  }

  if (!textureData->blob.data) {
    return;
  }

  GLenum glFormat = lovrConvertTextureFormat(textureData->format);
  GLenum glInternalFormat = lovrConvertTextureFormatInternal(textureData->format, texture->srgb);
  GLenum binding = (texture->type == TEXTURE_CUBE) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + slice : texture->glType;

  if (lovrIsTextureFormatCompressed(textureData->format)) {
    Mipmap m; int i;
    vec_foreach(&textureData->mipmaps, m, i) {
      switch (texture->type) {
        case TEXTURE_2D:
        case TEXTURE_CUBE:
          glCompressedTexImage2D(binding, i, glInternalFormat, m.width, m.height, 0, m.size, m.data);
          break;
        case TEXTURE_ARRAY:
        case TEXTURE_VOLUME:
          glCompressedTexSubImage3D(binding, i, 0, 0, slice, m.width, m.height, 1, glInternalFormat, m.size, m.data);
          break;
      }
    }
  } else {
    switch (texture->type) {
      case TEXTURE_2D:
      case TEXTURE_CUBE:
        glTexSubImage2D(binding, 0, 0, 0, textureData->width, textureData->height, glFormat, GL_UNSIGNED_BYTE, textureData->blob.data);
        break;
      case TEXTURE_ARRAY:
      case TEXTURE_VOLUME:
        glTexSubImage3D(binding, 0, 0, 0, slice, textureData->width, textureData->height, 1, glFormat, GL_UNSIGNED_BYTE, textureData->blob.data);
        break;
    }

    if (texture->mipmaps) {
      glGenerateMipmap(texture->glType);
    }
  }
}

TextureFilter lovrTextureGetFilter(Texture* texture) {
  return texture->filter;
}

void lovrTextureSetFilter(Texture* texture, TextureFilter filter) {
  float anisotropy = filter.mode == FILTER_ANISOTROPIC ? MAX(filter.anisotropy, 1.) : 1.;
  gpuBindTexture(texture, 0);
  texture->filter = filter;

  switch (filter.mode) {
    case FILTER_NEAREST:
      glTexParameteri(texture->glType, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(texture->glType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      break;

    case FILTER_BILINEAR:
      if (texture->mipmaps) {
        glTexParameteri(texture->glType, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(texture->glType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      } else {
        glTexParameteri(texture->glType, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(texture->glType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      }
      break;

    case FILTER_TRILINEAR:
    case FILTER_ANISOTROPIC:
      if (texture->mipmaps) {
        glTexParameteri(texture->glType, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(texture->glType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      } else {
        glTexParameteri(texture->glType, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(texture->glType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      }
      break;
  }

  glTexParameteri(texture->glType, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
}

TextureWrap lovrTextureGetWrap(Texture* texture) {
  return texture->wrap;
}

void lovrTextureSetWrap(Texture* texture, TextureWrap wrap) {
  texture->wrap = wrap;
  gpuBindTexture(texture, 0);
  glTexParameteri(texture->glType, GL_TEXTURE_WRAP_S, lovrConvertWrapMode(wrap.s));
  glTexParameteri(texture->glType, GL_TEXTURE_WRAP_T, lovrConvertWrapMode(wrap.t));
  if (texture->type == TEXTURE_CUBE || texture->type == TEXTURE_VOLUME) {
    glTexParameteri(texture->glType, GL_TEXTURE_WRAP_R, lovrConvertWrapMode(wrap.r));
  }
}
