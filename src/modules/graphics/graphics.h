#include <stdbool.h>
#include <stdint.h>

#pragma once

struct Blob;
struct Image;
struct WindowFlags;

typedef struct Buffer Buffer;
typedef struct Texture Texture;
typedef struct Pipeline Pipeline;
typedef struct Canvas Canvas;
typedef struct Shader Shader;
typedef struct Bundle Bundle;

typedef struct {
  bool bptc;
  bool astc;
  bool pointSize;
  bool wireframe;
  bool multiview;
  bool multiblend;
  bool anisotropy;
  bool depthClamp;
  bool depthNudgeClamp;
  bool clipDistance;
  bool cullDistance;
  bool fullIndexBufferRange;
  bool indirectDrawCount;
  bool indirectDrawFirstInstance;
  bool extraShaderInputs;
  bool dynamicIndexing;
  bool float64;
  bool int64;
  bool int16;
} GraphicsFeatures;

typedef struct {
  uint32_t textureSize2D;
  uint32_t textureSize3D;
  uint32_t textureSizeCube;
  uint32_t textureLayers;
  uint32_t renderSize[2];
  uint32_t renderViews;
  uint32_t bundleCount;
  uint32_t bundleSlots;
  uint32_t uniformBufferRange;
  uint32_t storageBufferRange;
  uint32_t uniformBufferAlign;
  uint32_t storageBufferAlign;
  uint32_t vertexAttributes;
  uint32_t vertexAttributeOffset;
  uint32_t vertexBuffers;
  uint32_t vertexBufferStride;
  uint32_t vertexShaderOutputs;
  uint32_t computeCount[3];
  uint32_t computeGroupSize[3];
  uint32_t computeGroupVolume;
  uint32_t computeSharedMemory;
  uint32_t indirectDrawCount;
  uint64_t allocationSize;
  uint32_t pointSize[2];
  float anisotropy;
} GraphicsLimits;

bool lovrGraphicsInit(bool debug);
void lovrGraphicsDestroy(void);
void lovrGraphicsCreateWindow(struct WindowFlags* window);
bool lovrGraphicsHasWindow(void);
uint32_t lovrGraphicsGetWidth(void);
uint32_t lovrGraphicsGetHeight(void);
float lovrGraphicsGetPixelDensity(void);
void lovrGraphicsGetFeatures(GraphicsFeatures* features);
void lovrGraphicsGetLimits(GraphicsLimits* limits);
void lovrGraphicsBegin(void);
void lovrGraphicsFlush(void);

// Buffer

enum {
  BUFFER_VERTEX    = (1 << 0),
  BUFFER_INDEX     = (1 << 1),
  BUFFER_UNIFORM   = (1 << 2),
  BUFFER_COMPUTE   = (1 << 3),
  BUFFER_PARAMETER = (1 << 4),
  BUFFER_COPY      = (1 << 5),
  BUFFER_WRITE     = (1 << 6),
  BUFFER_RETAIN    = (1 << 7)
};

typedef enum {
  FIELD_I8,
  FIELD_U8,
  FIELD_I16,
  FIELD_U16,
  FIELD_I32,
  FIELD_U32,
  FIELD_F32,
  FIELD_F64,
  FIELD_I8x2,
  FIELD_U8x2,
  FIELD_I8Nx2,
  FIELD_U8Nx2,
  FIELD_I16x2,
  FIELD_U16x2,
  FIELD_I16Nx2,
  FIELD_U16Nx2,
  FIELD_I32x2,
  FIELD_U32x2,
  FIELD_F32x2,
  FIELD_I32x3,
  FIELD_U32x3,
  FIELD_F32x3,
  FIELD_I8x4,
  FIELD_U8x4,
  FIELD_I8Nx4,
  FIELD_U8Nx4,
  FIELD_I16x4,
  FIELD_U16x4,
  FIELD_I16Nx4,
  FIELD_U16Nx4,
  FIELD_I32x4,
  FIELD_U32x4,
  FIELD_F32x4,
  FIELD_MAT2,
  FIELD_MAT3,
  FIELD_MAT4
} FieldType;

typedef struct {
  uint32_t flags;
  uint32_t length;
  uint32_t stride;
  uint32_t fieldCount;
  FieldType types[16];
  uint16_t offsets[16];
  void** initialContents;
  const char* label;
} BufferInfo;

Buffer* lovrBufferCreate(BufferInfo* info);
void lovrBufferDestroy(void* ref);
const BufferInfo* lovrBufferGetInfo(Buffer* buffer);
void* lovrBufferMap(Buffer* buffer);
void lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t size);
void lovrBufferCopy(Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size);
void lovrBufferRead(Buffer* buffer, uint32_t offset, uint32_t size, void (*callback)(void* data, uint64_t size, void* userdata), void* userdata);

