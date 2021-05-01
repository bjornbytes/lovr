#include "data/image.h"
#include "data/blob.h"
#include "lib/stb/stb_image.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define FOUR_CC(a, b, c, d) ((uint32_t) (((d)<<24) | ((c)<<16) | ((b)<<8) | (a)))

static size_t getPixelSize(TextureFormat format) {
  switch (format) {
    case FORMAT_R8: return 1;
    case FORMAT_RG8: return 2;
    case FORMAT_RGBA8: return 4;
    case FORMAT_R16: return 2;
    case FORMAT_RG16: return 4;
    case FORMAT_RGBA16: return 8;
    case FORMAT_R16F: return 2;
    case FORMAT_RG16F: return 4;
    case FORMAT_RGBA16F: return 8;
    case FORMAT_R32F: return 4;
    case FORMAT_RG32F: return 8;
    case FORMAT_RGBA32F: return 16;
    case FORMAT_RGB565: return 2;
    case FORMAT_RGB5A1: return 2;
    case FORMAT_RGB10A2: return 4;
    case FORMAT_RG11B10F: return 4;
    case FORMAT_D16: return 2;
    case FORMAT_D24S8: return 4;
    case FORMAT_D32F: return 4;
    default: return 0;
  }
}

/*
// Modified from ddsparse (https://bitbucket.org/slime73/ddsparse)
static bool parseDDS(uint8_t* data, size_t size, Image* image) {
  enum {
    DDPF_ALPHAPIXELS = 0x000001,
    DDPF_ALPHA       = 0x000002,
    DDPF_FOURCC      = 0x000004,
    DDPF_RGB         = 0x000040,
    DDPF_YUV         = 0x000200,
    DDPF_LUMINANCE   = 0x020000
  };

  typedef enum D3D10ResourceDimension {
    D3D10_RESOURCE_DIMENSION_UNKNOWN   = 0,
    D3D10_RESOURCE_DIMENSION_BUFFER    = 1,
    D3D10_RESOURCE_DIMENSION_TEXTURE1D = 2,
    D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3,
    D3D10_RESOURCE_DIMENSION_TEXTURE3D = 4
  } D3D10ResourceDimension;

  typedef enum DXGIFormat {
    DXGI_FORMAT_UNKNOWN                     = 0,

    DXGI_FORMAT_R32G32B32A32_TYPELESS       = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT          = 2,
    DXGI_FORMAT_R32G32B32A32_UINT           = 3,
    DXGI_FORMAT_R32G32B32A32_SINT           = 4,

    DXGI_FORMAT_R32G32B32_TYPELESS          = 5,
    DXGI_FORMAT_R32G32B32_FLOAT             = 6,
    DXGI_FORMAT_R32G32B32_UINT              = 7,
    DXGI_FORMAT_R32G32B32_SINT              = 8,

    DXGI_FORMAT_R16G16B16A16_TYPELESS       = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT          = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM          = 11,
    DXGI_FORMAT_R16G16B16A16_UINT           = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM          = 13,
    DXGI_FORMAT_R16G16B16A16_SINT           = 14,

    DXGI_FORMAT_R32G32_TYPELESS             = 15,
    DXGI_FORMAT_R32G32_FLOAT                = 16,
    DXGI_FORMAT_R32G32_UINT                 = 17,
    DXGI_FORMAT_R32G32_SINT                 = 18,

    DXGI_FORMAT_R32G8X24_TYPELESS           = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,

    DXGI_FORMAT_R10G10B10A2_TYPELESS        = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM           = 24,
    DXGI_FORMAT_R10G10B10A2_UINT            = 25,

    DXGI_FORMAT_R11G11B10_FLOAT             = 26,

    DXGI_FORMAT_R8G8B8A8_TYPELESS           = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM              = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB         = 29,
    DXGI_FORMAT_R8G8B8A8_UINT               = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM              = 31,
    DXGI_FORMAT_R8G8B8A8_SINT               = 32,

    DXGI_FORMAT_R16G16_TYPELESS             = 33,
    DXGI_FORMAT_R16G16_FLOAT                = 34,
    DXGI_FORMAT_R16G16_UNORM                = 35,
    DXGI_FORMAT_R16G16_UINT                 = 36,
    DXGI_FORMAT_R16G16_SNORM                = 37,
    DXGI_FORMAT_R16G16_SINT                 = 38,

    DXGI_FORMAT_R32_TYPELESS                = 39,
    DXGI_FORMAT_D32_FLOAT                   = 40,
    DXGI_FORMAT_R32_FLOAT                   = 41,
    DXGI_FORMAT_R32_UINT                    = 42,
    DXGI_FORMAT_R32_SINT                    = 43,

    DXGI_FORMAT_R24G8_TYPELESS              = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT           = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS       = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT        = 47,

    DXGI_FORMAT_R8G8_TYPELESS               = 48,
    DXGI_FORMAT_R8G8_UNORM                  = 49,
    DXGI_FORMAT_R8G8_UINT                   = 50,
    DXGI_FORMAT_R8G8_SNORM                  = 51,
    DXGI_FORMAT_R8G8_SINT                   = 52,

    DXGI_FORMAT_R16_TYPELESS                = 53,
    DXGI_FORMAT_R16_FLOAT                   = 54,
    DXGI_FORMAT_D16_UNORM                   = 55,
    DXGI_FORMAT_R16_UNORM                   = 56,
    DXGI_FORMAT_R16_UINT                    = 57,
    DXGI_FORMAT_R16_SNORM                   = 58,
    DXGI_FORMAT_R16_SINT                    = 59,

    DXGI_FORMAT_R8_TYPELESS                 = 60,
    DXGI_FORMAT_R8_UNORM                    = 61,
    DXGI_FORMAT_R8_UINT                     = 62,
    DXGI_FORMAT_R8_SNORM                    = 63,
    DXGI_FORMAT_R8_SINT                     = 64,
    DXGI_FORMAT_A8_UNORM                    = 65,

    DXGI_FORMAT_R1_UNORM                    = 66,

    DXGI_FORMAT_R9G9B9E5_SHAREDEXP          = 67,

    DXGI_FORMAT_R8G8_B8G8_UNORM             = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM             = 69,

    DXGI_FORMAT_BC1_TYPELESS                = 70,
    DXGI_FORMAT_BC1_UNORM                   = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB              = 72,

    DXGI_FORMAT_BC2_TYPELESS                = 73,
    DXGI_FORMAT_BC2_UNORM                   = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB              = 75,

    DXGI_FORMAT_BC3_TYPELESS                = 76,
    DXGI_FORMAT_BC3_UNORM                   = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB              = 78,

    DXGI_FORMAT_BC4_TYPELESS                = 79,
    DXGI_FORMAT_BC4_UNORM                   = 80,
    DXGI_FORMAT_BC4_SNORM                   = 81,

    DXGI_FORMAT_BC5_TYPELESS                = 82,
    DXGI_FORMAT_BC5_UNORM                   = 83,
    DXGI_FORMAT_BC5_SNORM                   = 84,

    DXGI_FORMAT_B5G6R5_UNORM                = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM              = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM              = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM              = 88,

    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS           = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB         = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS           = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB         = 93,

    DXGI_FORMAT_BC6H_TYPELESS               = 94,
    DXGI_FORMAT_BC6H_UF16                   = 95,
    DXGI_FORMAT_BC6H_SF16                   = 96,

    DXGI_FORMAT_BC7_TYPELESS                = 97,
    DXGI_FORMAT_BC7_UNORM                   = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB              = 99
  } DXGIFormat;

  typedef struct DDSPixelFormat {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
  } DDSPixelFormat;

  typedef struct DDSHeader {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved[11];

    DDSPixelFormat format;

    uint32_t caps1;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
  } DDSHeader;

  typedef struct DDSHeader10 {
    DXGIFormat dxgiFormat;
    D3D10ResourceDimension resourceDimension;

    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t reserved;
  } DDSHeader10;

  if (size < sizeof(uint32_t) + sizeof(DDSHeader) || *(uint32_t*) data != FOUR_CC('D', 'D', 'S', ' ')) {
    return false;
  }

  // Header
  size_t offset = sizeof(uint32_t);
  DDSHeader* header = (DDSHeader*) (data + offset);
  offset += sizeof(DDSHeader);
  if (header->size != sizeof(DDSHeader) || header->format.size != sizeof(DDSPixelFormat)) {
    return false;
  }

  // DX10 header
  if ((header->format.flags & DDPF_FOURCC) && (header->format.fourCC == FOUR_CC('D', 'X', '1', '0'))) {
    if (size < (sizeof(uint32_t) + sizeof(DDSHeader) + sizeof(DDSHeader10))) {
      return false;
    }

    DDSHeader10* header10 = (DDSHeader10*) (data + offset);
    offset += sizeof(DDSHeader10);

    // Only accept 2D textures
    D3D10ResourceDimension dimension = header10->resourceDimension;
    if (dimension != D3D10_RESOURCE_DIMENSION_TEXTURE2D && dimension != D3D10_RESOURCE_DIMENSION_UNKNOWN) {
      return false;
    }

    // Can't deal with texture arrays and cubemaps.
    if (header10->arraySize > 1) {
      return false;
    }

    // Ensure DXT 1/3/5
    switch (header10->dxgiFormat) {
      case DXGI_FORMAT_BC1_TYPELESS:
      case DXGI_FORMAT_BC1_UNORM:
      case DXGI_FORMAT_BC1_UNORM_SRGB:
        image->format = FORMAT_DXT1;
        break;
      case DXGI_FORMAT_BC2_TYPELESS:
      case DXGI_FORMAT_BC2_UNORM:
      case DXGI_FORMAT_BC2_UNORM_SRGB:
        image->format = FORMAT_DXT3;
        break;
      case DXGI_FORMAT_BC3_TYPELESS:
      case DXGI_FORMAT_BC3_UNORM:
      case DXGI_FORMAT_BC3_UNORM_SRGB:
        image->format = FORMAT_DXT5;
        break;
      default:
        return 1;
    }
  } else {
    if ((header->format.flags & DDPF_FOURCC) == 0) {
      return false;
    }

    // Ensure DXT 1/3/5
    switch (header->format.fourCC) {
      case FOUR_CC('D', 'X', 'T', '1'): image->format = FORMAT_DXT1; break;
      case FOUR_CC('D', 'X', 'T', '3'): image->format = FORMAT_DXT3; break;
      case FOUR_CC('D', 'X', 'T', '5'): image->format = FORMAT_DXT5; break;
      default: return false;
    }
  }

  uint32_t width = image->width = header->width;
  uint32_t height = image->height = header->height;
  uint32_t mipmapCount = image->mipmapCount = MAX(header->mipMapCount, 1);
  image->mipmaps = malloc(mipmapCount * sizeof(Mipmap));
  size_t blockBytes = 0;

  switch (image->format) {
    case FORMAT_DXT1: blockBytes = 8; break;
    case FORMAT_DXT3: blockBytes = 16; break;
    case FORMAT_DXT5: blockBytes = 16; break;
    default: break;
  }

  // Load mipmaps
  for (uint32_t i = 0; i < mipmapCount; i++) {
    uint32_t numBlocksWide = width ? MAX(1, (width + 3) / 4) : 0;
    uint32_t numBlocksHigh = height ? MAX(1, (height + 3) / 4) : 0;
    size_t mipmapSize = numBlocksWide * numBlocksHigh * blockBytes;

    // Overflow check
    if (mipmapSize == 0 || (offset + mipmapSize) > size) {
      free(image->mipmaps);
      return false;
    }

    image->mipmaps[i] = (Mipmap) { .width = width, .height = height, .data = &data[offset], .size = mipmapSize };
    offset += mipmapSize;
    width = MAX(width >> 1, 1);
    height = MAX(height >> 1, 1);
  }

  image->blob->data = NULL;

  return true;
}
*/

