#include "data/textureData.h"
#include "graphics/opengl.h"
#include "data/modelData.h"
#include <stdbool.h>

#pragma once

typedef enum {
  TEXTURE_2D,
  TEXTURE_CUBE,
  TEXTURE_ARRAY,
  TEXTURE_VOLUME
} TextureType;

typedef struct {
  Ref ref;
  TextureType type;
  TextureFormat format;
  int width;
  int height;
  int depth;
  int mipmapCount;
  TextureFilter filter;
  TextureWrap wrap;
  int msaa;
  bool srgb;
  bool mipmaps;
  bool allocated;
  GPU_TEXTURE_FIELDS
} Texture;

Texture* lovrTextureInit(Texture* texture, TextureType type, TextureData** slices, int sliceCount, bool srgb, bool mipmaps, int msaa);
Texture* lovrTextureInitFromHandle(Texture* texture, uint32_t handle, TextureType type);
#define lovrTextureCreate(...) lovrTextureInit(lovrAlloc(Texture), __VA_ARGS__)
#define lovrTextureCreateFromHandle(...) lovrTextureInitFromHandle(lovrAlloc(Texture), __VA_ARGS__)
void lovrTextureDestroy(void* ref);
void lovrTextureAllocate(Texture* texture, int width, int height, int depth, TextureFormat format);
void lovrTextureReplacePixels(Texture* texture, TextureData* data, int x, int y, int slice, int mipmap);
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
