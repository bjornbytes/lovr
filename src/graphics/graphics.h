#include "graphics/gpu.h"
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

#define MAX_TRANSFORMS 60
#define INTERNAL_TRANSFORMS 4
#define MAX_LAYERS 4
#define MAX_PIPELINES 14
#define INTERNAL_PIPELINES 2
#define DEFAULT_SHADER_COUNT 5
#define MAX_TEXTURES 16

typedef void (*StencilCallback)(void* userdata);

typedef struct {
  mat4 transform;
  DefaultShader shader;
  Material* material;
  Texture* textures[MAX_MATERIAL_TEXTURES];
  Mesh* mesh;
  MeshDrawMode mode;
  struct {
    uint32_t count;
    float* data;
  } vertex;
  struct {
    uint32_t count;
    uint16_t* data;
  } index;
  struct {
    int start;
    int count;
  } range;
  int instances;
} GraphicsDraw;

typedef struct {
  bool initialized;
  GLFWwindow* window;
  Shader* defaultShaders[DEFAULT_SHADER_COUNT];
  Material* defaultMaterial;
  Font* defaultFont;
  TextureFilter defaultFilter;
  bool gammaCorrect;
  Mesh* mesh;
  float transforms[MAX_TRANSFORMS + INTERNAL_TRANSFORMS][16];
  int transform;
  Layer layers[MAX_LAYERS];
  int layer;
  Pipeline pipelines[MAX_PIPELINES + INTERNAL_PIPELINES];
  int pipeline;
  bool stencilWriting;
  bool stencilEnabled;
} GraphicsState;

// Base
void lovrGraphicsInit();
void lovrGraphicsDestroy();
void lovrGraphicsReset();
void lovrGraphicsClear(Color* color, float* depth, int* stencil);
void lovrGraphicsPresent();
void lovrGraphicsCreateWindow(int w, int h, bool fullscreen, int msaa, const char* title, const char* icon);
void lovrGraphicsGetDimensions(int* width, int* height);
GraphicsLimits lovrGraphicsGetLimits();
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
VertexPointer lovrGraphicsGetVertexPointer(uint32_t capacity);
void lovrGraphicsDraw(GraphicsDraw* draw);
void lovrGraphicsPushLayer(Canvas** canvas, int count, bool user);
void lovrGraphicsPopLayer();
void lovrGraphicsPushPipeline();
void lovrGraphicsPopPipeline();
void lovrGraphicsSetCamera(mat4 projection, mat4 view);
void lovrGraphicsSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
