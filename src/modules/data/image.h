#include "core/util.h"
#include <stdint.h>
#include <stdbool.h>

#pragma once

struct Blob;

typedef enum {
  FORMAT_R8,
  FORMAT_RG8,
  FORMAT_RGBA8,
  FORMAT_R16,
  FORMAT_RG16,
  FORMAT_RGBA16,
  FORMAT_R16F,
  FORMAT_RG16F,
  FORMAT_RGBA16F,
  FORMAT_R32F,
  FORMAT_RG32F,
  FORMAT_RGBA32F,
  FORMAT_RGB565,
  FORMAT_RGB5A1,
  FORMAT_RGB10A2,
  FORMAT_RG11B10F,
  FORMAT_D16,
  FORMAT_D24S8,
  FORMAT_D32F,
  FORMAT_BC6,
  FORMAT_BC7,
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

typedef struct Image {
  uint32_t ref;
  struct Blob* blob;
  uint32_t width;
  uint32_t height;
  struct Blob* source;
  TextureFormat format;
  Mipmap* mipmaps;
  uint32_t mipmapCount;
} Image;

Image* lovrImageCreate(uint32_t width, uint32_t height, struct Blob* contents, uint8_t value, TextureFormat format);
Image* lovrImageCreateFromBlob(struct Blob* blob, bool flip);
void lovrImageDestroy(void* ref);
void lovrImageGetPixel(Image* image, uint32_t x, uint32_t y, float color[4]);
void lovrImageSetPixel(Image* image, uint32_t x, uint32_t y, float color[4]);
struct Blob* lovrImageEncode(Image* image);
void lovrImagePaste(Image* image, Image* source, uint32_t dx, uint32_t dy, uint32_t sx, uint32_t sy, uint32_t w, uint32_t h);
