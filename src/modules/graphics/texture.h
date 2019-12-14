#include "data/textureData.h"
#include "graphics/graphics.h"
#include "graphics/opengl.h"
#include "data/modelData.h"
#include <stdbool.h>

#pragma once

struct TextureData;

typedef enum {
  TEXTURE_2D,
  TEXTURE_CUBE,
  TEXTURE_ARRAY,
  TEXTURE_VOLUME
} TextureType;

typedef struct Texture {
  TextureType type;
  TextureFormat format;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t mipmapCount;
  CompareMode compareMode;
  TextureFilter filter;
  TextureWrap wrap;
  uint32_t msaa;
  bool srgb;
  bool mipmaps;
  bool allocated;
  GPU_TEXTURE_FIELDS
} Texture;

Texture* lovrTextureInit(Texture* texture, TextureType type, struct TextureData** slices, uint32_t sliceCount, bool srgb, bool mipmaps, uint32_t msaa);
Texture* lovrTextureInitFromHandle(Texture* texture, uint32_t handle, TextureType type, uint32_t depth);
#define lovrTextureCreate(...) lovrTextureInit(lovrAlloc(Texture), __VA_ARGS__)
#define lovrTextureCreateFromHandle(...) lovrTextureInitFromHandle(lovrAlloc(Texture), __VA_ARGS__)
void lovrTextureDestroy(void* ref);
void lovrTextureAllocate(Texture* texture, uint32_t width, uint32_t height, uint32_t depth, TextureFormat format);
void lovrTextureReplacePixels(Texture* texture, struct TextureData* data, uint32_t x, uint32_t y, uint32_t slice, uint32_t mipmap);
uint32_t lovrTextureGetWidth(Texture* texture, uint32_t mipmap);
uint32_t lovrTextureGetHeight(Texture* texture, uint32_t mipmap);
uint32_t lovrTextureGetDepth(Texture* texture, uint32_t mipmap);
uint32_t lovrTextureGetMipmapCount(Texture* texture);
uint32_t lovrTextureGetMSAA(Texture* texture);
TextureType lovrTextureGetType(Texture* texture);
TextureFormat lovrTextureGetFormat(Texture* texture);
CompareMode lovrTextureGetCompareMode(Texture* texture);
void lovrTextureSetCompareMode(Texture* texture, CompareMode compareMode);
TextureFilter lovrTextureGetFilter(Texture* texture);
void lovrTextureSetFilter(Texture* texture, TextureFilter filter);
TextureWrap lovrTextureGetWrap(Texture* texture);
void lovrTextureSetWrap(Texture* texture, TextureWrap wrap);
