#include "graphics/canvas.h"
#include "graphics/font.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "math/math.h"
#include "util.h"
#include "lib/glfw.h"
#include <stdbool.h>

#pragma once

#define MAX_CANVASES 4
#define MAX_LAYERS 4
#define MAX_TRANSFORMS 60
#define INTERNAL_TRANSFORMS 4
#define DEFAULT_SHADER_COUNT 5
#define MAX_TEXTURES 16

typedef void (*StencilCallback)(void* userdata);

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
  ARC_MODE_PIE,
  ARC_MODE_OPEN,
  ARC_MODE_CLOSED
} ArcMode;

typedef enum {
  WINDING_CLOCKWISE,
  WINDING_COUNTERCLOCKWISE
} Winding;

typedef enum {
  COMPARE_NONE,
  COMPARE_EQUAL,
  COMPARE_NEQUAL,
  COMPARE_LESS,
  COMPARE_LEQUAL,
  COMPARE_GREATER,
  COMPARE_GEQUAL
} CompareMode;

typedef enum {
  STENCIL_REPLACE,
  STENCIL_INCREMENT,
  STENCIL_DECREMENT,
  STENCIL_INCREMENT_WRAP,
  STENCIL_DECREMENT_WRAP,
  STENCIL_INVERT
} StencilAction;

typedef struct {
  float projection[16];
  float view[16];
  Canvas* canvas;
  int viewport[4];
} Layer;

typedef struct {
  bool initialized;
  float pointSizes[2];
  int textureSize;
  int textureMSAA;
  float textureAnisotropy;
} GraphicsLimits;

typedef struct {
  int drawCalls;
  int shaderSwitches;
} GraphicsStats;

typedef struct {
  bool initialized;
  GLFWwindow* window;
  Shader* defaultShaders[DEFAULT_SHADER_COUNT];
  Material* defaultMaterial;
  Font* defaultFont;
  Texture* defaultTexture;
  float transforms[MAX_TRANSFORMS + INTERNAL_TRANSFORMS][16];
  int transform;
  Layer layers[MAX_LAYERS];
  int layer;
  Color backgroundColor;
  BlendMode blendMode;
  BlendAlphaMode blendAlphaMode;
  Canvas* canvas[MAX_CANVASES];
  int canvasCount;
  Color color;
  bool culling;
  TextureFilter defaultFilter;
  CompareMode depthTest;
  bool depthWrite;
  Font* font;
  bool gammaCorrect;
  GraphicsLimits limits;
  float lineWidth;
  float pointSize;
  Shader* shader;
  CompareMode stencilMode;
  int stencilValue;
  Winding winding;
  bool wireframe;
  Mesh* mesh;
  Texture* textures[MAX_TEXTURES];
  bool stencilEnabled;
  bool stencilWriting;
  uint32_t program;
  uint32_t framebuffer;
  uint32_t viewport[4];
  uint32_t vertexArray;
  uint32_t vertexBuffer;
  uint32_t indexBuffer;
  GraphicsStats stats;
} GraphicsState;

// Base
void lovrGraphicsInit();
void lovrGraphicsDestroy();
void lovrGraphicsReset();
void lovrGraphicsClear(bool clearColor, bool clearDepth, bool clearStencil, Color color, float depth, int stencil);
void lovrGraphicsPresent();
void lovrGraphicsCreateWindow(int w, int h, bool fullscreen, int msaa, const char* title, const char* icon);
int lovrGraphicsGetWidth();
int lovrGraphicsGetHeight();
GraphicsStats lovrGraphicsGetStats();

