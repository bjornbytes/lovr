#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

struct Blob;
struct Image;
struct Rasterizer;
struct ModelData;

typedef struct Buffer Buffer;
typedef struct Texture Texture;
typedef struct Sampler Sampler;
typedef struct Shader Shader;
typedef struct Material Material;
typedef struct Font Font;
typedef struct Model Model;
typedef struct Readback Readback;
typedef struct Tally Tally;
typedef struct Pass Pass;

typedef struct {
  uint32_t deviceId;
  uint32_t vendorId;
  const char* name;
  const char* renderer;
  uint32_t subgroupSize;
  bool discrete;
} GraphicsDevice;

typedef struct {
  bool textureBC;
  bool textureASTC;
  bool wireframe;
  bool depthClamp;
  bool indirectDrawFirstInstance;
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
  uint32_t uniformBuffersPerStage;
  uint32_t storageBuffersPerStage;
  uint32_t sampledTexturesPerStage;
  uint32_t storageTexturesPerStage;
  uint32_t samplersPerStage;
  uint32_t resourcesPerShader;
  uint32_t uniformBufferRange;
  uint32_t storageBufferRange;
  uint32_t uniformBufferAlign;
  uint32_t storageBufferAlign;
  uint32_t vertexAttributes;
  uint32_t vertexBufferStride;
  uint32_t vertexShaderOutputs;
  uint32_t clipDistances;
  uint32_t cullDistances;
  uint32_t clipAndCullDistances;
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
  uint32_t pipelineSwitches;
  uint32_t bundleSwitches;
} GraphicsStats;

enum {
  TEXTURE_FEATURE_SAMPLE   = (1 << 0),
  TEXTURE_FEATURE_FILTER   = (1 << 1),
  TEXTURE_FEATURE_RENDER   = (1 << 2),
  TEXTURE_FEATURE_BLEND    = (1 << 3),
  TEXTURE_FEATURE_STORAGE  = (1 << 4),
  TEXTURE_FEATURE_ATOMIC   = (1 << 5),
  TEXTURE_FEATURE_BLIT_SRC = (1 << 6),
  TEXTURE_FEATURE_BLIT_DST = (1 << 7)
};

bool lovrGraphicsInit(bool debug, bool vsync);
void lovrGraphicsDestroy(void);

void lovrGraphicsGetDevice(GraphicsDevice* device);
void lovrGraphicsGetFeatures(GraphicsFeatures* features);
void lovrGraphicsGetLimits(GraphicsLimits* limits);
void lovrGraphicsGetStats(GraphicsStats* stats);
bool lovrGraphicsIsFormatSupported(uint32_t format, uint32_t features);

void lovrGraphicsGetBackground(float background[4]);
void lovrGraphicsSetBackground(float background[4]);
Font* lovrGraphicsGetDefaultFont(void);

void lovrGraphicsSubmit(Pass** passes, uint32_t count);
void lovrGraphicsWait(void);

// Buffer

typedef enum {
  FIELD_I8x4,
  FIELD_U8x4,
  FIELD_SN8x4,
  FIELD_UN8x4,
  FIELD_UN10x3,
  FIELD_I16,
  FIELD_I16x2,
  FIELD_I16x4,
  FIELD_U16,
  FIELD_U16x2,
  FIELD_U16x4,
  FIELD_SN16x2,
  FIELD_SN16x4,
  FIELD_UN16x2,
  FIELD_UN16x4,
  FIELD_I32,
  FIELD_I32x2,
  FIELD_I32x3,
  FIELD_I32x4,
  FIELD_U32,
  FIELD_U32x2,
  FIELD_U32x3,
  FIELD_U32x4,
  FIELD_F16x2,
  FIELD_F16x4,
  FIELD_F32,
  FIELD_F32x2,
  FIELD_F32x3,
  FIELD_F32x4,
  FIELD_MAT2,
  FIELD_MAT3,
  FIELD_MAT4,
  FIELD_INDEX16,
  FIELD_INDEX32
} FieldType;

