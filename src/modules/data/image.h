#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Blob;

typedef enum {
  FORMAT_R8,
  FORMAT_RG8,
  FORMAT_RGBA8,
  FORMAT_BGRA8,
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
  FORMAT_D24,
  FORMAT_D32F,
  FORMAT_D24S8,
  FORMAT_D32FS8,
  FORMAT_BC1,
  FORMAT_BC2,
  FORMAT_BC3,
  FORMAT_BC4U,
  FORMAT_BC4S,
  FORMAT_BC5U,
  FORMAT_BC5S,
  FORMAT_BC6UF,
  FORMAT_BC6SF,
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

typedef void MapPixelCallback(void* userdata, uint32_t x, uint32_t y, float pixel[4]);

typedef struct Image Image;

Image* lovrImageCreateRaw(uint32_t width, uint32_t height, TextureFormat format, bool srgb);
Image* lovrImageCreateFromFile(struct Blob* blob);
void lovrImageDestroy(void* ref);
bool lovrImageIsSRGB(Image* image);
bool lovrImageIsPremultiplied(Image* image);
bool lovrImageIsCube(Image* image);
bool lovrImageIsDepth(Image* image);
bool lovrImageIsCompressed(Image* image);
struct Blob* lovrImageGetBlob(Image* image);
uint32_t lovrImageGetWidth(Image* image, uint32_t level);
uint32_t lovrImageGetHeight(Image* image, uint32_t level);
uint32_t lovrImageGetLayerCount(Image* image);
uint32_t lovrImageGetLevelCount(Image* image);
TextureFormat lovrImageGetFormat(Image* image);
size_t lovrImageGetLayerSize(Image* image, uint32_t level);
void* lovrImageGetLayerData(Image* image, uint32_t level, uint32_t layer);
bool lovrImageGetPixel(Image* image, uint32_t x, uint32_t y, float pixel[4]);
bool lovrImageSetPixel(Image* image, uint32_t x, uint32_t y, float pixel[4]);
bool lovrImageMapPixel(Image* image, uint32_t x, uint32_t y, uint32_t w, uint32_t h, MapPixelCallback* callback, void* userdata);
bool lovrImageCopy(Image* src, Image* dst, uint32_t srcOffset[2], uint32_t dstOffset[2], uint32_t extent[2]);
struct Blob* lovrImageEncode(Image* image);
