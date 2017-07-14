#include "loaders/texture.h"
#include "lib/stb/stb_image.h"
#include <stdlib.h>
#include <string.h>

TextureFormatInfo lovrTextureFormats[] = {
  {
    .internalFormat = GL_RGB,
    .format = GL_RGB,
    .channels = 3
  },
  {
    .internalFormat = GL_RGBA,
    .format = GL_RGBA,
    .channels = 4
  }
};

TextureData* lovrTextureDataGetBlank(int width, int height, uint8_t value, TextureFormat format) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  size_t size = width * height * lovrTextureFormats[format].channels * sizeof(uint8_t);
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
  int size = sizeof(uint8_t) * width * height * lovrTextureFormats[textureData->format].channels;
  textureData->width = width;
  textureData->height = height;
  textureData->data = realloc(textureData->data, size);
  memset(textureData->data, value, size);
}

void lovrTextureDataDestroy(TextureData* textureData) {
  free(textureData->data);
  free(textureData);
}