typedef struct {
  uint32_t hash;
  uint32_t location;
  uint32_t type;
  uint32_t offset;
} BufferField;

typedef enum {
  LAYOUT_PACKED,
  LAYOUT_STD140,
  LAYOUT_STD430
} BufferLayout;

typedef struct {
  uint32_t length;
  uint32_t stride;
  uint32_t fieldCount;
  BufferField fields[16];
  const char* label;
  uintptr_t handle;
} BufferInfo;

Buffer* lovrGraphicsGetBuffer(BufferInfo* info, void** data);
Buffer* lovrBufferCreate(const BufferInfo* info, void** data);
void lovrBufferDestroy(void* ref);
const BufferInfo* lovrBufferGetInfo(Buffer* buffer);
bool lovrBufferIsTemporary(Buffer* buffer);
void* lovrBufferMap(Buffer* buffer, uint32_t offset, uint32_t size);
void lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t size);

// Texture

typedef enum {
  TEXTURE_2D,
  TEXTURE_3D,
  TEXTURE_CUBE,
  TEXTURE_ARRAY
} TextureType;

enum {
  TEXTURE_SAMPLE   = (1 << 0),
  TEXTURE_RENDER   = (1 << 1),
  TEXTURE_STORAGE  = (1 << 2),
  TEXTURE_TRANSFER = (1 << 3)
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
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t mipmaps;
  uint32_t samples;
  uint32_t usage;
  bool srgb;
  uintptr_t handle;
  uint32_t imageCount;
  struct Image** images;
  const char* label;
} TextureInfo;

Texture* lovrGraphicsGetWindowTexture(void);
Texture* lovrTextureCreate(const TextureInfo* info);
Texture* lovrTextureCreateView(const TextureViewInfo* view);
void lovrTextureDestroy(void* ref);
const TextureInfo* lovrTextureGetInfo(Texture* texture);

// Sampler

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

Sampler* lovrGraphicsGetDefaultSampler(FilterMode mode);
Sampler* lovrSamplerCreate(const SamplerInfo* info);
void lovrSamplerDestroy(void* ref);
const SamplerInfo* lovrSamplerGetInfo(Sampler* sampler);

// Shader

typedef enum {
  SHADER_UNLIT,
  SHADER_CUBE,
  SHADER_PANO,
  SHADER_FILL,
  SHADER_FONT,
  DEFAULT_SHADER_COUNT
} DefaultShader;

typedef enum {
  SHADER_GRAPHICS,
  SHADER_COMPUTE
} ShaderType;

typedef enum {
  STAGE_VERTEX,
  STAGE_FRAGMENT,
  STAGE_COMPUTE
} ShaderStage;

typedef struct {
  const void* code;
  size_t size;
} ShaderSource;

typedef struct {
  const char* name;
  uint32_t id;
  double value;
} ShaderFlag;

typedef struct {
  ShaderType type;
  ShaderSource source[2];
  uint32_t flagCount;
  ShaderFlag* flags;
  const char* label;
} ShaderInfo;

ShaderSource lovrGraphicsCompileShader(ShaderStage stage, ShaderSource* source);
Shader* lovrGraphicsGetDefaultShader(DefaultShader type);
Shader* lovrShaderCreate(const ShaderInfo* info);
Shader* lovrShaderClone(Shader* parent, ShaderFlag* flags, uint32_t count);
void lovrShaderDestroy(void* ref);
const ShaderInfo* lovrShaderGetInfo(Shader* shader);
bool lovrShaderHasStage(Shader* shader, ShaderStage stage);
bool lovrShaderHasAttribute(Shader* shader, const char* name, uint32_t location);
void lovrShaderGetLocalWorkgroupSize(Shader* shader, uint32_t size[3]);

// Material

