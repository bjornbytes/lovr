#include "../glfw.h"

struct Buffer;

#ifndef LOVR_TEXTURE_TYPES
#define LOVR_TEXTURE_TYPES

typedef enum {
  FILTER_NEAREST = GL_NEAREST,
  FILTER_LINEAR = GL_LINEAR
} FilterMode;

typedef enum {
  WRAP_CLAMP = GL_CLAMP_TO_EDGE,
  WRAP_REPEAT = GL_REPEAT,
  WRAP_MIRRORED_REPEAT = GL_MIRRORED_REPEAT,
  WRAP_CLAMP_ZERO = GL_CLAMP_TO_BORDER
} WrapMode;

typedef struct {
  GLuint id;
  GLuint buffer;
  int width;
  int height;
  FilterMode filterMin;
  FilterMode filterMag;
  WrapMode wrapHorizontal;
  WrapMode wrapVertical;
} Texture;

#endif

Texture* lovrTextureCreate(void* data, int size);
Texture* lovrTextureCreateFromBuffer(struct Buffer* buffer);
void lovrTextureDestroy(Texture* texture);
void lovrTextureBind(Texture* texture);
void lovrTextureRefresh(Texture* texture);
int lovrTextureGetHeight(Texture* texture);
int lovrTextureGetWidth(Texture* texture);
void lovrTextureGetFilter(Texture* texture, FilterMode* min, FilterMode* mag);
void lovrTextureSetFilter(Texture* texture, FilterMode min, FilterMode mag);
void lovrTextureGetWrap(Texture* texture, WrapMode* horizontal, WrapMode* vertical);
void lovrTextureSetWrap(Texture* texture, WrapMode horizontal, WrapMode vertical);
