#include "data/blob.h"
#include "util.h"
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
  FORMAT_DXT1,
  FORMAT_DXT3,
  FORMAT_DXT5
} TextureFormat;

typedef struct {
  int width;
  int height;
  void* data;
  size_t size;
} Mipmap;

typedef vec_t(Mipmap) vec_mipmap_t;

typedef struct {
  Blob blob;
  int width;
  int height;
  Blob* source;
  TextureFormat format;
  vec_mipmap_t mipmaps;
} TextureData;

TextureData* lovrTextureDataCreate(int width, int height, uint8_t value, TextureFormat format);
TextureData* lovrTextureDataCreateFromBlob(Blob* blob, bool flip);
Color lovrTextureDataGetPixel(TextureData* textureData, int x, int y);
void lovrTextureDataSetPixel(TextureData* textureData, int x, int y, Color color);
bool lovrTextureDataEncode(TextureData* textureData, const char* filename);
void lovrTextureDataDestroy(void* ref);
