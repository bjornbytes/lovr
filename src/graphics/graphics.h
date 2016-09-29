#include "buffer.h"
#include "model.h"
#include "shader.h"
#include "../matrix.h"

#ifndef LOVR_GRAPHICS_TYPES
#define LOVR_GRAPHICS_TYPES
typedef vec_t(mat4) vec_mat4_t;

#define LOVR_COLOR(r, g, b, a) ((a << 0) | (b << 8) | (g << 16) | (r << 24))
#define LOVR_COLOR_R(c) (c >> 24 & 0xff)
#define LOVR_COLOR_G(c) (c >> 16 & 0xff)
#define LOVR_COLOR_B(c) (c >> 8  & 0xff)
#define LOVR_COLOR_A(c) (c >> 0  & 0xff)

typedef struct {
  int x;
  int y;
  int width;
  int height;
} ScissorRectangle;

typedef struct {
  Shader* activeShader;
  Shader* defaultShader;
  vec_mat4_t transforms;
  mat4 lastTransform;
  mat4 projection;
  mat4 lastProjection;
  unsigned int color;
  unsigned int lastColor;
  unsigned char colorMask;
  char isScissorEnabled;
  ScissorRectangle scissor;
} GraphicsState;
#endif

void lovrGraphicsInit();
void lovrGraphicsReset();
void lovrGraphicsClear(int color, int depth);
void lovrGraphicsPresent();
void lovrGraphicsPrepare();
void lovrGraphicsGetBackgroundColor(float* r, float* g, float* b, float* a);
void lovrGraphicsSetBackgroundColor(float r, float g, float b, float a);
void lovrGraphicsGetColor(unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a);
void lovrGraphicsSetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void lovrGraphicsGetColorMask(unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a);
void lovrGraphicsSetColorMask(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
char lovrGraphicsIsScissorEnabled();
void lovrGraphicsSetScissorEnabled(char isEnabled);
void lovrGraphicsGetScissor(int* x, int* y, int* width, int* height);
void lovrGraphicsSetScissor(int x, int y, int width, int height);
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
void lovrGraphicsGetDimensions(int* width, int* height);
Buffer* lovrGraphicsNewBuffer(int size, BufferDrawMode drawMode, BufferUsage usage);
Model* lovrGraphicsNewModel(const char* path);
Shader* lovrGraphicsNewShader(const char* vertexSource, const char* fragmentSource);
