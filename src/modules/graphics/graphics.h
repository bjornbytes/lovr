#include <stdbool.h>
#include <stdint.h>

#pragma once

struct Image;

typedef struct Buffer Buffer;
typedef struct Texture Texture;
typedef struct Sampler Sampler;
typedef struct Shader Shader;
typedef struct Batch Batch;

typedef struct {
  uint32_t vendorId;
  uint32_t deviceId;
  const char* deviceName;
  const char* renderer;
  uint32_t driverMajor;
  uint32_t driverMinor;
  uint32_t driverPatch;
  bool discrete;
} GraphicsHardware;

typedef struct {
  bool bptc;
  bool astc;
  bool pointSize;
  bool wireframe;
  bool multiblend;
  bool anisotropy;
  bool depthClamp;
  bool depthNudgeClamp;
  bool clipDistance;
  bool cullDistance;
  bool fullIndexBufferRange;
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
  uint32_t renderWidth;
  uint32_t renderHeight;
  uint32_t renderViews;
  uint32_t bundleCount;
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
  uint32_t pointSize[2];
  float anisotropy;
} GraphicsLimits;

typedef enum {
  LOAD_KEEP,
  LOAD_CLEAR,
  LOAD_DISCARD
} LoadOp;

typedef enum {
  STORE_KEEP,
  STORE_DISCARD
} StoreOp;

typedef struct {
  struct { Texture *color[4], *depth; } textures;
  struct { LoadOp color[4], depth, stencil; } load;
  struct { StoreOp color[4], depth, stencil; } store;
  struct { float color[4][4], depth; uint8_t stencil; } clear;
  uint32_t depthFormat;
  uint32_t samples;
  uint32_t views;
} Canvas;

bool lovrGraphicsInit(bool debug, uint32_t blockSize);
void lovrGraphicsDestroy(void);
void lovrGraphicsGetHardware(GraphicsHardware* hardware);
void lovrGraphicsGetFeatures(GraphicsFeatures* features);
void lovrGraphicsGetLimits(GraphicsLimits* limits);
void lovrGraphicsBegin(void);
void lovrGraphicsSubmit(void);
void lovrGraphicsRender(Canvas* canvas, Batch** batches, uint32_t count, uint32_t order);
void lovrGraphicsCompute(Batch** batches, uint32_t count, uint32_t order);

// Buffer

enum {
  BUFFER_VERTEX   = (1 << 0),
  BUFFER_INDEX    = (1 << 1),
  BUFFER_UNIFORM  = (1 << 2),
  BUFFER_STORAGE  = (1 << 3),
  BUFFER_INDIRECT = (1 << 4),
  BUFFER_COPYFROM = (1 << 5),
  BUFFER_COPYTO   = (1 << 6)
};

typedef enum {
  FIELD_I8,
  FIELD_U8,
  FIELD_I16,
  FIELD_U16,
  FIELD_I32,
  FIELD_U32,
  FIELD_F32,
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
  uint32_t usage;
  uint32_t length;
  uint16_t stride;
  uint16_t fieldCount;
  uint16_t offsets[16];
  uint8_t types[16];
  uintptr_t handle;
  const char* label;
} BufferInfo;

Buffer* lovrGraphicsGetBuffer(BufferInfo* info);
Buffer* lovrBufferCreate(BufferInfo* info);
void lovrBufferDestroy(void* ref);
const BufferInfo* lovrBufferGetInfo(Buffer* buffer);
void* lovrBufferMap(Buffer* buffer, uint32_t offset, uint32_t size);
void lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t size);
void lovrBufferCopy(Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size);
void lovrBufferRead(Buffer* buffer, uint32_t offset, uint32_t size, void (*callback)(void*, uint32_t, void*), void* userdata);

// Texture

typedef enum {
  TEXTURE_2D,
  TEXTURE_CUBE,
  TEXTURE_VOLUME,
  TEXTURE_ARRAY
} TextureType;

enum {
  TEXTURE_SAMPLE   = (1 << 0),
  TEXTURE_RENDER   = (1 << 1),
  TEXTURE_STORAGE  = (1 << 2),
  TEXTURE_COPYFROM = (1 << 3),
  TEXTURE_COPYTO   = (1 << 4)
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
  uint32_t usage;
  uint32_t format;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t mipmaps;
  uint32_t samples;
  bool srgb;
  uintptr_t handle;
  const char* label;
} TextureInfo;

Texture* lovrGraphicsGetWindowTexture(void);
Texture* lovrTextureCreate(TextureInfo* info);
Texture* lovrTextureCreateView(TextureViewInfo* view);
void lovrTextureDestroy(void* ref);
const TextureInfo* lovrTextureGetInfo(Texture* texture);
Sampler* lovrTextureGetSampler(Texture* texture);
void lovrTextureSetSampler(Texture* texture, Sampler* sampler);
void lovrTextureWrite(Texture* texture, uint16_t offset[4], uint16_t extent[3], void* data, uint32_t step[2]);
void lovrTexturePaste(Texture* texture, struct Image* image, uint16_t srcOffset[4], uint16_t dstOffset[2], uint16_t extent[2]);
void lovrTextureClear(Texture* texture, uint16_t layer, uint16_t layerCount, uint16_t level, uint16_t levelCount, float color[4]);
void lovrTextureRead(Texture* texture, uint16_t offset[4], uint16_t extent[3], void (*callback)(void*, uint32_t, void*), void* userdata);
void lovrTextureCopy(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t extent[3]);
void lovrTextureBlit(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t srcExtent[3], uint16_t dstExtent[3], bool nearest);

