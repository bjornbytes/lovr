#include "glfw.h"
#include "util.h"

struct Buffer;

#ifndef LOVR_TEXTURE_TYPES
#define LOVR_TEXTURE_TYPES

typedef struct {
  void* data;
  int width;
  int height;
  int channels;
} TextureData;

typedef enum {
  TEXTURE_IMAGE = GL_TEXTURE_2D
} TextureType;

typedef enum {
  FILTER_NEAREST = GL_NEAREST,
  FILTER_LINEAR = GL_LINEAR
} FilterMode;

typedef enum {
  WRAP_CLAMP = GL_CLAMP_TO_EDGE,
  WRAP_REPEAT = GL_REPEAT,
  WRAP_MIRRORED_REPEAT = GL_MIRRORED_REPEAT
} WrapMode;

typedef struct {
  Ref ref;
  TextureData* textureData;
  TextureType type;
  GLuint id;
  int width;
  int height;
  FilterMode filterMin;
  FilterMode filterMag;
  WrapMode wrapHorizontal;
  WrapMode wrapVertical;
  struct Buffer* buffer;
} Texture;

#endif

Texture* lovrTextureCreate(TextureData* textureData);
void lovrTextureDestroy(const Ref* ref);
void lovrTextureDataDestroy(TextureData* textureData);
void lovrTextureBind(Texture* texture);
int lovrTextureGetHeight(Texture* texture);
int lovrTextureGetWidth(Texture* texture);
void lovrTextureGetFilter(Texture* texture, FilterMode* min, FilterMode* mag);
void lovrTextureSetFilter(Texture* texture, FilterMode min, FilterMode mag);
void lovrTextureGetWrap(Texture* texture, WrapMode* horizontal, WrapMode* vertical);
void lovrTextureSetWrap(Texture* texture, WrapMode horizontal, WrapMode vertical);
