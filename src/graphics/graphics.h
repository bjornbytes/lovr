#include "graphics/buffer.h"
#include "graphics/model.h"
#include "graphics/shader.h"
#include "graphics/skybox.h"
#include "graphics/texture.h"
#include "matrix.h"

#ifndef LOVR_GRAPHICS_TYPES
#define LOVR_GRAPHICS_TYPES

#define MAX_TRANSFORMS 64

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

typedef enum {
  COMPARE_NONE = 0,
  COMPARE_EQUAL = GL_EQUAL,
  COMPARE_NOT_EQUAL = GL_NOTEQUAL,
  COMPARE_LESS = GL_LESS,
  COMPARE_LEQUAL = GL_LEQUAL,
  COMPARE_GEQUAL = GL_GEQUAL,
  COMPARE_GREATER = GL_GREATER
} CompareMode;

typedef struct {
  Shader* activeShader;
  Shader* defaultShader;
  Shader* skyboxShader;
  Texture* defaultTexture;
  int transform;
  mat4 transforms[MAX_TRANSFORMS];
  mat4 projection;
  unsigned int color;
  char colorMask;
  int isScissorEnabled;
  ScissorRectangle scissor;
  GLuint shapeBuffer;
  GLuint shapeIndexBuffer;
  vec_float_t shapeData;
  vec_uint_t shapeIndices;
  float lineWidth;
  float pointSize;
  int isCullingEnabled;
  PolygonWinding polygonWinding;
  CompareMode depthTest;
  int isWireframe;
} GraphicsState;

#endif

// Base
void lovrGraphicsInit();
void lovrGraphicsDestroy();
void lovrGraphicsReset();
void lovrGraphicsClear(int color, int depth);
void lovrGraphicsPresent();
void lovrGraphicsPrepare();

// State
void lovrGraphicsGetBackgroundColor(float* r, float* g, float* b, float* a);
void lovrGraphicsSetBackgroundColor(float r, float g, float b, float a);
void lovrGraphicsGetColor(unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a);
void lovrGraphicsSetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void lovrGraphicsGetColorMask(char* r, char* g, char* b, char* a);
void lovrGraphicsSetColorMask(char r, char g, char b, char a);
int lovrGraphicsIsScissorEnabled();
void lovrGraphicsSetScissorEnabled(int isEnabled);
void lovrGraphicsGetScissor(int* x, int* y, int* width, int* height);
void lovrGraphicsSetScissor(int x, int y, int width, int height);
Shader* lovrGraphicsGetShader();
void lovrGraphicsSetShader(Shader* shader);
void lovrGraphicsBindTexture(Texture* texture);
void lovrGraphicsSetProjection(float near, float far, float fov);
void lovrGraphicsSetProjectionRaw(mat4 projection);
float lovrGraphicsGetLineWidth();
void lovrGraphicsSetLineWidth(float width);
float lovrGraphicsGetPointSize();
void lovrGraphicsSetPointSize(float size);
int lovrGraphicsIsCullingEnabled();
void lovrGraphicsSetCullingEnabled(int isEnabled);
PolygonWinding lovrGraphicsGetPolygonWinding();
void lovrGraphicsSetPolygonWinding(PolygonWinding winding);
CompareMode lovrGraphicsGetDepthTest();
void lovrGraphicsSetDepthTest(CompareMode depthTest);
int lovrGraphicsIsWireframe();
void lovrGraphicsSetWireframe(int wireframe);
int lovrGraphicsGetWidth();
int lovrGraphicsGetHeight();

// Transforms
int lovrGraphicsPush();
int lovrGraphicsPop();
void lovrGraphicsOrigin();
void lovrGraphicsTranslate(float x, float y, float z);
void lovrGraphicsRotate(float angle, float ax, float ay, float az);
void lovrGraphicsScale(float x, float y, float z);
void lovrGraphicsTransform(float tx, float ty, float tz, float sx, float sy, float sz, float angle, float ax, float ay, float az);
void lovrGraphicsMatrixTransform(mat4 transform);

// Primitives
void lovrGraphicsSetShapeData(float* data, int dataCount, unsigned int* indices, int indicesCount);
void lovrGraphicsDrawLinedShape(GLenum mode);
void lovrGraphicsDrawFilledShape();
void lovrGraphicsPoints(float* points, int count);
void lovrGraphicsLine(float* points, int count);
void lovrGraphicsTriangle(DrawMode mode, float* points);
void lovrGraphicsPlane(DrawMode mode, float x, float y, float z, float size, float nx, float ny, float nz);
void lovrGraphicsCube(DrawMode mode, float x, float y, float z, float size, float angle, float axisX, float axisY, float axisZ);
void lovrGraphicsSkybox(Skybox* skybox, float angle, float ax, float ay, float az);
