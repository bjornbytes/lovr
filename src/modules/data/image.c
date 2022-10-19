#include "data/image.h"
#include "data/blob.h"
#include "util.h"
#include "lib/stb/stb_image.h"
#include <stdlib.h>
#include <string.h>

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

Image* lovrImageCreateRaw(uint32_t width, uint32_t height, TextureFormat format) {
  lovrCheck(width > 0 && height > 0, "Image dimensions must be positive");
  lovrCheck(format < FORMAT_BC1, "Blank images cannot be compressed");
  size_t size = measure(width, height, format);
  void* data = malloc(size);
  Image* image = calloc(1, sizeof(Image));
  lovrAssert(image && data, "Out of memory");
  image->ref = 1;
  image->width = width;
  image->height = height;
  image->format = format;
  image->layers = 1;
  image->levels = 1;
  image->blob = lovrBlobCreate(data, size, "Image");
  image->mipmaps[0] = (Mipmap) { data, size, 0 };
  return image;
}

static Image* loadDDS(Blob* blob);
static Image* loadASTC(Blob* blob);
static Image* loadKTX1(Blob* blob);
static Image* loadKTX2(Blob* blob);
static Image* loadSTB(Blob* blob);

Image* lovrImageCreateFromFile(Blob* blob) {
  Image* image = NULL;

  if ((image = loadDDS(blob)) != NULL) return image;
  if ((image = loadASTC(blob)) != NULL) return image;
  if ((image = loadKTX1(blob)) != NULL) return image;
  if ((image = loadKTX2(blob)) != NULL) return image;
  if ((image = loadSTB(blob)) != NULL) return image;

  lovrThrow("Could not load image from '%s': Image file format not recognized", blob->name);
  return NULL;
}

