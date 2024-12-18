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
typedef struct Mesh Mesh;
typedef struct Model Model;
typedef struct Readback Readback;
typedef struct Pass Pass;

typedef struct {
  bool debug;
  bool vsync;
  bool stencil;
  bool antialias;
  void* cacheData;
  size_t cacheSize;
} GraphicsConfig;

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
  bool depthResolve;
  bool indirectDrawFirstInstance;
  bool packedBuffers;
  bool float64;
  bool int64;
  bool int16;
} GraphicsFeatures;

typedef struct {
  uint32_t textureSize2D;
  uint32_t textureSize3D;
  uint32_t textureSizeCube;
  uint32_t textureLayers;
  uint32_t textureSamples;
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
  uint32_t workgroupCount[3];
  uint32_t workgroupSize[3];
  uint32_t totalWorkgroupSize;
  uint32_t computeSharedMemory;
  uint32_t indirectDrawCount;
  uint32_t instances;
  float anisotropy;
  float pointSize;
} GraphicsLimits;

enum {
  TEXTURE_FEATURE_SAMPLE  = (1 << 0),
  TEXTURE_FEATURE_RENDER  = (1 << 1),
  TEXTURE_FEATURE_STORAGE = (1 << 2),
  TEXTURE_FEATURE_BLIT    = (1 << 3)
};

bool lovrGraphicsInit(GraphicsConfig* config);
void lovrGraphicsDestroy(void);
bool lovrGraphicsIsInitialized(void);

void lovrGraphicsGetDevice(GraphicsDevice* device);
void lovrGraphicsGetFeatures(GraphicsFeatures* features);
void lovrGraphicsGetLimits(GraphicsLimits* limits);
uint32_t lovrGraphicsGetFormatSupport(uint32_t format, uint32_t features);
void lovrGraphicsGetShaderCache(void* data, size_t* size);

void lovrGraphicsGetBackgroundColor(float background[4]);
void lovrGraphicsSetBackgroundColor(float background[4]);

bool lovrGraphicsIsTimingEnabled(void);
void lovrGraphicsSetTimingEnabled(bool enable);
bool lovrGraphicsSubmit(Pass** passes, uint32_t count);
bool lovrGraphicsPresent(void);
bool lovrGraphicsWait(void);

// Buffer

typedef enum {
  TYPE_I8x4,
  TYPE_U8x4,
  TYPE_SN8x4,
  TYPE_UN8x4,
  TYPE_SN10x3,
  TYPE_UN10x3,
  TYPE_I16,
  TYPE_I16x2,
  TYPE_I16x4,
  TYPE_U16,
  TYPE_U16x2,
  TYPE_U16x4,
  TYPE_SN16x2,
  TYPE_SN16x4,
  TYPE_UN16x2,
  TYPE_UN16x4,
  TYPE_I32,
  TYPE_I32x2,
  TYPE_I32x3,
  TYPE_I32x4,
  TYPE_U32,
  TYPE_U32x2,
  TYPE_U32x3,
  TYPE_U32x4,
  TYPE_F16x2,
  TYPE_F16x4,
  TYPE_F32,
  TYPE_F32x2,
  TYPE_F32x3,
  TYPE_F32x4,
  TYPE_MAT2,
  TYPE_MAT3,
  TYPE_MAT4,
  TYPE_INDEX16,
  TYPE_INDEX32
} DataType;

typedef struct DataField {
  const char* name;
  uint32_t hash;
  DataType type;
  uint32_t offset;
  uint32_t length;
  uint32_t stride;
  uint32_t fieldCount;
  struct DataField* fields;
  struct DataField* parent;
} DataField;

typedef enum {
  LAYOUT_PACKED,
  LAYOUT_STD140,
  LAYOUT_STD430
} DataLayout;

uint32_t lovrGraphicsAlignFields(DataField* parent, DataLayout layout);

typedef struct {
  uint32_t size;
  uint32_t fieldCount;
  DataField* format;
  uintptr_t handle;
} BufferInfo;

