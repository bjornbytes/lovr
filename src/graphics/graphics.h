#include "graphics/buffer.h"
#include "graphics/font.h"
#include "graphics/model.h"
#include "graphics/shader.h"
#include "graphics/skybox.h"
#include "graphics/texture.h"
#include "math/math.h"

#pragma once

#define MAX_TRANSFORMS 64
#define MAX_CANVASES 4

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
  int x;
  int y;
  int width;
  int height;
} ScissorRectangle;

typedef struct CanvasState {
  int framebuffer;
  float projection[16];
  int viewport[4];
} CanvasState;

typedef struct {
  Shader* activeShader;
  Shader* defaultShader;
  Shader* skyboxShader;
  Shader* fullscreenShader;
  Font* activeFont;
  Texture* activeTexture;
  Texture* defaultTexture;
  float transforms[MAX_TRANSFORMS][16];
  CanvasState* canvases[MAX_CANVASES];
  int transform;
  int canvas;
  unsigned int color;
  char colorMask;
  int isScissorEnabled;
  ScissorRectangle scissor;
  GLuint shapeArray;
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
Font* lovrGraphicsGetFont();
void lovrGraphicsSetFont(Font* font);
void lovrGraphicsBindTexture(Texture* texture);
mat4 lovrGraphicsGetProjection();
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
void lovrGraphicsPushCanvas();
void lovrGraphicsPopCanvas();
void lovrGraphicsSetViewport(int x, int y, int w, int h);
void lovrGraphicsBindFramebuffer(int framebuffer);

// Transforms
int lovrGraphicsPush();
int lovrGraphicsPop();
void lovrGraphicsOrigin();
void lovrGraphicsTranslate(float x, float y, float z);
void lovrGraphicsRotate(float angle, float ax, float ay, float az);
void lovrGraphicsScale(float x, float y, float z);
void lovrGraphicsMatrixTransform(mat4 transform);

// Primitives
void lovrGraphicsSetShapeData(float* data, int length);
void lovrGraphicsSetIndexData(unsigned int* data, int length);
void lovrGraphicsDrawPrimitive(GLenum mode, Texture* texture, int hasNormals, int hasTexCoords, int useIndices);
void lovrGraphicsPoints(float* points, int count);
void lovrGraphicsLine(float* points, int count);
void lovrGraphicsTriangle(DrawMode mode, float* points);
void lovrGraphicsPlane(DrawMode mode, Texture* texture, float x, float y, float z, float size, float nx, float ny, float nz);
void lovrGraphicsPlaneFullscreen(Texture* texture);
void lovrGraphicsCube(DrawMode mode, Texture* texture, mat4 transform);
void lovrGraphicsSkybox(Skybox* skybox, float angle, float ax, float ay, float az);
void lovrGraphicsPrint(const char* str);
