#include "graphics/canvas.h"
#include "graphics/font.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "math/math.h"
#include "util.h"
#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

#pragma once

#define MAX_TRANSFORMS 64
#define MAX_PIPELINES 16
#define MAX_VERTICES (1 << 16)
#define MAX_INDICES (1 << 16)
#define MAX_BATCH_SIZE 192 // Enough to fit in any UBO

typedef void (*StencilCallback)(void* userdata);

typedef enum {
  ARC_MODE_PIE,
  ARC_MODE_OPEN,
  ARC_MODE_CLOSED
} ArcMode;

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
  COMPARE_NONE,
  COMPARE_EQUAL,
  COMPARE_NEQUAL,
  COMPARE_LESS,
  COMPARE_LEQUAL,
  COMPARE_GREATER,
  COMPARE_GEQUAL
} CompareMode;

typedef enum {
  STYLE_FILL,
  STYLE_LINE
} DrawStyle;

typedef enum {
  STENCIL_REPLACE,
  STENCIL_INCREMENT,
  STENCIL_DECREMENT,
  STENCIL_INCREMENT_WRAP,
  STENCIL_DECREMENT_WRAP,
  STENCIL_INVERT
} StencilAction;

typedef enum {
  WINDING_CLOCKWISE,
  WINDING_COUNTERCLOCKWISE
} Winding;

typedef struct {
  bool computeShaders;
  bool singlepass;
} GpuFeatures;

typedef struct {
  bool initialized;
  float pointSizes[2];
  int textureSize;
  int textureMSAA;
  float textureAnisotropy;
  int blockSize;
} GpuLimits;

typedef struct {
  int shaderSwitches;
  int drawCalls;
} GpuStats;

// Internal

typedef struct {
  bool stereo;
  Canvas* canvas;
  float viewMatrix[2][16];
  float projection[2][16];
} Camera;

typedef struct {
  bool alphaSampling;
  BlendMode blendMode;
  BlendAlphaMode blendAlphaMode;
  bool culling;
  CompareMode depthTest;
  bool depthWrite;
  float lineWidth;
  CompareMode stencilMode;
  int stencilValue;
  Winding winding;
  bool wireframe;
} Pipeline;

typedef struct {
  Mesh* mesh;
  DrawMode mode;
  struct { uint32_t count; float* data; } vertex;
  struct { uint32_t count; uint16_t* data; } index;
  DefaultShader shader;
  Texture* diffuseTexture;
  Texture* environmentMap;
  Material* material;
  mat4 transform;
  float* pose;
  int instances;
  bool mono;
} DrawRequest;

typedef struct {
  bool initialized;
  bool gammaCorrect;
  int width;
  int height;
  Camera camera;
  Shader* defaultShaders[MAX_DEFAULT_SHADERS];
  Material* defaultMaterial;
  Font* defaultFont;
  Mesh* defaultMesh;
  TextureFilter defaultFilter;
  float transforms[MAX_TRANSFORMS][16];
  int transform;
  Pipeline pipelines[MAX_PIPELINES];
  Pipeline* pipeline;
  int pipelineIndex;
  Color backgroundColor;
  Canvas* canvas;
  Color color;
  Font* font;
  float pointSize;
  Shader* shader;
  DrawRequest batch;
  int batchVertex;
  int batchIndex;
  int batchSize;
  int vertexCursor;
  int indexCursor;
  ShaderBlock* block;
  Buffer* vertexMap;
  Buffer* identityBuffer;
} GraphicsState;

// Base
bool lovrGraphicsInit(bool gammaCorrect);
void lovrGraphicsDestroy();
void lovrGraphicsPresent();
void lovrGraphicsSetWindow(WindowFlags* flags);
int lovrGraphicsGetWidth();
int lovrGraphicsGetHeight();
void lovrGraphicsSetCamera(Camera* camera, bool clear);
Buffer* lovrGraphicsGetIdentityBuffer();
#define lovrGraphicsGetSupported lovrGpuGetSupported
#define lovrGraphicsGetLimits lovrGpuGetLimits
#define lovrGraphicsGetStats lovrGpuGetStats

// State
void lovrGraphicsReset();
void lovrGraphicsPushPipeline();
void lovrGraphicsPopPipeline();
bool lovrGraphicsGetAlphaSampling();
void lovrGraphicsSetAlphaSampling(bool sample);
Color lovrGraphicsGetBackgroundColor();
void lovrGraphicsSetBackgroundColor(Color color);
void lovrGraphicsGetBlendMode(BlendMode* mode, BlendAlphaMode* alphaMode);
void lovrGraphicsSetBlendMode(BlendMode mode, BlendAlphaMode alphaMode);
Canvas* lovrGraphicsGetCanvas();
void lovrGraphicsSetCanvas(Canvas* canvas);
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
void lovrGraphicsTranslate(vec3 translation);
void lovrGraphicsRotate(quat rotation);
void lovrGraphicsScale(vec3 scale);
void lovrGraphicsMatrixTransform(mat4 transform);
void lovrGraphicsSetProjection(mat4 projection);

// Rendering
float* lovrGraphicsGetVertexPointer(uint32_t count);
uint16_t* lovrGraphicsGetIndexPointer(uint32_t count);
void lovrGraphicsClear(Color* color, float* depth, int* stencil);
void lovrGraphicsDiscard(bool color, bool depth, bool stencil);
void lovrGraphicsFlush();
void lovrGraphicsDraw(DrawRequest* draw);
void lovrGraphicsPoints(uint32_t count);
void lovrGraphicsLine(uint32_t count);
void lovrGraphicsTriangle(DrawStyle style, Material* material, float points[9]);
void lovrGraphicsPlane(DrawStyle style, Material* material, mat4 transform);
void lovrGraphicsBox(DrawStyle style, Material* material, mat4 transform);
void lovrGraphicsArc(DrawStyle style, ArcMode, Material* material, mat4 transform, float theta1, float theta2, int segments);
void lovrGraphicsCircle(DrawStyle style, Material* material, mat4 transform, int segments);
void lovrGraphicsCylinder(Material* material, float x1, float y1, float z1, float x2, float y2, float z2, float r1, float r2, bool capped, int segments);
void lovrGraphicsSphere(Material* material, mat4 transform, int segments);
void lovrGraphicsSkybox(Texture* texture, float angle, float ax, float ay, float az);
void lovrGraphicsPrint(const char* str, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign);
void lovrGraphicsFill(Texture* texture, float u, float v, float w, float h);
#define lovrGraphicsStencil lovrGpuStencil
#define lovrGraphicsCompute lovrGpuCompute

// GPU API
void lovrGpuInit(bool srgb, getProcAddressProc getProcAddress);
void lovrGpuDestroy();
void lovrGpuBindPipeline(Pipeline* pipeline);
void lovrGpuSetViewports(float* viewports, int count);
void lovrGpuClear(Canvas* canvas, Color* color, float* depth, int* stencil);
void lovrGpuDiscard(Canvas* canvas, bool color, bool depth, bool stencil);
void lovrGpuStencil(StencilAction action, int replaceValue, StencilCallback callback, void* userdata);
void lovrGpuCompute(Shader* shader, int x, int y, int z);
void lovrGpuPresent();
void lovrGpuDirtyTexture();
const GpuFeatures* lovrGpuGetSupported();
const GpuLimits* lovrGpuGetLimits();
const GpuStats* lovrGpuGetStats();
