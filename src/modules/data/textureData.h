#include "data/blob.h"
#include "core/util.h"
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

typedef struct TextureData {
  Blob* blob;
  uint32_t width;
  uint32_t height;
  Blob* source;
  TextureFormat format;
  Mipmap* mipmaps;
  uint32_t mipmapCount;
} TextureData;

TextureData* lovrTextureDataInit(TextureData* textureData, uint32_t width, uint32_t height, Blob* contents, uint8_t value, TextureFormat format);
TextureData* lovrTextureDataInitFromBlob(TextureData* textureData, Blob* blob, bool flip);
#define lovrTextureDataCreate(...) lovrTextureDataInit(lovrAlloc(TextureData), __VA_ARGS__)
#define lovrTextureDataCreateFromBlob(...) lovrTextureDataInitFromBlob(lovrAlloc(TextureData), __VA_ARGS__)
Color lovrTextureDataGetPixel(TextureData* textureData, uint32_t x, uint32_t y);
void lovrTextureDataSetPixel(TextureData* textureData, uint32_t x, uint32_t y, Color color);
bool lovrTextureDataEncode(TextureData* textureData, const char* filename);
void lovrTextureDataPaste(TextureData* textureData, TextureData* source, uint32_t dx, uint32_t dy, uint32_t sx, uint32_t sy, uint32_t w, uint32_t h);
void lovrTextureDataDestroy(void* ref);
