#include "filesystem/blob.h"
#include "lib/glfw.h"
#include <stdint.h>

#pragma once

typedef enum {
  FORMAT_RGB,
  FORMAT_RGBA
} TextureFormat;

typedef struct {
  GLenum internalFormat;
  GLenum format;
  int channels;
} TextureFormatInfo;

extern TextureFormatInfo lovrTextureFormats[];

typedef struct {
  int width;
  int height;
  TextureFormat format;
  void* data;
} TextureData;

TextureData* lovrTextureDataGetBlank(int width, int height, uint8_t value, TextureFormat format);
TextureData* lovrTextureDataGetEmpty(int width, int height, TextureFormat format);
TextureData* lovrTextureDataFromBlob(Blob* blob);
void lovrTextureDataResize(TextureData* textureData, int width, int height, uint8_t value);
void lovrTextureDataDestroy(TextureData* textureData);
