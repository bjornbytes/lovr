#include "loaders/texture.h"
#include "util.h"
#include <stdlib.h>

TextureData* lovrTextureDataGetEmpty() {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  uint8_t* pixel = malloc(4 * sizeof(uint8_t));
  pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0xff;

  textureData->width = 1;
  textureData->height = 1;
  textureData->channels = 4;
  textureData->data = pixel;

  return textureData;
}

TextureData* lovrTextureDataFromFile(void* data, int size) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  void* image = loadImage(data, size, &textureData->width, &textureData->height, &textureData->channels, 4);

  if (image) {
    textureData->data = image;
    return textureData;
  }

  free(textureData);
  return NULL;
}

#ifndef EMSCRIPTEN
TextureData* lovrTextureDataFromOpenVRModel(OpenVRModel* vrModel) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  RenderModel_TextureMap_t* texture = vrModel->texture;
  textureData->width = texture->unWidth;
  textureData->height = texture->unHeight;
  textureData->data = texture->rubTextureMapData;
  return textureData;
}
#endif