static bool parseKTX(uint8_t* bytes, size_t size, Image* image) {
  typedef struct {
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
  } KTXHeader;

  union {
    uint8_t* u8;
    uint32_t* u32;
    KTXHeader* ktx;
  } data = { .u8 = bytes };

  uint8_t magic[] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };

  if (size < sizeof(magic) || memcmp(data.ktx->magic, magic, sizeof(magic))) {
    return false;
  }

  if (data.ktx->endianness != 0x04030201 || data.ktx->numberOfArrayElements > 0 || data.ktx->numberOfFaces > 1 || data.ktx->pixelDepth > 1) {
    return false;
  }

  switch (data.ktx->glInternalFormat) {
    case 0x93B0: case 0x93D0: image->format = FORMAT_ASTC_4x4; break;
    case 0x93B1: case 0x93D1: image->format = FORMAT_ASTC_5x4; break;
    case 0x93B2: case 0x93D2: image->format = FORMAT_ASTC_5x5; break;
    case 0x93B3: case 0x93D3: image->format = FORMAT_ASTC_6x5; break;
    case 0x93B4: case 0x93D4: image->format = FORMAT_ASTC_6x6; break;
    case 0x93B5: case 0x93D5: image->format = FORMAT_ASTC_8x5; break;
    case 0x93B6: case 0x93D6: image->format = FORMAT_ASTC_8x6; break;
    case 0x93B7: case 0x93D7: image->format = FORMAT_ASTC_8x8; break;
    case 0x93B8: case 0x93D8: image->format = FORMAT_ASTC_10x5; break;
    case 0x93B9: case 0x93D9: image->format = FORMAT_ASTC_10x6; break;
    case 0x93BA: case 0x93DA: image->format = FORMAT_ASTC_10x8; break;
    case 0x93BB: case 0x93DB: image->format = FORMAT_ASTC_10x10; break;
    case 0x93BC: case 0x93DC: image->format = FORMAT_ASTC_12x10; break;
    case 0x93BD: case 0x93DD: image->format = FORMAT_ASTC_12x12; break;
    default: lovrThrow("Unsupported KTX format '%d' (please open an issue)", data.ktx->glInternalFormat);
  }

  uint32_t width = image->width = data.ktx->pixelWidth;
  uint32_t height = image->height = data.ktx->pixelHeight;
  uint32_t mipmapCount = image->mipmapCount = data.ktx->numberOfMipmapLevels;
  image->mipmaps = malloc(mipmapCount * sizeof(Mipmap));

  data.u8 += sizeof(KTXHeader) + data.ktx->bytesOfKeyValueData;
  for (uint32_t i = 0; i < mipmapCount; i++) {
    image->mipmaps[i] = (Mipmap) { .width = width, .height = height, .data = data.u8 + sizeof(uint32_t), .size = *data.u32 };
    width = MAX(width >> 1, 1u);
    height = MAX(height >> 1, 1u);
    data.u8 = (uint8_t*) ALIGN(data.u8 + sizeof(uint32_t) + *data.u32, 4);
  }

  return true;
}

