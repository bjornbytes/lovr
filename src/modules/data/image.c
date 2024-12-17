#include "data/image.h"
#include "data/blob.h"
#include "util.h"
#include "lib/stb/stb_image.h"
#include <stdlib.h>
#include <string.h>

enum {
  IMAGE_SRGB = (1 << 0),
  IMAGE_PREMULTIPLIED = (1 << 1),
  IMAGE_CUBEMAP = (1 << 2)
};

typedef struct {
  void* data;
  size_t size;
  size_t stride;
} Mipmap;

struct Image {
  uint32_t ref;
  uint32_t flags;
  uint32_t width;
  uint32_t height;
  uint32_t format;
  uint32_t layers;
  uint32_t levels;
  struct Blob* blob;
  Mipmap mipmaps[1];
};

static size_t measure(uint32_t w, uint32_t h, TextureFormat format) {
  switch (format) {
    case FORMAT_R8: return w * h * 1;
    case FORMAT_RG8: return w * h * 2;
    case FORMAT_RGBA8: return w * h * 4;
    case FORMAT_R16: return w * h * 2;
    case FORMAT_RG16: return w * h * 4;
    case FORMAT_RGBA16: return w * h * 8;
    case FORMAT_R16F: return w * h * 2;
    case FORMAT_RG16F: return w * h * 4;
    case FORMAT_RGBA16F: return w * h * 8;
    case FORMAT_R32F: return w * h * 4;
    case FORMAT_RG32F: return w * h * 8;
    case FORMAT_RGBA32F: return w * h * 16;
    case FORMAT_RGB565: return w * h * 2;
    case FORMAT_RGB5A1: return w * h * 2;
    case FORMAT_RGB10A2: return w * h * 4;
    case FORMAT_RG11B10F: return w * h * 4;
    case FORMAT_D16: return w * h * 2;
    case FORMAT_D24: return w * h * 4;
    case FORMAT_D32F: return w * h * 4;
    case FORMAT_D24S8: return w * h * 4;
    case FORMAT_D32FS8: return w * h * 5;
    case FORMAT_BC1: return ((w + 3) / 4) * ((h + 3) / 4) * 8;
    case FORMAT_BC2: return ((w + 3) / 4) * ((h + 3) / 4) * 16;
    case FORMAT_BC3: return ((w + 3) / 4) * ((h + 3) / 4) * 16;
    case FORMAT_BC4U: return ((w + 3) / 4) * ((h + 3) / 4) * 8;
    case FORMAT_BC4S: return ((w + 3) / 4) * ((h + 3) / 4) * 8;
    case FORMAT_BC5U: return ((w + 3) / 4) * ((h + 3) / 4) * 16;
    case FORMAT_BC5S: return ((w + 3) / 4) * ((h + 3) / 4) * 16;
    case FORMAT_BC6UF: return ((w + 3) / 4) * ((h + 3) / 4) * 16;
    case FORMAT_BC6SF: return ((w + 3) / 4) * ((h + 3) / 4) * 16;
    case FORMAT_BC7: return ((w + 3) / 4) * ((h + 3) / 4) * 16;
    case FORMAT_ASTC_4x4: return ((w + 3) / 4) * ((h + 3) / 4) * 16;
    case FORMAT_ASTC_5x4: return ((w + 4) / 5) * ((h + 3) / 4) * 16;
    case FORMAT_ASTC_5x5: return ((w + 4) / 5) * ((h + 4) / 5) * 16;
    case FORMAT_ASTC_6x5: return ((w + 5) / 6) * ((h + 4) / 5) * 16;
    case FORMAT_ASTC_6x6: return ((w + 5) / 6) * ((h + 5) / 6) * 16;
    case FORMAT_ASTC_8x5: return ((w + 7) / 8) * ((h + 4) / 5) * 16;
    case FORMAT_ASTC_8x6: return ((w + 7) / 8) * ((h + 5) / 6) * 16;
    case FORMAT_ASTC_8x8: return ((w + 7) / 8) * ((h + 7) / 8) * 16;
    case FORMAT_ASTC_10x5: return ((w + 9) / 10) * ((h + 4) / 5) * 16;
    case FORMAT_ASTC_10x6: return ((w + 9) / 10) * ((h + 5) / 6) * 16;
    case FORMAT_ASTC_10x8: return ((w + 9) / 10) * ((h + 7) / 8) * 16;
    case FORMAT_ASTC_10x10: return ((w + 9) / 10) * ((h + 9) / 10) * 16;
    case FORMAT_ASTC_12x10: return ((w + 11) / 12) * ((h + 9) / 10) * 16;
    case FORMAT_ASTC_12x12: return ((w + 11) / 12) * ((h + 11) / 12) * 16;
    default: lovrUnreachable();
  }
}

Image* lovrImageCreateRaw(uint32_t width, uint32_t height, TextureFormat format, bool srgb) {
  lovrCheck(width > 0 && height > 0, "Image dimensions must be positive");
  lovrCheck(format < FORMAT_BC1, "Blank images cannot be compressed");
  size_t size = measure(width, height, format);
  void* data = lovrMalloc(size);
  Image* image = lovrCalloc(sizeof(Image));
  image->ref = 1;
  image->flags = srgb ? IMAGE_SRGB : 0;
  image->width = width;
  image->height = height;
  image->format = format;
  image->layers = 1;
  image->levels = 1;
  image->blob = lovrBlobCreate(data, size, "Image");
  image->mipmaps[0] = (Mipmap) { data, size, 0 };
  return image;
}

static bool loadDDS(Blob* blob, Image** image);
static bool loadASTC(Blob* blob, Image** image);
static bool loadKTX1(Blob* blob, Image** image);
static bool loadKTX2(Blob* blob, Image** image);
static bool loadSTB(Blob* blob, Image** image);

Image* lovrImageCreateFromFile(Blob* blob) {
  Image* image = NULL;
  if (!image && !loadDDS(blob, &image)) return NULL;
  if (!image && !loadASTC(blob, &image)) return NULL;
  if (!image && !loadKTX1(blob, &image)) return NULL;
  if (!image && !loadKTX2(blob, &image)) return NULL;
  if (!image && !loadSTB(blob, &image)) return NULL;
  if (!image) lovrSetError("Could not load image from '%s': Image file format not recognized", blob->name);
  return image;
}

void lovrImageDestroy(void* ref) {
  Image* image = ref;
  lovrRelease(image->blob, lovrBlobDestroy);
  lovrFree(image);
}

bool lovrImageIsSRGB(Image* image) {
  return image->flags & IMAGE_SRGB;
}

bool lovrImageIsPremultiplied(Image* image) {
  return image->flags & IMAGE_PREMULTIPLIED;
}

bool lovrImageIsCube(Image* image) {
  return image->flags & IMAGE_CUBEMAP;
}

bool lovrImageIsDepth(Image* image) {
  switch (image->format) {
    case FORMAT_D16:
    case FORMAT_D24:
    case FORMAT_D32F:
    case FORMAT_D24S8:
    case FORMAT_D32FS8:
      return true;
    default:
      return false;
  }
}

bool lovrImageIsCompressed(Image* image) {
  return image->format >= FORMAT_BC1;
}

Blob* lovrImageGetBlob(Image* image) {
  return image->blob;
}

uint32_t lovrImageGetWidth(Image* image, uint32_t level) {
  return MAX(image->width >> level, 1);
}

uint32_t lovrImageGetHeight(Image* image, uint32_t level) {
  return MAX(image->height >> level, 1);
}

uint32_t lovrImageGetLayerCount(Image* image) {
  return image->layers;
}

