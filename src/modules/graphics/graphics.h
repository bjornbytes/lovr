#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Image;
struct ModelData;
struct Rasterizer;

typedef struct Buffer Buffer;
typedef struct Texture Texture;
typedef struct Sampler Sampler;
typedef struct Shader Shader;
typedef struct Material Material;
typedef struct Batch Batch;
typedef struct Model Model;
typedef struct Font Font;

typedef struct {
  uint32_t vendorId;
  uint32_t deviceId;
  const char* deviceName;
  const char* renderer;
  uint32_t driverMajor;
  uint32_t driverMinor;
  uint32_t driverPatch;
  uint32_t subgroupSize;
  bool discrete;
} GraphicsHardware;

typedef struct {
  bool bptc;
  bool astc;
  bool wireframe;
  bool depthClamp;
  bool clipDistance;
  bool cullDistance;
  bool fullIndexBufferRange;
  bool indirectDrawFirstInstance;
  bool dynamicIndexing;
  bool nonUniformIndexing;
  bool float64;
  bool int64;
  bool int16;
} GraphicsFeatures;

typedef struct {
  uint32_t textureSize2D;
  uint32_t textureSize3D;
  uint32_t textureSizeCube;
  uint32_t textureLayers;
  uint32_t renderSize[3];
  uint32_t uniformBufferRange;
  uint32_t storageBufferRange;
  uint32_t uniformBufferAlign;
  uint32_t storageBufferAlign;
  uint32_t vertexAttributes;
  uint32_t vertexBufferStride;
  uint32_t vertexShaderOutputs;
  uint32_t computeDispatchCount[3];
  uint32_t computeWorkgroupSize[3];
  uint32_t computeWorkgroupVolume;
  uint32_t computeSharedMemory;
  uint32_t shaderConstantSize;
  uint32_t indirectDrawCount;
  uint32_t instances;
  float anisotropy;
  float pointSize;
} GraphicsLimits;

typedef struct {
  uint32_t memory;
  uint32_t bufferMemory;
  uint32_t textureMemory;
  uint32_t buffers;
  uint32_t textures;
  uint32_t samplers;
  uint32_t shaders;
  uint32_t scratchMemory;
  uint32_t renderPasses;
  uint32_t computePasses;
  uint32_t transferPasses;
  uint32_t pipelineBinds;
  uint32_t bundleBinds;
  uint32_t drawCalls;
  uint32_t dispatches;
  uint32_t workgroups;
  uint32_t copies;
  float blocks;
  float canvases;
  float pipelines;
  float layouts;
  float bunches;
} GraphicsStats;

enum {
  TEXTURE_FEATURE_SAMPLE  = (1 << 0),
  TEXTURE_FEATURE_FILTER  = (1 << 1),
  TEXTURE_FEATURE_RENDER  = (1 << 2),
  TEXTURE_FEATURE_BLEND   = (1 << 3),
  TEXTURE_FEATURE_STORAGE = (1 << 4),
  TEXTURE_FEATURE_BLIT    = (1 << 5)
};

typedef enum {
  PASS_RENDER,
  PASS_COMPUTE,
  PASS_TRANSFER,
  PASS_BATCH
} PassType;

typedef enum {
  LOAD_KEEP,
  LOAD_CLEAR,
  LOAD_DISCARD
} LoadAction;

typedef struct {
  Texture* texture;
  uint32_t format;
  LoadAction load;
  float clear;
} DepthBuffer;

typedef struct {
  Texture* textures[4];
  LoadAction loads[4];
  float clears[4][4];
  DepthBuffer depth;
  uint32_t samples;
} Canvas;