// Texture

typedef enum {
  TEXTURE_2D,
  TEXTURE_CUBE,
  TEXTURE_VOLUME,
  TEXTURE_ARRAY
} TextureType;

enum {
  TEXTURE_SAMPLE  = (1 << 0),
  TEXTURE_RENDER  = (1 << 1),
  TEXTURE_COMPUTE = (1 << 2),
  TEXTURE_COPY    = (1 << 3)
};

typedef struct {
  Texture* parent;
  TextureType type;
  uint32_t layerIndex;
  uint32_t layerCount;
  uint32_t levelIndex;
  uint32_t levelCount;
} TextureViewInfo;

typedef struct {
  Texture* parent;
  TextureType type;
  uint32_t format;
  uint32_t size[3];
  uint32_t mipmaps;
  uint32_t samples;
  uint32_t flags;
  bool srgb;
  const char* label;
} TextureInfo;

Texture* lovrTextureCreate(TextureInfo* info);
Texture* lovrTextureCreateView(TextureViewInfo* view);
void lovrTextureDestroy(void* ref);
const TextureInfo* lovrTextureGetInfo(Texture* texture);
void lovrTextureWrite(Texture* texture, uint16_t offset[4], uint16_t extent[3], void* data, uint32_t step[2]);
void lovrTexturePaste(Texture* texture, struct Image* image, uint16_t srcOffset[4], uint16_t dstOffset[2], uint16_t extent[2]);
void lovrTextureClear(Texture* texture, uint16_t layer, uint16_t layerCount, uint16_t level, uint16_t levelCount, float color[4]);
void lovrTextureRead(Texture* texture, uint16_t offset[4], uint16_t extent[3], void (*callback)(void* data, uint64_t size, void* userdata), void* userdata);
void lovrTextureCopy(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t extent[3]);
void lovrTextureBlit(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t srcExtent[3], uint16_t dstExtent[3], bool nearest);

// Pipeline

typedef enum {
  BLEND_ALPHA_MULTIPLY,
  BLEND_PREMULTIPLIED
} BlendAlphaMode;

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
  COMPARE_NONE,
  COMPARE_EQUAL,
  COMPARE_NEQUAL,
  COMPARE_LESS,
  COMPARE_LEQUAL,
  COMPARE_GREATER,
  COMPARE_GEQUAL
} CompareMode;

typedef enum {
  CULL_NONE,
  CULL_FRONT,
  CULL_BACK
} CullMode;

typedef enum {
  STENCIL_KEEP,
  STENCIL_REPLACE,
  STENCIL_INCREMENT,
  STENCIL_DECREMENT,
  STENCIL_INCREMENT_WRAP,
  STENCIL_DECREMENT_WRAP,
  STENCIL_INVERT
} StencilAction;

typedef enum {
  WINDING_COUNTERCLOCKWISE,
  WINDING_CLOCKWISE
} Winding;

typedef struct {
  const char* label;
} PipelineInfo;

Pipeline* lovrPipelineCreate(PipelineInfo* info);
void lovrPipelineDestroy(void* ref);
const PipelineInfo* lovrPipelineGetInfo(Pipeline* pipeline);

// Canvas

#define MAX_ATTACHMENTS 4

typedef enum {
  LOAD_KEEP,
  LOAD_CLEAR,
  LOAD_DISCARD
} LoadAction;

typedef enum {
  SAVE_KEEP,
  SAVE_DISCARD
} SaveAction;

typedef enum {
  ATTACHMENT_COLOR,
  ATTACHMENT_DEPTH,
  ATTACHMENT_MULTISAMPLE
} AttachmentType;

typedef struct {
  uint32_t format;
  LoadAction load;
  SaveAction save;
  bool srgb;
} ColorAttachment;

typedef struct {
  uint32_t format;
  LoadAction load, stencilLoad;
  SaveAction save, stencilSave;
  bool enabled;
} DepthAttachment;

typedef struct {
  ColorAttachment color[MAX_ATTACHMENTS];
  DepthAttachment depth;
  uint32_t colorCount;
  uint32_t samples;
  uint32_t views;
  const char* label;
} CanvasInfo;

typedef void StencilCallback(void* userdata);

Canvas* lovrCanvasCreate(CanvasInfo* info);
void lovrCanvasDestroy(void* ref);
const CanvasInfo* lovrCanvasGetInfo(Canvas* canvas);
void lovrCanvasSetTextures(Canvas* canvas, AttachmentType type, Texture** textures, uint32_t index, uint32_t count);
void lovrCanvasBegin(Canvas* canvas);
void lovrCanvasFinish(Canvas* canvas);
bool lovrCanvasIsActive(Canvas* canvas);