Buffer* lovrBufferCreate(const BufferInfo* info, void** data);
void lovrBufferDestroy(void* ref);
const BufferInfo* lovrBufferGetInfo(Buffer* buffer);
void* lovrBufferGetData(Buffer* buffer, uint32_t offset, uint32_t extent);
void* lovrBufferSetData(Buffer* buffer, uint32_t offset, uint32_t extent);
bool lovrBufferCopy(Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t extent);
bool lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t extent, uint32_t value);

// Texture

typedef enum {
  TEXTURE_2D,
  TEXTURE_3D,
  TEXTURE_CUBE,
  TEXTURE_ARRAY
} TextureType;

enum {
  TEXTURE_SAMPLE    = (1 << 0),
  TEXTURE_RENDER    = (1 << 1),
  TEXTURE_STORAGE   = (1 << 2),
  TEXTURE_TRANSFER  = (1 << 3),
  TEXTURE_FOVEATION = (1 << 4)
};

typedef struct {
  TextureType type;
  uint32_t format;
  uint32_t width;
  uint32_t height;
  uint32_t layers;
  uint32_t mipmaps;
  uint32_t samples;
  uint32_t usage;
  bool srgb;
  bool xr;
  uint32_t imageCount;
  struct Image** images;
  const char* label;
  uintptr_t handle;
} TextureInfo;

typedef struct {
  TextureType type;
  uint32_t layerIndex;
  uint32_t layerCount;
  uint32_t levelIndex;
  uint32_t levelCount;
  const char* label;
} TextureViewInfo;

typedef enum {
  FILTER_NEAREST,
  FILTER_LINEAR
} FilterMode;

bool lovrGraphicsGetWindowTexture(Texture** texture);
Texture* lovrTextureCreate(const TextureInfo* info);
Texture* lovrTextureCreateView(Texture* parent, const TextureViewInfo* info);
void lovrTextureDestroy(void* ref);
const TextureInfo* lovrTextureGetInfo(Texture* texture);
struct Image* lovrTextureGetPixels(Texture* texture, uint32_t offset[4], uint32_t extent[3]);
bool lovrTextureSetPixels(Texture* texture, struct Image* image, uint32_t texOffset[4], uint32_t imgOffset[4], uint32_t extent[3]);
bool lovrTextureCopy(Texture* src, Texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t extent[3]);
bool lovrTextureBlit(Texture* src, Texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t srcExtent[3], uint32_t dstExtent[3], FilterMode filter);
bool lovrTextureClear(Texture* texture, float value[4], uint32_t layer, uint32_t layerCount, uint32_t level, uint32_t levelCount);
bool lovrTextureGenerateMipmaps(Texture* texture, uint32_t base, uint32_t count);
Sampler* lovrTextureGetSampler(Texture* texture);
void lovrTextureSetSampler(Texture* texture, Sampler* sampler);
Material* lovrTextureToMaterial(Texture* texture);

// Sampler

typedef enum {
  WRAP_CLAMP,
  WRAP_REPEAT,
  WRAP_MIRROR,
  WRAP_BORDER
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
  SHADER_NORMAL,
  SHADER_FONT,
  SHADER_CUBEMAP,
  SHADER_EQUIRECT,
  SHADER_FILL_2D,
  SHADER_FILL_ARRAY,
  SHADER_MASK,
  SHADER_ANIMATOR,
  SHADER_BLENDER,
  SHADER_TALLY_MERGE,
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
  ShaderStage stage;
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
  ShaderSource* stages;
  uint32_t stageCount;
  ShaderFlag* flags;
  uint32_t flagCount;
  const char* label;
  bool isDefault;
  bool raw;
} ShaderInfo;

typedef void* ShaderIncluder(const char* filename, size_t* bytesRead);

bool lovrGraphicsCompileShader(ShaderSource* stages, ShaderSource* outputs, uint32_t count, ShaderIncluder* includer, bool raw);
ShaderSource lovrGraphicsGetDefaultShaderSource(DefaultShader type, ShaderStage stage);
Shader* lovrGraphicsGetDefaultShader(DefaultShader type);
Shader* lovrShaderCreate(const ShaderInfo* info);
Shader* lovrShaderClone(Shader* parent, ShaderFlag* flags, uint32_t count);
void lovrShaderDestroy(void* ref);
const ShaderInfo* lovrShaderGetInfo(Shader* shader);
bool lovrShaderHasStage(Shader* shader, ShaderStage stage);
bool lovrShaderHasAttribute(Shader* shader, const char* name, uint32_t location);
bool lovrShaderHasVariable(Shader* shader, const char* name);
void lovrShaderGetWorkgroupSize(Shader* shader, uint32_t size[3]);
const DataField* lovrShaderGetBufferFormat(Shader* shader, const char* name, uint32_t* fieldCount);

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
  float normalScale;
  float alphaCutoff;
} MaterialData;