typedef enum {
  STACK_TRANSFORM,
  STACK_PIPELINE,
  STACK_LABEL
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
  STENCIL_ZERO,
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
  FIELD_U10Nx3,
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

typedef enum {
  ATTRIBUTE_POSITION,
  ATTRIBUTE_NORMAL,
  ATTRIBUTE_TEXCOORD,
  ATTRIBUTE_COLOR,
  ATTRIBUTE_TANGENT,
  ATTRIBUTE_JOINTS,
  ATTRIBUTE_WEIGHTS
} DefaultAttribute;

typedef enum {
  DRAW_POINTS,
  DRAW_LINES,
  DRAW_TRIANGLES
} DrawMode;

typedef enum {
  SHADER_UNLIT,
  SHADER_FILL,
  SHADER_CUBE,
  SHADER_PANO,
  DEFAULT_SHADER_COUNT
} DefaultShader;

typedef enum {
  VERTEX_STANDARD,
  VERTEX_POSITION,
  VERTEX_SUPREME,
  VERTEX_SKINNED,
  VERTEX_EMPTY,
  VERTEX_FORMAT_COUNT
} VertexFormat;

typedef struct {
  DrawMode mode;
  DefaultShader shader;
  Material* material;
  struct {
    Buffer* buffer;
    VertexFormat format;
    const void* data;
    void** pointer;
    uint32_t count;
  } vertex;
  struct {
    Buffer* buffer;
    const void* data;
    void** pointer;
    uint32_t count;
    uint32_t stride;
  } index;
  uint32_t start;
  uint32_t count;
  uint32_t instances;
  uint32_t base;
  Buffer* indirect;
  uint32_t offset;
} DrawInfo;

typedef enum {
  ALIGN_LEFT,
  ALIGN_CENTER,
  ALIGN_RIGHT
} HorizontalAlign;

typedef enum {
  ALIGN_TOP,
  ALIGN_MIDDLE,
  ALIGN_BOTTOM
} VerticalAlign;

bool lovrGraphicsInit(bool debug, bool vsync, uint32_t blockSize);
void lovrGraphicsDestroy(void);

void lovrGraphicsGetHardware(GraphicsHardware* hardware);
void lovrGraphicsGetFeatures(GraphicsFeatures* features);
void lovrGraphicsGetLimits(GraphicsLimits* limits);
void lovrGraphicsGetStats(GraphicsStats* stats);
bool lovrGraphicsIsFormatSupported(uint32_t format, uint32_t features);

void lovrGraphicsPrepare(void);
void lovrGraphicsSubmit(void);
void lovrGraphicsWait(void);

void lovrGraphicsBeginRender(Canvas* canvas, uint32_t order);
void lovrGraphicsBeginCompute(uint32_t order);
void lovrGraphicsBeginTransfer(uint32_t order);
void lovrGraphicsBeginBatch(Batch* batch);
void lovrGraphicsFinish(void);

void lovrGraphicsGetBackground(float background[4]);
void lovrGraphicsSetBackground(float background[4]);
void lovrGraphicsGetViewMatrix(uint32_t index, float* viewMatrix);
void lovrGraphicsSetViewMatrix(uint32_t index, float* viewMatrix);
void lovrGraphicsGetProjection(uint32_t index, float* projection);
void lovrGraphicsSetProjection(uint32_t index, float* projection);
void lovrGraphicsSetViewport(float viewport[4], float depthRange[2]);
void lovrGraphicsSetScissor(uint32_t scissor[4]);

void lovrGraphicsPush(StackType type, const char* label);
void lovrGraphicsPop(StackType type);
void lovrGraphicsOrigin(void);
void lovrGraphicsTranslate(float* translation);
void lovrGraphicsRotate(float* rotation);
void lovrGraphicsScale(float* scale);
void lovrGraphicsTransform(float* transform);

void lovrGraphicsSetAlphaToCoverage(bool enabled);
void lovrGraphicsSetBlendMode(BlendMode mode, BlendAlphaMode alphaMode);
void lovrGraphicsSetColorMask(bool r, bool g, bool b, bool a);
void lovrGraphicsSetCullMode(CullMode mode);
void lovrGraphicsSetDepthTest(CompareMode test);
void lovrGraphicsSetDepthWrite(bool write);
void lovrGraphicsSetDepthOffset(float offset, float sloped);
void lovrGraphicsSetDepthClamp(bool clamp);
void lovrGraphicsSetShader(Shader* shader);
void lovrGraphicsSetStencilTest(CompareMode test, uint8_t value, uint8_t mask);
void lovrGraphicsSetStencilWrite(StencilAction actions[3], uint8_t value, uint8_t mask);
void lovrGraphicsSetWinding(Winding winding);
void lovrGraphicsSetWireframe(bool wireframe);

void lovrGraphicsSetBuffer(const char* name, size_t length, uint32_t slot, Buffer* buffer, uint32_t offset, uint32_t extent);
void lovrGraphicsSetTexture(const char* name, size_t length, uint32_t slot, Texture* texture);
void lovrGraphicsSetSampler(const char* name, size_t length, uint32_t slot, Sampler* sampler);
void lovrGraphicsSetConstant(const char* name, size_t length, void** data, FieldType* type);
void lovrGraphicsSetColor(float color[4]);

uint32_t lovrGraphicsMesh(DrawInfo* info, float* transform);
uint32_t lovrGraphicsPoints(Material* material, uint32_t count, float** vertices);
uint32_t lovrGraphicsLine(Material* material, uint32_t count, float** vertices);
uint32_t lovrGraphicsPlane(Material* material, float* transform, uint32_t detail);
uint32_t lovrGraphicsBox(Material* material, float* transform);
uint32_t lovrGraphicsCircle(Material* material, float* transform, uint32_t detail);
uint32_t lovrGraphicsCone(Material* material, float* transform, uint32_t detail);
uint32_t lovrGraphicsCylinder(Material* material, float* transform, uint32_t detail, bool capped);
uint32_t lovrGraphicsSphere(Material* material, float* transform, uint32_t detail);
uint32_t lovrGraphicsSkybox(Material* material);
uint32_t lovrGraphicsFill(Material* material);
void lovrGraphicsModel(Model* model, float* transform, uint32_t node, bool children, uint32_t instances);
uint32_t lovrGraphicsPrint(Font* font, const char* text, uint32_t length, float* transform, float wrap, HorizontalAlign halign, VerticalAlign valign);
void lovrGraphicsReplay(Batch* batch);

void lovrGraphicsCompute(uint32_t x, uint32_t y, uint32_t z, Buffer* buffer, uint32_t offset);

// Buffer

typedef enum {
  BUFFER_VERTEX,
  BUFFER_INDEX,
  BUFFER_UNIFORM,
  BUFFER_COMPUTE
} BufferType;

typedef struct {
  BufferType type;
  uint32_t length;
  uint32_t stride;
  uint32_t fieldCount;
  uint32_t locations[16];
  uint32_t offsets[16];
  FieldType types[16];
  VertexFormat format;
  uintptr_t handle;
} BufferInfo;

Buffer* lovrGraphicsGetBuffer(BufferInfo* info, void** data);
Buffer* lovrBufferCreate(BufferInfo* info, void** data);
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
  TEXTURE_SAMPLE  = (1 << 0),
  TEXTURE_RENDER  = (1 << 1),
  TEXTURE_STORAGE = (1 << 2),
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
  uint32_t usage;
  uint32_t format;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t mipmaps;
  uint32_t samples;
  bool srgb;
  uintptr_t handle;
  struct Image** images;
  const char* label;
} TextureInfo;

