#include "openvr.h"
#include <stdint.h>

#pragma once

typedef enum {
  FORMAT_RED,
  FORMAT_RG,
  FORMAT_RGB,
  FORMAT_RGBA
} TextureFormat;

typedef struct {
  void* data;
  int width;
  int height;
  int channels;
  TextureFormat format;
} TextureData;

TextureData* lovrTextureDataGetBlank(int width, int height, uint8_t value, TextureFormat format);
TextureData* lovrTextureDataGetEmpty(int width, int height, TextureFormat format);
TextureData* lovrTextureDataFromFile(void* data, int size);
TextureData* lovrTextureDataFromOpenVRModel(OpenVRModel* vrModel);
void lovrTextureDataResize(TextureData* textureData, int width, int height, uint8_t value);
void lovrTextureDataDestroy(TextureData* textureData);
