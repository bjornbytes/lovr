#include "graphics/canvas.h"
#include "graphics/font.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "lib/math.h"
#include "util.h"
#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

#pragma once

#define MAX_TRANSFORMS 64
#define MAX_BATCHES 16
#define MAX_LOCKS 4

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
  bool stereo;
  Canvas* canvas;
  float viewMatrix[2][16];
  float projection[2][16];
} Camera;

typedef struct {
  float transform[16];
  Color color;
} DrawData;

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

typedef enum {
  STREAM_VERTEX,
  STREAM_INDEX,
  STREAM_DRAW_ID,
  STREAM_DRAW_DATA,
  MAX_BUFFER_ROLES
} BufferRole;

typedef enum {
  BATCH_POINTS,
  BATCH_LINES,
  BATCH_TRIANGLES,
  BATCH_PLANE,
  BATCH_BOX,
  BATCH_ARC,
  BATCH_SPHERE,
  BATCH_SKYBOX,
  BATCH_TEXT,
  BATCH_FILL,
  BATCH_MESH
} BatchType;

typedef union {
  struct { DrawStyle style; } triangles;
  struct { DrawStyle style; } plane;
  struct { DrawStyle style; } box;
  struct { DrawStyle style; ArcMode mode; float r1; float r2; int segments; } arc;
  struct { int segments; } sphere;
  struct { float u; float v; float w; float h; } fill;
  struct { Mesh* object; DrawMode mode; uint32_t rangeStart; uint32_t rangeCount; uint32_t instances; float* pose; } mesh;
} BatchParams;

typedef struct {
  BatchType type;
  BatchParams params;
  DefaultShader shader;
  Pipeline* pipeline;
  Material* material;
  Texture* diffuseTexture;
  Texture* environmentMap;
  mat4 transform;
  uint32_t vertexCount;
  uint32_t indexCount;
  float** vertices;
  uint16_t** indices;
  uint16_t* baseVertex;
} BatchRequest;

typedef struct {
  BatchType type;
  BatchParams params;
  Canvas* canvas;
  Shader* shader;
  Pipeline pipeline;
  Material* material;
  uint32_t vertexStart;
  uint32_t vertexCount;
  uint32_t indexStart;
  uint32_t indexCount;
  uint32_t drawStart;
  uint32_t drawCount;
  DrawData* drawData;
} Batch;

typedef struct {
  bool initialized;
  bool gammaCorrect;
  int width;
  int height;
  Camera camera;
  Shader* defaultShaders[MAX_DEFAULT_SHADERS];
  Material* defaultMaterial;
  Font* defaultFont;
  TextureFilter defaultFilter;
  float transforms[MAX_TRANSFORMS][16];
  int transform;
  Color backgroundColor;
  Canvas* canvas;
  Color color;
  Font* font;
  Pipeline pipeline;
  float pointSize;
  Shader* shader;
  uint32_t maxDraws;
  Mesh* mesh;
  Mesh* instancedMesh;
  Buffer* identityBuffer;
  Buffer* buffers[MAX_BUFFER_ROLES];
  size_t cursors[MAX_BUFFER_ROLES];
  void* locks[MAX_BUFFER_ROLES][MAX_LOCKS];
  Batch batches[MAX_BATCHES];
  uint8_t batchCount;
} GraphicsState;

// Base
bool lovrGraphicsInit(bool gammaCorrect);
void lovrGraphicsDestroy();
void lovrGraphicsPresent();
void lovrGraphicsSetWindow(WindowFlags* flags);
int lovrGraphicsGetWidth();
int lovrGraphicsGetHeight();
void lovrGraphicsSetCamera(Camera* camera, bool clear);
void* lovrGraphicsMapBuffer(BufferRole role, uint32_t count);
Buffer* lovrGraphicsGetIdentityBuffer();
#define lovrGraphicsGetSupported lovrGpuGetSupported
#define lovrGraphicsGetLimits lovrGpuGetLimits
#define lovrGraphicsGetStats lovrGpuGetStats

// State
void lovrGraphicsReset();
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
void lovrGraphicsSetLineWidth(uint8_t width);
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
void lovrGraphicsClear(Color* color, float* depth, int* stencil);
void lovrGraphicsDiscard(bool color, bool depth, bool stencil);
void lovrGraphicsBatch(BatchRequest* req);
void lovrGraphicsFlush();
void lovrGraphicsFlushCanvas(Canvas* canvas);
void lovrGraphicsFlushShader(Shader* shader);
void lovrGraphicsFlushMaterial(Material* material);
void lovrGraphicsFlushMesh(Mesh* mesh);
void lovrGraphicsPoints(uint32_t count, float** vertices);
void lovrGraphicsLine(uint32_t count, float** vertices);
void lovrGraphicsTriangle(DrawStyle style, Material* material, uint32_t count, float** vertices);
void lovrGraphicsPlane(DrawStyle style, Material* material, mat4 transform);
void lovrGraphicsBox(DrawStyle style, Material* material, mat4 transform);
void lovrGraphicsArc(DrawStyle style, ArcMode mode, Material* material, mat4 transform, float r1, float r2, int segments);
void lovrGraphicsCircle(DrawStyle style, Material* material, mat4 transform, int segments);
void lovrGraphicsCylinder(Material* material, float x1, float y1, float z1, float x2, float y2, float z2, float r1, float r2, bool capped, int segments);
void lovrGraphicsSphere(Material* material, mat4 transform, int segments);
void lovrGraphicsSkybox(Texture* texture, float angle, float ax, float ay, float az);
void lovrGraphicsPrint(const char* str, size_t length, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign);
void lovrGraphicsFill(Texture* texture, float u, float v, float w, float h);
#define lovrGraphicsStencil lovrGpuStencil
#define lovrGraphicsCompute lovrGpuCompute

// GPU

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
  int blockAlign;
} GpuLimits;

typedef struct {
  int shaderSwitches;
  int drawCalls;
} GpuStats;

typedef struct {
  Mesh* mesh;
  Canvas* canvas;
  Shader* shader;
  Pipeline pipeline;
  DrawMode drawMode;
  uint32_t instances;
  uint32_t rangeStart;
  uint32_t rangeCount;
  uint32_t width : 15;
  uint32_t height : 15;
  bool stereo : 1;
} DrawCommand;

void lovrGpuInit(bool srgb, getProcAddressProc getProcAddress);
void lovrGpuDestroy();
void lovrGpuClear(Canvas* canvas, Color* color, float* depth, int* stencil);
void lovrGpuCompute(Shader* shader, int x, int y, int z);
void lovrGpuDiscard(Canvas* canvas, bool color, bool depth, bool stencil);
void lovrGpuDraw(DrawCommand* draw);
void lovrGpuStencil(StencilAction action, int replaceValue, StencilCallback callback, void* userdata);
void lovrGpuPresent();
void lovrGpuDirtyTexture();
void* lovrGpuLock();
void lovrGpuUnlock(void* lock);
void lovrGpuDestroyLock(void* lock);
const GpuFeatures* lovrGpuGetSupported();
const GpuLimits* lovrGpuGetLimits();
const GpuStats* lovrGpuGetStats();
