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

Texture* lovrTextureCreate(TextureType type, TextureData** slices, int sliceCount, bool srgb, bool mipmaps, int msaa);
void lovrTextureDestroy(void* ref);
void lovrTextureAllocate(Texture* texture, int width, int height, int depth, TextureFormat format);
void lovrTextureReplacePixels(Texture* texture, TextureData* data, int x, int y, int slice, int mipmap);
uint32_t lovrTextureGetId(Texture* texture);
int lovrTextureGetWidth(Texture* texture, int mipmap);
int lovrTextureGetHeight(Texture* texture, int mipmap);
int lovrTextureGetDepth(Texture* texture, int mipmap);
int lovrTextureGetMipmapCount(Texture* texture);
int lovrTextureGetMSAA(Texture* texture);
TextureType lovrTextureGetType(Texture* texture);
TextureFormat lovrTextureGetFormat(Texture* texture);
TextureFilter lovrTextureGetFilter(Texture* texture);
void lovrTextureSetFilter(Texture* texture, TextureFilter filter);
TextureWrap lovrTextureGetWrap(Texture* texture);
void lovrTextureSetWrap(Texture* texture, TextureWrap wrap);