// Sampler

typedef enum {
  SAMPLER_NEAREST,
  SAMPLER_BILINEAR,
  SAMPLER_TRILINEAR,
  SAMPLER_ANISOTROPIC,
  MAX_DEFAULT_SAMPLERS
} DefaultSampler;

typedef enum {
  FILTER_NEAREST,
  FILTER_LINEAR
} FilterMode;

typedef enum {
  WRAP_CLAMP,
  WRAP_REPEAT,
  WRAP_MIRROR
} WrapMode;

typedef enum {
  COMPARE_NONE,
  COMPARE_EQUAL,
  COMPARE_NEQUAL,
  COMPARE_LESS,
  COMPARE_LEQUAL,
  COMPARE_GREATER,
  COMPARE_GEQUAL
} CompareMode;

typedef struct {
  FilterMode min, mag, mip;
  WrapMode wrap[3];
  CompareMode compare;
  float anisotropy;
  float range[2];
} SamplerInfo;

Sampler* lovrSamplerCreate(SamplerInfo* info);
Sampler* lovrGraphicsGetDefaultSampler(DefaultSampler type);
void lovrSamplerDestroy(void* ref);
const SamplerInfo* lovrSamplerGetInfo(Sampler* sampler);

// Shader

typedef enum {
  SHADER_GRAPHICS,
  SHADER_COMPUTE
} ShaderType;

typedef struct {
  ShaderType type;
  const void* source[2];
  uint32_t length[2];
  const char** dynamicBuffers;
  uint32_t dynamicBufferCount;
  const char* label;
} ShaderInfo;

Shader* lovrShaderCreate(ShaderInfo* info);
void lovrShaderDestroy(void* ref);
const ShaderInfo* lovrShaderGetInfo(Shader* shader);
bool lovrShaderResolveName(Shader* shader, uint64_t hash, uint32_t* group, uint32_t* id);

// Batch

typedef enum {
  BATCH_RENDER,
  BATCH_COMPUTE
} BatchType;

typedef struct {
  BatchType type;
  uint32_t capacity;
  uint32_t geometryMemory;
} BatchInfo;

typedef enum {
  STACK_TRANSFORM,
  STACK_PIPELINE
} StackType;

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
  CULL_NONE,
  CULL_FRONT,
  CULL_BACK
} CullMode;

typedef enum {
  DRAW_POINTS,
  DRAW_LINES,
  DRAW_TRIANGLES
} DrawMode;

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

typedef enum {
  DRAW_LINE,
  DRAW_FILL
} DrawStyle;

Batch* lovrGraphicsGetBatch(BatchInfo* info);
Batch* lovrBatchCreate(BatchInfo* info);
void lovrBatchDestroy(void* ref);
const BatchInfo* lovrBatchGetInfo(Batch* batch);
uint32_t lovrBatchGetCount(Batch* batch);
void lovrBatchReset(Batch* batch);
void lovrBatchGetViewMatrix(Batch* batch, uint32_t index, float* viewMatrix);
void lovrBatchSetViewMatrix(Batch* batch, uint32_t index, float* viewMatrix);
void lovrBatchGetProjection(Batch* batch, uint32_t index, float* projection);
void lovrBatchSetProjection(Batch* batch, uint32_t index, float* projection);
void lovrBatchPush(Batch* batch, StackType type);
void lovrBatchPop(Batch* batch, StackType type);
void lovrBatchOrigin(Batch* batch);
void lovrBatchTranslate(Batch* batch, float* translation);
void lovrBatchRotate(Batch* batch, float* rotation);
void lovrBatchScale(Batch* batch, float* scale);
void lovrBatchTransform(Batch* batch, float* transform);
void lovrBatchSetAlphaToCoverage(Batch* batch, bool enabled);
void lovrBatchSetBlendMode(Batch* batch, BlendMode mode, BlendAlphaMode alphaMode);
void lovrBatchSetColorMask(Batch* batch, bool r, bool g, bool b, bool a);
void lovrBatchSetCullMode(Batch* batch, CullMode mode);
void lovrBatchSetDepthTest(Batch* batch, CompareMode test, bool write);
void lovrBatchSetDepthNudge(Batch* batch, float nudge, float sloped, float clamp);
void lovrBatchSetDepthClamp(Batch* batch, bool clamp);
void lovrBatchSetShader(Batch* batch, Shader* shader);
void lovrBatchSetStencilTest(Batch* batch, CompareMode test, uint8_t value);
void lovrBatchSetWinding(Batch* batch, Winding winding);
void lovrBatchSetWireframe(Batch* batch, bool wireframe);
void lovrBatchCube(Batch* batch, DrawStyle style, float* transform);
