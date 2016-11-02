#include "buffer.h"
#include "model.h"
#include "shader.h"
#include "skybox.h"
#include "../matrix.h"

#ifndef LOVR_GRAPHICS_TYPES
#define LOVR_GRAPHICS_TYPES

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

typedef enum {
  DRAW_MODE_FILL,
  DRAW_MODE_LINE
} DrawMode;

typedef enum {
  POLYGON_WINDING_CLOCKWISE = GL_CW,
  POLYGON_WINDING_COUNTERCLOCKWISE = GL_CCW
} PolygonWinding;

typedef struct {
  Shader* activeShader;
  Shader* lastShader;
  Shader* defaultShader;
  Shader* skyboxShader;
  vec_mat4_t transforms;
  mat4 lastTransform;
  mat4 projection;
  mat4 lastProjection;
  unsigned int color;
  unsigned int lastColor;
  char colorMask;
  char isScissorEnabled;
  ScissorRectangle scissor;
  GLuint shapeArray;
  GLuint shapeBuffer;
  GLuint shapeIndexBuffer;
  vec_float_t shapeData;
  vec_uint_t shapeIndices;
  float lineWidth;
  char isCullingEnabled;
  PolygonWinding polygonWinding;
} GraphicsState;
#endif

void lovrGraphicsInit();
void lovrGraphicsDestroy();
void lovrGraphicsReset();
void lovrGraphicsClear(int color, int depth);
void lovrGraphicsPresent();
void lovrGraphicsPrepare();
void lovrGraphicsGetBackgroundColor(float* r, float* g, float* b, float* a);
void lovrGraphicsSetBackgroundColor(float r, float g, float b, float a);
void lovrGraphicsGetColor(unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a);
void lovrGraphicsSetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void lovrGraphicsGetColorMask(char* r, char* g, char* b, char* a);
void lovrGraphicsSetColorMask(char r, char g, char b, char a);
char lovrGraphicsIsScissorEnabled();
void lovrGraphicsSetScissorEnabled(char isEnabled);
void lovrGraphicsGetScissor(int* x, int* y, int* width, int* height);
void lovrGraphicsSetScissor(int x, int y, int width, int height);
Shader* lovrGraphicsGetShader();
void lovrGraphicsSetShader(Shader* shader);
void lovrGraphicsSetProjection(float near, float far, float fov);
void lovrGraphicsSetProjectionRaw(mat4 projection);
float lovrGraphicsGetLineWidth();
void lovrGraphicsSetLineWidth(float width);
char lovrGraphicsIsCullingEnabled();
void lovrGraphicsSetCullingEnabled(char isEnabled);
PolygonWinding lovrGraphicsGetPolygonWinding();
void lovrGraphicsSetPolygonWinding(PolygonWinding winding);
int lovrGraphicsPush();
int lovrGraphicsPop();
void lovrGraphicsOrigin();
void lovrGraphicsTranslate(float x, float y, float z);
void lovrGraphicsRotate(float w, float x, float y, float z);
void lovrGraphicsScale(float x, float y, float z);
void lovrGraphicsTransform(float tx, float ty, float tz, float sx, float sy, float sz, float angle, float ax, float ay, float az);
void lovrGraphicsMatrixTransform(mat4 transform);
void lovrGraphicsGetDimensions(int* width, int* height);
void lovrGraphicsSetShapeData(float* data, int dataCount, unsigned int* indices, int indicesCount);
void lovrGraphicsDrawLinedShape(GLenum mode);
void lovrGraphicsDrawFilledShape();
void lovrGraphicsLine(float* points, int count);
void lovrGraphicsPlane(DrawMode mode, float x, float y, float z, float size, float nx, float ny, float nz);
void lovrGraphicsCube(DrawMode mode, float x, float y, float z, float size, float angle, float axisX, float axisY, float axisZ);
void lovrGraphicsSkybox(Skybox* skybox, float angle, float ax, float ay, float az);
