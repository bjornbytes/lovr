#include "loaders/texture.h"
#include "vendor/stb/stb_image.h"
#include <stdlib.h>
#include <string.h>

TextureData* lovrTextureDataGetBlank(int width, int height, uint8_t value) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  int channels = 4;
  int size = sizeof(uint8_t) * width * height * channels;
  uint8_t* data = malloc(size);
  memset(data, value, size);

  textureData->data = data;
  textureData->width = width;
  textureData->height = height;
  textureData->channels = channels;

  return textureData;
}

TextureData* lovrTextureDataGetEmpty(int width, int height) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  textureData->data = NULL;
  textureData->width = width;
  textureData->height = height;
  textureData->channels = 4;

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
    return textureData;
  }

  free(textureData);
  return NULL;
}

TextureData* lovrTextureDataFromOpenVRModel(OpenVRModel* vrModel) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  RenderModel_TextureMap_t* texture = vrModel->texture;
  textureData->width = texture->unWidth;
  textureData->height = texture->unHeight;
  textureData->data = texture->rubTextureMapData;
  return textureData;
}