static bool parseASTC(uint8_t* bytes, size_t size, Image* image) {
  typedef struct {
    uint32_t magic;
    uint8_t blockX;
    uint8_t blockY;
    uint8_t blockZ;
    uint8_t width[3];
    uint8_t height[3];
    uint8_t depth[3];
  } ASTCHeader;

  union {
    uint8_t* u8;
    uint32_t* u32;
    ASTCHeader* astc;
  } data = { .u8 = bytes };

  uint32_t magic = 0x5ca1ab13;

  if (size <= sizeof(*data.astc) || data.astc->magic != magic) {
    return false;
  }

  uint32_t bx = data.astc->blockX, by = data.astc->blockY, bz = data.astc->blockZ;
  if (bx == 4 && by == 4 && bz == 1) { image->format = FORMAT_ASTC_4x4; }
  else if (bx == 5 && by == 4 && bz == 1) { image->format = FORMAT_ASTC_5x4; }
  else if (bx == 5 && by == 5 && bz == 1) { image->format = FORMAT_ASTC_5x5; }
  else if (bx == 6 && by == 5 && bz == 1) { image->format = FORMAT_ASTC_6x5; }
  else if (bx == 6 && by == 6 && bz == 1) { image->format = FORMAT_ASTC_6x6; }
  else if (bx == 8 && by == 5 && bz == 1) { image->format = FORMAT_ASTC_8x5; }
  else if (bx == 8 && by == 6 && bz == 1) { image->format = FORMAT_ASTC_8x6; }
  else if (bx == 8 && by == 8 && bz == 1) { image->format = FORMAT_ASTC_8x8; }
  else if (bx == 10 && by == 5 && bz == 1) { image->format = FORMAT_ASTC_10x5; }
  else if (bx == 10 && by == 6 && bz == 1) { image->format = FORMAT_ASTC_10x6; }
  else if (bx == 10 && by == 8 && bz == 1) { image->format = FORMAT_ASTC_10x8; }
  else if (bx == 10 && by == 10 && bz == 1) { image->format = FORMAT_ASTC_10x10; }
  else if (bx == 12 && by == 10 && bz == 1) { image->format = FORMAT_ASTC_12x10; }
  else if (bx == 12 && by == 12 && bz == 1) { image->format = FORMAT_ASTC_12x12; }
  else { lovrThrow("Unsupported ASTC format %dx%dx%d", bx, by, bz); }

  image->width = data.astc->width[0] + (data.astc->width[1] << 8) + (data.astc->width[2] << 16);
  image->height = data.astc->height[0] + (data.astc->height[1] << 8) + (data.astc->height[2] << 16);

  size_t imageSize = ((image->width + bx - 1) / bx) * ((image->height + by - 1) / by) * (128 / 8);

  if (imageSize > size - sizeof(ASTCHeader)) {
    return false;
  }

  image->mipmapCount = 1;
  image->mipmaps = malloc(sizeof(Mipmap));
  image->mipmaps[0] = (Mipmap) {
    .width = image->width,
    .height = image->height,
    .data = data.u8 + sizeof(ASTCHeader),
    .size = imageSize
  };

  return true;
}