bool lovrCanvasGetAlphaToCoverage(Canvas* canvas);
void lovrCanvasSetAlphaToCoverage(Canvas* canvas, bool enabled);
void lovrCanvasGetBlendMode(Canvas* canvas, uint32_t target, BlendMode* mode, BlendAlphaMode* alphaMode);
void lovrCanvasSetBlendMode(Canvas* canvas, uint32_t target, BlendMode mode, BlendAlphaMode alphaMode);
void lovrCanvasGetColorMask(Canvas* canvas, uint32_t target, bool* r, bool* g, bool* b, bool* a);
void lovrCanvasSetColorMask(Canvas* canvas, uint32_t target, bool r, bool g, bool b, bool a);
CullMode lovrCanvasGetCullMode(Canvas* canvas);
void lovrCanvasSetCullMode(Canvas* canvas, CullMode mode);
void lovrCanvasGetDepthTest(Canvas* canvas, CompareMode* test, bool* write);
void lovrCanvasSetDepthTest(Canvas* canvas, CompareMode test, bool write);
void lovrCanvasGetDepthNudge(Canvas* canvas, float* nudge, float* sloped, float* clamp);
void lovrCanvasSetDepthNudge(Canvas* canvas, float nudge, float sloped, float clamp);
bool lovrCanvasGetDepthClamp(Canvas* canvas);
void lovrCanvasSetDepthClamp(Canvas* canvas, bool clamp);
Shader* lovrCanvasGetShader(Canvas* canvas);
void lovrCanvasSetShader(Canvas* canvas, Shader* shader);
void lovrCanvasGetStencilTest(Canvas* canvas, CompareMode* test, uint8_t* value);
void lovrCanvasSetStencilTest(Canvas* canvas, CompareMode test, uint8_t value);
Winding lovrCanvasGetWinding(Canvas* canvas);
void lovrCanvasSetWinding(Canvas* canvas, Winding winding);
bool lovrCanvasIsWireframe(Canvas* canvas);
void lovrCanvasSetWireframe(Canvas* canvas, bool wireframe);

void lovrCanvasPush(Canvas* canvas);
void lovrCanvasPop(Canvas* canvas);
void lovrCanvasOrigin(Canvas* canvas);
void lovrCanvasTranslate(Canvas* canvas, float* translation);
void lovrCanvasRotate(Canvas* canvas, float* rotation);
void lovrCanvasScale(Canvas* canvas, float* scale);
void lovrCanvasTransform(Canvas* canvas, float* transform);
void lovrCanvasGetViewMatrix(Canvas* canvas, uint32_t index, float* viewMatrix);
void lovrCanvasSetViewMatrix(Canvas* canvas, uint32_t index, float* viewMatrix);
void lovrCanvasGetProjection(Canvas* canvas, uint32_t index, float* projection);
void lovrCanvasSetProjection(Canvas* canvas, uint32_t index, float* projection);

void lovrCanvasStencil(Canvas* canvas, StencilAction action, StencilAction depthFailAction, uint8_t value, StencilCallback* callback, void* userdata);

// Shader

typedef enum {
  SHADER_GRAPHICS,
  SHADER_COMPUTE
} ShaderType;

typedef struct {
  ShaderType type;
  struct Blob* vertex;
  struct Blob* fragment;
  struct Blob* compute;
  const char** dynamicBuffers;
  uint32_t dynamicBufferCount;
  const char* label;
} ShaderInfo;

Shader* lovrShaderCreate(ShaderInfo* info);
void lovrShaderDestroy(void* ref);
const ShaderInfo* lovrShaderGetInfo(Shader* shader);
bool lovrShaderResolveName(Shader* shader, uint64_t hash, uint32_t* group, uint32_t* id);
void lovrShaderCompute(Shader* shader, uint32_t x, uint32_t y, uint32_t z);
void lovrShaderComputeIndirect(Shader* shader, Buffer* buffer, uint32_t offset);

// Bundle

Bundle* lovrBundleCreate(Shader* shader, uint32_t group);
void lovrBundleDestroy(void* ref);
Shader* lovrBundleGetShader(Bundle* bundle);
uint32_t lovrBundleGetGroup(Bundle* bundle);
bool lovrBundleBindBuffer(Bundle* bundle, uint32_t id, uint32_t element, Buffer* buffer, uint32_t offset, uint32_t extent);
bool lovrBundleBindTexture(Bundle* bundle, uint32_t id, uint32_t element, Texture* texture);
