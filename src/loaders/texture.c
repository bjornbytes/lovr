#include "loaders/texture.h"
#include "lib/stb/stb_image.h"
#include <stdlib.h>
#include <string.h>

TextureData* lovrTextureDataGetBlank(int width, int height, uint8_t value, TextureFormat format) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  int channels = 0;
  switch (format) {
    case FORMAT_RED: channels = 1; break;
    case FORMAT_RG: channels = 2; break;
    case FORMAT_RGB: channels = 3; break;
    case FORMAT_RGBA: channels = 4; break;
  }

  int size = sizeof(uint8_t) * width * height * channels;
  uint8_t* data = malloc(size);
  memset(data, value, size);

  textureData->data = data;
  textureData->width = width;
  textureData->height = height;
  textureData->channels = channels;
  textureData->format = format;

  return textureData;
}

TextureData* lovrTextureDataGetEmpty(int width, int height, TextureFormat format) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  int channels = 0;
  switch (format) {
    case FORMAT_RED: channels = 1; break;
    case FORMAT_RG: channels = 2; break;
    case FORMAT_RGB: channels = 3; break;
    case FORMAT_RGBA: channels = 4; break;
  }

  textureData->data = NULL;
  textureData->width = width;
  textureData->height = height;
  textureData->channels = channels;
  textureData->format = format;

  return textureData;
}

TextureData* lovrTextureDataFromFile(void* data, int size) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  int* w = &textureData->width;
  int* h = &textureData->height;
  int* c = &textureData->channels;
  stbi_set_flip_vertically_on_load(1);
  void* image = stbi_load_from_memory(data, size, w, h, c, 4);

  if (image) {
    textureData->data = image;
    textureData->format = FORMAT_RGBA;
    return textureData;
  }

  free(textureData);
  return NULL;
}

void lovrTextureDataResize(TextureData* textureData, int width, int height, uint8_t value) {
  int size = sizeof(uint8_t) * width * height * textureData->channels;
  textureData->width = width;
  textureData->height = height;
  textureData->data = realloc(textureData->data, size);
  memset(textureData->data, value, size);
}

void lovrTextureDataDestroy(TextureData* textureData) {
  free(textureData->data);
  free(textureData);
}