Image* lovrImageCreate(uint32_t width, uint32_t height, Blob* contents, uint8_t value, TextureFormat format) {
  Image* image = calloc(1, sizeof(Image));
  lovrAssert(image, "Out of memory");
  image->ref = 1;
  size_t pixelSize = getPixelSize(format);
  size_t size = width * height * pixelSize;
  lovrAssert(width > 0 && height > 0, "Image dimensions must be positive");
  lovrAssert(format < FORMAT_BC6, "Blank images cannot be compressed");
  lovrAssert(!contents || contents->size >= size, "Image Blob is too small (%d bytes needed, got %d)", size, contents->size);
  image->width = width;
  image->height = height;
  image->format = format;
  void* data = malloc(size);
  lovrAssert(data, "Out of memory");
  if (contents) {
    memcpy(data, contents->data, size);
  } else {
    memset(data, value, size);
  }
  image->blob = lovrBlobCreate(data, size, "Image");
  return image;
}

Image* lovrImageCreateFromBlob(Blob* blob, bool flip) {
  Image* image = calloc(1, sizeof(Image));
  lovrAssert(image, "Out of memory");
  image->ref = 1;
  image->blob = lovrBlobCreate(NULL, 0, NULL);
  /*if (parseDDS(blob->data, blob->size, image)) {
    image->source = blob;
    lovrRetain(blob);
    return image;
  } else */if (parseKTX(blob->data, blob->size, image)) {
    image->source = blob;
    lovrRetain(blob);
    return image;
  } else if (parseASTC(blob->data, blob->size, image)) {
    image->source = blob;
    lovrRetain(blob);
    return image;
  }

  int width, height;
  int length = (int) blob->size;
  stbi_set_flip_vertically_on_load_thread(flip);
  if (stbi_is_16_bit_from_memory(blob->data, length)) {
    int channels;
    image->blob->data = stbi_load_16_from_memory(blob->data, length, &width, &height, &channels, 0);
    switch (channels) {
      case 1:
        image->format = FORMAT_R16;
        image->blob->size = 2 * width * height;
        break;
      case 2:
        image->format = FORMAT_RG16;
        image->blob->size = 4 * width * height;
        break;
      case 4:
        image->format = FORMAT_RGBA16;
        image->blob->size = 8 * width * height;
        break;
      default:
        lovrThrow("Unsupported channel count for 16 bit image: %d", channels);
    }
  } else if (stbi_is_hdr_from_memory(blob->data, length)) {
    image->format = FORMAT_RGBA32F;
    image->blob->data = stbi_loadf_from_memory(blob->data, length, &width, &height, NULL, 4);
    image->blob->size = 16 * width * height;
  } else {
    image->format = FORMAT_RGBA8;
    image->blob->data = stbi_load_from_memory(blob->data, length, &width, &height, NULL, 4);
    image->blob->size = 4 * width * height;
  }

  if (!image->blob->data) {
    lovrThrow("Could not load image from '%s'", blob->name);
    lovrRelease(image->blob, lovrBlobDestroy);
    free(image);
    return NULL;
  }

  image->width = width;
  image->height = height;
  image->mipmapCount = 0;
  return image;
}

