#include "loaders/texture.h"
#include "lib/glfw.h"
#include "util.h"

#pragma once

typedef enum {
  TEXTURE_2D = GL_TEXTURE_2D,
  TEXTURE_CUBE = GL_TEXTURE_CUBE_MAP
} TextureType;

typedef enum {
  FILTER_NEAREST,
  FILTER_BILINEAR,
  FILTER_TRILINEAR,
  FILTER_ANISOTROPIC
} FilterMode;

typedef struct {
  FilterMode mode;
  float anisotropy;
} TextureFilter;

typedef enum {
  WRAP_CLAMP = GL_CLAMP_TO_EDGE,
  WRAP_REPEAT = GL_REPEAT,
  WRAP_MIRRORED_REPEAT = GL_MIRRORED_REPEAT
} WrapMode;

typedef struct {
  WrapMode s;
  WrapMode t;
  WrapMode r;
} TextureWrap;

typedef enum {
  PROJECTION_ORTHOGRAPHIC,
  PROJECTION_PERSPECTIVE
} TextureProjection;

typedef struct {
  Ref ref;
  TextureType type;
  TextureData* slices[6];
  int sliceCount;
  int width;
  int height;
  GLuint id;
  GLuint msaaId;
  GLuint framebuffer;
  GLuint resolveFramebuffer;
  GLuint depthBuffer;
  TextureProjection projection;
  TextureFilter filter;
  TextureWrap wrap;
  int msaa;
} Texture;

GLenum lovrTextureGetGLFormat(TextureFormat format);

Texture* lovrTextureCreate(TextureType type, TextureData* data[6], int count);
Texture* lovrTextureCreateWithFramebuffer(TextureData* textureData, TextureProjection projection, int msaa);
void lovrTextureDestroy(const Ref* ref);
void lovrTextureBindFramebuffer(Texture* texture);
void lovrTextureResolveMSAA(Texture* texture);
void lovrTextureRefresh(Texture* texture);
TextureFilter lovrTextureGetFilter(Texture* texture);
void lovrTextureSetFilter(Texture* texture, TextureFilter filter);
TextureWrap lovrTextureGetWrap(Texture* texture);
void lovrTextureSetWrap(Texture* texture, TextureWrap wrap);
