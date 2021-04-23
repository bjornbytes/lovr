#include <stdbool.h>
#include <stdint.h>

#pragma once

struct Blob;
struct WindowFlags;

typedef struct Buffer Buffer;
typedef struct Texture Texture;
typedef struct Shader Shader;
typedef struct Bundle Bundle;

typedef void StencilCallback(void* userdata);

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

typedef enum {
  LOAD_KEEP,
  LOAD_CLEAR,
  LOAD_DISCARD
} LoadOp;

typedef enum {
  SAVE_KEEP,
  SAVE_DISCARD
} SaveOp;

typedef struct {
  struct {
    Texture* texture;
    Texture* resolve;
    LoadOp load;
    SaveOp save;
    float clear[4];
  } color[4];
  struct {
    bool enabled;
    Texture* texture;
    uint32_t format;
    LoadOp load;
    SaveOp save;
    float clear;
    struct {
      LoadOp load;
      SaveOp save;
      uint8_t clear;
    } stencil;
  } depth;
  uint32_t samples;
} Canvas;

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
  WINDING_COUNTERCLOCKWISE,
  WINDING_CLOCKWISE
} Winding;

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
void lovrGraphicsRender(Canvas* canvas);
void lovrGraphicsEndPass(void);
void lovrGraphicsBind(uint32_t group, Bundle* bundle);
bool lovrGraphicsGetAlphaToCoverage(void);
void lovrGraphicsSetAlphaToCoverage(bool enabled);
void lovrGraphicsGetBlendMode(uint32_t target, BlendMode* mode, BlendAlphaMode* alphaMode);
void lovrGraphicsSetBlendMode(uint32_t target, BlendMode mode, BlendAlphaMode alphaMode);
void lovrGraphicsGetColorMask(uint32_t target, bool* r, bool* g, bool* b, bool* a);
void lovrGraphicsSetColorMask(uint32_t target, bool r, bool g, bool b, bool a);
CullMode lovrGraphicsGetCullMode(void);
void lovrGraphicsSetCullMode(CullMode mode);
void lovrGraphicsGetDepthTest(CompareMode* test, bool* write);
void lovrGraphicsSetDepthTest(CompareMode test, bool write);
void lovrGraphicsGetDepthNudge(float* nudge, float* sloped, float* clamp);
void lovrGraphicsSetDepthNudge(float nudge, float sloped, float clamp);
bool lovrGraphicsGetDepthClamp(void);
void lovrGraphicsSetDepthClamp(bool clamp);
Shader* lovrGraphicsGetShader(void);
void lovrGraphicsSetShader(Shader* shader);
void lovrGraphicsGetStencilTest(CompareMode* test, uint8_t* value);
void lovrGraphicsSetStencilTest(CompareMode test, uint8_t value);
Winding lovrGraphicsGetWinding(void);
void lovrGraphicsSetWinding(Winding winding);
bool lovrGraphicsIsWireframe(void);
void lovrGraphicsSetWireframe(bool wireframe);
void lovrGraphicsPush(void);
void lovrGraphicsPop(void);
void lovrGraphicsOrigin(void);
void lovrGraphicsTranslate(float* translation);
void lovrGraphicsRotate(float* rotation);
void lovrGraphicsScale(float* scale);
void lovrGraphicsTransform(float* transform);
void lovrGraphicsGetViewMatrix(uint32_t index, float* viewMatrix);
void lovrGraphicsSetViewMatrix(uint32_t index, float* viewMatrix);
void lovrGraphicsGetProjection(uint32_t index, float* projection);
void lovrGraphicsSetProjection(uint32_t index, float* projection);
void lovrGraphicsStencil(StencilAction action, StencilAction depthFailAction, uint8_t value, StencilCallback* callback, void* userdata);

// Buffer

typedef enum {
  BUFFER_STATIC,
  BUFFER_DYNAMIC,
  BUFFER_STREAM
} BufferType;

typedef enum {
  BUFFER_VERTEX,
  BUFFER_INDEX,
  BUFFER_UNIFORM,
  BUFFER_COMPUTE,
  BUFFER_INDIRECT,
  BUFFER_COPY_SRC,
  BUFFER_COPY_DST
} BufferUsage;

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
  uint16_t count;
  uint16_t stride;
  FieldType types[16];
  uint16_t offsets[16];
} BufferFormat;

typedef struct {
  uint32_t size;
  uint32_t usage;
  BufferType type;
  void** mapping;
  BufferFormat format;
  const char* label;
} BufferInfo;

Buffer* lovrBufferCreate(BufferInfo* info);
void lovrBufferDestroy(void* ref);
const BufferInfo* lovrBufferGetInfo(Buffer* buffer);
void* lovrBufferMap(Buffer* buffer);

// Texture

typedef enum {
  TEXTURE_2D,
  TEXTURE_CUBE,
  TEXTURE_VOLUME,
  TEXTURE_ARRAY
} TextureType;

typedef enum {
  TEXTURE_SAMPLE,
  TEXTURE_RENDER,
  TEXTURE_COMPUTE,
  TEXTURE_UPLOAD,
  TEXTURE_DOWNLOAD
} TextureUsage;

typedef struct {
  Texture* source;
  TextureType type;
  uint32_t layerIndex;
  uint32_t layerCount;
  uint32_t mipmapIndex;
  uint32_t mipmapCount;
} TextureView;

typedef struct {
  TextureView view;
  TextureType type;
  uint32_t format;
  uint32_t size[3];
  uint32_t mipmaps;
  uint32_t samples;
  uint32_t usage;
  bool srgb;
  const char* label;
} TextureInfo;

Texture* lovrTextureCreate(TextureInfo* info);
Texture* lovrTextureCreateView(TextureView* view);
void lovrTextureDestroy(void* ref);
const TextureInfo* lovrTextureGetInfo(Texture* texture);
void lovrTextureGetPixels(Texture* texture, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t layer, uint32_t level, void (*callback)(void* data, uint64_t size, void* context), void* context);

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

// Bundle

Bundle* lovrBundleCreate(Shader* shader, uint32_t group);
void lovrBundleDestroy(void* ref);
uint32_t lovrBundleGetGroup(Bundle* bundle);
bool lovrBundleBindBuffer(Bundle* bundle, uint32_t id, uint32_t element, Buffer* buffer, uint32_t offset, uint32_t extent);
bool lovrBundleBindTexture(Bundle* bundle, uint32_t id, uint32_t element, Texture* texture);