Texture* lovrGraphicsGetWindowTexture(void);
Texture* lovrGraphicsGetDefaultTexture(void);
Texture* lovrTextureCreate(TextureInfo* info);
Texture* lovrTextureCreateView(TextureViewInfo* view);
void lovrTextureDestroy(void* ref);
const TextureInfo* lovrTextureGetInfo(Texture* texture);
void lovrTextureWrite(Texture* texture, uint16_t offset[4], uint16_t extent[3], void* data, uint32_t step[2]);
void lovrTexturePaste(Texture* texture, struct Image* image, uint16_t srcOffset[4], uint16_t dstOffset[2], uint16_t extent[2]);
void lovrTextureClear(Texture* texture, uint16_t layer, uint16_t layerCount, uint16_t level, uint16_t levelCount, float color[4]);
void lovrTextureRead(Texture* texture, uint16_t offset[4], uint16_t extent[3], void (*callback)(void*, uint32_t, void*), void* userdata);
void lovrTextureCopy(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t extent[3]);
void lovrTextureBlit(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t srcExtent[3], uint16_t dstExtent[3], bool nearest);
void lovrTextureGenerateMipmaps(Texture* texture);

// Sampler

typedef enum {
  SAMPLER_NEAREST,
  SAMPLER_BILINEAR,
  SAMPLER_TRILINEAR,
  SAMPLER_ANISOTROPIC,
  DEFAULT_SAMPLER_COUNT
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

typedef struct {
  FilterMode min, mag, mip;
  WrapMode wrap[3];
  CompareMode compare;
  float anisotropy;
  float range[2];
} SamplerInfo;

Sampler* lovrSamplerCreate(SamplerInfo* info);
void lovrSamplerDestroy(void* ref);
const SamplerInfo* lovrSamplerGetInfo(Sampler* sampler);

// Shader

typedef enum {
  SHADER_GRAPHICS,
  SHADER_COMPUTE
} ShaderType;

typedef struct {
  const char* name;
  uint32_t id;
  double value;
} ShaderFlag;

typedef struct {
  ShaderType type;
  const void* source[2];
  uint32_t length[2];
  ShaderFlag* flags;
  uint32_t flagCount;
  const char* label;
} ShaderInfo;

Shader* lovrShaderCreate(ShaderInfo* info);
Shader* lovrShaderClone(Shader* shader, ShaderFlag* flags, uint32_t count);
Shader* lovrShaderCreateDefault(DefaultShader type, ShaderFlag* flags, uint32_t count);
Shader* lovrGraphicsGetDefaultShader(DefaultShader type);
void lovrShaderDestroy(void* ref);
const ShaderInfo* lovrShaderGetInfo(Shader* shader);

// Material

typedef enum {
  MATERIAL_BASIC,
  MATERIAL_PHYSICAL
} DefaultMaterial;

typedef enum {
  PROPERTY_SCALAR,
  PROPERTY_VECTOR,
  PROPERTY_TEXTURE
} MaterialPropertyType;

typedef struct {
  const char* name;
  MaterialPropertyType type;
  union {
    Texture* texture;
    float vector[4];
    double scalar;
  } value;
} MaterialProperty;

typedef struct {
  Shader* shader;
  DefaultMaterial type;
  uint32_t propertyCount;
  MaterialProperty* properties;
} MaterialInfo;

Material* lovrMaterialCreate(MaterialInfo* info);
void lovrMaterialDestroy(void* ref);

// Batch

typedef struct {
  Canvas* canvas;
  uint32_t capacity;
  uint32_t bufferSize;
  bool transient;
} BatchInfo;

typedef enum {
  SORT_OPAQUE,
  SORT_TRANSPARENT
} SortMode;

Batch* lovrBatchCreate(BatchInfo* info);
void lovrBatchDestroy(void* ref);
const BatchInfo* lovrBatchGetInfo(Batch* batch);
uint32_t lovrBatchGetCount(Batch* batch);
void lovrBatchReset(Batch* batch);
void lovrBatchSort(Batch* batch, SortMode mode);
void lovrBatchFilter(Batch* batch, bool (*predicate)(void* context, uint32_t i), void* context);

// Model

typedef struct {
  struct ModelData* data;
  DefaultMaterial material;
  Shader* shader;
} ModelInfo;

Model* lovrModelCreate(ModelInfo* info);
void lovrModelDestroy(void* ref);
struct ModelData* lovrModelGetModelData(Model* model);
void lovrModelResetPose(Model* model);

// Font

Font* lovrFontCreate(struct Rasterizer* rasterizer);
void lovrFontDestroy(void* ref);