void lovrImageDestroy(void* ref) {
  Image* image = ref;
  lovrRelease(image->source, lovrBlobDestroy);
  free(image->mipmaps);
  lovrRelease(image->blob, lovrBlobDestroy);
  free(image);
}

void lovrImageGetPixel(Image* image, uint32_t x, uint32_t y, float color[4]) {
  lovrAssert(image->blob->data, "Image does not have any pixel data");
  lovrAssert(x < image->width && y < image->height, "getPixel coordinates must be within Image bounds");
  size_t index = (image->height - (y + 1)) * image->width + x;
  size_t pixelSize = getPixelSize(image->format);
  uint8_t* u8 = (uint8_t*) image->blob->data + pixelSize * index;
  float* f32 = (float*) u8;
  switch (image->format) {
    case FORMAT_RGBA8:
      color[0] = u8[0] / 255.f;
      color[1] = u8[1] / 255.f;
      color[2] = u8[2] / 255.f;
      color[3] = u8[3] / 255.f;
      return;
    case FORMAT_R32F:
      color[0] = f32[0];
      color[1] = 1.f;
      color[2] = 1.f;
      color[3] = 1.f;
      return;
    case FORMAT_RG32F:
      color[0] = f32[0];
      color[1] = f32[1];
      color[2] = 1.f;
      color[3] = 1.f;
      return;
    case FORMAT_RGBA32F:
      color[0] = f32[0];
      color[1] = f32[1];
      color[2] = f32[2];
      color[3] = f32[3];
      return;
    default: lovrThrow("Unsupported format for Image:getPixel");
  }
}