typedef struct {
  float color[4];
  float glow[4];
  float uvShift[2];
  float uvScale[2];
  float sdfRange[2];
  float metalness;
  float roughness;
  float clearcoat;
  float clearcoatRoughness;
  float occlusionStrength;
  float glowStrength;
  float normalScale;
  float alphaCutoff;
  float pointSize;
} MaterialData;

typedef struct {
  MaterialData data;
  Texture* texture;
  Texture* glowTexture;
  Texture* occlusionTexture;
  Texture* metalnessTexture;
  Texture* roughnessTexture;
  Texture* clearcoatTexture;
  Texture* normalTexture;
} MaterialInfo;

Material* lovrMaterialCreate(const MaterialInfo* info);
void lovrMaterialDestroy(void* ref);
const MaterialInfo* lovrMaterialGetInfo(Material* material);

// Font

typedef struct {
  struct Rasterizer* rasterizer;
  double spread;
} FontInfo;

typedef struct ColoredString {
  float color[4];
  const char* string;
  size_t length;
} ColoredString;

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

typedef struct {
  struct { float x, y; } position;
  struct { uint16_t u, v; } uv;
  struct { uint8_t r, g, b, a; } color;
} GlyphVertex;

Font* lovrFontCreate(const FontInfo* info);
void lovrFontDestroy(void* ref);
const FontInfo* lovrFontGetInfo(Font* font);
float lovrFontGetPixelDensity(Font* font);
void lovrFontSetPixelDensity(Font* font, float pixelDensity);
float lovrFontGetLineSpacing(Font* font);
void lovrFontSetLineSpacing(Font* font, float spacing);
float lovrFontGetKerning(Font* font, uint32_t first, uint32_t second);
float lovrFontGetWidth(Font* font, ColoredString* strings, uint32_t count);
void lovrFontGetLines(Font* font, ColoredString* strings, uint32_t count, float wrap, void (*callback)(void* context, const char* string, size_t length), void* context);
void lovrFontGetVertices(Font* font, ColoredString* strings, uint32_t count, float wrap, HorizontalAlign halign, VerticalAlign valign, GlyphVertex* vertices, uint32_t* glyphCount, uint32_t* lineCount, Material** material, bool flip);

// Model

typedef struct {
  struct ModelData* data;
  bool mipmaps;
} ModelInfo;

typedef enum {
  SPACE_LOCAL,
  SPACE_GLOBAL
} CoordinateSpace;

typedef enum {
  MESH_POINTS,
  MESH_LINES,
  MESH_TRIANGLES
} MeshMode;

typedef struct {
  MeshMode mode;
  Material* material;
  uint32_t start;
  uint32_t count;
  uint32_t base;
  bool indexed;
} ModelDraw;

Model* lovrModelCreate(const ModelInfo* info);
void lovrModelDestroy(void* ref);
const ModelInfo* lovrModelGetInfo(Model* model);
uint32_t lovrModelGetNodeDrawCount(Model* model, uint32_t node);
void lovrModelGetNodeDraw(Model* model, uint32_t node, uint32_t index, ModelDraw* draw);
void lovrModelResetNodeTransforms(Model* model);
void lovrModelAnimate(Model* model, uint32_t animationIndex, float time, float alpha);
void lovrModelGetNodeTransform(Model* model, uint32_t node, float position[4], float scale[4], float rotation[4], CoordinateSpace space);
void lovrModelSetNodeTransform(Model* model, uint32_t node, float position[4], float scale[4], float rotation[4], float alpha);
Texture* lovrModelGetTexture(Model* model, uint32_t index);
Material* lovrModelGetMaterial(Model* model, uint32_t index);
Buffer* lovrModelGetVertexBuffer(Model* model);
Buffer* lovrModelGetIndexBuffer(Model* model);

// Readback

typedef enum {
  READBACK_BUFFER,
  READBACK_TEXTURE,
  READBACK_TALLY
} ReadbackType;

