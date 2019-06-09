#include "data/blob.h"
#include "util.h"
#include "lib/vec/vec.h"
#include <stdint.h>
#include <stdbool.h>

#pragma once

typedef enum {
  FORMAT_RGB,
  FORMAT_RGBA,
  FORMAT_RGBA4,
  FORMAT_RGBA16F,
  FORMAT_RGBA32F,
  FORMAT_R16F,
  FORMAT_R32F,
  FORMAT_RG16F,
  FORMAT_RG32F,
  FORMAT_RGB5A1,
  FORMAT_RGB10A2,
  FORMAT_RG11B10F,
  FORMAT_D16,
  FORMAT_D32F,
  FORMAT_D24S8,
  FORMAT_DXT1,
  FORMAT_DXT3,
  FORMAT_DXT5,
  FORMAT_ASTC_4x4,
  FORMAT_ASTC_5x4,
  FORMAT_ASTC_5x5,
  FORMAT_ASTC_6x5,
  FORMAT_ASTC_6x6,
  FORMAT_ASTC_8x5,
  FORMAT_ASTC_8x6,
  FORMAT_ASTC_8x8,
  FORMAT_ASTC_10x5,
  FORMAT_ASTC_10x6,
  FORMAT_ASTC_10x8,
  FORMAT_ASTC_10x10,
  FORMAT_ASTC_12x10,
  FORMAT_ASTC_12x12
} TextureFormat;

typedef struct {
  uint32_t width;
  uint32_t height;
  size_t size;
  void* data;
} Mipmap;

typedef vec_t(Mipmap) vec_mipmap_t;

typedef struct TextureData {
  Blob blob;
  uint32_t width;
  uint32_t height;
  Blob* source;
  TextureFormat format;
  vec_mipmap_t mipmaps;
} TextureData;

TextureData* lovrTextureDataInit(TextureData* textureData, uint32_t width, uint32_t height, uint8_t value, TextureFormat format);
TextureData* lovrTextureDataInitFromBlob(TextureData* textureData, Blob* blob, bool flip);
#define lovrTextureDataCreate(...) lovrTextureDataInit(lovrAlloc(TextureData), __VA_ARGS__)
#define lovrTextureDataCreateFromBlob(...) lovrTextureDataInitFromBlob(lovrAlloc(TextureData), __VA_ARGS__)
Color lovrTextureDataGetPixel(TextureData* textureData, uint32_t x, uint32_t y);
void lovrTextureDataSetPixel(TextureData* textureData, uint32_t x, uint32_t y, Color color);
bool lovrTextureDataEncode(TextureData* textureData, const char* filename);
void lovrTextureDataDestroy(void* ref);