void lovrImageSetPixel(Image* image, uint32_t x, uint32_t y, float color[4]) {
  lovrAssert(image->blob->data, "Image does not have any pixel data");
  lovrAssert(x < image->width && y < image->height, "setPixel coordinates must be within Image bounds");
  size_t index = (image->height - (y + 1)) * image->width + x;
  size_t pixelSize = getPixelSize(image->format);
  uint8_t* u8 = (uint8_t*) image->blob->data + pixelSize * index;
  float* f32 = (float*) u8;
  switch (image->format) {
    case FORMAT_RGBA8:
      u8[0] = (uint8_t) (color[0] * 255.f + .5f);
      u8[1] = (uint8_t) (color[1] * 255.f + .5f);
      u8[2] = (uint8_t) (color[2] * 255.f + .5f);
      u8[3] = (uint8_t) (color[3] * 255.f + .5f);
      break;

    case FORMAT_R32F:
      f32[0] = color[0];
      break;

    case FORMAT_RG32F:
      f32[0] = color[0];
      f32[1] = color[1];
      break;

    case FORMAT_RGBA32F:
      f32[0] = color[0];
      f32[1] = color[1];
      f32[2] = color[2];
      f32[3] = color[3];
      break;

    default: lovrThrow("Unsupported format for Image:setPixel");
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
  lovrAssert(image->format == FORMAT_RGBA8, "Only RGBA images can be encoded");
  uint32_t w = image->width;
  uint32_t h = image->height;
  uint8_t* pixels = (uint8_t*) image->blob->data + (h - 1) * w * 4;
  int32_t stride = -1 * (int) (w * 4);

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

void lovrImagePaste(Image* image, Image* source, uint32_t dx, uint32_t dy, uint32_t sx, uint32_t sy, uint32_t w, uint32_t h) {
  lovrAssert(image->format == source->format, "Currently Image must have the same format to paste");
  lovrAssert(image->format < FORMAT_BC6, "Compressed Image cannot be pasted");
  size_t pixelSize = getPixelSize(image->format);
  lovrAssert(dx + w <= image->width && dy + h <= image->height, "Attempt to paste outside of destination Image bounds");
  lovrAssert(sx + w <= source->width && sy + h <= source->height, "Attempt to paste from outside of source Image bounds");
  uint8_t* src = (uint8_t*) source->blob->data + ((source->height - 1 - sy) * source->width + sx) * pixelSize;
  uint8_t* dst = (uint8_t*) image->blob->data + ((image->height - 1 - dy) * image->width + sx) * pixelSize;
  for (uint32_t y = 0; y < h; y++) {
    memcpy(dst, src, w * pixelSize);
    src -= source->width * pixelSize;
    dst -= image->width * pixelSize;
  }
}