uint32_t lovrImageGetLevelCount(Image* image) {
  return image->levels;
}

TextureFormat lovrImageGetFormat(Image* image) {
  return image->format;
}

size_t lovrImageGetLayerSize(Image* image, uint32_t level) {
  if (level >= image->levels) return 0;
  return image->mipmaps[level].size;
}

void* lovrImageGetLayerData(Image* image, uint32_t level, uint32_t layer) {
  if (layer >= image->layers || level >= image->levels) return NULL;
  return (uint8_t*) image->mipmaps[level].data + layer * image->mipmaps[level].stride;
}

typedef union { void* raw; uint8_t* u8; uint16_t* u16; float* f32; } ImagePointer;

static void getPixelR8(ImagePointer src, float* dst) { for (uint32_t i = 0; i < 1; i++) dst[i] = src.u8[i] / 255.f; }
static void getPixelRG8(ImagePointer src, float* dst) { for (uint32_t i = 0; i < 2; i++) dst[i] = src.u8[i] / 255.f; }
static void getPixelRGBA8(ImagePointer src, float* dst) { for (uint32_t i = 0; i < 4; i++) dst[i] = src.u8[i] / 255.f; }
static void getPixelR16(ImagePointer src, float* dst) { for (uint32_t i = 0; i < 1; i++) dst[i] = src.u16[i] / 65535.f; }
static void getPixelRG16(ImagePointer src, float* dst) { for (uint32_t i = 0; i < 2; i++) dst[i] = src.u16[i] / 65535.f; }
static void getPixelRGBA16(ImagePointer src, float* dst) { for (uint32_t i = 0; i < 4; i++) dst[i] = src.u16[i] / 65535.f; }
static void getPixelR16F(ImagePointer src, float* dst) { for (uint32_t i = 0; i < 1; i++) dst[i] = float16to32(src.u16[i]); }
static void getPixelRG16F(ImagePointer src, float* dst) { for (uint32_t i = 0; i < 2; i++) dst[i] = float16to32(src.u16[i]); }
static void getPixelRGBA16F(ImagePointer src, float* dst) { for (uint32_t i = 0; i < 4; i++) dst[i] = float16to32(src.u16[i]); }
static void getPixelR32F(ImagePointer src, float* dst) { for (uint32_t i = 0; i < 1; i++) dst[i] = src.f32[i]; }
static void getPixelRG32F(ImagePointer src, float* dst) { for (uint32_t i = 0; i < 2; i++) dst[i] = src.f32[i]; }
static void getPixelRGBA32F(ImagePointer src, float* dst) { for (uint32_t i = 0; i < 4; i++) dst[i] = src.f32[i]; }

static void setPixelR8(float* src, ImagePointer dst) { for (uint32_t i = 0; i < 1; i++) dst.u8[i] = (uint8_t) (src[i] * 255.f + .5f); }
static void setPixelRG8(float* src, ImagePointer dst) { for (uint32_t i = 0; i < 2; i++) dst.u8[i] = (uint8_t) (src[i] * 255.f + .5f); }
static void setPixelRGBA8(float* src, ImagePointer dst) { for (uint32_t i = 0; i < 4; i++) dst.u8[i] = (uint8_t) (src[i] * 255.f + .5f); }
static void setPixelR16(float* src, ImagePointer dst) { for (uint32_t i = 0; i < 1; i++) dst.u16[i] = (uint16_t) (src[i] * 65535.f + .5f); }
static void setPixelRG16(float* src, ImagePointer dst) { for (uint32_t i = 0; i < 2; i++) dst.u16[i] = (uint16_t) (src[i] * 65535.f + .5f); }
static void setPixelRGBA16(float* src, ImagePointer dst) { for (uint32_t i = 0; i < 4; i++) dst.u16[i] = (uint16_t) (src[i] * 65535.f + .5f); }
static void setPixelR16F(float* src, ImagePointer dst) { for (uint32_t i = 0; i < 1; i++) dst.u16[i] = float32to16(src[i]); }
static void setPixelRG16F(float* src, ImagePointer dst) { for (uint32_t i = 0; i < 2; i++) dst.u16[i] = float32to16(src[i]); }
static void setPixelRGBA16F(float* src, ImagePointer dst) { for (uint32_t i = 0; i < 4; i++) dst.u16[i] = float32to16(src[i]); }
static void setPixelR32F(float* src, ImagePointer dst) { for (uint32_t i = 0; i < 1; i++) dst.f32[i] = src[i]; }
static void setPixelRG32F(float* src, ImagePointer dst) { for (uint32_t i = 0; i < 2; i++) dst.f32[i] = src[i]; }
static void setPixelRGBA32F(float* src, ImagePointer dst) { for (uint32_t i = 0; i < 4; i++) dst.f32[i] = src[i]; }

bool lovrImageGetPixel(Image* image, uint32_t x, uint32_t y, float pixel[4]) {
  lovrCheck(!lovrImageIsCompressed(image), "Unable to access individual pixels of a compressed image");
  lovrCheck(x < image->width && y < image->height, "Pixel coordinates must be within Image bounds");
  size_t offset = measure(y * image->width + x, 1, image->format);
  ImagePointer p = { .u8 = (uint8_t*) image->mipmaps[0].data + offset };
  switch (image->format) {
    case FORMAT_R8: getPixelR8(p, pixel); return true;
    case FORMAT_RG8: getPixelRG8(p, pixel); return true;
    case FORMAT_RGBA8: getPixelRGBA8(p, pixel); return true;
    case FORMAT_R16: getPixelR16(p, pixel); return true;
    case FORMAT_RG16: getPixelRG16(p, pixel); return true;
    case FORMAT_RGBA16: getPixelRGBA16(p, pixel); return true;
    case FORMAT_R16F: getPixelR16F(p, pixel); return true;
    case FORMAT_RG16F: getPixelRG16F(p, pixel); return true;
    case FORMAT_RGBA16F: getPixelRGBA16F(p, pixel); return true;
    case FORMAT_R32F: getPixelR32F(p, pixel); return true;
    case FORMAT_RG32F: getPixelRG32F(p, pixel); return true;
    case FORMAT_RGBA32F: getPixelRGBA32F(p, pixel); return true;
    default: return lovrSetError("Unsupported format for Image:getPixel");
  }
}

bool lovrImageSetPixel(Image* image, uint32_t x, uint32_t y, float pixel[4]) {
  lovrCheck(!lovrImageIsCompressed(image), "Unable to access individual pixels of a compressed image");
  lovrCheck(x < image->width && y < image->height, "Pixel coordinates must be within Image bounds");
  size_t offset = measure(y * image->width + x, 1, image->format);
  ImagePointer p = { .u8 = (uint8_t*) image->mipmaps[0].data + offset };
  switch (image->format) {
    case FORMAT_R8: setPixelR8(pixel, p); return true;
    case FORMAT_RG8: setPixelRG8(pixel, p); return true;
    case FORMAT_RGBA8: setPixelRGBA8(pixel, p); return true;
    case FORMAT_R16: setPixelR16(pixel, p); return true;
    case FORMAT_RG16: setPixelRG16(pixel, p); return true;
    case FORMAT_RGBA16: setPixelRGBA16(pixel, p); return true;
    case FORMAT_R16F: setPixelR16F(pixel, p); return true;
    case FORMAT_RG16F: setPixelRG16F(pixel, p); return true;
    case FORMAT_RGBA16F: setPixelRGBA16F(pixel, p); return true;
    case FORMAT_R32F: setPixelR32F(pixel, p); return true;
    case FORMAT_RG32F: setPixelRG32F(pixel, p); return true;
    case FORMAT_RGBA32F: setPixelRGBA32F(pixel, p); return true;
    default: return lovrSetError("Unsupported format for Image:setPixel");
  }
}

