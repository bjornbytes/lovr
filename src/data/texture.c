#include "data/texture.h"
#include "math/math.h"
#include "lib/dds.h"
#include "lib/stb/stb_image.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define FOUR_CC(a, b, c, d) ((uint32_t) (((d)<<24) | ((c)<<16) | ((b)<<8) | (a)))

// Modified from ddsparse (https://bitbucket.org/slime73/ddsparse)
static int parseDDS(uint8_t* data, size_t size, TextureData* textureData) {
  if (size < sizeof(uint32_t) + sizeof(DDSHeader) || *(uint32_t*) data != FOUR_CC('D', 'D', 'S', ' ')) {
    return 1;
  }

  // Header
  size_t offset = sizeof(uint32_t);
  DDSHeader* header = (DDSHeader*) (data + offset);
  offset += sizeof(DDSHeader);
  if (header->size != sizeof(DDSHeader) || header->format.size != sizeof(DDSPixelFormat)) {
    return 1;
  }

  // DX10 header
  if ((header->format.flags & DDPF_FOURCC) && (header->format.fourCC == FOUR_CC('D', 'X', '1', '0'))) {
    if (size < (sizeof(uint32_t) + sizeof(DDSHeader) + sizeof(DDSHeader10))) {
      return 1;
    }

    DDSHeader10* header10 = (DDSHeader10*) (data + offset);
    offset += sizeof(DDSHeader10);

    // Only accept 2D textures
    D3D10ResourceDimension dimension = header10->resourceDimension;
    if (dimension != D3D10_RESOURCE_DIMENSION_TEXTURE2D && dimension != D3D10_RESOURCE_DIMENSION_UNKNOWN) {
      return 1;
    }

    // Can't deal with texture arrays and cubemaps.
    if (header10->arraySize > 1) {
      return 1;
    }

    // Ensure DXT 1/3/5
    switch (header10->dxgiFormat) {
      case DXGI_FORMAT_BC1_TYPELESS:
      case DXGI_FORMAT_BC1_UNORM:
      case DXGI_FORMAT_BC1_UNORM_SRGB:
        textureData->format = FORMAT_DXT1;
        break;
      case DXGI_FORMAT_BC2_TYPELESS:
      case DXGI_FORMAT_BC2_UNORM:
      case DXGI_FORMAT_BC2_UNORM_SRGB:
        textureData->format = FORMAT_DXT3;
        break;
      case DXGI_FORMAT_BC3_TYPELESS:
      case DXGI_FORMAT_BC3_UNORM:
      case DXGI_FORMAT_BC3_UNORM_SRGB:
        textureData->format = FORMAT_DXT5;
        break;
      default:
        return 1;
    }
  } else {
    if ((header->format.flags & DDPF_FOURCC) == 0) {
      return 1;
    }

    // Ensure DXT 1/3/5
    switch (header->format.fourCC) {
      case FOUR_CC('D', 'X', 'T', '1'): textureData->format = FORMAT_DXT1; break;
      case FOUR_CC('D', 'X', 'T', '3'): textureData->format = FORMAT_DXT3; break;
      case FOUR_CC('D', 'X', 'T', '5'): textureData->format = FORMAT_DXT5; break;
      default: return 1;
    }
  }

  int width = textureData->width = header->width;
  int height = textureData->height = header->height;
  int mipmapCount = header->mipMapCount;
  int blockBytes = 0;

  switch (textureData->format) {
    case FORMAT_DXT1: blockBytes = 8; break;
    case FORMAT_DXT3: blockBytes = 16; break;
    case FORMAT_DXT5: blockBytes = 16; break;
    default: break;
  }

  // Load mipmaps
  for (int i = 0; i < mipmapCount; i++) {
    size_t numBlocksWide = width ? MAX(1, (width + 3) / 4) : 0;
    size_t numBlocksHigh = height ? MAX(1, (height + 3) / 4) : 0;
    size_t mipmapSize = numBlocksWide * numBlocksHigh * blockBytes;

    // Overflow check
    if (mipmapSize == 0 || (offset + mipmapSize) > size) {
      vec_deinit(&textureData->mipmaps);
      return 1;
    }

    Mipmap mipmap = { .width = width, .height = height, .data = &data[offset], .size = mipmapSize };
    vec_push(&textureData->mipmaps, mipmap);
    offset += mipmapSize;
    width = MAX(width >> 1, 1);
    height = MAX(height >> 1, 1);
  }

  textureData->data = NULL;

  return 0;
}

TextureData* lovrTextureDataGetBlank(int width, int height, uint8_t value, TextureFormat format) {
  TextureData* textureData = lovrAlloc(sizeof(TextureData), lovrTextureDataDestroy);
  if (!textureData) return NULL;

  size_t pixelSize = 0;
  switch (format) {
    case FORMAT_RGB: pixelSize = 3; break;
    case FORMAT_RGBA: pixelSize = 4; break;
    case FORMAT_RGBA16F: pixelSize = 8; break;
    case FORMAT_RGBA32F: pixelSize = 16; break;
    case FORMAT_RG11B10F: pixelSize = 4; break;
    default: lovrThrow("Unable to create a blank compressed texture");
  }

  size_t size = width * height * pixelSize;
  textureData->width = width;
  textureData->height = height;
  textureData->format = format;
  textureData->data = memset(malloc(size), value, size);
  textureData->blob = NULL;
  vec_init(&textureData->mipmaps);
  textureData->generateMipmaps = false;
  return textureData;
}

TextureData* lovrTextureDataGetEmpty(int width, int height, TextureFormat format) {
  TextureData* textureData = lovrAlloc(sizeof(TextureData), lovrTextureDataDestroy);
  if (!textureData) return NULL;

  textureData->width = width;
  textureData->height = height;
  textureData->format = format;
  textureData->data = NULL;
  textureData->blob = NULL;
  vec_init(&textureData->mipmaps);
  textureData->generateMipmaps = false;
  return textureData;
}

TextureData* lovrTextureDataFromBlob(Blob* blob) {
  TextureData* textureData = lovrAlloc(sizeof(TextureData), lovrTextureDataDestroy);
  if (!textureData) return NULL;

  vec_init(&textureData->mipmaps);

  if (!parseDDS(blob->data, blob->size, textureData)) {
    textureData->blob = blob;
    lovrRetain(&blob->ref);
    return textureData;
  }

  stbi_set_flip_vertically_on_load(1);
  textureData->format = FORMAT_RGBA;
  textureData->data = stbi_load_from_memory(blob->data, blob->size, &textureData->width, &textureData->height, NULL, 4);
  textureData->blob = NULL;
  textureData->generateMipmaps = true;

  if (!textureData->data) {
    lovrThrow("Could not load texture data from '%s'", blob->name);
    free(textureData);
    return NULL;
  }

  return textureData;
}

void lovrTextureDataDestroy(const Ref* ref) {
  TextureData* textureData = containerof(ref, TextureData);
  if (textureData->blob) {
    lovrRelease(&textureData->blob->ref);
  }
  vec_deinit(&textureData->mipmaps);
  free(textureData->data);
  free(textureData);
}