typedef struct {
  ReadbackType type;
  union {
    struct {
      Buffer* object;
      uint32_t offset;
      uint32_t extent;
    } buffer;
    struct {
      Texture* object;
      uint32_t offset[4];
      uint32_t extent[2];
    } texture;
    struct {
      Tally* object;
      uint32_t index;
      uint32_t count;
    } tally;
  };
} ReadbackInfo;

Readback* lovrReadbackCreate(const ReadbackInfo* info);
void lovrReadbackDestroy(void* ref);
const ReadbackInfo* lovrReadbackGetInfo(Readback* readback);
bool lovrReadbackIsComplete(Readback* readback);
bool lovrReadbackWait(Readback* readback);
void* lovrReadbackGetData(Readback* readback);
struct Image* lovrReadbackGetImage(Readback* readback);

// Tally

typedef enum {
  TALLY_TIMER,
  TALLY_PIXEL,
  TALLY_STAGE
} TallyType;

typedef struct {
  TallyType type;
  uint32_t count;
  uint32_t views;
} TallyInfo;

Tally* lovrTallyCreate(const TallyInfo* info);
void lovrTallyDestroy(void* ref);
const TallyInfo* lovrTallyGetInfo(Tally* tally);

// Pass

typedef enum {
  PASS_RENDER,
  PASS_COMPUTE,
  PASS_TRANSFER
} PassType;

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
  STYLE_FILL,
  STYLE_LINE
} DrawStyle;

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
  bool mipmap;
} Canvas;

typedef struct {
  PassType type;
  Canvas canvas;
  const char* label;
} PassInfo;