bool lovrImageMapPixel(Image* image, uint32_t x0, uint32_t y0, uint32_t w, uint32_t h, MapPixelCallback* callback, void* userdata) {
  lovrCheck(!lovrImageIsCompressed(image), "Unable to access individual pixels of a compressed image");
  lovrCheck(x0 + w <= image->width, "Pixel rectangle must be within Image bounds");
  lovrCheck(y0 + h <= image->height, "Pixel rectangle must be within Image bounds");
  void (*getPixel)(ImagePointer src, float* dst);
  void (*setPixel)(float* src, ImagePointer dst);
  switch (image->format) {
    case FORMAT_R8: getPixel = getPixelR8, setPixel = setPixelR8; break;
    case FORMAT_RG8: getPixel = getPixelRG8, setPixel = setPixelRG8; break;
    case FORMAT_RGBA8: getPixel = getPixelRGBA8, setPixel = setPixelRGBA8; break;
    case FORMAT_R16: getPixel = getPixelR16, setPixel = setPixelR16; break;
    case FORMAT_RG16: getPixel = getPixelRG16, setPixel = setPixelRG16; break;
    case FORMAT_RGBA16: getPixel = getPixelRGBA16, setPixel = setPixelRGBA16; break;
    case FORMAT_R16F: getPixel = getPixelR16F, setPixel = setPixelR16F; break;
    case FORMAT_RG16F: getPixel = getPixelRG16F, setPixel = setPixelRG16F; break;
    case FORMAT_RGBA16F: getPixel = getPixelRGBA16F, setPixel = setPixelRGBA16F; break;
    case FORMAT_R32F: getPixel = getPixelR32F, setPixel = setPixelR32F; break;
    case FORMAT_RG32F: getPixel = getPixelRG32F, setPixel = setPixelRG32F; break;
    case FORMAT_RGBA32F: getPixel = getPixelRGBA32F, setPixel = setPixelRGBA32F; break;
    default: return lovrSetError("Unsupported format for Image:mapPixel");
  }
  float pixel[4] = { 0.f, 0.f, 0.f, 1.f };
  uint32_t width = image->width;
  char* data = image->mipmaps[0].data;
  size_t stride = measure(1, 1, image->format);
  for (uint32_t y = y0; y < y0 + h; y++) {
    ImagePointer p = { .u8 = (uint8_t*) data + (y * width + x0) * stride };
    for (uint32_t x = x0; x < x0 + w; x++) {
      getPixel(p, pixel);
      callback(userdata, x, y, pixel);
      setPixel(pixel, p);
      p.u8 += stride;
    }
  }
  return true;
}

bool lovrImageCopy(Image* src, Image* dst, uint32_t srcOffset[2], uint32_t dstOffset[2], uint32_t extent[2]) {
  lovrCheck(src->format == dst->format, "To copy between Images, their formats must match");
  lovrCheck(!lovrImageIsCompressed(src), "Compressed Images cannot be copied");
  lovrCheck(dstOffset[0] + extent[0] <= dst->width, "Image copy region extends past the destination image width");
  lovrCheck(dstOffset[1] + extent[1] <= dst->height, "Image copy region extends past the destination image height");
  lovrCheck(srcOffset[0] + extent[0] <= src->width, "Image copy region extends past the source image width");
  lovrCheck(srcOffset[1] + extent[1] <= src->height, "Image copy region extends past the source image height");
  size_t pixelSize = measure(1, 1, src->format);
  uint8_t* p = (uint8_t*) lovrImageGetLayerData(src, 0, 0) + (srcOffset[1] * src->width + srcOffset[0]) * pixelSize;
  uint8_t* q = (uint8_t*) lovrImageGetLayerData(dst, 0, 0) + (dstOffset[1] * dst->width + dstOffset[0]) * pixelSize;
  for (uint32_t y = 0; y < extent[1]; y++) {
    memcpy(q, p, extent[0] * pixelSize);
    p += src->width * pixelSize;
    q += dst->width * pixelSize;
  }
  return true;
}

static uint32_t crc_lookup[256];
static bool crc_ready = false;
static void crc_init(void) {
  if (!crc_ready) {
    crc_ready = true;
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t x = i;
      for (uint32_t b = 0; b < 8; b++) {
        if (x & 1) {
          x = 0xedb88320L ^ (x >> 1);
        } else {
          x >>= 1;
        }
        crc_lookup[i] = x;
      }
    }
  }
}

static uint32_t crc32(uint8_t* data, size_t length) {
  uint32_t c = 0xffffffff;
  for (size_t i = 0; i < length; i++) c = crc_lookup[(c ^ data[i]) & 0xff] ^ (c >> 8);
  return c ^ 0xffffffff;
}

