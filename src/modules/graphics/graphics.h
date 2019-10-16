#include "graphics/font.h"
#include "data/modelData.h"
#include "core/maf.h"
#include "util.h"
#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

#pragma once

struct Buffer;
struct Canvas;
struct Font;
struct Material;
struct Mesh;
struct Shader;
struct Texture;

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
  BLEND_NONE
} BlendMode;

typedef enum {
  BLEND_ALPHA_MULTIPLY,
  BLEND_PREMULTIPLIED
} BlendAlphaMode;

typedef enum {
  COMPARE_EQUAL,
  COMPARE_NEQUAL,
  COMPARE_LESS,
  COMPARE_LEQUAL,
  COMPARE_GREATER,
  COMPARE_GEQUAL,
  COMPARE_NONE
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
  bool stereo;
  struct Canvas* canvas;
  float viewMatrix[2][16];
  float projection[2][16];
} Camera;

typedef struct {
  bool alphaSampling : 1;
  uint8_t blendMode : 3; // BlendMode
  uint8_t blendAlphaMode : 1; // BlendAlphaMode
  bool culling : 1;
  uint8_t depthTest : 3; // CompareMode
  bool depthWrite : 1;
  uint8_t lineWidth : 8;
  uint8_t stencilValue: 8;
  uint8_t stencilMode : 3; // CompareMode
  uint8_t winding : 1; // Winding
  bool wireframe : 1;
} Pipeline;

// Base
bool lovrGraphicsInit();
void lovrGraphicsDestroy(void);
void lovrGraphicsPresent(void);
void lovrGraphicsCreateWindow(WindowFlags* flags);
int lovrGraphicsGetWidth(void);
int lovrGraphicsGetHeight(void);
float lovrGraphicsGetPixelDensity(void);
const Camera* lovrGraphicsGetCamera(void);
void lovrGraphicsSetCamera(Camera* camera, bool clear);
struct Buffer* lovrGraphicsGetIdentityBuffer(void);
#define lovrGraphicsTick lovrGpuTick
#define lovrGraphicsTock lovrGpuTock
#define lovrGraphicsGetFeatures lovrGpuGetFeatures
#define lovrGraphicsGetLimits lovrGpuGetLimits
#define lovrGraphicsGetStats lovrGpuGetStats

// State
void lovrGraphicsReset(void);
bool lovrGraphicsGetAlphaSampling(void);
void lovrGraphicsSetAlphaSampling(bool sample);
Color lovrGraphicsGetBackgroundColor(void);
void lovrGraphicsSetBackgroundColor(Color color);
void lovrGraphicsGetBlendMode(BlendMode* mode, BlendAlphaMode* alphaMode);
void lovrGraphicsSetBlendMode(BlendMode mode, BlendAlphaMode alphaMode);
struct Canvas* lovrGraphicsGetCanvas(void);
void lovrGraphicsSetCanvas(struct Canvas* canvas);
Color lovrGraphicsGetColor(void);
void lovrGraphicsSetColor(Color color);
bool lovrGraphicsIsCullingEnabled(void);
void lovrGraphicsSetCullingEnabled(bool culling);
TextureFilter lovrGraphicsGetDefaultFilter(void);
void lovrGraphicsSetDefaultFilter(TextureFilter filter);
void lovrGraphicsGetDepthTest(CompareMode* mode, bool* write);
void lovrGraphicsSetDepthTest(CompareMode depthTest, bool write);
struct Font* lovrGraphicsGetFont(void);
void lovrGraphicsSetFont(struct Font* font);
float lovrGraphicsGetLineWidth(void);
void lovrGraphicsSetLineWidth(uint8_t width);
float lovrGraphicsGetPointSize(void);
void lovrGraphicsSetPointSize(float size);
struct Shader* lovrGraphicsGetShader(void);
void lovrGraphicsSetShader(struct Shader* shader);
void lovrGraphicsGetStencilTest(CompareMode* mode, int* value);
void lovrGraphicsSetStencilTest(CompareMode mode, int value);
Winding lovrGraphicsGetWinding(void);
void lovrGraphicsSetWinding(Winding winding);
bool lovrGraphicsIsWireframe(void);
void lovrGraphicsSetWireframe(bool wireframe);

