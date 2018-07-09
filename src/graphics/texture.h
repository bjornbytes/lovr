#include "data/textureData.h"
#include <stdbool.h>

#pragma once

typedef enum {
  TEXTURE_2D,
  TEXTURE_CUBE,
  TEXTURE_ARRAY,
  TEXTURE_VOLUME
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
  WRAP_CLAMP,
  WRAP_REPEAT,
  WRAP_MIRRORED_REPEAT
} WrapMode;

typedef struct {
  WrapMode s;
  WrapMode t;
  WrapMode r;
} TextureWrap;

typedef struct Texture Texture;

Texture* lovrTextureCreate(TextureType type, TextureData** slices, int depth, bool srgb, bool mipmaps);
void lovrTextureDestroy(void* ref);
uint32_t lovrTextureGetId(Texture* texture); // FIXME temporary
int lovrTextureGetWidth(Texture* texture);
int lovrTextureGetHeight(Texture* texture);
int lovrTextureGetDepth(Texture* texture);
TextureType lovrTextureGetType(Texture* texture);
void lovrTextureReplacePixels(Texture* texture, TextureData* data, int slice);
TextureFilter lovrTextureGetFilter(Texture* texture);
void lovrTextureSetFilter(Texture* texture, TextureFilter filter);
TextureWrap lovrTextureGetWrap(Texture* texture);
void lovrTextureSetWrap(Texture* texture, TextureWrap wrap);
