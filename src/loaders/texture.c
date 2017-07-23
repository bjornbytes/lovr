#include "loaders/texture.h"
#include "math/math.h"
#include "lib/dds.h"
#include "lib/stb/stb_image.h"
#include <stdlib.h>
#include <string.h>

const TextureFormat FORMAT_RGB = {
  .glInternalFormat = GL_RGB,
  .glFormat = GL_RGB,
  .compressed = 0,
  .blockBytes = 2
};

const TextureFormat FORMAT_RGBA = {
  .glInternalFormat = GL_RGBA,
  .glFormat = GL_RGBA,
  .compressed = 0,
  .blockBytes = 4
};

const TextureFormat FORMAT_DXT1 = {
  .glInternalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
  .glFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
  .compressed = 1,
  .blockBytes = 8
};

const TextureFormat FORMAT_DXT3 = {
  .glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
  .glFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
  .compressed = 1,
  .blockBytes = 16
};

const TextureFormat FORMAT_DXT5 = {
  .glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
  .glFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
  .compressed = 1,
  .blockBytes = 16
};

#define FOUR_CC(a, b, c, d) ((uint32_t) (((d)<<24) | ((c)<<16) | ((b)<<8) | (a)))

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

  // Load mipmaps
  vec_init(&textureData->mipmaps.list);
  for (int i = 0; i < mipmapCount; i++) {
    size_t numBlocksWide = width ? MAX(1, (width + 3) / 4) : 0;
    size_t numBlocksHigh = height ? MAX(1, (height + 3) / 4) : 0;
    size_t mipmapSize = numBlocksWide * numBlocksHigh * textureData->format.blockBytes;

    // Overflow check
    if (mipmapSize == 0 || (offset + mipmapSize) > size) {
      vec_deinit(&textureData->mipmaps.list);
      return 1;
    }

    Mipmap mipmap = { .width = width, .height = height, .data = &data[offset], .size = mipmapSize };
    vec_push(&textureData->mipmaps.list, mipmap);
    offset += mipmapSize;
    width = MAX(width >> 1, 1);
    height = MAX(height >> 1, 1);
  }

  textureData->data = NULL;

  return 0;
}

TextureData* lovrTextureDataGetBlank(int width, int height, uint8_t value, TextureFormat format) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  size_t size = width * height * format.blockBytes;
  textureData->width = width;
  textureData->height = height;
  textureData->format = format;
  textureData->data = memset(malloc(size), value, size);
  textureData->mipmaps.generated = 0;
  textureData->blob = NULL;
  return textureData;
}

TextureData* lovrTextureDataGetEmpty(int width, int height, TextureFormat format) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  textureData->width = width;
  textureData->height = height;
  textureData->format = format;
  textureData->data = NULL;
  textureData->mipmaps.generated = 0;
  textureData->blob = NULL;
  return textureData;
}

TextureData* lovrTextureDataFromBlob(Blob* blob) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  if (!parseDDS(blob->data, blob->size, textureData)) {
    textureData->blob = blob;
    lovrRetain(&blob->ref);
    return textureData;
  }

  stbi_set_flip_vertically_on_load(0);
  textureData->format = FORMAT_RGBA;
  textureData->data = stbi_load_from_memory(blob->data, blob->size, &textureData->width, &textureData->height, NULL, 4);
  textureData->mipmaps.generated = 1;
  textureData->blob = NULL;

  if (!textureData->data) {
    error("Could not load texture data from '%s'", blob->name);
    free(textureData);
    return NULL;
  }

  return textureData;
}

void lovrTextureDataResize(TextureData* textureData, int width, int height, uint8_t value) {
  if (textureData->format.compressed || textureData->mipmaps.generated) {
    error("Can't resize a compressed texture or a texture with generated mipmaps.");
  }

  int size = width * height * textureData->format.blockBytes;
  textureData->width = width;
  textureData->height = height;
  textureData->data = realloc(textureData->data, size);
  memset(textureData->data, value, size);
}

void lovrTextureDataDestroy(TextureData* textureData) {
  if (textureData->blob) {
    lovrRelease(&textureData->blob->ref);
  }
  if (textureData->format.compressed) {
    vec_deinit(&textureData->mipmaps.list);
  }
  free(textureData->data);
  free(textureData);
}
