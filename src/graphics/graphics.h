#include "graphics/font.h"
#include "graphics/shader.h"
#include "graphics/skybox.h"
#include "graphics/texture.h"
#include "math/math.h"
#include "lib/glfw.h"

#pragma once

#define MAX_CANVASES 4
#define MAX_TRANSFORMS 60
#define INTERNAL_TRANSFORMS 4
#define DEFAULT_SHADER_COUNT 4

typedef enum {
  BLEND_ALPHA,
  BLEND_ADD,
  BLEND_SUBTRACT,
  BLEND_MULTIPLY,
  BLEND_LIGHTEN,
  BLEND_DARKEN,
  BLEND_SCREEN,
  BLEND_REPLACE
} BlendMode;

typedef enum {
  BLEND_ALPHA_MULTIPLY,
  BLEND_PREMULTIPLIED
} BlendAlphaMode;

typedef enum {
  DRAW_MODE_FILL,
  DRAW_MODE_LINE
} DrawMode;

typedef enum {
  WINDING_CLOCKWISE = GL_CW,
  WINDING_COUNTERCLOCKWISE = GL_CCW
} Winding;

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
  int initialized;
  float pointSizes[2];
  int textureSize;
  int textureMSAA;
  float textureAnisotropy;
} GraphicsLimits;

typedef struct {
  int framebuffer;
  float projection[16];
  int viewport[4];
} CanvasState;

typedef enum {
  MATRIX_MODEL,
  MATRIX_VIEW
} MatrixType;

typedef struct {
  GLFWwindow* window;
  Shader* defaultShaders[DEFAULT_SHADER_COUNT];
  DefaultShader defaultShader;
  Font* defaultFont;
  Texture* defaultTexture;
  float transforms[MAX_TRANSFORMS + INTERNAL_TRANSFORMS][2][16];
  int transform;
  Color backgroundColor;
  BlendMode blendMode;
  BlendAlphaMode blendAlphaMode;
  Color color;
  int culling;
  TextureFilter defaultFilter;
  CompareMode depthTest;
  Font* font;
  GraphicsLimits limits;
  float lineWidth;
  float pointSize;
  Shader* shader;
  Winding winding;
  int wireframe;
  uint32_t streamVAO;
  uint32_t streamVBO;
  uint32_t streamIBO;
  vec_float_t streamData;
  vec_uint_t streamIndices;
  CanvasState canvases[MAX_CANVASES];
  int canvas;
  Texture* texture;
  uint32_t program;
  uint32_t vertexArray;
  uint32_t vertexBuffer;
  uint32_t indexBuffer;
} GraphicsState;

// Base
void lovrGraphicsInit();
void lovrGraphicsDestroy();
void lovrGraphicsReset();
void lovrGraphicsClear(int color, int depth);
void lovrGraphicsPresent();
void lovrGraphicsPrepare();
void lovrGraphicsCreateWindow(int w, int h, int fullscreen, int msaa, const char* title, const char* icon);
int lovrGraphicsGetWidth();
int lovrGraphicsGetHeight();

// State
Color lovrGraphicsGetBackgroundColor();
void lovrGraphicsSetBackgroundColor(Color color);
void lovrGraphicsGetBlendMode(BlendMode* mode, BlendAlphaMode* alphaMode);
void lovrGraphicsSetBlendMode(BlendMode mode, BlendAlphaMode alphaMode);
Color lovrGraphicsGetColor();
void lovrGraphicsSetColor(Color color);
int lovrGraphicsIsCullingEnabled();
void lovrGraphicsSetCullingEnabled(int culling);
TextureFilter lovrGraphicsGetDefaultFilter();
void lovrGraphicsSetDefaultFilter(TextureFilter filter);
CompareMode lovrGraphicsGetDepthTest();
void lovrGraphicsSetDepthTest(CompareMode depthTest);
Font* lovrGraphicsGetFont();
void lovrGraphicsSetFont(Font* font);
GraphicsLimits lovrGraphicsGetLimits();
float lovrGraphicsGetLineWidth();
void lovrGraphicsSetLineWidth(float width);
float lovrGraphicsGetPointSize();
void lovrGraphicsSetPointSize(float size);
Shader* lovrGraphicsGetShader();
void lovrGraphicsSetShader(Shader* shader);
Winding lovrGraphicsGetWinding();
void lovrGraphicsSetWinding(Winding winding);
int lovrGraphicsIsWireframe();
void lovrGraphicsSetWireframe(int wireframe);

// Transforms
void lovrGraphicsPush();
void lovrGraphicsPop();
void lovrGraphicsOrigin();
void lovrGraphicsTranslate(MatrixType type, float x, float y, float z);
void lovrGraphicsRotate(MatrixType type, float angle, float ax, float ay, float az);
void lovrGraphicsScale(MatrixType type, float x, float y, float z);
void lovrGraphicsMatrixTransform(MatrixType type, mat4 transform);

// Primitives
void lovrGraphicsPoints(float* points, int count);
void lovrGraphicsLine(float* points, int count);
void lovrGraphicsTriangle(DrawMode mode, float* points);
void lovrGraphicsPlane(DrawMode mode, Texture* texture, mat4 transform);
void lovrGraphicsPlaneFullscreen(Texture* texture);
void lovrGraphicsBox(DrawMode mode, Texture* texture, mat4 transform);
void lovrGraphicsCylinder(float x1, float y1, float z1, float x2, float y2, float z2, float r1, float r2, int capped, int segments);
void lovrGraphicsSphere(Texture* texture, mat4 transform, int segments, Skybox* skybox);
void lovrGraphicsSkybox(Skybox* skybox, float angle, float ax, float ay, float az);
void lovrGraphicsPrint(const char* str, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign);

// Internal State
void lovrGraphicsPushCanvas();
void lovrGraphicsPopCanvas();
mat4 lovrGraphicsGetProjection();
void lovrGraphicsSetProjection(mat4 projection);
void lovrGraphicsSetViewport(int x, int y, int w, int h);
void lovrGraphicsBindFramebuffer(int framebuffer);
Texture* lovrGraphicsGetTexture();
void lovrGraphicsBindTexture(Texture* texture);
void lovrGraphicsSetDefaultShader(DefaultShader defaultShader);
Shader* lovrGraphicsGetActiveShader();
void lovrGraphicsBindProgram(uint32_t program);
void lovrGraphicsBindVertexArray(uint32_t vao);
void lovrGraphicsBindVertexBuffer(uint32_t vbo);
void lovrGraphicsBindIndexBuffer(uint32_t ibo);
GLFWwindow* lovrGraphicsGetWindow();