// Transforms
void lovrGraphicsPush(void);
void lovrGraphicsPop(void);
void lovrGraphicsOrigin(void);
void lovrGraphicsTranslate(vec3 translation);
void lovrGraphicsRotate(quat rotation);
void lovrGraphicsScale(vec3 scale);
void lovrGraphicsMatrixTransform(mat4 transform);
void lovrGraphicsSetProjection(mat4 projection);

// Rendering
void lovrGraphicsFlush(void);
void lovrGraphicsFlushCanvas(struct Canvas* canvas);
void lovrGraphicsFlushShader(struct Shader* shader);
void lovrGraphicsFlushMaterial(struct Material* material);
void lovrGraphicsFlushMesh(struct Mesh* mesh);
void lovrGraphicsClear(Color* color, float* depth, int* stencil);
void lovrGraphicsDiscard(bool color, bool depth, bool stencil);
void lovrGraphicsPoints(uint32_t count, float** vertices);
void lovrGraphicsLine(uint32_t count, float** vertices);
void lovrGraphicsTriangle(DrawStyle style, struct Material* material, uint32_t count, float** vertices);
void lovrGraphicsPlane(DrawStyle style, struct Material* material, mat4 transform, float u, float v, float w, float h);
void lovrGraphicsBox(DrawStyle style, struct Material* material, mat4 transform);
void lovrGraphicsArc(DrawStyle style, ArcMode mode, struct Material* material, mat4 transform, float r1, float r2, int segments);
void lovrGraphicsCircle(DrawStyle style, struct Material* material, mat4 transform, int segments);
void lovrGraphicsCylinder(struct Material* material, mat4 transform, float r1, float r2, bool capped, int segments);
void lovrGraphicsSphere(struct Material* material, mat4 transform, int segments);
void lovrGraphicsSkybox(struct Texture* texture);
void lovrGraphicsPrint(const char* str, size_t length, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign);
void lovrGraphicsFill(struct Texture* texture, float u, float v, float w, float h);
void lovrGraphicsDrawMesh(struct Mesh* mesh, mat4 transform, uint32_t instances, float* pose);
#define lovrGraphicsStencil lovrGpuStencil
#define lovrGraphicsCompute lovrGpuCompute

// GPU

typedef struct {
  bool astc;
  bool compute;
  bool dxt;
  bool instancedStereo;
  bool multiview;
  bool timers;
} GpuFeatures;

typedef struct {
  bool initialized;
  float pointSizes[2];
  int textureSize;
  int textureMSAA;
  float textureAnisotropy;
  int blockSize;
  int blockAlign;
} GpuLimits;

typedef struct {
  int shaderSwitches;
  int drawCalls;
} GpuStats;

typedef struct {
  struct Mesh* mesh;
  struct Canvas* canvas;
  struct Shader* shader;
  Pipeline pipeline;
  DrawMode topology;
  uint32_t rangeStart;
  uint32_t rangeCount;
  uint32_t instances;
} DrawCommand;

void lovrGpuInit(getProcAddressProc getProcAddress);
void lovrGpuDestroy(void);
void lovrGpuClear(struct Canvas* canvas, Color* color, float* depth, int* stencil);
void lovrGpuCompute(struct Shader* shader, int x, int y, int z);
void lovrGpuDiscard(struct Canvas* canvas, bool color, bool depth, bool stencil);
void lovrGpuDraw(DrawCommand* draw);
void lovrGpuStencil(StencilAction action, int replaceValue, bool drawVisible, CompareMode stencilMode, StencilCallback callback, void* userdata);
void lovrGpuPresent(void);
void lovrGpuDirtyTexture(void);
void lovrGpuTick(const char* label);
double lovrGpuTock(const char* label);
const GpuFeatures* lovrGpuGetFeatures(void);
const GpuLimits* lovrGpuGetLimits(void);
const GpuStats* lovrGpuGetStats(void);
