#include "graphics/font.h"
#include "graphics/shader.h"
#include "lib/maf.h"
#include "util.h"
#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

#pragma once

#define MAX_TRANSFORMS 64
#define MAX_BATCHES 16
#define MAX_DRAWS 256
#define MAX_LOCKS 4

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
  STREAM_TRANSFORM,
  STREAM_COLOR,
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
  BATCH_CYLINDER,
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
  struct { float r1; float r2; bool capped; int segments; } cylinder;
  struct { int segments; } sphere;
  struct { float u; float v; float w; float h; } fill;
  struct { struct Mesh* object; DrawMode mode; uint32_t rangeStart; uint32_t rangeCount; uint32_t instances; float* pose; } mesh;
} BatchParams;

typedef struct {
  BatchType type;
  BatchParams params;
  DrawMode drawMode;
  DefaultShader shader;
  Pipeline* pipeline;
  struct Material* material;
  struct Texture* diffuseTexture;
  struct Texture* environmentMap;
  mat4 transform;
  uint32_t vertexCount;
  uint32_t indexCount;
  float** vertices;
  uint16_t** indices;
  uint16_t* baseVertex;
  bool instanced;
} BatchRequest;

typedef struct {
  BatchType type;
  BatchParams params;
  DrawMode drawMode;
  struct Canvas* canvas;
  struct Shader* shader;
  Pipeline pipeline;
  struct Material* material;
  mat4 transforms;
  Color* colors;
  struct { uint32_t start; uint32_t count; } cursors[MAX_BUFFER_ROLES];
  uint32_t count;
  bool instanced;
} Batch;

typedef struct {
  bool initialized;
  bool gammaCorrect;
  int width;
  int height;
  Camera camera;
  struct Shader* defaultShaders[MAX_DEFAULT_SHADERS];
  struct Material* defaultMaterial;
  struct Font* defaultFont;
  TextureFilter defaultFilter;
  float transforms[MAX_TRANSFORMS][16];
  int transform;
  Color backgroundColor;
  struct Canvas* canvas;
  Color color;
  struct Font* font;
  Pipeline pipeline;
  float pointSize;
  struct Shader* shader;
  struct Mesh* mesh;
  struct Mesh* instancedMesh;
  struct Buffer* identityBuffer;
  struct Buffer* buffers[MAX_BUFFER_ROLES];
  uint32_t cursors[MAX_BUFFER_ROLES];
  void* locks[MAX_BUFFER_ROLES][MAX_LOCKS];
  Batch batches[MAX_BATCHES];
  uint8_t batchCount;
} GraphicsState;

// Base
bool lovrGraphicsInit(bool gammaCorrect);
void lovrGraphicsDestroy(void);
void lovrGraphicsPresent(void);
void lovrGraphicsCreateWindow(WindowFlags* flags);
int lovrGraphicsGetWidth(void);
int lovrGraphicsGetHeight(void);
float lovrGraphicsGetPixelDensity(void);
void lovrGraphicsSetCamera(Camera* camera, bool clear);
struct Buffer* lovrGraphicsGetIdentityBuffer(void);
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
bool lovrGraphicsIsGammaCorrect(void);
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
void lovrGraphicsClear(Color* color, float* depth, int* stencil);
void lovrGraphicsDiscard(bool color, bool depth, bool stencil);
void lovrGraphicsBatch(BatchRequest* req);
void lovrGraphicsFlush(void);
void lovrGraphicsFlushCanvas(struct Canvas* canvas);
void lovrGraphicsFlushShader(struct Shader* shader);
void lovrGraphicsFlushMaterial(struct Material* material);
void lovrGraphicsFlushMesh(struct Mesh* mesh);
void lovrGraphicsPoints(uint32_t count, float** vertices);
void lovrGraphicsLine(uint32_t count, float** vertices);
void lovrGraphicsTriangle(DrawStyle style, struct Material* material, uint32_t count, float** vertices);
void lovrGraphicsPlane(DrawStyle style, struct Material* material, mat4 transform, float u, float v, float w, float h);
void lovrGraphicsBox(DrawStyle style, struct Material* material, mat4 transform);
void lovrGraphicsArc(DrawStyle style, ArcMode mode, struct Material* material, mat4 transform, float r1, float r2, int segments);
void lovrGraphicsCircle(DrawStyle style, struct Material* material, mat4 transform, int segments);
void lovrGraphicsCylinder(struct Material* material, mat4 transform, float r1, float r2, bool capped, int segments);
void lovrGraphicsSphere(struct Material* material, mat4 transform, int segments);
void lovrGraphicsSkybox(struct Texture* texture, float angle, float ax, float ay, float az);
void lovrGraphicsPrint(const char* str, size_t length, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign);
void lovrGraphicsFill(struct Texture* texture, float u, float v, float w, float h);
#define lovrGraphicsStencil lovrGpuStencil
#define lovrGraphicsCompute lovrGpuCompute

// GPU

typedef struct {
  bool compute;
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
  struct Mesh* mesh;
  struct Canvas* canvas;
  struct Shader* shader;
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
void lovrGpuDestroy(void);
void lovrGpuClear(struct Canvas* canvas, Color* color, float* depth, int* stencil);
void lovrGpuCompute(struct Shader* shader, int x, int y, int z);
void lovrGpuDiscard(struct Canvas* canvas, bool color, bool depth, bool stencil);
void lovrGpuDraw(DrawCommand* draw);
void lovrGpuStencil(StencilAction action, int replaceValue, StencilCallback callback, void* userdata);
void lovrGpuPresent(void);
void lovrGpuDirtyTexture(void);
void* lovrGpuLock(void);
void lovrGpuUnlock(void* lock);
void lovrGpuDestroyLock(void* lock);
const GpuFeatures* lovrGpuGetFeatures(void);
const GpuLimits* lovrGpuGetLimits(void);
const GpuStats* lovrGpuGetStats(void);
