#include "loaders/texture.h"
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

TextureData* lovrTextureDataGetBlank(int width, int height, uint8_t value, TextureFormat format) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  size_t size = width * height * format.blockBytes;
  textureData->width = width;
  textureData->height = height;
  textureData->format = format;
  textureData->data = malloc(size);
  memset(textureData->data, value, size);
  return textureData;
}

TextureData* lovrTextureDataGetEmpty(int width, int height, TextureFormat format) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  textureData->width = width;
  textureData->height = height;
  textureData->format = format;
  textureData->data = NULL;
  return textureData;
}

TextureData* lovrTextureDataFromBlob(Blob* blob) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  stbi_set_flip_vertically_on_load(0);
  textureData->format = FORMAT_RGBA;
  textureData->data = stbi_load_from_memory(blob->data, blob->size, &textureData->width, &textureData->height, NULL, 4);

  if (!textureData->data) {
    error("Could not load texture data from '%s'", blob->name);
    free(textureData);
    return NULL;
  }

  return textureData;
}

void lovrTextureDataResize(TextureData* textureData, int width, int height, uint8_t value) {
  int size = width * height * textureData->format.blockBytes;
  textureData->width = width;
  textureData->height = height;
  textureData->data = realloc(textureData->data, size);
  memset(textureData->data, value, size);
}

void lovrTextureDataDestroy(TextureData* textureData) {
  free(textureData->data);
  free(textureData);
}