Blob* lovrImageEncode(Image* image) {
  lovrCheck(image->format == FORMAT_RGBA8, "Currently, only images with the rgba8 format can be encoded");
  uint32_t w = image->width;
  uint32_t h = image->height;
  uint8_t* pixels = (uint8_t*) image->blob->data;
  uint32_t stride = (int) (w * 4);

  // The world's worst png encoder
  // Encoding uses one unfiltered IDAT chunk, each row is an uncompressed huffman block
  // The total IDAT chunk data size is
  // - 2 bytes for the zlib header
  // - 6 bytes per block (5 bytes for the huffman block header + 1 byte scanline filter prefix)
  // - n bytes for the image data itself (width * height * 4)
  // - 4 bytes for the adler32 checksum

  uint8_t signature[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
  uint8_t header[13] = {
    w >> 24, w >> 16, w >> 8, w >> 0,
    h >> 24, h >> 16, h >> 8, h >> 0,
    8, 6, 0, 0, 0
  };

  size_t rowSize = w * 4;
  size_t imageSize = rowSize * h;
  size_t blockSize = rowSize + 1;
  size_t idatSize = 2 + (h * (5 + 1)) + imageSize + 4;

  size_t size = sizeof(signature);
  size += 4 + strlen("IHDR") + sizeof(header) + 4;
  size += 4 + strlen("IDAT") + idatSize + 4;
  size += 4 + strlen("IEND") + 4;
  uint8_t* data = lovrMalloc(size);

  crc_init();
  uint32_t crc;

  // Signature
  memcpy(data, signature, sizeof(signature));
  data += sizeof(signature);

  // IHDR
  memcpy(data, (uint8_t[4]) { 0, 0, 0, sizeof(header) }, 4);
  memcpy(data + 4, "IHDR", 4);
  memcpy(data + 8, header, sizeof(header));
  crc = crc32(data + 4, 4 + sizeof(header));
  memcpy(data + 8 + sizeof(header), (uint8_t[4]) { crc >> 24, crc >> 16, crc >> 8, crc >> 0 }, 4);
  data += 8 + sizeof(header) + 4;

  // IDAT
  memcpy(data, (uint8_t[4]) { idatSize >> 24 & 0xff, idatSize >> 16 & 0xff, idatSize >> 8 & 0xff, idatSize >> 0 & 0xff }, 4);
  memcpy(data + 4, "IDAT", 4);

  {
    uint8_t* p = data + 8;
    size_t length = imageSize;

    // adler32 counters
    uint64_t s1 = 1, s2 = 0;

    // zlib header
    *p++ = (7 << 4) + (8 << 0);
    *p++ = 1;

    while (length >= rowSize) {

      // 1 indicates the final block
      *p++ = (length == rowSize);

      // Write length and negated length
      memcpy(p + 0, &(uint16_t) {  blockSize & 0xffff }, 2);
      memcpy(p + 2, &(uint16_t) { ~blockSize & 0xffff }, 2);
      p += 4;

      // Write the filter method (0) and the row data
      *p++ = 0x00;
      memcpy(p, pixels, rowSize);

      // Update adler32
      s1 += 0;
      s2 += s1;
      for (size_t i = 0; i < rowSize; i++) {
        s1 = (s1 + pixels[i]);
        s2 = (s2 + s1);
      }
      s1 %= 65521;
      s2 %= 65521;

      // Update cursors
      p += rowSize;
      pixels += stride;
      length -= rowSize;
    }

    // Write adler32 checksum
    memcpy(p, (uint8_t[4]) { s2 >> 8, s2 >> 0, s1 >> 8, s1 >> 0 }, 4);
  }

  crc = crc32(data + 4, idatSize + 4);
  memcpy(data + 8 + idatSize, (uint8_t[4]) { crc >> 24, crc >> 16, crc >> 8, crc }, 4);
  data += 8 + idatSize + 4;

  // IEND
  memcpy(data, (uint8_t[4]) { 0 }, 4);
  memcpy(data + 4, "IEND", 4);
  crc = crc32(data + 4, 4);
  memcpy(data + 8, (uint8_t[4]) { crc >> 24, crc >> 16, crc >> 8, crc >> 0 }, 4);
  data += 8 + 4;

  return lovrBlobCreate(data - size, size, "Encoded Image");
}

static bool loadDDS(Blob* blob, Image** result) {
  enum { DDPF_FOURCC = 0x4, DDPF_RGB = 0x40 };
  enum { DDSD_DEPTH = 0x800000 };
  enum { DDS_RESOURCE_MISC_TEXTURECUBE = 0x4 };
  enum { DDS_ALPHA_MODE_PREMULTIPLIED = 0x2 };

  enum {
    D3D10_RESOURCE_DIMENSION_UNKNOWN,
    D3D10_RESOURCE_DIMENSION_BUFFER,
    D3D10_RESOURCE_DIMENSION_TEXTURE1D,
    D3D10_RESOURCE_DIMENSION_TEXTURE2D,
    D3D10_RESOURCE_DIMENSION_TEXTURE3D
  };

  enum {
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_R32G32B32A32_TYPELESS,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_UINT,
    DXGI_FORMAT_R32G32B32A32_SINT,
    DXGI_FORMAT_R32G32B32_TYPELESS,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32_UINT,
    DXGI_FORMAT_R32G32B32_SINT,
    DXGI_FORMAT_R16G16B16A16_TYPELESS,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R16G16B16A16_UINT,
    DXGI_FORMAT_R16G16B16A16_SNORM,
    DXGI_FORMAT_R16G16B16A16_SINT,
    DXGI_FORMAT_R32G32_TYPELESS,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32_UINT,
    DXGI_FORMAT_R32G32_SINT,
    DXGI_FORMAT_R32G8X24_TYPELESS,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,
    DXGI_FORMAT_R10G10B10A2_TYPELESS,
    DXGI_FORMAT_R10G10B10A2_UNORM,
    DXGI_FORMAT_R10G10B10A2_UINT,
    DXGI_FORMAT_R11G11B10_FLOAT,
    DXGI_FORMAT_R8G8B8A8_TYPELESS,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_R8G8B8A8_UINT,
    DXGI_FORMAT_R8G8B8A8_SNORM,
    DXGI_FORMAT_R8G8B8A8_SINT,
    DXGI_FORMAT_R16G16_TYPELESS,
    DXGI_FORMAT_R16G16_FLOAT,
    DXGI_FORMAT_R16G16_UNORM,
    DXGI_FORMAT_R16G16_UINT,
    DXGI_FORMAT_R16G16_SNORM,
    DXGI_FORMAT_R16G16_SINT,
    DXGI_FORMAT_R32_TYPELESS,
    DXGI_FORMAT_D32_FLOAT,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32_UINT,
    DXGI_FORMAT_R32_SINT,
    DXGI_FORMAT_R24G8_TYPELESS,
    DXGI_FORMAT_D24_UNORM_S8_UINT,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT,
    DXGI_FORMAT_R8G8_TYPELESS,
    DXGI_FORMAT_R8G8_UNORM,
    DXGI_FORMAT_R8G8_UINT,
    DXGI_FORMAT_R8G8_SNORM,
    DXGI_FORMAT_R8G8_SINT,
    DXGI_FORMAT_R16_TYPELESS,
    DXGI_FORMAT_R16_FLOAT,
    DXGI_FORMAT_D16_UNORM,
    DXGI_FORMAT_R16_UNORM,
    DXGI_FORMAT_R16_UINT,
    DXGI_FORMAT_R16_SNORM,
    DXGI_FORMAT_R16_SINT,
    DXGI_FORMAT_R8_TYPELESS,
    DXGI_FORMAT_R8_UNORM,
    DXGI_FORMAT_R8_UINT,
    DXGI_FORMAT_R8_SNORM,
    DXGI_FORMAT_R8_SINT,
    DXGI_FORMAT_A8_UNORM,
    DXGI_FORMAT_R1_UNORM,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
    DXGI_FORMAT_R8G8_B8G8_UNORM,
    DXGI_FORMAT_G8R8_G8B8_UNORM,
    DXGI_FORMAT_BC1_TYPELESS,
    DXGI_FORMAT_BC1_UNORM,
    DXGI_FORMAT_BC1_UNORM_SRGB,
    DXGI_FORMAT_BC2_TYPELESS,
    DXGI_FORMAT_BC2_UNORM,
    DXGI_FORMAT_BC2_UNORM_SRGB,
    DXGI_FORMAT_BC3_TYPELESS,
    DXGI_FORMAT_BC3_UNORM,
    DXGI_FORMAT_BC3_UNORM_SRGB,
    DXGI_FORMAT_BC4_TYPELESS,
    DXGI_FORMAT_BC4_UNORM,
    DXGI_FORMAT_BC4_SNORM,
    DXGI_FORMAT_BC5_TYPELESS,
    DXGI_FORMAT_BC5_UNORM,
    DXGI_FORMAT_BC5_SNORM,
    DXGI_FORMAT_B5G6R5_UNORM,
    DXGI_FORMAT_B5G5R5A1_UNORM,
    DXGI_FORMAT_B8G8R8A8_UNORM,
    DXGI_FORMAT_B8G8R8X8_UNORM,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
    DXGI_FORMAT_B8G8R8A8_TYPELESS,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    DXGI_FORMAT_B8G8R8X8_TYPELESS,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
    DXGI_FORMAT_BC6H_TYPELESS,
    DXGI_FORMAT_BC6H_UF16,
    DXGI_FORMAT_BC6H_SF16,
    DXGI_FORMAT_BC7_TYPELESS,
    DXGI_FORMAT_BC7_UNORM,
    DXGI_FORMAT_BC7_UNORM_SRGB
  };

  typedef struct DDSHeader {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipmapCount;
    uint32_t reserved[11];
    struct {
      uint32_t size;
      uint32_t flags;
      uint32_t fourCC;
      uint32_t rgbBitCount;
      uint32_t rMask;
      uint32_t gMask;
      uint32_t bMask;
      uint32_t aMask;
    } format;
    uint32_t caps[4];
    uint32_t reserved2;
  } DDSHeader;

  typedef struct DDSHeader10 {
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
  } DDSHeader10;

  if (blob->size < 4 + sizeof(DDSHeader)) return true;

  // Magic
  char* data = blob->data;
  size_t length = blob->size;
  uint32_t magic;
  memcpy(&magic, data, 4);
  if (magic != 0x20534444) return true;
  length -= 4;
  data += 4;

  // Header
  DDSHeader* header = (DDSHeader*) data;
  if (length < sizeof(DDSHeader)) return true;
  if (header->size != sizeof(DDSHeader) || header->format.size != sizeof(header->format)) return true;
  length -= sizeof(DDSHeader);
  data += sizeof(DDSHeader);

  TextureFormat format = ~0u;
  uint32_t layers = 1;
  uint32_t flags = 0;

  // Header10
  if ((header->format.flags & DDPF_FOURCC) && !memcmp(&header->format.fourCC, "DX10", 4)) {
    if (length < sizeof(DDSHeader10)) return true;
    DDSHeader10* header10 = (DDSHeader10*) data;
    length -= sizeof(DDSHeader10);
    data += sizeof(DDSHeader10);

    switch (header10->dxgiFormat) {
      case DXGI_FORMAT_R32G32B32A32_TYPELESS:
      case DXGI_FORMAT_R32G32B32A32_FLOAT:
        format = FORMAT_RGBA32F;
        break;

      case DXGI_FORMAT_R16G16B16A16_TYPELESS:
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
        format = FORMAT_RGBA16F;
        break;

      case DXGI_FORMAT_R16G16B16A16_UNORM:
        format = FORMAT_RGBA16;
        break;

      case DXGI_FORMAT_R32G32_TYPELESS:
      case DXGI_FORMAT_R32G32_FLOAT:
        format = FORMAT_RG32F;
        break;

      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        format = FORMAT_D32FS8;
        break;

      case DXGI_FORMAT_R10G10B10A2_TYPELESS:
      case DXGI_FORMAT_R10G10B10A2_UNORM:
        format = FORMAT_RGB10A2;
        break;

      case DXGI_FORMAT_R11G11B10_FLOAT:
        format = FORMAT_RG11B10F;
        break;

      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        flags |= IMAGE_SRGB; /* fallthrough */
      case DXGI_FORMAT_R8G8B8A8_TYPELESS:
      case DXGI_FORMAT_R8G8B8A8_UNORM:
        format = FORMAT_RGBA8;
        break;

      case DXGI_FORMAT_R16G16_TYPELESS:
      case DXGI_FORMAT_R16G16_FLOAT:
        format = FORMAT_RG16F;
        break;

      case DXGI_FORMAT_R16G16_UNORM:
        format = FORMAT_RG16;
        break;

      case DXGI_FORMAT_D32_FLOAT:
        format = FORMAT_D32F;
        break;

      case DXGI_FORMAT_R32_FLOAT:
        format = FORMAT_R32F;
        break;

      case DXGI_FORMAT_D24_UNORM_S8_UINT:
        format = FORMAT_D24S8;
        break;

      case DXGI_FORMAT_R8G8_TYPELESS:
      case DXGI_FORMAT_R8G8_UNORM:
        format = FORMAT_RG8;
        break;

      case DXGI_FORMAT_R16_TYPELESS:
      case DXGI_FORMAT_R16_FLOAT:
        format = FORMAT_R16F;
        break;

      case DXGI_FORMAT_D16_UNORM:
        format = FORMAT_D16;
        break;

      case DXGI_FORMAT_R16_UNORM:
        format = FORMAT_R16;
        break;

      case DXGI_FORMAT_R8_TYPELESS:
      case DXGI_FORMAT_R8_UNORM:
        format = FORMAT_R8;
        break;

      case DXGI_FORMAT_BC1_UNORM_SRGB:
        flags |= IMAGE_SRGB; /* fallthrough */
      case DXGI_FORMAT_BC1_TYPELESS:
      case DXGI_FORMAT_BC1_UNORM:
        format = FORMAT_BC1;
        break;

      case DXGI_FORMAT_BC2_UNORM_SRGB:
        flags |= IMAGE_SRGB; /* fallthrough */
      case DXGI_FORMAT_BC2_TYPELESS:
      case DXGI_FORMAT_BC2_UNORM:
        format = FORMAT_BC2;
        break;

      case DXGI_FORMAT_BC3_UNORM_SRGB:
        flags |= IMAGE_SRGB; /* fallthrough */
      case DXGI_FORMAT_BC3_TYPELESS:
      case DXGI_FORMAT_BC3_UNORM:
        format = FORMAT_BC3;
        break;

      case DXGI_FORMAT_BC4_TYPELESS:
      case DXGI_FORMAT_BC4_UNORM:
        format = FORMAT_BC4U;
        break;
      case DXGI_FORMAT_BC4_SNORM:
        format = FORMAT_BC4S;
        break;

      case DXGI_FORMAT_BC5_TYPELESS:
      case DXGI_FORMAT_BC5_UNORM:
        format = FORMAT_BC5U;
        break;
      case DXGI_FORMAT_BC5_SNORM:
        format = FORMAT_BC5S;
        break;

      case DXGI_FORMAT_B5G6R5_UNORM:
        format = FORMAT_RGB565;
        break;

      case DXGI_FORMAT_B5G5R5A1_UNORM:
        format = FORMAT_RGB5A1;
        break;

      case DXGI_FORMAT_BC6H_UF16:
        format = FORMAT_BC6SF;
        break;
      case DXGI_FORMAT_BC6H_TYPELESS:
      case DXGI_FORMAT_BC6H_SF16:
        format = FORMAT_BC6UF;
        break;

      case DXGI_FORMAT_BC7_UNORM_SRGB:
        flags |= IMAGE_SRGB; /* fallthrough */
      case DXGI_FORMAT_BC7_TYPELESS:
      case DXGI_FORMAT_BC7_UNORM:
        format = FORMAT_BC7;
        break;

      default:
        lovrSetError("DDS file uses an unsupported DXGI format (%d)", header10->dxgiFormat);
        return false;
    }

    lovrAssert(header10->resourceDimension != D3D10_RESOURCE_DIMENSION_TEXTURE3D, "Loading 3D DDS images is not supported");
    layers = header10->arraySize;

    if (header10->miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE) {
      flags |= IMAGE_CUBEMAP;
    }

    if (header10->miscFlags2 & DDS_ALPHA_MODE_PREMULTIPLIED) {
      flags |= IMAGE_PREMULTIPLIED;
    }
  } else if (header->format.flags & DDPF_FOURCC) {
    if (!memcmp(&header->format.fourCC, "DXT1", 4)) format = FORMAT_BC1;
    else if (!memcmp(&header->format.fourCC, "DXT2", 4)) format = FORMAT_BC2, flags |= IMAGE_PREMULTIPLIED;
    else if (!memcmp(&header->format.fourCC, "DXT3", 4)) format = FORMAT_BC2;
    else if (!memcmp(&header->format.fourCC, "DXT4", 4)) format = FORMAT_BC3, flags |= IMAGE_PREMULTIPLIED;
    else if (!memcmp(&header->format.fourCC, "DXT5", 4)) format = FORMAT_BC3;
    else if (!memcmp(&header->format.fourCC, "BC4U", 4)) format = FORMAT_BC4U;
    else if (!memcmp(&header->format.fourCC, "ATI1", 4)) format = FORMAT_BC4U;
    else if (!memcmp(&header->format.fourCC, "BC4S", 4)) format = FORMAT_BC4S;
    else if (!memcmp(&header->format.fourCC, "ATI2", 4)) format = FORMAT_BC5U;
    else if (!memcmp(&header->format.fourCC, "BC5S", 4)) format = FORMAT_BC5S;
    else if (header->format.fourCC == 0x6f) format = FORMAT_R16F;
    else if (header->format.fourCC == 0x70) format = FORMAT_RG16F;
    else if (header->format.fourCC == 0x71) format = FORMAT_RGBA16F;
    else if (header->format.fourCC == 0x72) format = FORMAT_R32F;
    else if (header->format.fourCC == 0x73) format = FORMAT_RG32F;
    else if (header->format.fourCC == 0x74) format = FORMAT_RGBA32F;
    else return lovrSetError("DDS file uses an unsupported FourCC format (%d)", header->format.fourCC);
  } else {
    return lovrSetError("DDS file uses an unsupported format"); // TODO could handle more uncompressed formats
  }

  lovrAssert(~header->flags & DDSD_DEPTH, "Loading 3D DDS images is not supported");
  uint32_t levels = MAX(1, header->mipmapCount);

  Image* image = lovrCalloc(offsetof(Image, mipmaps) + levels * sizeof(Mipmap));
  image->ref = 1;
  image->flags = flags;
  image->width = header->width;
  image->height = header->height;
  image->format = format;
  image->layers = layers;
  image->levels = levels;
  image->blob = blob;

  size_t stride = 0;
  for (uint32_t i = 0, width = image->width, height = image->height; i < levels; i++) {
    size_t size = measure(width, height, format);

    if (length < size) {
      lovrFree(image);
      return lovrSetError("DDS file overflow");
    }

    image->mipmaps[i] = (Mipmap) { data, size, 0 };
    width = MAX(width >> 1, 1);
    height = MAX(height >> 1, 1);
    stride += size;
    length -= size;
    data += size;
  }

  for (uint32_t i = 0; i < levels; i++) {
    image->mipmaps[i].stride = stride;
  }

  lovrRetain(blob);
  *result = image;
  return true;
}

static bool loadASTC(Blob* blob, Image** result) {
  struct {
    uint32_t magic;
    uint8_t blockX;
    uint8_t blockY;
    uint8_t blockZ;
    uint8_t width[3];
    uint8_t height[3];
    uint8_t depth[3];
  } header;

  if (blob->size <= sizeof(header)) {
    return true;
  }

  memcpy(&header, blob->data, sizeof(header));

  if (header.magic != 0x5ca1ab13) {
    return true;
  }

  TextureFormat format;

  uint32_t bx = header.blockX, by = header.blockY, bz = header.blockZ;
  if (bx == 4 && by == 4 && bz == 1) { format = FORMAT_ASTC_4x4; }
  else if (bx == 5 && by == 4 && bz == 1) { format = FORMAT_ASTC_5x4; }
  else if (bx == 5 && by == 5 && bz == 1) { format = FORMAT_ASTC_5x5; }
  else if (bx == 6 && by == 5 && bz == 1) { format = FORMAT_ASTC_6x5; }
  else if (bx == 6 && by == 6 && bz == 1) { format = FORMAT_ASTC_6x6; }
  else if (bx == 8 && by == 5 && bz == 1) { format = FORMAT_ASTC_8x5; }
  else if (bx == 8 && by == 6 && bz == 1) { format = FORMAT_ASTC_8x6; }
  else if (bx == 8 && by == 8 && bz == 1) { format = FORMAT_ASTC_8x8; }
  else if (bx == 10 && by == 5 && bz == 1) { format = FORMAT_ASTC_10x5; }
  else if (bx == 10 && by == 6 && bz == 1) { format = FORMAT_ASTC_10x6; }
  else if (bx == 10 && by == 8 && bz == 1) { format = FORMAT_ASTC_10x8; }
  else if (bx == 10 && by == 10 && bz == 1) { format = FORMAT_ASTC_10x10; }
  else if (bx == 12 && by == 10 && bz == 1) { format = FORMAT_ASTC_12x10; }
  else if (bx == 12 && by == 12 && bz == 1) { format = FORMAT_ASTC_12x12; }
  else { return lovrSetError("Unsupported ASTC format %dx%dx%d", bx, by, bz); }

  uint32_t width = header.width[0] + (header.width[1] << 8) + (header.width[2] << 16);
  uint32_t height = header.height[0] + (header.height[1] << 8) + (header.height[2] << 16);

  size_t imageSize = ((width + bx - 1) / bx) * ((height + by - 1) / by) * (128 / 8);

  if (imageSize > blob->size - sizeof(header)) {
    return lovrSetError("ASTC size overflows file size");
  }

  Image* image = lovrCalloc(sizeof(Image));
  image->ref = 1;
  image->width = width;
  image->height = height;
  image->format = format;
  image->flags = IMAGE_SRGB;
  image->layers = 1;
  image->levels = 1;
  image->blob = blob;
  lovrRetain(blob);
  image->mipmaps[0] = (Mipmap) { (char*) blob->data + sizeof(header), imageSize, 0 };
  *result = image;
  return true;
}

static bool loadKTX1(Blob* blob, Image** result) {
  struct {
    uint8_t magic[12];
    uint32_t endianness;
    uint32_t glType;
    uint32_t glTypeSize;
    uint32_t glFormat;
    uint32_t glInternalFormat;
    uint32_t glBaseInternalFormat;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
    uint32_t pixelDepth;
    uint32_t numberOfArrayElements;
    uint32_t numberOfFaces;
    uint32_t numberOfMipmapLevels;
    uint32_t bytesOfKeyValueData;
  } header;

  if (blob->size <= sizeof(header)) {
    return true;
  }

  memcpy(&header, blob->data, sizeof(header));

  uint8_t magic[] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };

  if (memcmp(header.magic, magic, sizeof(magic)) || header.endianness != 0x04030201) {
    return true;
  }

  char* data = blob->data;
  size_t length = blob->size;
  data += sizeof(header);
  length -= sizeof(header);

  if (length < header.bytesOfKeyValueData) {
    return lovrSetError("Invalid KTX file");
  }

  data += header.bytesOfKeyValueData;
  length -= header.bytesOfKeyValueData;

  lovrCheck(header.pixelWidth > 0, "KTX image dimensions must be positive");
  lovrCheck(header.pixelHeight > 0, "Unable to load 1D KTX images");
  lovrCheck(header.pixelDepth == 0, "Unable to load 3D KTX images");
  lovrCheck(header.numberOfFaces == 1 || header.numberOfFaces == 6, "KTX files must have 1 or 6 faces");
  lovrCheck(header.numberOfFaces == 1 || header.numberOfArrayElements == 0, "KTX files with cubemap arrays are not currently supported");

  uint32_t layers = MAX(header.numberOfArrayElements, 1);
  uint32_t levels = MAX(header.numberOfMipmapLevels, 1);

  Image* image = lovrCalloc(offsetof(Image, mipmaps) + levels * sizeof(Mipmap));
  image->ref = 1;
  image->width = header.pixelWidth;
  image->height = header.pixelHeight;
  image->layers = layers;
  image->levels = levels;
  image->blob = blob;

  if (header.numberOfFaces == 6) {
    image->flags |= IMAGE_CUBEMAP;
    image->layers = 6;
  }

  // Format

  struct { uint32_t type, format, internalFormat, srgbInternalFormat; } lookup[] = {
    [FORMAT_R8]         = { 0x1401, 0x1903, 0x8229, 0 },
    [FORMAT_RG8]        = { 0x1401, 0x8227, 0x822B, 0 },
    [FORMAT_RGBA8]      = { 0x1401, 0x1908, 0x8058, 0x8C43 },
    [FORMAT_R16]        = { 0x1403, 0x1903, 0x822A, 0 },
    [FORMAT_RG16]       = { 0x1403, 0x8227, 0x822C, 0 },
    [FORMAT_RGBA16]     = { 0x1403, 0x1908, 0x805B, 0 },
    [FORMAT_R16F]       = { 0x140B, 0x1903, 0x822D, 0 },
    [FORMAT_RG16F]      = { 0x140B, 0x8227, 0x822F, 0 },
    [FORMAT_RGBA16F]    = { 0x140B, 0x1908, 0x881A, 0 },
    [FORMAT_R32F]       = { 0x1406, 0x1903, 0x822E, 0 },
    [FORMAT_RG32F]      = { 0x1406, 0x8227, 0x8230, 0 },
    [FORMAT_RGBA32F]    = { 0x1406, 0x1908, 0x8814, 0 },
    [FORMAT_RGB565]     = { 0x8363, 0x1907, 0x8D62, 0 },
    [FORMAT_RGB5A1]     = { 0x8034, 0x1908, 0x8057, 0 },
    [FORMAT_RGB10A2]    = { 0x8368, 0x1908, 0x8059, 0 },
    [FORMAT_RG11B10F]   = { 0x8C3A, 0x1907, 0x8C3A, 0 },
    [FORMAT_D16]        = { 0x1403, 0x1902, 0x81A5, 0 },
    [FORMAT_D24]        = { 0x1405, 0x1902, 0x81A6, 0 },
    [FORMAT_D32F]       = { 0x1406, 0x1902, 0x8CAC, 0 },
    [FORMAT_D24S8]      = { 0x84FA, 0x84F9, 0x88F0, 0 },
    [FORMAT_D32FS8]     = { 0x8DAD, 0x84F9, 0x8CAD, 0 },
    [FORMAT_BC1]        = { 0x0000, 0x0000, 0x83F1, 0x8C4D },
    [FORMAT_BC2]        = { 0x0000, 0x0000, 0x83F2, 0x8C4E },
    [FORMAT_BC3]        = { 0x0000, 0x0000, 0x83F3, 0x8C4F },
    [FORMAT_BC4U]       = { 0x0000, 0x0000, 0x8DBB, 0 },
    [FORMAT_BC4S]       = { 0x0000, 0x0000, 0x8DBC, 0 },
    [FORMAT_BC5U]       = { 0x0000, 0x0000, 0x8DBD, 0 },
    [FORMAT_BC5S]       = { 0x0000, 0x0000, 0x8DBE, 0 },
    [FORMAT_BC6UF]      = { 0x0000, 0x0000, 0x8E8F, 0 },
    [FORMAT_BC6SF]      = { 0x0000, 0x0000, 0x8E8E, 0 },
    [FORMAT_BC7]        = { 0x0000, 0x0000, 0x8E8C, 0x8E8D },
    [FORMAT_ASTC_4x4]   = { 0x0000, 0x0000, 0x93B0, 0x93D0 },
    [FORMAT_ASTC_5x4]   = { 0x0000, 0x0000, 0x93B1, 0x93D1 },
    [FORMAT_ASTC_5x5]   = { 0x0000, 0x0000, 0x93B2, 0x93D2 },
    [FORMAT_ASTC_6x5]   = { 0x0000, 0x0000, 0x93B3, 0x93D3 },
    [FORMAT_ASTC_6x6]   = { 0x0000, 0x0000, 0x93B4, 0x93D4 },
    [FORMAT_ASTC_8x5]   = { 0x0000, 0x0000, 0x93B5, 0x93D5 },
    [FORMAT_ASTC_8x6]   = { 0x0000, 0x0000, 0x93B6, 0x93D6 },
    [FORMAT_ASTC_8x8]   = { 0x0000, 0x0000, 0x93B7, 0x93D7 },
    [FORMAT_ASTC_10x5]  = { 0x0000, 0x0000, 0x93B8, 0x93D8 },
    [FORMAT_ASTC_10x6]  = { 0x0000, 0x0000, 0x93B9, 0x93D9 },
    [FORMAT_ASTC_10x8]  = { 0x0000, 0x0000, 0x93BA, 0x93DA },
    [FORMAT_ASTC_10x10] = { 0x0000, 0x0000, 0x93BB, 0x93DB },
    [FORMAT_ASTC_12x10] = { 0x0000, 0x0000, 0x93BC, 0x93DC },
    [FORMAT_ASTC_12x12] = { 0x0000, 0x0000, 0x93BD, 0x93DD }
  };

  image->format = ~0u;
  for (uint32_t i = 0; i < COUNTOF(lookup); i++) {
    if (header.glType == lookup[i].type && header.glFormat == lookup[i].format) {
      if (header.glInternalFormat == lookup[i].internalFormat) {
        image->format = i;
        break;
      } else if (lookup[i].srgbInternalFormat && header.glInternalFormat == lookup[i].srgbInternalFormat) {
        image->format = i;
        image->flags |= IMAGE_SRGB;
        break;
      }
    }
  }

  if (image->format == ~0u) {
    lovrFree(image);
    return lovrSetError("KTX1 file uses an unsupported image format (glType = 0x%x, glFormat = 0x%x, glInternalFormat = 0x%x)", header.glType, header.glFormat, header.glInternalFormat);
  }

  // Mipmaps
  uint32_t width = image->width;
  uint32_t height = image->height;
  size_t divisor = (image->flags & IMAGE_CUBEMAP) ? 1 : image->layers;
  for (uint32_t i = 0; i < image->levels; i++) {
    uint32_t levelSize;
    memcpy(&levelSize, data, 4);
    size_t size = measure(width, height, image->format);
    if (levelSize / divisor != size) {
      lovrFree(image);
      return lovrSetError("KTX size mismatch");
    }

    length -= 4;
    data += 4;

    size_t totalSize = size * image->layers;
    if (length < totalSize) {
      lovrFree(image);
      return lovrSetError("KTX file overflow");
    }

    image->mipmaps[i] = (Mipmap) { data, size, size };
    width = MAX(width >> 1, 1);
    height = MAX(height >> 1, 1);
    length -= totalSize;
    data += totalSize;
  }

  lovrRetain(blob);
  *result = image;
  return true;
}

