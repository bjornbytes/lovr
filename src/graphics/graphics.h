#include "buffer.h"
#include "model.h"
#include "shader.h"
#include "../matrix.h"

#ifndef LOVR_GRAPHICS_TYPES
#define LOVR_GRAPHICS_TYPES
typedef vec_t(mat4) vec_mat4_t;

typedef struct {
  Shader* activeShader;
  Shader* defaultShader;
  vec_mat4_t transforms;
  mat4 lastTransform;
  mat4 projection;
  mat4 lastProjection;
  char colorMask;
} GraphicsState;
#endif

void lovrGraphicsInit();
void lovrGraphicsReset();
void lovrGraphicsClear(int color, int depth);
void lovrGraphicsPresent();
void lovrGraphicsPrepare();
void lovrGraphicsGetBackgroundColor(float* r, float* g, float* b, float* a);
void lovrGraphicsSetBackgroundColor(float r, float g, float b, float a);
void lovrGraphicsGetColorMask(char* r, char* g, char* b, char* a);
void lovrGraphicsSetColorMask(char r, char g, char b, char a);
Shader* lovrGraphicsGetShader();
void lovrGraphicsSetShader(Shader* shader);
void lovrGraphicsSetProjection(float near, float far, float fov);
void lovrGraphicsSetProjectionRaw(mat4 projection);
int lovrGraphicsPush();
int lovrGraphicsPop();
void lovrGraphicsOrigin();
void lovrGraphicsTranslate(float x, float y, float z);
void lovrGraphicsRotate(float w, float x, float y, float z);
void lovrGraphicsScale(float x, float y, float z);
Buffer* lovrGraphicsNewBuffer(int size, BufferDrawMode drawMode, BufferUsage usage);
Model* lovrGraphicsNewModel(const char* path);
Shader* lovrGraphicsNewShader(const char* vertexSource, const char* fragmentSource);
