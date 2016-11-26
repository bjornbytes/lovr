#include "loaders/texture.h"
#include "util.h"
#include <stdlib.h>

TextureData* lovrTextureDataFromFile(void* data, int size) {
  TextureData* textureData = malloc(sizeof(TextureData));
  if (!textureData) return NULL;

  void* image = loadImage(data, size, &textureData->width, &textureData->height, &textureData->channels, 4);

  if (image) {
    textureData->data = image;
    return textureData;
  } else {
    free(textureData);
    return NULL;
  }
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
