#include "filesystem/blob.h"
#include "lib/glfw.h"
#include <stdint.h>
#include <stdbool.h>

#pragma once

// WEBGL_compressed_texture_s3tc_srgb isn't ratified yet...
#ifdef EMSCRIPTEN
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif

typedef struct {
  struct {
    GLenum linear;
    GLenum srgb;
  } glInternalFormat;
  GLenum glFormat;
  int blockBytes;
  bool compressed;
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
    bool generated;
  } mipmaps;
  Blob* blob;
} TextureData;

TextureData* lovrTextureDataGetBlank(int width, int height, uint8_t value, TextureFormat format);
TextureData* lovrTextureDataGetEmpty(int width, int height, TextureFormat format);
TextureData* lovrTextureDataFromBlob(Blob* blob);
void lovrTextureDataResize(TextureData* textureData, int width, int height, uint8_t value);
void lovrTextureDataDestroy(TextureData* textureData);