static bool loadKTX2(Blob* blob, Image** result) {
  typedef struct {
    uint8_t magic[12];
    uint32_t vkFormat;
    uint32_t typeSize;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
    uint32_t pixelDepth;
    uint32_t layerCount;
    uint32_t faceCount;
    uint32_t levelCount;
    uint32_t compression;
    uint32_t dfdByteOffset;
    uint32_t dfdByteLength;
    uint32_t kvdByteOffset;
    uint32_t kvdByteLength;
    uint64_t sgdByteOffset;
    uint64_t sgdByteLength;
    struct {
      uint64_t byteOffset;
      uint64_t byteLength;
      uint64_t uncompressedLength;
    } levels[1];
  } KTX2Header;

  char* data = blob->data;
  size_t length = blob->size;
  KTX2Header* header = (KTX2Header*) data;

  uint8_t magic[] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };

  if (length < sizeof(KTX2Header) || memcmp(header->magic, magic, sizeof(magic))) {
    return true;
  }

  lovrCheck(header->pixelWidth > 0, "KTX image dimensions must be positive");
  lovrCheck(header->pixelHeight > 0, "Unable to load 1D KTX images");
  lovrCheck(header->pixelDepth == 0, "Unable to load 3D KTX images");
  lovrCheck(header->faceCount == 1 || header->faceCount == 6, "Invalid KTX file (faceCount must be 1 or 6)");
  lovrCheck(header->layerCount == 0 || header->faceCount == 1, "Unable to load cubemap array KTX images");
  lovrCheck(!header->compression, "Supercompressed KTX files are not currently supported");

  uint32_t layers = MAX(header->layerCount, 1);
  uint32_t levels = MAX(header->levelCount, 1);

  Image* image = lovrCalloc(offsetof(Image, mipmaps) + levels * sizeof(Mipmap));
  image->ref = 1;
  image->width = header->pixelWidth;
  image->height = header->pixelHeight;
  image->layers = layers;
  image->levels = levels;
  image->blob = blob;

  if (header->faceCount == 6) {
    image->flags |= IMAGE_CUBEMAP;
    image->layers = 6;
  }

  // Format
  switch (header->vkFormat) {
    case 9:   image->format = FORMAT_R8; break;
    case 16:  image->format = FORMAT_RG8; break;
    case 37:  image->format = FORMAT_RGBA8; break;
    case 43:  image->format = FORMAT_RGBA8, image->flags |= IMAGE_SRGB; break;
    case 70:  image->format = FORMAT_R16; break;
    case 77:  image->format = FORMAT_RG16; break;
    case 91:  image->format = FORMAT_RGBA16; break;
    case 76:  image->format = FORMAT_R16F; break;
    case 83:  image->format = FORMAT_RG16F; break;
    case 97:  image->format = FORMAT_RGBA16F; break;
    case 100: image->format = FORMAT_R32F; break;
    case 103: image->format = FORMAT_RG32F; break;
    case 4:   image->format = FORMAT_RGB565; break;
    case 6:   image->format = FORMAT_RGB5A1; break;
    case 64:  image->format = FORMAT_RGB10A2; break;
    case 122: image->format = FORMAT_RG11B10F; break;
    case 124: image->format = FORMAT_D16; break;
    case 125: image->format = FORMAT_D24; break;
    case 126: image->format = FORMAT_D32F; break;
    case 129: image->format = FORMAT_D24S8; break;
    case 130: image->format = FORMAT_D32FS8; break;
    case 132: image->flags |= IMAGE_SRGB; /* fallthrough */ case 131: image->format = FORMAT_BC1; break;
    case 136: image->flags |= IMAGE_SRGB; /* fallthrough */ case 135: image->format = FORMAT_BC2; break;
    case 138: image->flags |= IMAGE_SRGB; /* fallthrough */ case 137: image->format = FORMAT_BC3; break;
    case 139: image->format = FORMAT_BC4U; break;
    case 140: image->format = FORMAT_BC4S; break;
    case 141: image->format = FORMAT_BC5U; break;
    case 142: image->format = FORMAT_BC5S; break;
    case 143: image->format = FORMAT_BC6UF; break;
    case 144: image->format = FORMAT_BC6SF; break;
    case 146: image->flags |= IMAGE_SRGB; /* fallthrough */ case 145: image->format = FORMAT_BC7; break;
    case 158: image->flags |= IMAGE_SRGB; /* fallthrough */ case 157: image->format = FORMAT_ASTC_4x4; break;
    case 160: image->flags |= IMAGE_SRGB; /* fallthrough */ case 159: image->format = FORMAT_ASTC_5x4; break;
    case 162: image->flags |= IMAGE_SRGB; /* fallthrough */ case 161: image->format = FORMAT_ASTC_5x5; break;
    case 164: image->flags |= IMAGE_SRGB; /* fallthrough */ case 163: image->format = FORMAT_ASTC_6x5; break;
    case 166: image->flags |= IMAGE_SRGB; /* fallthrough */ case 165: image->format = FORMAT_ASTC_6x6; break;
    case 168: image->flags |= IMAGE_SRGB; /* fallthrough */ case 167: image->format = FORMAT_ASTC_8x5; break;
    case 170: image->flags |= IMAGE_SRGB; /* fallthrough */ case 169: image->format = FORMAT_ASTC_8x6; break;
    case 172: image->flags |= IMAGE_SRGB; /* fallthrough */ case 171: image->format = FORMAT_ASTC_8x8; break;
    case 174: image->flags |= IMAGE_SRGB; /* fallthrough */ case 173: image->format = FORMAT_ASTC_10x5; break;
    case 176: image->flags |= IMAGE_SRGB; /* fallthrough */ case 175: image->format = FORMAT_ASTC_10x6; break;
    case 178: image->flags |= IMAGE_SRGB; /* fallthrough */ case 177: image->format = FORMAT_ASTC_10x8; break;
    case 180: image->flags |= IMAGE_SRGB; /* fallthrough */ case 179: image->format = FORMAT_ASTC_10x10; break;
    case 182: image->flags |= IMAGE_SRGB; /* fallthrough */ case 181: image->format = FORMAT_ASTC_12x10; break;
    case 184: image->flags |= IMAGE_SRGB; /* fallthrough */ case 183: image->format = FORMAT_ASTC_12x12; break;
    default:
      lovrFree(image);
      return lovrSetError("KTX file uses an unsupported image format");
  }

  // Mipmaps
  uint32_t width = image->width;
  uint32_t height = image->height;
  for (uint32_t i = 0; i < image->levels; i++) {
    uint64_t offset = header->levels[i].byteOffset;
    uint64_t size = header->levels[i].byteLength;
    size_t stride = size / image->layers;

    if (offset + size > blob->size) {
      lovrFree(image);
      return lovrSetError("KTX file overflow");
    }

    if (measure(width, height, image->format) != size) {
      lovrFree(image);
      return lovrSetError("KTX size mismatch");
    }

    image->mipmaps[i] = (Mipmap) { data + offset, stride, stride };
    width = MAX(width >> 1, 1);
    height = MAX(height >> 1, 1);
  }

  lovrRetain(blob);
  *result = image;
  return true;
}