typedef struct {
  MaterialData data;
  Texture* texture;
  Texture* glowTexture;
  Texture* metalnessTexture;
  Texture* roughnessTexture;
  Texture* clearcoatTexture;
  Texture* occlusionTexture;
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

Font* lovrGraphicsGetDefaultFont(void);
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
bool lovrFontGetVertices(Font* font, ColoredString* strings, uint32_t count, float wrap, HorizontalAlign halign, VerticalAlign valign, GlyphVertex* vertices, uint32_t* glyphCount, uint32_t* lineCount, Material** material, bool flip);

// Mesh

typedef enum {
  MESH_CPU,
  MESH_GPU
} MeshStorage;

typedef enum {
  DRAW_POINTS,
  DRAW_LINES,
  DRAW_TRIANGLES
} DrawMode;

typedef struct {
  Buffer* vertexBuffer;
  DataField* vertexFormat;
  MeshStorage storage;
} MeshInfo;

Mesh* lovrMeshCreate(const MeshInfo* info, void** data);
void lovrMeshDestroy(void* ref);
const DataField* lovrMeshGetVertexFormat(Mesh* mesh);
Buffer* lovrMeshGetVertexBuffer(Mesh* mesh);
Buffer* lovrMeshGetIndexBuffer(Mesh* mesh);
bool lovrMeshSetIndexBuffer(Mesh* mesh, Buffer* buffer);
void* lovrMeshGetVertices(Mesh* mesh, uint32_t index, uint32_t count);
void* lovrMeshSetVertices(Mesh* mesh, uint32_t index, uint32_t count);
bool lovrMeshGetIndices(Mesh* mesh, void** indices, uint32_t* count, DataType* type);
void* lovrMeshSetIndices(Mesh* mesh, uint32_t count, DataType type);
bool lovrMeshGetTriangles(Mesh* mesh, float** vertices, uint32_t** indices, uint32_t* vertexCount, uint32_t* indexCount);
bool lovrMeshGetBoundingBox(Mesh* mesh, float box[6]);
void lovrMeshSetBoundingBox(Mesh* mesh, float box[6]);
bool lovrMeshComputeBoundingBox(Mesh* mesh);
DrawMode lovrMeshGetDrawMode(Mesh* mesh);
void lovrMeshSetDrawMode(Mesh* mesh, DrawMode mode);
void lovrMeshGetDrawRange(Mesh* mesh, uint32_t* start, uint32_t* count, uint32_t* offset);
bool lovrMeshSetDrawRange(Mesh* mesh, uint32_t start, uint32_t count, uint32_t offset);
Material* lovrMeshGetMaterial(Mesh* mesh);
void lovrMeshSetMaterial(Mesh* mesh, Material* material);

// Model

typedef struct {
  struct ModelData* data;
  bool materials;
  bool mipmaps;
} ModelInfo;

typedef enum {
  ORIGIN_ROOT,
  ORIGIN_PARENT
} OriginType;

Model* lovrModelCreate(const ModelInfo* info);
Model* lovrModelClone(Model* model);
void lovrModelDestroy(void* ref);
const ModelInfo* lovrModelGetInfo(Model* model);
void lovrModelResetNodeTransforms(Model* model);
void lovrModelResetBlendShapes(Model* model);
bool lovrModelAnimate(Model* model, uint32_t animationIndex, float time, float alpha);
float lovrModelGetBlendShapeWeight(Model* model, uint32_t index);
void lovrModelSetBlendShapeWeight(Model* model, uint32_t index, float weight);
void lovrModelGetNodeTransform(Model* model, uint32_t node, float* position, float* scale, float* rotation, OriginType origin);
void lovrModelSetNodeTransform(Model* model, uint32_t node, float* position, float* scale, float* rotation, float alpha);
Buffer* lovrModelGetVertexBuffer(Model* model);
Buffer* lovrModelGetIndexBuffer(Model* model);
Mesh* lovrModelGetMesh(Model* model, uint32_t index);
Texture* lovrModelGetTexture(Model* model, uint32_t index);
Material* lovrModelGetMaterial(Model* model, uint32_t index);

// Readback

Readback* lovrReadbackCreateBuffer(Buffer* buffer, uint32_t offset, uint32_t extent);
Readback* lovrReadbackCreateTexture(Texture* texture, uint32_t offset[4], uint32_t extent[3]);
void lovrReadbackDestroy(void* ref);
bool lovrReadbackIsComplete(Readback* readback);
bool lovrReadbackWait(Readback* readback, bool* waited);
void* lovrReadbackGetData(Readback* readback, DataField** format, uint32_t* count);
struct Blob* lovrReadbackGetBlob(Readback* readback);
struct Image* lovrReadbackGetImage(Readback* readback);

// Pass

typedef struct {
  uint32_t draws;
  uint32_t computes;
  uint32_t drawsCulled;
  size_t cpuMemoryReserved;
  size_t cpuMemoryUsed;
  double submitTime;
  double gpuTime;
} PassStats;

typedef enum {
  LOAD_CLEAR,
  LOAD_DISCARD,
  LOAD_KEEP
} LoadAction;

typedef struct {
  Texture* texture;
  Texture* resolve;
} CanvasTexture;

typedef enum {
  STACK_TRANSFORM,
  STACK_STATE
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

bool lovrGraphicsGetWindowPass(Pass** pass);
Pass* lovrPassCreate(const char* label);
void lovrPassDestroy(void* ref);
void lovrPassReset(Pass* pass);
const PassStats* lovrPassGetStats(Pass* pass);
const char* lovrPassGetLabel(Pass* pass);

void lovrPassGetCanvas(Pass* pass, CanvasTexture color[4], CanvasTexture* depth, uint32_t* depthFormat, Texture** foveation, uint32_t* samples);
bool lovrPassSetCanvas(Pass* pass, CanvasTexture color[4], CanvasTexture* depth, uint32_t depthFormat, Texture* foveation, uint32_t samples);
void lovrPassGetClear(Pass* pass, LoadAction loads[4], float clears[4][4], LoadAction* depthLoad, float* depthClear);
bool lovrPassSetClear(Pass* pass, LoadAction loads[4], float clears[4][4], LoadAction depthLoad, float depthClear);
uint32_t lovrPassGetAttachmentCount(Pass* pass, bool* depth);
uint32_t lovrPassGetWidth(Pass* pass);
uint32_t lovrPassGetHeight(Pass* pass);

uint32_t lovrPassGetViewCount(Pass* pass);
bool lovrPassGetViewMatrix(Pass* pass, uint32_t index, float viewMatrix[16]);
bool lovrPassSetViewMatrix(Pass* pass, uint32_t index, float viewMatrix[16]);
bool lovrPassGetProjection(Pass* pass, uint32_t index, float projection[16]);
bool lovrPassSetProjection(Pass* pass, uint32_t index, float projection[16]);

bool lovrPassPush(Pass* pass, StackType stack);
bool lovrPassPop(Pass* pass, StackType stack);
void lovrPassOrigin(Pass* pass);
void lovrPassTranslate(Pass* pass, float* translation);
void lovrPassRotate(Pass* pass, float* rotation);
void lovrPassScale(Pass* pass, float* scale);
void lovrPassTransform(Pass* pass, float* transform);

void lovrPassSetAlphaToCoverage(Pass* pass, bool enabled);
void lovrPassSetBlendMode(Pass* pass, uint32_t index, BlendMode mode, BlendAlphaMode alphaMode);
void lovrPassSetColor(Pass* pass, float color[4]);
void lovrPassSetColorWrite(Pass* pass, uint32_t index, bool r, bool g, bool b, bool a);
void lovrPassSetDepthTest(Pass* pass, CompareMode test);
void lovrPassSetDepthWrite(Pass* pass, bool write);
void lovrPassSetDepthOffset(Pass* pass, float offset, float sloped);
void lovrPassSetDepthClamp(Pass* pass, bool clamp);
void lovrPassSetFaceCull(Pass* pass, CullMode mode);
void lovrPassSetFont(Pass* pass, Font* font);
void lovrPassSetMaterial(Pass* pass, Material* material);
void lovrPassSetMeshMode(Pass* pass, DrawMode mode);
void lovrPassSetSampler(Pass* pass, Sampler* sampler);
void lovrPassSetScissor(Pass* pass, uint32_t scissor[4]);
void lovrPassSetShader(Pass* pass, Shader* shader);
bool lovrPassSetStencilTest(Pass* pass, CompareMode test, uint8_t value, uint8_t mask);
bool lovrPassSetStencilWrite(Pass* pass, StencilAction actions[3], uint8_t value, uint8_t mask);
void lovrPassSetViewCull(Pass* pass, bool enable);
void lovrPassSetViewport(Pass* pass, float viewport[6]);
void lovrPassSetWinding(Pass* pass, Winding winding);
void lovrPassSetWireframe(Pass* pass, bool wireframe);

bool lovrPassSendBuffer(Pass* pass, const char* name, size_t length, Buffer* buffer, uint32_t offset, uint32_t extent);
bool lovrPassSendTexture(Pass* pass, const char* name, size_t length, Texture* texture);
bool lovrPassSendSampler(Pass* pass, const char* name, size_t length, Sampler* sampler);
bool lovrPassSendData(Pass* pass, const char* name, size_t length, void** data, DataField** format);

bool lovrPassPoints(Pass* pass, uint32_t count, float** vertices);
bool lovrPassLine(Pass* pass, uint32_t count, float** vertices);
bool lovrPassPolygon(Pass* pass, uint32_t count, float** vertices);
bool lovrPassPlane(Pass* pass, float* transform, DrawStyle style, uint32_t cols, uint32_t rows);
bool lovrPassRoundrect(Pass* pass, float* transform, float radius, uint32_t segments);
bool lovrPassBox(Pass* pass, float* transform, DrawStyle style);
bool lovrPassCircle(Pass* pass, float* transform, DrawStyle style, float angle1, float angle2, uint32_t segments);
bool lovrPassSphere(Pass* pass, float* transform, uint32_t segmentsH, uint32_t segmentsV);
bool lovrPassCylinder(Pass* pass, float* transform, bool capped, float angle1, float angle2, uint32_t segments);
bool lovrPassCone(Pass* pass, float* transform, uint32_t segments);
bool lovrPassCapsule(Pass* pass, float* transform, uint32_t segments);
bool lovrPassTorus(Pass* pass, float* transform, uint32_t segmentsT, uint32_t segmentsP);
bool lovrPassText(Pass* pass, ColoredString* strings, uint32_t count, float* transform, float wrap, HorizontalAlign halign, VerticalAlign valign);
bool lovrPassSkybox(Pass* pass, Texture* texture);
bool lovrPassFill(Pass* pass, Texture* texture);
bool lovrPassMonkey(Pass* pass, float* transform);
bool lovrPassDrawModel(Pass* pass, Model* model, float* transform, uint32_t instances);
bool lovrPassDrawMesh(Pass* pass, Mesh* mesh, float* transform, uint32_t instances);
bool lovrPassDrawTexture(Pass* pass, Texture* texture, float* transform);
bool lovrPassMesh(Pass* pass, Buffer* vertices, Buffer* indices, float* transform, uint32_t start, uint32_t count, uint32_t instances, uint32_t baseVertex);
bool lovrPassMeshIndirect(Pass* pass, Buffer* vertices, Buffer* indices, Buffer* indirect, uint32_t count, uint32_t offset, uint32_t stride);

bool lovrPassBeginTally(Pass* pass, uint32_t* index);
bool lovrPassFinishTally(Pass* pass, uint32_t* index);
Buffer* lovrPassGetTallyBuffer(Pass* pass, uint32_t* offset);
bool lovrPassSetTallyBuffer(Pass* pass, Buffer* buffer, uint32_t offset);

bool lovrPassCompute(Pass* pass, uint32_t x, uint32_t y, uint32_t z, Buffer* indirect, uint32_t offset);
void lovrPassBarrier(Pass* pass);