Pass* lovrGraphicsGetPass(PassInfo* info);
void lovrPassDestroy(void* ref);
const PassInfo* lovrPassGetInfo(Pass* pass);
void lovrPassGetViewMatrix(Pass* pass, uint32_t index, float viewMatrix[16]);
void lovrPassSetViewMatrix(Pass* pass, uint32_t index, float viewMatrix[16]);
void lovrPassGetProjection(Pass* pass, uint32_t index, float projection[16]);
void lovrPassSetProjection(Pass* pass, uint32_t index, float projection[16]);
void lovrPassPush(Pass* pass, StackType stack);
void lovrPassPop(Pass* pass, StackType stack);
void lovrPassOrigin(Pass* pass);
void lovrPassTranslate(Pass* pass, float* translation);
void lovrPassRotate(Pass* pass, float* rotation);
void lovrPassScale(Pass* pass, float* scale);
void lovrPassTransform(Pass* pass, float* transform);
void lovrPassSetAlphaToCoverage(Pass* pass, bool enabled);
void lovrPassSetBlendMode(Pass* pass, BlendMode mode, BlendAlphaMode alphaMode);
void lovrPassSetColor(Pass* pass, float color[4]);
void lovrPassSetColorWrite(Pass* pass, bool r, bool g, bool b, bool a);
void lovrPassSetCullMode(Pass* pass, CullMode mode);
void lovrPassSetDepthTest(Pass* pass, CompareMode test);
void lovrPassSetDepthWrite(Pass* pass, bool write);
void lovrPassSetDepthOffset(Pass* pass, float offset, float sloped);
void lovrPassSetDepthClamp(Pass* pass, bool clamp);
void lovrPassSetMaterial(Pass* pass, Material* material, Texture* texture);
void lovrPassSetMeshMode(Pass* pass, MeshMode mode);
void lovrPassSetSampler(Pass* pass, Sampler* sampler);
void lovrPassSetScissor(Pass* pass, uint32_t scissor[4]);
void lovrPassSetShader(Pass* pass, Shader* shader);
void lovrPassSetStencilTest(Pass* pass, CompareMode test, uint8_t value, uint8_t mask);
void lovrPassSetStencilWrite(Pass* pass, StencilAction actions[3], uint8_t value, uint8_t mask);
void lovrPassSetViewport(Pass* pass, float viewport[4], float depthRange[2]);
void lovrPassSetWinding(Pass* pass, Winding winding);
void lovrPassSetWireframe(Pass* pass, bool wireframe);
void lovrPassSendBuffer(Pass* pass, const char* name, size_t length, uint32_t slot, Buffer* buffer, uint32_t offset, uint32_t extent);
void lovrPassSendTexture(Pass* pass, const char* name, size_t length, uint32_t slot, Texture* texture);
void lovrPassSendSampler(Pass* pass, const char* name, size_t length, uint32_t slot, Sampler* sampler);
void lovrPassSendValue(Pass* pass, const char* name, size_t length, void** data, FieldType* type);
void lovrPassPoints(Pass* pass, uint32_t count, float** vertices);
void lovrPassLine(Pass* pass, uint32_t count, float** vertices);
void lovrPassPlane(Pass* pass, float* transform, DrawStyle style, uint32_t cols, uint32_t rows);
void lovrPassBox(Pass* pass, float* transform, DrawStyle style);
void lovrPassCircle(Pass* pass, float* transform, DrawStyle style, float angle1, float angle2, uint32_t segments);
void lovrPassSphere(Pass* pass, float* transform, uint32_t segmentsH, uint32_t segmentsV);
void lovrPassCylinder(Pass* pass, float* transform, bool capped, float angle1, float angle2, uint32_t segments);
void lovrPassCapsule(Pass* pass, float* transform, uint32_t segments);
void lovrPassTorus(Pass* pass, float* transform, uint32_t segmentsT, uint32_t segmentsP);
void lovrPassText(Pass* pass, Font* font, ColoredString* strings, uint32_t count, float* transform, float wrap, HorizontalAlign halign, VerticalAlign valign);
void lovrPassSkybox(Pass* pass, Texture* texture);
void lovrPassFill(Pass* pass, Texture* texture);
void lovrPassMonkey(Pass* pass, float* transform);
void lovrPassDrawModel(Pass* pass, Model* model, float* transform, uint32_t node, bool recurse, uint32_t instances);
void lovrPassMesh(Pass* pass, Buffer* vertices, Buffer* indices, float* transform, uint32_t start, uint32_t count, uint32_t instances);
void lovrPassMultimesh(Pass* pass, Buffer* vertices, Buffer* indices, Buffer* indirect, uint32_t count, uint32_t offset, uint32_t stride);
void lovrPassCompute(Pass* pass, uint32_t x, uint32_t y, uint32_t z, Buffer* indirect, uint32_t offset);
void lovrPassClearBuffer(Pass* pass, Buffer* buffer, uint32_t offset, uint32_t extent);
void lovrPassClearTexture(Pass* pass, Texture* texture, float value[4], uint32_t layer, uint32_t layerCount, uint32_t level, uint32_t levelCount);
void* lovrPassCopyDataToBuffer(Pass* pass, Buffer* buffer, uint32_t offset, uint32_t extent);
void lovrPassCopyBufferToBuffer(Pass* pass, Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t extent);
void lovrPassCopyTallyToBuffer(Pass* pass, Tally* src, Buffer* dst, uint32_t srcIndex, uint32_t dstOffset, uint32_t count);
void lovrPassCopyImageToTexture(Pass* pass, struct Image* src, Texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t extent[3]);
void lovrPassCopyTextureToTexture(Pass* pass, Texture* src, Texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t extent[3]);
void lovrPassBlit(Pass* pass, Texture* src, Texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t srcExtent[3], uint32_t dstExtent[3], FilterMode filter);
void lovrPassMipmap(Pass* pass, Texture* texture, uint32_t base, uint32_t count);
Readback* lovrPassReadBuffer(Pass* pass, Buffer* buffer, uint32_t index, uint32_t count);
Readback* lovrPassReadTexture(Pass* pass, Texture* texture, uint32_t offset[4], uint32_t extent[3]);
Readback* lovrPassReadTally(Pass* pass, Tally* tally, uint32_t index, uint32_t count);
void lovrPassTick(Pass* pass, Tally* tally, uint32_t index);
void lovrPassTock(Pass* pass, Tally* tally, uint32_t index);