static bool loadSTB(Blob* blob, Image** result) {
  void* data;
  uint32_t flags = 0;
  TextureFormat format;
  int width, height, channels;
  if (stbi_is_16_bit_from_memory(blob->data, (int) blob->size)) {
    data = stbi_load_16_from_memory(blob->data, (int) blob->size, &width, &height, &channels, 0);
    switch (channels) {
      case 1: format = FORMAT_R16; break;
      case 2: format = FORMAT_RG16; break;
      case 4: format = FORMAT_RGBA16; break;
      default: return lovrSetError("Unsupported channel count for 16 bit image: %d", channels);
    }
    flags = IMAGE_SRGB;
  } else if (stbi_is_hdr_from_memory(blob->data, (int) blob->size)) {
    data = stbi_loadf_from_memory(blob->data, (int) blob->size, &width, &height, NULL, 4);
    format = FORMAT_RGBA32F;
  } else {
    data = stbi_load_from_memory(blob->data, (int) blob->size, &width, &height, NULL, 4);
    format = FORMAT_RGBA8;
    flags = IMAGE_SRGB;
  }

  if (!data) {
    return true;
  }

  size_t size = measure(width, height, format);

  Image* image = lovrCalloc(sizeof(Image));
  image->ref = 1;
  image->flags = flags;
  image->width = width;
  image->height = height;
  image->format = format;
  image->layers = 1;
  image->levels = 1;
  image->blob = lovrBlobCreate(data, size, blob->name);
  image->mipmaps[0] = (Mipmap) { data, size, 0 };
  *result = image;
  return true;
}
