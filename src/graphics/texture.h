#include "data/textureData.h"
#include "lib/glfw.h"
#include "util.h"

#pragma once

typedef enum {
  TEXTURE_2D = GL_TEXTURE_2D,
  TEXTURE_CUBE = GL_TEXTURE_CUBE_MAP
} TextureType;

typedef enum {
  FILTER_NEAREST,
  FILTER_BILINEAR,
  FILTER_TRILINEAR,
  FILTER_ANISOTROPIC
} FilterMode;

typedef struct {
  FilterMode mode;
  float anisotropy;
} TextureFilter;

typedef enum {
  WRAP_CLAMP = GL_CLAMP_TO_EDGE,
  WRAP_REPEAT = GL_REPEAT,
  WRAP_MIRRORED_REPEAT = GL_MIRRORED_REPEAT
} WrapMode;

typedef struct {
  WrapMode s;
  WrapMode t;
  WrapMode r;
} TextureWrap;

typedef struct {
  Ref ref;
  TextureType type;
  TextureData* slices[6];
  int sliceCount;
  int width;
  int height;
  GLuint id;
  TextureFilter filter;
  TextureWrap wrap;
  bool srgb;
} Texture;

GLenum lovrTextureFormatGetGLFormat(TextureFormat format);
GLenum lovrTextureFormatGetGLInternalFormat(TextureFormat format, bool srgb);
bool lovrTextureFormatIsCompressed(TextureFormat format);

Texture* lovrTextureCreate(TextureType type, TextureData* data[6], int count, bool srgb);
void lovrTextureDestroy(const Ref* ref);
void lovrTextureReplacePixels(Texture* texture, TextureData* data, int slice);
TextureFilter lovrTextureGetFilter(Texture* texture);
void lovrTextureSetFilter(Texture* texture, TextureFilter filter);
TextureWrap lovrTextureGetWrap(Texture* texture);
void lovrTextureSetWrap(Texture* texture, TextureWrap wrap);
