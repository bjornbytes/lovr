#include "filesystem/blob.h"
#include "lib/glfw.h"
#include <stdint.h>

#pragma once

typedef struct {
  GLenum glInternalFormat;
  GLenum glFormat;
  int blockBytes;
  int compressed;
} TextureFormat;

extern const TextureFormat FORMAT_RGB, FORMAT_RGBA, FORMAT_DXT1, FORMAT_DXT3, FORMAT_DXT5;

typedef struct {
  int width;
  int height;
  void* data;
  size_t size;
} Mipmap;

typedef vec_t(Mipmap) vec_mipmap_t;

typedef struct {
  int width;
  int height;
  TextureFormat format;
  void* data;
  union MipmapType {
    vec_mipmap_t list;
    int generated;
  } mipmaps;
  Blob* blob;
} TextureData;

TextureData* lovrTextureDataGetBlank(int width, int height, uint8_t value, TextureFormat format);
TextureData* lovrTextureDataGetEmpty(int width, int height, TextureFormat format);
TextureData* lovrTextureDataFromBlob(Blob* blob);
void lovrTextureDataResize(TextureData* textureData, int width, int height, uint8_t value);
void lovrTextureDataDestroy(TextureData* textureData);