// State
Color lovrGraphicsGetBackgroundColor();
void lovrGraphicsSetBackgroundColor(Color color);
void lovrGraphicsGetBlendMode(BlendMode* mode, BlendAlphaMode* alphaMode);
void lovrGraphicsSetBlendMode(BlendMode mode, BlendAlphaMode alphaMode);
void lovrGraphicsGetCanvas(Canvas** canvas, int* count);
void lovrGraphicsSetCanvas(Canvas** canvas, int count);
Color lovrGraphicsGetColor();
void lovrGraphicsSetColor(Color color);
bool lovrGraphicsIsCullingEnabled();
void lovrGraphicsSetCullingEnabled(bool culling);
TextureFilter lovrGraphicsGetDefaultFilter();
void lovrGraphicsSetDefaultFilter(TextureFilter filter);
void lovrGraphicsGetDepthTest(CompareMode* mode, bool* write);
void lovrGraphicsSetDepthTest(CompareMode depthTest, bool write);
Font* lovrGraphicsGetFont();
void lovrGraphicsSetFont(Font* font);
bool lovrGraphicsIsGammaCorrect();
void lovrGraphicsSetGammaCorrect(bool gammaCorrect);
GraphicsLimits lovrGraphicsGetLimits();
float lovrGraphicsGetLineWidth();
void lovrGraphicsSetLineWidth(float width);
float lovrGraphicsGetPointSize();
void lovrGraphicsSetPointSize(float size);
Shader* lovrGraphicsGetShader();
void lovrGraphicsSetShader(Shader* shader);
void lovrGraphicsGetStencilTest(CompareMode* mode, int* value);
void lovrGraphicsSetStencilTest(CompareMode mode, int value);
Winding lovrGraphicsGetWinding();
void lovrGraphicsSetWinding(Winding winding);
bool lovrGraphicsIsWireframe();
void lovrGraphicsSetWireframe(bool wireframe);

// Transforms
void lovrGraphicsPush();
void lovrGraphicsPop();
void lovrGraphicsOrigin();
void lovrGraphicsTranslate(float x, float y, float z);
void lovrGraphicsRotate(float angle, float ax, float ay, float az);
void lovrGraphicsScale(float x, float y, float z);
void lovrGraphicsMatrixTransform(mat4 transform);

// Primitives
void lovrGraphicsPoints(uint32_t count);
void lovrGraphicsLine(uint32_t count);
void lovrGraphicsTriangle(DrawMode mode, Material* material, float points[9]);
void lovrGraphicsPlane(DrawMode mode, Material* material, mat4 transform);
void lovrGraphicsBox(DrawMode mode, Material* material, mat4 transform);
void lovrGraphicsArc(DrawMode mode, ArcMode, Material* material, mat4 transform, float theta1, float theta2, int segments);
void lovrGraphicsCircle(DrawMode mode, Material* material, mat4 transform, int segments);
void lovrGraphicsCylinder(Material* material, float x1, float y1, float z1, float x2, float y2, float z2, float r1, float r2, bool capped, int segments);
void lovrGraphicsSphere(Material* material, mat4 transform, int segments);
void lovrGraphicsSkybox(Texture* texture, float angle, float ax, float ay, float az);
void lovrGraphicsPrint(const char* str, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign);
void lovrGraphicsStencil(StencilAction action, int replaceValue, StencilCallback callback, void* userdata);
void lovrGraphicsFill(Texture* texture);

// Internal
void lovrGraphicsDraw(Mesh* mesh, mat4 transform, DefaultShader shader, int instances);
VertexPointer lovrGraphicsGetVertexPointer(uint32_t capacity);
void lovrGraphicsPushLayer(Canvas* canvas);
void lovrGraphicsPopLayer();
void lovrGraphicsSetCamera(mat4 projection, mat4 view);
void lovrGraphicsSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
Texture* lovrGraphicsGetTexture(int slot);
void lovrGraphicsBindTexture(Texture* texture, GLenum type, int slot);
Material* lovrGraphicsGetDefaultMaterial();
void lovrGraphicsUseProgram(uint32_t program);
void lovrGraphicsBindFramebuffer(uint32_t framebuffer);
void lovrGraphicsBindVertexArray(uint32_t vao);
void lovrGraphicsBindVertexBuffer(uint32_t vbo);
void lovrGraphicsBindIndexBuffer(uint32_t ibo);