void lovrImageDestroy(void* ref) {
  Image* image = ref;
  lovrRelease(image->blob, lovrBlobDestroy);
  free(image);
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

void lovrImageGetPixel(Image* image, uint32_t x, uint32_t y, float pixel[4]) {
  lovrCheck(!lovrImageIsCompressed(image), "Unable to access individual pixels of a compressed image");
  lovrAssert(x < image->width && y < image->height, "Pixel coordinates must be within Image bounds");
  size_t offset = measure(y * image->width + x, 1, image->format);
  uint8_t* u8 = (uint8_t*) image->mipmaps[0].data + offset;
  uint16_t* u16 = (uint16_t*) u8;
  float* f32 = (float*) u8;
  switch (image->format) {
    case FORMAT_R8:
      pixel[0] = u8[0] / 255.f;
      return;
    case FORMAT_RG8:
      pixel[0] = u8[0] / 255.f;
      pixel[1] = u8[1] / 255.f;
      return;
    case FORMAT_RGBA8:
      pixel[0] = u8[0] / 255.f;
      pixel[1] = u8[1] / 255.f;
      pixel[2] = u8[2] / 255.f;
      pixel[3] = u8[3] / 255.f;
      return;
    case FORMAT_R16:
      pixel[0] = u16[0] / 65535.f;
      return;
    case FORMAT_RG16:
      pixel[0] = u16[0] / 65535.f;
      pixel[1] = u16[1] / 65535.f;
      return;
    case FORMAT_RGBA16:
      pixel[0] = u16[0] / 65535.f;
      pixel[1] = u16[1] / 65535.f;
      pixel[2] = u16[2] / 65535.f;
      pixel[3] = u16[3] / 65535.f;
      return;
    case FORMAT_R32F:
      pixel[0] = f32[0];
      return;
    case FORMAT_RG32F:
      pixel[0] = f32[0];
      pixel[1] = f32[1];
      return;
    case FORMAT_RGBA32F:
      pixel[0] = f32[0];
      pixel[1] = f32[1];
      pixel[2] = f32[2];
      pixel[3] = f32[3];
      return;
    default: lovrThrow("Unsupported format for Image:getPixel");
  }
}

void lovrImageSetPixel(Image* image, uint32_t x, uint32_t y, float pixel[4]) {
  lovrCheck(!lovrImageIsCompressed(image), "Unable to access individual pixels of a compressed image");
  lovrAssert(x < image->width && y < image->height, "Pixel coordinates must be within Image bounds");
  size_t offset = measure(y * image->width + x, 1, image->format);
  uint8_t* u8 = (uint8_t*) image->mipmaps[0].data + offset;
  uint16_t* u16 = (uint16_t*) u8;
  float* f32 = (float*) u8;
  switch (image->format) {
    case FORMAT_R8:
      u8[0] = (uint8_t) (pixel[0] * 255.f + .5f);
      break;
    case FORMAT_RG8:
      u8[0] = (uint8_t) (pixel[0] * 255.f + .5f);
      u8[1] = (uint8_t) (pixel[1] * 255.f + .5f);
      break;
    case FORMAT_RGBA8:
      u8[0] = (uint8_t) (pixel[0] * 255.f + .5f);
      u8[1] = (uint8_t) (pixel[1] * 255.f + .5f);
      u8[2] = (uint8_t) (pixel[2] * 255.f + .5f);
      u8[3] = (uint8_t) (pixel[3] * 255.f + .5f);
      break;
    case FORMAT_R16:
      u16[0] = (uint16_t) (pixel[0] * 65535.f + .5f);
      break;
    case FORMAT_RG16:
      u16[0] = (uint16_t) (pixel[0] * 65535.f + .5f);
      u16[1] = (uint16_t) (pixel[1] * 65535.f + .5f);
      break;
    case FORMAT_RGBA16:
      u16[0] = (uint16_t) (pixel[0] * 65535.f + .5f);
      u16[1] = (uint16_t) (pixel[1] * 65535.f + .5f);
      u16[2] = (uint16_t) (pixel[2] * 65535.f + .5f);
      u16[3] = (uint16_t) (pixel[3] * 65535.f + .5f);
      break;
    case FORMAT_R32F:
      f32[0] = pixel[0];
      break;
    case FORMAT_RG32F:
      f32[0] = pixel[0];
      f32[1] = pixel[1];
      break;
    case FORMAT_RGBA32F:
      f32[0] = pixel[0];
      f32[1] = pixel[1];
      f32[2] = pixel[2];
      f32[3] = pixel[3];
      break;
    default: lovrThrow("Unsupported format for Image:setPixel");
  }
}

void lovrImageCopy(Image* src, Image* dst, uint32_t srcOffset[2], uint32_t dstOffset[2], uint32_t extent[2]) {
  lovrAssert(src->format == dst->format, "To copy between Images, their formats must match");
  lovrAssert(!lovrImageIsCompressed(src), "Compressed Images cannot be copied");
  lovrAssert(dstOffset[0] + extent[0] <= dst->width, "Image copy region extends past the destination image width");
  lovrAssert(dstOffset[1] + extent[1] <= dst->height, "Image copy region extends past the destination image height");
  lovrAssert(srcOffset[0] + extent[0] <= src->width, "Image copy region extends past the source image width");
  lovrAssert(srcOffset[1] + extent[1] <= src->height, "Image copy region extends past the source image height");
  size_t pixelSize = measure(1, 1, src->format);
  uint8_t* p = (uint8_t*) lovrImageGetLayerData(src, 0, 0) + (srcOffset[1] * src->width + srcOffset[0]) * pixelSize;
  uint8_t* q = (uint8_t*) lovrImageGetLayerData(dst, 0, 0) + (dstOffset[1] * dst->width + dstOffset[0]) * pixelSize;
  for (uint32_t y = 0; y < extent[1]; y++) {
    memcpy(q, p, extent[0] * pixelSize);
    p += src->width * pixelSize;
    q += dst->width * pixelSize;
  }
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
  lovrAssert(image->format == FORMAT_RGBA8, "Only images with the rgba8 format can be encoded");
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
  uint8_t* data = malloc(size);
  lovrAssert(data, "Out of memory");

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

static Image* loadDDS(Blob* blob) {
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

  if (blob->size < 4 + sizeof(DDSHeader)) return NULL;

  // Magic
  char* data = blob->data;
  size_t length = blob->size;
  uint32_t magic;
  memcpy(&magic, data, 4);
  if (magic != 0x20534444) return false;
  length -= 4;
  data += 4;

  // Header
  DDSHeader* header = (DDSHeader*) data;
  if (length < sizeof(DDSHeader)) return NULL;
  if (header->size != sizeof(DDSHeader) || header->format.size != sizeof(header->format)) return NULL;
  length -= sizeof(DDSHeader);
  data += sizeof(DDSHeader);

  TextureFormat format = ~0u;
  uint32_t layers = 1;
  uint32_t flags = 0;

  // Header10
  if ((header->format.flags & DDPF_FOURCC) && !memcmp(&header->format.fourCC, "DX10", 4)) {
    if (length < sizeof(DDSHeader10)) return NULL;
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

      default: lovrThrow("DDS file uses an unsupported DXGI format (%d)", header10->dxgiFormat);
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
    else lovrThrow("DDS file uses an unsupported FourCC format (%d)", header->format.fourCC);
  } else {
    lovrThrow("DDS file uses an unsupported format"); // TODO could handle more uncompressed formats
  }

  lovrAssert(~header->flags & DDSD_DEPTH, "Loading 3D DDS images is not supported");
  uint32_t levels = MAX(1, header->mipmapCount);

  Image* image = calloc(1, offsetof(Image, mipmaps) + levels * sizeof(Mipmap));
  lovrAssert(image, "Out of memory");
  image->ref = 1;
  image->flags = flags;
  image->width = header->width;
  image->height = header->height;
  image->format = format;
  image->layers = layers;
  image->levels = levels;
  image->blob = blob;
  lovrRetain(blob);

  size_t stride = 0;
  for (uint32_t i = 0, width = image->width, height = image->height; i < levels; i++) {
    size_t size = measure(width, height, format);
    lovrAssert(length >= size, "DDS file overflow");
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

  return image;
}

static Image* loadASTC(Blob* blob) {
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
    return NULL;
  }

  memcpy(&header, blob->data, sizeof(header));

  if (header.magic != 0x5ca1ab13) {
    return NULL;
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
  else { lovrThrow("Unsupported ASTC format %dx%dx%d", bx, by, bz); }

  uint32_t width = header.width[0] + (header.width[1] << 8) + (header.width[2] << 16);
  uint32_t height = header.height[0] + (header.height[1] << 8) + (header.height[2] << 16);

  size_t imageSize = ((width + bx - 1) / bx) * ((height + by - 1) / by) * (128 / 8);

  if (imageSize > blob->size - sizeof(header)) {
    return NULL;
  }

  Image* image = calloc(1, sizeof(Image));
  lovrAssert(image, "Out of memory");
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
  return image;
}

static Image* loadKTX1(Blob* blob) {
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
    return NULL;
  }

  memcpy(&header, blob->data, sizeof(header));

  uint8_t magic[] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };

  if (memcmp(header.magic, magic, sizeof(magic)) || header.endianness != 0x04030201) {
    return NULL;
  }

  char* data = blob->data;
  size_t length = blob->size;
  data += sizeof(header);
  length -= sizeof(header);

  if (length < header.bytesOfKeyValueData) {
    return NULL;
  }

  data += header.bytesOfKeyValueData;
  length -= header.bytesOfKeyValueData;

  lovrAssert(header.pixelWidth > 0, "KTX image dimensions must be positive");
  lovrAssert(header.pixelHeight > 0, "Unable to load 1D KTX images");
  lovrAssert(header.pixelDepth == 0, "Unable to load 3D KTX images");

  uint32_t layers = MAX(header.numberOfArrayElements, 1);
  uint32_t levels = MAX(header.numberOfMipmapLevels, 1);

  Image* image = calloc(1, offsetof(Image, mipmaps) + levels * sizeof(Mipmap));
  lovrAssert(image, "Out of memory");
  image->ref = 1;
  image->width = header.pixelWidth;
  image->height = header.pixelHeight;
  image->layers = layers;
  image->levels = levels;
  image->blob = blob;
  lovrRetain(blob);

  if (header.numberOfFaces > 1) {
    lovrAssert(header.numberOfFaces == 6, "KTX files must have 1 or 6 faces");
    lovrAssert(header.numberOfArrayElements == 0, "KTX files with cubemap arrays are not supported");
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
  lovrAssert(image->format != ~0u, "KTX1 file uses an unsupported image format (glType = %d, glFormat = %d, glInternalFormat = %d)", header.glType, header.glFormat, header.glInternalFormat);

  // Mipmaps
  uint32_t width = image->width;
  uint32_t height = image->height;
  size_t divisor = (image->flags & IMAGE_CUBEMAP) ? 1 : image->layers;
  for (uint32_t i = 0; i < image->levels; i++) {
    uint32_t levelSize;
    memcpy(&levelSize, data, 4);
    size_t size = measure(width, height, image->format);
    lovrAssert(levelSize / divisor == size, "KTX size mismatch");
    length -= 4;
    data += 4;

    size_t totalSize = size * image->layers;
    lovrAssert(length >= totalSize, "KTX file overflow");
    image->mipmaps[i] = (Mipmap) { data, size, size };
    width = MAX(width >> 1, 1);
    height = MAX(height >> 1, 1);
    length -= totalSize;
    data += totalSize;
  }

  return image;
}

static Image* loadKTX2(Blob* blob) {
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
    return NULL;
  }

  lovrAssert(header->pixelWidth > 0, "KTX image dimensions must be positive");
  lovrAssert(header->pixelHeight > 0, "Unable to load 1D KTX images");
  lovrAssert(header->pixelDepth == 0, "Unable to load 3D KTX images");
  lovrAssert(header->faceCount == 1 || header->faceCount == 6, "Invalid KTX file (faceCount must be 1 or 6)");
  lovrAssert(header->layerCount == 0 || header->faceCount == 1, "Unable to load cubemap array KTX images");
  lovrAssert(!header->compression, "Supercompressed KTX files are not currently supported");

  uint32_t layers = MAX(header->layerCount, 1);
  uint32_t levels = MAX(header->levelCount, 1);

  Image* image = calloc(1, offsetof(Image, mipmaps) + levels * sizeof(Mipmap));
  lovrAssert(image, "Out of memory");
  image->ref = 1;
  image->width = header->pixelWidth;
  image->height = header->pixelHeight;
  image->layers = layers;
  image->levels = levels;
  image->blob = blob;
  lovrRetain(blob);

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
    case 126: image->format = FORMAT_D32F; break;
    case 129: image->format = FORMAT_D24S8; break;
    case 130: image->format = FORMAT_D32FS8; break;
    case 132: image->flags |= IMAGE_SRGB; case 131: image->format = FORMAT_BC1; break;
    case 136: image->flags |= IMAGE_SRGB; case 135: image->format = FORMAT_BC2; break;
    case 138: image->flags |= IMAGE_SRGB; case 137: image->format = FORMAT_BC3; break;
    case 139: image->format = FORMAT_BC4U; break;
    case 140: image->format = FORMAT_BC4S; break;
    case 141: image->format = FORMAT_BC5U; break;
    case 142: image->format = FORMAT_BC5S; break;
    case 143: image->format = FORMAT_BC6UF; break;
    case 144: image->format = FORMAT_BC6SF; break;
    case 146: image->flags |= IMAGE_SRGB; case 145: image->format = FORMAT_BC7; break;
    case 158: image->flags |= IMAGE_SRGB; case 157: image->format = FORMAT_ASTC_4x4; break;
    case 160: image->flags |= IMAGE_SRGB; case 159: image->format = FORMAT_ASTC_5x4; break;
    case 162: image->flags |= IMAGE_SRGB; case 161: image->format = FORMAT_ASTC_5x5; break;
    case 164: image->flags |= IMAGE_SRGB; case 163: image->format = FORMAT_ASTC_6x5; break;
    case 166: image->flags |= IMAGE_SRGB; case 165: image->format = FORMAT_ASTC_6x6; break;
    case 168: image->flags |= IMAGE_SRGB; case 167: image->format = FORMAT_ASTC_8x5; break;
    case 170: image->flags |= IMAGE_SRGB; case 169: image->format = FORMAT_ASTC_8x6; break;
    case 172: image->flags |= IMAGE_SRGB; case 171: image->format = FORMAT_ASTC_8x8; break;
    case 174: image->flags |= IMAGE_SRGB; case 173: image->format = FORMAT_ASTC_10x5; break;
    case 176: image->flags |= IMAGE_SRGB; case 175: image->format = FORMAT_ASTC_10x6; break;
    case 178: image->flags |= IMAGE_SRGB; case 177: image->format = FORMAT_ASTC_10x8; break;
    case 180: image->flags |= IMAGE_SRGB; case 179: image->format = FORMAT_ASTC_10x10; break;
    case 182: image->flags |= IMAGE_SRGB; case 181: image->format = FORMAT_ASTC_12x10; break;
    case 184: image->flags |= IMAGE_SRGB; case 183: image->format = FORMAT_ASTC_12x12; break;
    default: lovrThrow("KTX file uses an unsupported image format");
  }

  // Mipmaps
  uint32_t width = image->width;
  uint32_t height = image->height;
  for (uint32_t i = 0; i < image->levels; i++) {
    uint64_t offset = header->levels[i].byteOffset;
    uint64_t size = header->levels[i].byteLength;
    size_t stride = size / image->layers;
    lovrAssert(offset + size <= blob->size, "KTX file overflow");
    lovrAssert(measure(width, height, image->format) == size, "KTX size mismatch");
    image->mipmaps[i] = (Mipmap) { data + offset, stride, stride };
    width = MAX(width >> 1, 1);
    height = MAX(height >> 1, 1);
  }

  return image;
}

static Image* loadSTB(Blob* blob) {
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
      default: lovrThrow("Unsupported channel count for 16 bit image: %d", channels);
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
    return NULL;
  }

  size_t size = measure(width, height, format);

  Image* image = calloc(1, sizeof(Image));
  lovrAssert(image, "Out of memory");
  image->ref = 1;
  image->flags = flags;
  image->width = width;
  image->height = height;
  image->format = format;
  image->layers = 1;
  image->levels = 1;
  image->blob = lovrBlobCreate(data, size, blob->name);
  image->mipmaps[0] = (Mipmap) { data, size, 0 };
  return image;
}
