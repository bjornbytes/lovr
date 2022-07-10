#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "data/modelData.h"
#include "data/rasterizer.h"
#include "headset/headset.h"
#include "math/math.h"
#include "core/gpu.h"
#include "core/maf.h"
#include "core/spv.h"
#include "core/os.h"
#include "util.h"
#include "monkey.h"
#include "shaders.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef LOVR_USE_GLSLANG
#include "glslang_c_interface.h"
#include "resource_limits_c.h"
#endif

uint32_t os_vk_create_surface(void* instance, void** surface);
const char** os_vk_get_instance_extensions(uint32_t* count);

#define MAX_FRAME_MEMORY (1 << 30)
#define MAX_SHADER_RESOURCES 32
#define MATERIALS_PER_BLOCK 1024

typedef struct {
  struct { float x, y, z; } position;
  struct { float x, y, z; } normal;
  struct { float u, v; } uv;
} ShapeVertex;

typedef struct {
  struct { float x, y; } position;
  struct { uint16_t u, v; } uv;
  struct { uint8_t r, g, b, a; } color;
} GlyphVertex;

typedef struct {
  struct { float x, y, z; } position;
  struct { float x, y, z; } normal;
  struct { float u, v; } uv;
  struct { uint8_t r, g, b, a; } color;
  struct { float x, y, z; } tangent;
} ModelVertex;

typedef struct {
  gpu_phase readPhase;
  gpu_phase writePhase;
  gpu_cache pendingReads;
  gpu_cache pendingWrite;
  uint32_t lastWriteIndex;
} Sync;

struct Buffer {
  uint32_t ref;
  uint32_t size;
  char* pointer;
  gpu_buffer* gpu;
  BufferInfo info;
  uint64_t hash;
  Sync sync;
};

struct Texture {
  uint32_t ref;
  gpu_texture* gpu;
  gpu_texture* renderView;
  Material* material;
  TextureInfo info;
  Sync sync;
};

struct Sampler {
  uint32_t ref;
  gpu_sampler* gpu;
  SamplerInfo info;
};

typedef struct {
  uint32_t hash;
  uint32_t offset;
  FieldType type;
} ShaderConstant;

typedef struct {
  uint32_t hash;
  uint32_t binding;
  uint32_t stageMask;
  gpu_slot_type type;
} ShaderResource;

typedef struct {
  uint32_t location;
  uint32_t hash;
} ShaderAttribute;

struct Shader {
  uint32_t ref;
  Shader* parent;
  gpu_shader* gpu;
  ShaderInfo info;
  uint32_t layout;
  uint32_t computePipeline;
  uint32_t bufferMask;
  uint32_t textureMask;
  uint32_t samplerMask;
  uint32_t storageMask;
  uint32_t constantSize;
  uint32_t constantCount;
  uint32_t resourceCount;
  uint32_t attributeCount;
  ShaderConstant* constants;
  ShaderResource* resources;
  ShaderAttribute* attributes;
  uint32_t flagCount;
  uint32_t overrideCount;
  gpu_shader_flag* flags;
  uint32_t* flagLookup;
  bool hasCustomAttributes;
};

struct Material {
  uint32_t ref;
  uint32_t next;
  uint32_t tick;
  uint16_t index;
  uint16_t block;
  gpu_bundle* bundle;
  MaterialInfo info;
};

typedef struct {
  uint32_t codepoint;
  float advance;
  uint16_t x, y;
  uint16_t uv[4];
  float box[4];
} Glyph;

struct Font {
  uint32_t ref;
  FontInfo info;
  Material* material;
  arr_t(Glyph) glyphs;
  map_t glyphLookup;
  map_t kerning;
  float pixelDensity;
  float lineSpacing;
  uint32_t padding;
  Texture* atlas;
  uint32_t atlasWidth;
  uint32_t atlasHeight;
  uint32_t rowHeight;
  uint32_t atlasX;
  uint32_t atlasY;
};

typedef struct {
  float transform[16];
  float cofactor[16];
  float color[4];
} DrawData;

typedef enum {
  VERTEX_SHAPE,
  VERTEX_POINT,
  VERTEX_GLYPH,
  VERTEX_MODEL,
  VERTEX_EMPTY,
  VERTEX_FORMAT_COUNT
} VertexFormat;

typedef struct {
  VertexMode mode;
  DefaultShader shader;
  Material* material;
  float* transform;
  struct {
    Buffer* buffer;
    VertexFormat format;
    uint32_t count;
    const void* data;
    void** pointer;
  } vertex;
  struct {
    Buffer* buffer;
    uint32_t count;
    uint32_t stride;
    const void* data;
    void** pointer;
  } index;
  uint32_t start;
  uint32_t count;
  uint32_t instances;
  uint32_t base;
} Draw;

typedef struct {
  float properties[3][4];
} NodeTransform;

struct Model {
  uint32_t ref;
  ModelInfo info;
  Draw* draws;
  Buffer* rawVertexBuffer;
  Buffer* vertexBuffer;
  Buffer* indexBuffer;
  Buffer* skinBuffer;
  Texture** textures;
  Material** materials;
  NodeTransform* localTransforms;
  float* globalTransforms;
  bool transformsDirty;
  uint32_t lastReskin;
};

struct Tally {
  uint32_t ref;
  uint32_t tick;
  TallyInfo info;
  gpu_tally* gpu;
  gpu_buffer* buffer;
  uint64_t* masks;
};

typedef struct {
  float view[16];
  float projection[16];
  float viewProjection[16];
  float inverseProjection[16];
} Camera;

typedef struct {
  float color[4];
  Shader* shader;
  Sampler* sampler;
  Material* material;
  uint64_t formatHash;
  gpu_pipeline_info info;
  float viewport[4];
  float depthRange[2];
  uint32_t scissor[4];
  VertexMode drawMode;
  bool dirty;
} Pipeline;

typedef struct {
  Sync* sync;
  Buffer* buffer;
  Texture* texture;
  gpu_phase phase;
  gpu_cache cache;
} Access;

struct Pass {
  uint32_t ref;
  PassInfo info;
  gpu_stream* stream;
  float* transform;
  float transforms[16][16];
  uint32_t transformIndex;
  Pipeline* pipeline;
  Pipeline pipelines[4];
  uint32_t pipelineIndex;
  bool samplerDirty;
  bool materialDirty;
  char constants[256];
  bool constantsDirty;
  gpu_binding bindings[32];
  uint32_t bindingMask;
  bool bindingsDirty;
  Camera* cameras;
  uint32_t cameraCount;
  bool cameraDirty;
  DrawData* drawData;
  uint32_t drawCount;
  gpu_binding builtins[3];
  gpu_buffer* vertexBuffer;
  gpu_buffer* indexBuffer;
  arr_t(Access) access;
};

typedef struct {
  Material* list;
  gpu_buffer* buffer;
  void* pointer;
  gpu_bundle_pool* bundlePool;
  gpu_bundle* bundles;
  uint32_t head;
  uint32_t tail;
} MaterialBlock;

typedef struct {
  gpu_texture* texture;
  uint32_t hash;
  uint32_t tick;
} TempAttachment;

typedef struct {
  void* next;
  gpu_bundle_pool* gpu;
  gpu_bundle* bundles;
  uint32_t cursor;
  uint32_t tick;
} BundlePool;

typedef struct {
  uint64_t hash;
  gpu_layout* gpu;
  BundlePool* head;
  BundlePool* tail;
} Layout;

typedef struct {
  char* memory;
  uint32_t cursor;
  uint32_t length;
} Allocator;

static struct {
  bool initialized;
  bool active;
  uint32_t tick;
  gpu_stream* stream;
  bool hasTextureUpload;
  bool hasMaterialUpload;
  bool hasGlyphUpload;
  bool hasReskin;
  gpu_device_info device;
  gpu_features features;
  gpu_limits limits;
  GraphicsStats stats;
  float background[4];
  Texture* window;
  Font* defaultFont;
  Buffer* defaultBuffer;
  Texture* defaultTexture;
  Sampler* defaultSamplers[2];
  Shader* animator;
  Shader* timeWizard;
  Shader* defaultShaders[DEFAULT_SHADER_COUNT];
  gpu_vertex_format vertexFormats[VERTEX_FORMAT_COUNT];
  Material* defaultMaterial;
  uint32_t materialBlock;
  arr_t(MaterialBlock) materialBlocks;
  arr_t(TempAttachment) attachments;
  arr_t(Pass*) passes;
  map_t pipelineLookup;
  arr_t(gpu_pipeline*) pipelines;
  arr_t(Layout) layouts;
  uint32_t builtinLayout;
  uint32_t materialLayout;
  Allocator allocator;
} state;

// Helpers

static void* tempAlloc(size_t size);
static void* tempGrow(void* p, size_t size);
static uint32_t tempPush(void);
static void tempPop(uint32_t stack);
static int u64cmp(const void* a, const void* b);
static void beginFrame(void);
static void cleanupPasses(void);
static uint32_t getLayout(gpu_slot* slots, uint32_t count);
static gpu_bundle* getBundle(uint32_t layout);
static gpu_texture* getAttachment(uint32_t size[2], uint32_t layers, TextureFormat format, bool srgb, uint32_t samples);
static size_t measureTexture(TextureFormat format, uint16_t w, uint16_t h, uint16_t d);
static void checkTextureBounds(const TextureInfo* info, uint32_t offset[4], uint32_t extent[3]);
static void mipmapTexture(gpu_stream* stream, Texture* texture, uint32_t base, uint32_t count);
static ShaderResource* findShaderResource(Shader* shader, const char* name, size_t length, uint32_t slot);
static void trackBuffer(Pass* pass, Buffer* buffer, gpu_phase phase, gpu_cache cache);
static void trackTexture(Pass* pass, Texture* texture, gpu_phase phase, gpu_cache cache);
static void updateModelTransforms(Model* model, uint32_t nodeIndex, float* parent);
static void checkShaderFeatures(uint32_t* features, uint32_t count);
static void onMessage(void* context, const char* message, bool severe);

// Entry

bool lovrGraphicsInit(bool debug, bool vsync) {
  if (state.initialized) return false;

  gpu_config config = {
    .debug = debug,
    .callback = onMessage,
    .engineName = "LOVR",
    .engineVersion = { LOVR_VERSION_MAJOR, LOVR_VERSION_MINOR, LOVR_VERSION_PATCH },
    .device = &state.device,
    .features = &state.features,
    .limits = &state.limits
  };

#ifdef LOVR_VK
  if (os_window_is_open()) {
    config.vk.getInstanceExtensions = os_vk_get_instance_extensions;
    config.vk.createSurface = os_vk_create_surface;
    config.vk.surface = true;
    config.vk.vsync = vsync;
  }
#endif

#if defined LOVR_VK && !defined LOVR_DISABLE_HEADSET
  if (lovrHeadsetInterface) {
    config.vk.getPhysicalDevice = lovrHeadsetInterface->getVulkanPhysicalDevice;
    config.vk.createInstance = lovrHeadsetInterface->createVulkanInstance;
    config.vk.createDevice = lovrHeadsetInterface->createVulkanDevice;
  }
#endif

  if (!gpu_init(&config)) {
    lovrThrow("Failed to initialize GPU");
  }

  lovrAssert(state.limits.uniformBufferRange >= 65536, "LÖVR requires the GPU to support a uniform buffer range of at least 64KB");

  // Temporary frame memory uses a large 1GB virtual memory allocation, committing pages as needed
  state.allocator.length = 1 << 14;
  state.allocator.memory = os_vm_init(MAX_FRAME_MEMORY);
  os_vm_commit(state.allocator.memory, state.allocator.length);

  map_init(&state.pipelineLookup, 64);
  arr_init(&state.pipelines, realloc);
  arr_init(&state.layouts, realloc);
  arr_init(&state.materialBlocks, realloc);
  arr_init(&state.attachments, realloc);
  arr_init(&state.passes, realloc);

  gpu_slot builtinSlots[] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER, GPU_STAGE_ALL }, // Cameras
    { 1, GPU_SLOT_UNIFORM_BUFFER, GPU_STAGE_ALL }, // Draw data
    { 2, GPU_SLOT_SAMPLER, GPU_STAGE_ALL } // Default sampler
  };

  state.builtinLayout = getLayout(builtinSlots, COUNTOF(builtinSlots));

  gpu_slot materialSlots[] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER, GPU_STAGE_VERTEX | GPU_STAGE_FRAGMENT }, // Data
    { 1, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_VERTEX | GPU_STAGE_FRAGMENT }, // Color
    { 2, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_VERTEX | GPU_STAGE_FRAGMENT }, // Glow
    { 3, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_VERTEX | GPU_STAGE_FRAGMENT }, // Occlusion
    { 4, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_VERTEX | GPU_STAGE_FRAGMENT }, // Metalness
    { 5, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_VERTEX | GPU_STAGE_FRAGMENT }, // Roughness
    { 6, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_VERTEX | GPU_STAGE_FRAGMENT }, // Clearcoat
    { 7, GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_VERTEX | GPU_STAGE_FRAGMENT } // Normal
  };

  state.materialLayout = getLayout(materialSlots, COUNTOF(materialSlots));

  float data[] = { 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f };

  state.defaultBuffer = lovrBufferCreate(&(BufferInfo) {
    .length = sizeof(data),
    .stride = 1,
    .label = "Default Buffer"
  }, NULL);

  beginFrame();
  gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
  float* pointer = gpu_map(scratchpad, sizeof(data), 4, GPU_MAP_WRITE);
  memcpy(pointer, data, sizeof(data));
  gpu_copy_buffers(state.stream, scratchpad, state.defaultBuffer->gpu, 0, 0, sizeof(data));

  Image* image = lovrImageCreateRaw(4, 4, FORMAT_RGBA8);

  float white[4] = { 1.f, 1.f, 1.f, 1.f };
  for (uint32_t y = 0; y < 4; y++) {
    for (uint32_t x = 0; x < 4; x++) {
      lovrImageSetPixel(image, x, y, white);
    }
  }

  state.defaultTexture = lovrTextureCreate(&(TextureInfo) {
    .type = TEXTURE_2D,
    .usage = TEXTURE_SAMPLE,
    .format = FORMAT_RGBA8,
    .width = 4,
    .height = 4,
    .depth = 1,
    .mipmaps = 1,
    .samples = 1,
    .srgb = true,
    .imageCount = 1,
    .images = &image,
    .label = "Default Texture"
  });

  lovrRelease(image, lovrImageDestroy);

  for (uint32_t i = 0; i < 2; i++) {
    state.defaultSamplers[i] = lovrSamplerCreate(&(SamplerInfo) {
      .min = i == 0 ? FILTER_NEAREST : FILTER_LINEAR,
      .mag = i == 0 ? FILTER_NEAREST : FILTER_LINEAR,
      .mip = i == 0 ? FILTER_NEAREST : FILTER_LINEAR,
      .wrap = { WRAP_REPEAT, WRAP_REPEAT, WRAP_REPEAT }
    });
  }

  state.vertexFormats[VERTEX_SHAPE] = (gpu_vertex_format) {
    .bufferCount = 2,
    .attributeCount = 5,
    .bufferStrides[0] = sizeof(ShapeVertex),
    .attributes[0] = { 0, 10, offsetof(ShapeVertex, position), GPU_TYPE_F32x3 },
    .attributes[1] = { 0, 11, offsetof(ShapeVertex, normal), GPU_TYPE_F32x3 },
    .attributes[2] = { 0, 12, offsetof(ShapeVertex, uv), GPU_TYPE_F32x2 },
    .attributes[3] = { 1, 13, 16, GPU_TYPE_F32x4 },
    .attributes[4] = { 1, 14, 0, GPU_TYPE_F32x4 }
  };

  state.vertexFormats[VERTEX_POINT] = (gpu_vertex_format) {
    .bufferCount = 2,
    .attributeCount = 5,
    .bufferStrides[0] = 12,
    .attributes[0] = { 0, 10, 0, GPU_TYPE_F32x3 },
    .attributes[1] = { 1, 11, 0, GPU_TYPE_F32x4 },
    .attributes[2] = { 1, 12, 0, GPU_TYPE_F32x4 },
    .attributes[3] = { 1, 13, 16, GPU_TYPE_F32x4 },
    .attributes[4] = { 1, 14, 0, GPU_TYPE_F32x4 }
  };

  state.vertexFormats[VERTEX_GLYPH] = (gpu_vertex_format) {
    .bufferCount = 2,
    .attributeCount = 5,
    .bufferStrides[0] = sizeof(GlyphVertex),
    .attributes[0] = { 0, 10, offsetof(GlyphVertex, position), GPU_TYPE_F32x2 },
    .attributes[1] = { 1, 11, 0, GPU_TYPE_F32x4 },
    .attributes[2] = { 0, 12, offsetof(GlyphVertex, uv), GPU_TYPE_UN16x2 },
    .attributes[3] = { 0, 13, offsetof(GlyphVertex, color), GPU_TYPE_UN8x4 },
    .attributes[4] = { 1, 14, 0, GPU_TYPE_F32x4 }
  };

  state.vertexFormats[VERTEX_MODEL] = (gpu_vertex_format) {
    .bufferCount = 2,
    .attributeCount = 5,
    .bufferStrides[0] = sizeof(ModelVertex),
    .attributes[0] = { 0, 10, offsetof(ModelVertex, position), GPU_TYPE_F32x3 },
    .attributes[1] = { 0, 11, offsetof(ModelVertex, normal), GPU_TYPE_F32x3 },
    .attributes[2] = { 0, 12, offsetof(ModelVertex, uv), GPU_TYPE_F32x2 },
    .attributes[3] = { 0, 13, offsetof(ModelVertex, color), GPU_TYPE_UN8x4 },
    .attributes[4] = { 0, 14, offsetof(ModelVertex, tangent), GPU_TYPE_F32x3 }
  };

  state.vertexFormats[VERTEX_EMPTY] = (gpu_vertex_format) {
    .bufferCount = 2,
    .attributeCount = 5,
    .attributes[0] = { 1, 10, 0, GPU_TYPE_F32x2 },
    .attributes[1] = { 1, 11, 0, GPU_TYPE_F32x4 },
    .attributes[2] = { 1, 12, 0, GPU_TYPE_F32x2 },
    .attributes[3] = { 1, 13, 16, GPU_TYPE_F32x4 },
    .attributes[4] = { 1, 14, 0, GPU_TYPE_F32x4 }
  };

  state.defaultMaterial = lovrMaterialCreate(&(MaterialInfo) {
    .data.color = { 1.f, 1.f, 1.f, 1.f },
    .texture = state.defaultTexture
  });

  float16Init();
  glslang_initialize_process();
  state.initialized = true;
  return true;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  cleanupPasses();
  arr_free(&state.passes);
  lovrRelease(state.window, lovrTextureDestroy);
  for (uint32_t i = 0; i < state.attachments.length; i++) {
    gpu_texture_destroy(state.attachments.data[i].texture);
    free(state.attachments.data[i].texture);
  }
  arr_free(&state.attachments);
  lovrRelease(state.defaultFont, lovrFontDestroy);
  lovrRelease(state.defaultBuffer, lovrBufferDestroy);
  lovrRelease(state.defaultTexture, lovrTextureDestroy);
  lovrRelease(state.defaultSamplers[0], lovrSamplerDestroy);
  lovrRelease(state.defaultSamplers[1], lovrSamplerDestroy);
  lovrRelease(state.animator, lovrShaderDestroy);
  lovrRelease(state.timeWizard, lovrShaderDestroy);
  for (uint32_t i = 0; i < COUNTOF(state.defaultShaders); i++) {
    lovrRelease(state.defaultShaders[i], lovrShaderDestroy);
  }
  lovrRelease(state.defaultMaterial, lovrMaterialDestroy);
  for (uint32_t i = 0; i < state.materialBlocks.length; i++) {
    MaterialBlock* block = &state.materialBlocks.data[i];
    gpu_buffer_destroy(block->buffer);
    gpu_bundle_pool_destroy(block->bundlePool);
    free(block->list);
    free(block->buffer);
    free(block->bundlePool);
    free(block->bundles);
  }
  arr_free(&state.materialBlocks);
  for (uint32_t i = 0; i < state.pipelines.length; i++) {
    gpu_pipeline_destroy(state.pipelines.data[i]);
    free(state.pipelines.data[i]);
  }
  map_free(&state.pipelineLookup);
  arr_free(&state.pipelines);
  for (uint32_t i = 0; i < state.layouts.length; i++) {
    BundlePool* pool = state.layouts.data[i].head;
    while (pool) {
      BundlePool* next = pool->next;
      gpu_bundle_pool_destroy(pool->gpu);
      free(pool->gpu);
      free(pool->bundles);
      free(pool);
      pool = next;
    }
    gpu_layout_destroy(state.layouts.data[i].gpu);
    free(state.layouts.data[i].gpu);
  }
  arr_free(&state.layouts);
  gpu_destroy();
  glslang_finalize_process();
  os_vm_free(state.allocator.memory, MAX_FRAME_MEMORY);
  memset(&state, 0, sizeof(state));
}

void lovrGraphicsGetDevice(GraphicsDevice* device) {
  device->deviceId = state.device.deviceId;
  device->vendorId = state.device.vendorId;
  device->name = state.device.deviceName;
  device->renderer = state.device.renderer;
  device->subgroupSize = state.device.subgroupSize;
  device->discrete = state.device.discrete;
}

void lovrGraphicsGetFeatures(GraphicsFeatures* features) {
  features->textureBC = state.features.textureBC;
  features->textureASTC = state.features.textureASTC;
  features->wireframe = state.features.wireframe;
  features->depthClamp = state.features.depthClamp;
  features->indirectDrawFirstInstance = state.features.indirectDrawFirstInstance;
  features->float64 = state.features.float64;
  features->int64 = state.features.int64;
  features->int16 = state.features.int16;
}

void lovrGraphicsGetLimits(GraphicsLimits* limits) {
  limits->textureSize2D = state.limits.textureSize2D;
  limits->textureSize3D = state.limits.textureSize3D;
  limits->textureSizeCube = state.limits.textureSizeCube;
  limits->textureLayers = state.limits.textureLayers;
  limits->renderSize[0] = state.limits.renderSize[0];
  limits->renderSize[1] = state.limits.renderSize[1];
  limits->renderSize[2] = state.limits.renderSize[2];
  limits->uniformBuffersPerStage = MIN(state.limits.uniformBuffersPerStage - 3, MAX_SHADER_RESOURCES);
  limits->storageBuffersPerStage = MIN(state.limits.storageBuffersPerStage, MAX_SHADER_RESOURCES);
  limits->sampledTexturesPerStage = MIN(state.limits.sampledTexturesPerStage - 7, MAX_SHADER_RESOURCES);
  limits->storageTexturesPerStage = MIN(state.limits.storageTexturesPerStage, MAX_SHADER_RESOURCES);
  limits->samplersPerStage = MIN(state.limits.samplersPerStage - 1, MAX_SHADER_RESOURCES);
  limits->resourcesPerShader = MAX_SHADER_RESOURCES;
  limits->uniformBufferRange = state.limits.uniformBufferRange;
  limits->storageBufferRange = state.limits.storageBufferRange;
  limits->uniformBufferAlign = state.limits.uniformBufferAlign;
  limits->storageBufferAlign = state.limits.storageBufferAlign;
  limits->vertexAttributes = 10;
  limits->vertexBufferStride = state.limits.vertexBufferStride;
  limits->vertexShaderOutputs = 10;
  limits->clipDistances = state.limits.clipDistances;
  limits->cullDistances = state.limits.cullDistances;
  limits->clipAndCullDistances = state.limits.clipAndCullDistances;
  memcpy(limits->computeDispatchCount, state.limits.computeDispatchCount, 3 * sizeof(uint32_t));
  memcpy(limits->computeWorkgroupSize, state.limits.computeWorkgroupSize, 3 * sizeof(uint32_t));
  limits->computeWorkgroupVolume = state.limits.computeWorkgroupVolume;
  limits->computeSharedMemory = state.limits.computeSharedMemory;
  limits->shaderConstantSize = MIN(state.limits.pushConstantSize, 256);
  limits->indirectDrawCount = state.limits.indirectDrawCount;
  limits->instances = state.limits.instances;
  limits->anisotropy = state.limits.anisotropy;
  limits->pointSize = state.limits.pointSize;
}

void lovrGraphicsGetStats(GraphicsStats* stats) {
  *stats = state.stats;
}

bool lovrGraphicsIsFormatSupported(uint32_t format, uint32_t features) {
  uint8_t supports = state.features.formats[format];
  if (!features) return supports;
  if ((features & TEXTURE_FEATURE_SAMPLE) && !(supports & GPU_FEATURE_SAMPLE)) return false;
  if ((features & TEXTURE_FEATURE_FILTER) && !(supports & GPU_FEATURE_FILTER)) return false;
  if ((features & TEXTURE_FEATURE_RENDER) && !(supports & GPU_FEATURE_RENDER)) return false;
  if ((features & TEXTURE_FEATURE_BLEND) && !(supports & GPU_FEATURE_BLEND)) return false;
  if ((features & TEXTURE_FEATURE_STORAGE) && !(supports & GPU_FEATURE_STORAGE)) return false;
  if ((features & TEXTURE_FEATURE_ATOMIC) && !(supports & GPU_FEATURE_ATOMIC)) return false;
  if ((features & TEXTURE_FEATURE_BLIT_SRC) && !(supports & GPU_FEATURE_BLIT_SRC)) return false;
  if ((features & TEXTURE_FEATURE_BLIT_DST) && !(supports & GPU_FEATURE_BLIT_DST)) return false;
  return true;
}

void lovrGraphicsGetBackground(float background[4]) {
  background[0] = lovrMathLinearToGamma(state.background[0]);
  background[1] = lovrMathLinearToGamma(state.background[1]);
  background[2] = lovrMathLinearToGamma(state.background[2]);
  background[3] = state.background[3];
}

void lovrGraphicsSetBackground(float background[4]) {
  state.background[0] = lovrMathGammaToLinear(background[0]);
  state.background[1] = lovrMathGammaToLinear(background[1]);
  state.background[2] = lovrMathGammaToLinear(background[2]);
  state.background[3] = background[3];
}

Font* lovrGraphicsGetDefaultFont() {
  if (!state.defaultFont) {
    Rasterizer* rasterizer = lovrRasterizerCreate(NULL, 32);
    state.defaultFont = lovrFontCreate(&(FontInfo) {
      .rasterizer = rasterizer,
      .spread = 4.
    });
    lovrRelease(rasterizer, lovrRasterizerDestroy);
  }

  return state.defaultFont;
}

void lovrGraphicsSubmit(Pass** passes, uint32_t count) {
  if (!state.active) {
    return;
  }

  bool present = false;
  uint32_t total = count + 1;
  gpu_stream** streams = tempAlloc(total * sizeof(gpu_stream*));
  gpu_barrier* barriers = tempAlloc(count * sizeof(gpu_barrier));
  memset(barriers, 0, count * sizeof(gpu_barrier));

  streams[0] = state.stream;

  if (state.hasTextureUpload) {
    barriers[0].prev |= GPU_PHASE_TRANSFER;
    barriers[0].next |= GPU_PHASE_SHADER_VERTEX;
    barriers[0].next |= GPU_PHASE_SHADER_FRAGMENT;
    barriers[0].next |= GPU_PHASE_SHADER_COMPUTE;
    barriers[0].flush |= GPU_CACHE_TRANSFER_WRITE;
    barriers[0].clear |= GPU_CACHE_TEXTURE;
    state.hasTextureUpload = false;
  }

  if (state.hasMaterialUpload) {
    barriers[0].prev |= GPU_PHASE_TRANSFER;
    barriers[0].next |= GPU_PHASE_SHADER_VERTEX;
    barriers[0].next |= GPU_PHASE_SHADER_FRAGMENT;
    barriers[0].flush |= GPU_CACHE_TRANSFER_WRITE;
    barriers[0].clear |= GPU_CACHE_UNIFORM;
    state.hasMaterialUpload = false;
  }

  if (state.hasGlyphUpload) {
    barriers[0].prev |= GPU_PHASE_TRANSFER;
    barriers[0].next |= GPU_PHASE_SHADER_FRAGMENT;
    barriers[0].flush |= GPU_CACHE_TRANSFER_WRITE;
    barriers[0].clear |= GPU_CACHE_TEXTURE;
    state.hasGlyphUpload = false;
  }

  if (state.hasReskin) {
    barriers[0].prev |= GPU_PHASE_SHADER_COMPUTE;
    barriers[0].next |= GPU_PHASE_INPUT_VERTEX;
    barriers[0].flush |= GPU_CACHE_STORAGE_WRITE;
    barriers[0].clear |= GPU_CACHE_VERTEX;
    state.hasReskin = false;
  }

  // End passes
  for (uint32_t i = 0; i < count; i++) {
    streams[i + 1] = passes[i]->stream;

    if (passes[i]->info.type == PASS_RENDER) {
      gpu_render_end(passes[i]->stream);

      Canvas* canvas = &passes[i]->info.canvas;

      for (uint32_t j = 0; j < COUNTOF(canvas->textures) && canvas->textures[j]; j++) {
        if (canvas->mipmap && canvas->textures[j]->info.mipmaps > 1) {
          barriers[i].prev |= GPU_PHASE_COLOR;
          barriers[i].next |= GPU_PHASE_TRANSFER;
          barriers[i].flush |= GPU_CACHE_COLOR_WRITE;
          barriers[i].clear |= GPU_CACHE_TRANSFER_READ;
          mipmapTexture(passes[i]->stream, canvas->textures[j], 0, ~0u);
        }

        if (canvas->textures[j] == state.window) {
          present = true;
        }
      }

      if (canvas->mipmap && canvas->depth.texture && canvas->depth.texture->info.mipmaps > 1) {
        barriers[i].prev |= GPU_PHASE_DEPTH_LATE;
        barriers[i].next |= GPU_PHASE_TRANSFER;
        barriers[i].flush |= GPU_CACHE_DEPTH_WRITE;
        barriers[i].clear |= GPU_CACHE_TRANSFER_READ;
        mipmapTexture(passes[i]->stream, canvas->depth.texture, 0, ~0u);
      }
    } else if (passes[i]->info.type == PASS_COMPUTE) {
      gpu_compute_end(passes[i]->stream);
    }
  }

  // Synchronization
  for (uint32_t i = 0; i < count; i++) {
    Pass* pass = passes[i];

    for (size_t j = 0; j < pass->access.length; j++) {
      // access is the incoming resource access performed by the pass
      Access* access = &pass->access.data[j];

      // sync is the existing state of the resource at the time of the access
      Sync* sync = access->sync;

      // barrier is a barrier that will be emitted at the end of the last pass that wrote to the
      // resource (or the first pass if the last write was in a previous frame).  The barrier
      // specifies 'phases' and 'caches'.  The 'next' phase will wait for the 'prev' phase to
      // finish.  In between, the 'flush' cache is flushed and the 'clear' cache is cleared.
      gpu_barrier* barrier = &barriers[sync->lastWriteIndex];

      // Only the first write in a pass is considered, because each pass can only perform one type
      // of write to a resource, and there is not currently any intra-pass synchronization.
      if (sync->lastWriteIndex == i + 1) {
        continue;
      }

      // There are 4 types of access patterns:
      // - read after read:
      //   - no hazard, no barrier necessary
      // - read after write:
      //   - needs execution dependency to ensure the read happens after the write
      //   - needs to flush the writes from the cache
      //   - needs to clear the cache for the read so it gets the new data
      //   - only needs to happen once for each type of read after a write (tracked by pendingReads)
      //     - if a second read happens, the first read would have already synchronized (transitive)
      // - write after write:
      //   - needs execution dependency to ensure writes don't overlap
      //   - needs to flush and clear the cache
      //   - resets the 'last write' and clears pendingReads
      // - write after read:
      //   - needs execution dependency to ensure write starts after read is finished
      //   - does not need to flush any caches
      //   - does clear the write cache
      //   - resets the 'last write' and clears pendingReads

      uint32_t read = access->cache & GPU_CACHE_READ;
      uint32_t write = access->cache & GPU_CACHE_WRITE;
      uint32_t newReads = read & ~sync->pendingReads;
      bool hasNewReads = newReads || (access->phase & ~sync->readPhase);
      bool readAfterWrite = read && sync->pendingWrite && hasNewReads;
      bool writeAfterWrite = write && sync->pendingWrite && !sync->pendingReads;
      bool writeAfterRead = write && sync->pendingReads;

      if (readAfterWrite) {
        barrier->prev |= sync->writePhase;
        barrier->next |= access->phase;
        barrier->flush |= sync->pendingWrite;
        barrier->clear |= newReads;
        sync->readPhase |= access->phase;
        sync->pendingReads |= read;
      }

      if (writeAfterWrite) {
        barrier->prev |= sync->writePhase;
        barrier->next |= access->phase;
        barrier->flush |= sync->pendingWrite;
        barrier->clear |= write;
      }

      if (writeAfterRead) {
        barrier->prev |= sync->readPhase;
        barrier->next |= access->phase;
        sync->readPhase = 0;
        sync->pendingReads = 0;
      }

      if (write) {
        sync->writePhase = access->phase;
        sync->pendingWrite = write;
        sync->lastWriteIndex = i + 1;
      }
    }
  }

  for (uint32_t i = 0; i < count; i++) {
    for (uint32_t j = 0; j < passes[i]->access.length; j++) {
      passes[i]->access.data[j].sync->lastWriteIndex = 0;
    }
  }

  for (uint32_t i = 0; i < count; i++) {
    gpu_sync(streams[i], &barriers[i], 1);
  }

  for (uint32_t i = 0; i < total; i++) {
    gpu_stream_end(streams[i]);
  }

  gpu_submit(streams, total, present);

  cleanupPasses();
  arr_clear(&state.passes);

  state.stats.pipelineSwitches = 0;
  state.stats.bundleSwitches = 0;

  if (present) {
    state.window->gpu = NULL;
    state.window->renderView = NULL;
  }

  state.stream = NULL;
  state.active = false;
}

void lovrGraphicsWait() {
  gpu_wait();
}

// Buffer

Buffer* lovrGraphicsGetBuffer(BufferInfo* info, void** data) {
  uint32_t size = info->length * info->stride;
  lovrCheck(size > 0, "Buffer size can not be zero");
  lovrCheck(size <= 1 << 30, "Max buffer size is 1GB");

  Buffer* buffer = tempAlloc(sizeof(Buffer) + gpu_sizeof_buffer());
  buffer->ref = 1;
  buffer->size = size;
  buffer->gpu = (gpu_buffer*) (buffer + 1);
  buffer->info = *info;
  buffer->hash = hash64(info->fields, info->fieldCount * sizeof(BufferField));

  buffer->pointer = gpu_map(buffer->gpu, size, state.limits.uniformBufferAlign, GPU_MAP_WRITE);

  if (data) {
    *data = buffer->pointer;
  }

  return buffer;
}

Buffer* lovrBufferCreate(BufferInfo* info, void** data) {
  uint32_t size = info->length * info->stride;
  lovrCheck(size > 0, "Buffer size can not be zero");
  lovrCheck(size <= 1 << 30, "Max buffer size is 1GB");

  Buffer* buffer = calloc(1, sizeof(Buffer) + gpu_sizeof_buffer());
  lovrAssert(buffer, "Out of memory");
  buffer->ref = 1;
  buffer->size = size;
  buffer->gpu = (gpu_buffer*) (buffer + 1);
  buffer->info = *info;
  buffer->hash = hash64(info->fields, info->fieldCount * sizeof(BufferField));

  gpu_buffer_init(buffer->gpu, &(gpu_buffer_info) {
    .size = buffer->size,
    .label = info->label,
    .pointer = data
  });

  if (data && *data == NULL) {
    beginFrame();
    gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
    *data = gpu_map(scratchpad, size, 4, GPU_MAP_WRITE);
    gpu_copy_buffers(state.stream, scratchpad, buffer->gpu, 0, 0, size);
    buffer->sync.writePhase = GPU_PHASE_TRANSFER;
    buffer->sync.pendingWrite = GPU_CACHE_TRANSFER_WRITE;
  }

  return buffer;
}

void lovrBufferDestroy(void* ref) {
  Buffer* buffer = ref;
  if (lovrBufferIsTemporary(buffer)) return;
  gpu_buffer_destroy(buffer->gpu);
  free(buffer);
}

const BufferInfo* lovrBufferGetInfo(Buffer* buffer) {
  return &buffer->info;
}

bool lovrBufferIsTemporary(Buffer* buffer) {
  return buffer->pointer != NULL;
}

void* lovrBufferMap(Buffer* buffer, uint32_t offset, uint32_t size) {
  lovrAssert(buffer->pointer, "This function can only be called on temporary buffers");
  return buffer->pointer + offset;
}

void lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t size) {
  lovrAssert(buffer->pointer, "This function can only be called on temporary buffers");
  lovrCheck(size % 4 == 0, "Buffer clear size must be a multiple of 4");
  lovrCheck(offset % 4 == 0, "Buffer clear offset must be a multiple of 4");
  lovrCheck(offset + size <= buffer->size, "Tried to clear past the end of the Buffer");
  memset(buffer->pointer + offset, 0, size);
}

// Texture

Texture* lovrGraphicsGetWindowTexture() {
  if (!state.window) {
    state.window = malloc(sizeof(Texture));
    lovrAssert(state.window, "Out of memory");
    int width, height;
    os_window_get_fbsize(&width, &height);
    state.window->ref = 1;
    state.window->gpu = NULL;
    state.window->renderView = NULL;
    state.window->info = (TextureInfo) {
      .type = TEXTURE_2D,
      .format = GPU_FORMAT_SURFACE,
      .width = width,
      .height = height,
      .depth = 1,
      .mipmaps = 1,
      .samples = 1,
      .usage = TEXTURE_RENDER,
      .srgb = true
    };
  }

  if (!state.window->gpu) {
    beginFrame();
    state.window->gpu = gpu_surface_acquire();
    state.window->renderView = state.window->gpu;
  }

  return state.window;
}

Texture* lovrTextureCreate(TextureInfo* info) {
  uint32_t limits[] = {
    [TEXTURE_2D] = state.limits.textureSize2D,
    [TEXTURE_3D] = state.limits.textureSize3D,
    [TEXTURE_CUBE] = state.limits.textureSizeCube,
    [TEXTURE_ARRAY] = state.limits.textureSize2D
  };

  uint32_t limit = limits[info->type];
  uint32_t mipmapCap = log2(MAX(MAX(info->width, info->height), (info->type == TEXTURE_3D ? info->depth : 1))) + 1;
  uint32_t mipmaps = CLAMP(info->mipmaps, 1, mipmapCap);
  uint8_t supports = state.features.formats[info->format];

  lovrCheck(info->width > 0, "Texture width must be greater than zero");
  lovrCheck(info->height > 0, "Texture height must be greater than zero");
  lovrCheck(info->depth > 0, "Texture depth must be greater than zero");
  lovrCheck(info->width <= limit, "Texture %s exceeds the limit for this texture type (%d)", "width", limit);
  lovrCheck(info->height <= limit, "Texture %s exceeds the limit for this texture type (%d)", "height", limit);
  lovrCheck(info->depth <= limit || info->type != TEXTURE_3D, "Texture %s exceeds the limit for this texture type (%d)", "depth", limit);
  lovrCheck(info->depth <= state.limits.textureLayers || info->type != TEXTURE_ARRAY, "Texture %s exceeds the limit for this texture type (%d)", "depth", limit);
  lovrCheck(info->depth == 1 || info->type != TEXTURE_2D, "2D textures must have a depth of 1");
  lovrCheck(info->depth == 6 || info->type != TEXTURE_CUBE, "Cubemaps must have a depth of 6");
  lovrCheck(info->width == info->height || info->type != TEXTURE_CUBE, "Cubemaps must be square");
  lovrCheck(measureTexture(info->format, info->width, info->height, info->depth) < 1 << 30, "Memory for a Texture can not exceed 1GB"); // TODO mip?
  lovrCheck(info->samples == 1 || info->samples == 4, "Texture multisample count must be 1 or 4...for now");
  lovrCheck(info->samples == 1 || info->type != TEXTURE_CUBE, "Cubemaps can not be multisampled");
  lovrCheck(info->samples == 1 || info->type != TEXTURE_3D, "Volume textures can not be multisampled");
  lovrCheck(info->samples == 1 || ~info->usage & TEXTURE_STORAGE, "Textures with the 'storage' flag can not be multisampled...for now");
  lovrCheck(info->samples == 1 || mipmaps == 1, "Multisampled textures can only have 1 mipmap");
  lovrCheck(~info->usage & TEXTURE_SAMPLE || (supports & GPU_FEATURE_SAMPLE), "GPU does not support the 'sample' flag for this format");
  lovrCheck(~info->usage & TEXTURE_RENDER || (supports & GPU_FEATURE_RENDER), "GPU does not support the 'render' flag for this format");
  lovrCheck(~info->usage & TEXTURE_STORAGE || (supports & GPU_FEATURE_STORAGE), "GPU does not support the 'storage' flag for this format");
  lovrCheck(~info->usage & TEXTURE_RENDER || info->width <= state.limits.renderSize[0], "Texture has 'render' flag but its size exceeds the renderSize limit");
  lovrCheck(~info->usage & TEXTURE_RENDER || info->height <= state.limits.renderSize[1], "Texture has 'render' flag but its size exceeds the renderSize limit");
  lovrCheck(mipmaps <= mipmapCap, "Texture has more than the max number of mipmap levels for its size (%d)", mipmapCap);
  lovrCheck((info->format < FORMAT_BC1 || info->format > FORMAT_BC7) || state.features.textureBC, "%s textures are not supported on this GPU", "BC");
  lovrCheck(info->format < FORMAT_ASTC_4x4 || state.features.textureASTC, "%s textures are not supported on this GPU", "ASTC");

  Texture* texture = calloc(1, sizeof(Texture) + gpu_sizeof_texture());
  lovrAssert(texture, "Out of memory");
  texture->ref = 1;
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = *info;
  texture->info.mipmaps = mipmaps;

  uint32_t levelCount = 0;
  uint32_t levelOffsets[16];
  uint32_t levelSizes[16];
  gpu_buffer* scratchpad = NULL;

  if (info->imageCount > 0) {
    levelCount = lovrImageGetLevelCount(info->images[0]);
    lovrCheck(info->type != TEXTURE_3D || levelCount == 1, "Images used to initialize 3D textures can not have mipmaps");

    uint32_t total = 0;
    for (uint32_t level = 0; level < levelCount; level++) {
      levelOffsets[level] = total;
      uint32_t width = MAX(info->width >> level, 1);
      uint32_t height = MAX(info->height >> level, 1);
      levelSizes[level] = measureTexture(info->format, width, height, info->depth);
      total += levelSizes[level];
    }

    scratchpad = tempAlloc(gpu_sizeof_buffer());
    char* data = gpu_map(scratchpad, total, 64, GPU_MAP_WRITE);

    for (uint32_t level = 0; level < levelCount; level++) {
      for (uint32_t layer = 0; layer < info->depth; layer++) {
        Image* image = info->imageCount == 1 ? info->images[0] : info->images[layer];
        uint32_t slice = info->imageCount == 1 ? layer : 0;
        uint32_t size = lovrImageGetLayerSize(image, level);
        lovrCheck(size == levelSizes[level] / info->depth, "Texture/Image size mismatch!");
        void* pixels = lovrImageGetLayerData(image, level, slice);
        memcpy(data, pixels, size);
        data += size;
      }
    }
  }

  beginFrame();

  gpu_texture_init(texture->gpu, &(gpu_texture_info) {
    .type = (gpu_texture_type) info->type,
    .format = (gpu_texture_format) info->format,
    .size = { info->width, info->height, info->depth },
    .mipmaps = texture->info.mipmaps,
    .samples = MAX(info->samples, 1),
    .usage =
      ((info->usage & TEXTURE_SAMPLE) ? GPU_TEXTURE_SAMPLE : 0) |
      ((info->usage & TEXTURE_RENDER) ? GPU_TEXTURE_RENDER : 0) |
      ((info->usage & TEXTURE_STORAGE) ? GPU_TEXTURE_STORAGE : 0) |
      ((info->usage & TEXTURE_TRANSFER) ? GPU_TEXTURE_COPY_SRC | GPU_TEXTURE_COPY_DST : 0) |
      ((info->usage == TEXTURE_RENDER) ? GPU_TEXTURE_TRANSIENT : 0),
    .srgb = info->srgb,
    .handle = info->handle,
    .label = info->label,
    .upload = {
      .stream = state.stream,
      .buffer = scratchpad,
      .levelCount = levelCount,
      .levelOffsets = levelOffsets,
      .generateMipmaps = levelCount < mipmaps
    }
  });

  // Automatically create a renderable view for renderable non-volume textures
  if ((info->usage & TEXTURE_RENDER) && info->type != TEXTURE_3D && info->depth <= state.limits.renderSize[2]) {
    if (info->mipmaps == 1) {
      texture->renderView = texture->gpu;
    } else {
      gpu_texture_view_info view = {
        .source = texture->gpu,
        .type = GPU_TEXTURE_ARRAY,
        .layerCount = info->depth,
        .levelCount = 1
      };

      texture->renderView = malloc(gpu_sizeof_texture());
      lovrAssert(texture->renderView, "Out of memory");
      lovrAssert(gpu_texture_init_view(texture->renderView, &view), "Failed to create texture view");
    }
  }

  // Sample-only textures are exempt from sync tracking to reduce overhead.  Instead, they are
  // manually synchronized with a single barrier after the upload stream.
  if (info->usage == TEXTURE_SAMPLE) {
    state.hasTextureUpload = true;
  } else if (levelCount > 0) {
    texture->sync.writePhase = GPU_PHASE_TRANSFER;
    texture->sync.pendingWrite = GPU_CACHE_TRANSFER_WRITE;
  }

  return texture;
}

Texture* lovrTextureCreateView(TextureViewInfo* view) {
  const TextureInfo* info = &view->parent->info;
  uint32_t maxDepth = info->type == TEXTURE_3D ? MAX(info->depth >> view->levelIndex, 1) : info->depth;
  lovrCheck(!info->parent, "Can't nest texture views");
  lovrCheck(view->type != TEXTURE_3D, "Texture views may not be volume textures");
  lovrCheck(view->layerCount > 0, "Texture view must have at least one layer");
  lovrCheck(view->levelCount > 0, "Texture view must have at least one mipmap");
  lovrCheck(view->layerIndex + view->layerCount <= maxDepth, "Texture view layer range exceeds depth of parent texture");
  lovrCheck(view->levelIndex + view->levelCount <= info->mipmaps, "Texture view mipmap range exceeds mipmap count of parent texture");
  lovrCheck(view->layerCount == 1 || view->type != TEXTURE_2D, "2D texture can only have a single layer");
  lovrCheck(view->levelCount == 1 || info->type != TEXTURE_3D, "Views of volume textures may only have a single mipmap level");
  lovrCheck(view->layerCount == 6 || view->type != TEXTURE_CUBE, "Cubemaps can only have a six layers");

  Texture* texture = calloc(1, sizeof(Texture) + gpu_sizeof_texture());
  lovrAssert(texture, "Out of memory");
  texture->ref = 1;
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = *info;

  texture->info.parent = view->parent;
  texture->info.mipmaps = view->levelCount;
  texture->info.width = MAX(info->width >> view->levelIndex, 1);
  texture->info.height = MAX(info->height >> view->levelIndex, 1);
  texture->info.depth = view->layerCount;

  gpu_texture_init_view(texture->gpu, &(gpu_texture_view_info) {
    .source = view->parent->gpu,
    .type = (gpu_texture_type) view->type,
    .layerIndex = view->layerIndex,
    .layerCount = view->layerCount,
    .levelIndex = view->levelIndex,
    .levelCount = view->levelCount
  });

  if (view->levelCount == 1 && view->type != TEXTURE_3D && view->layerCount <= 6) {
    texture->renderView = texture->gpu;
  }

  lovrRetain(view->parent);
  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  if (texture != state.window) {
    lovrRelease(texture->material, lovrMaterialDestroy);
    lovrRelease(texture->info.parent, lovrTextureDestroy);
    if (texture->renderView && texture->renderView != texture->gpu) gpu_texture_destroy(texture->renderView);
    if (texture->gpu) gpu_texture_destroy(texture->gpu);
  }
  free(texture);
}

const TextureInfo* lovrTextureGetInfo(Texture* texture) {
  return &texture->info;
}

static Material* lovrTextureGetMaterial(Texture* texture) {
  if (!texture->material) {
    texture->material = lovrMaterialCreate(&(MaterialInfo) {
      .data.color = { 1.f, 1.f, 1.f, 1.f },
      .data.uvScale = { 1.f, 1.f },
      .texture = texture
    });

    // Since the Material refcounts the Texture, this creates a cycle.  Release the texture to make
    // sure this is a weak relationship (the automaterial does not keep the texture refcounted).
    lovrRelease(texture, lovrTextureDestroy);
  }

  return texture->material;
}

// Sampler

Sampler* lovrGraphicsGetDefaultSampler(FilterMode mode) {
  return state.defaultSamplers[mode];
}

Sampler* lovrSamplerCreate(SamplerInfo* info) {
  lovrCheck(info->range[1] < 0.f || info->range[1] >= info->range[0], "Invalid Sampler mipmap range");
  lovrCheck(info->anisotropy <= state.limits.anisotropy, "Sampler anisotropy (%f) exceeds anisotropy limit (%f)", info->anisotropy, state.limits.anisotropy);

  Sampler* sampler = calloc(1, sizeof(Sampler) + gpu_sizeof_sampler());
  lovrAssert(sampler, "Out of memory");
  sampler->ref = 1;
  sampler->gpu = (gpu_sampler*) (sampler + 1);
  sampler->info = *info;

  gpu_sampler_info gpu = {
    .min = (gpu_filter) info->min,
    .mag = (gpu_filter) info->mag,
    .mip = (gpu_filter) info->mip,
    .wrap[0] = (gpu_wrap) info->wrap[0],
    .wrap[1] = (gpu_wrap) info->wrap[1],
    .wrap[2] = (gpu_wrap) info->wrap[2],
    .compare = (gpu_compare_mode) info->compare,
    .anisotropy = MIN(info->anisotropy, state.limits.anisotropy),
    .lodClamp = { info->range[0], info->range[1] }
  };

  gpu_sampler_init(sampler->gpu, &gpu);

  return sampler;
}

void lovrSamplerDestroy(void* ref) {
  Sampler* sampler = ref;
  gpu_sampler_destroy(sampler->gpu);
  free(sampler);
}

const SamplerInfo* lovrSamplerGetInfo(Sampler* sampler) {
  return &sampler->info;
}

// Shader

Blob* lovrGraphicsCompileShader(ShaderStage stage, Blob* source) {
  uint32_t spirv = 0x07230203;

  if (source->size % 4 == 0 && source->size >= 4 && !memcmp(source->data, &spirv, 4)) {
    return lovrRetain(source), source;
  }

#ifdef LOVR_USE_GLSLANG
  const glslang_stage_t stages[] = {
    [STAGE_VERTEX] = GLSLANG_STAGE_VERTEX,
    [STAGE_FRAGMENT] = GLSLANG_STAGE_FRAGMENT,
    [STAGE_COMPUTE] = GLSLANG_STAGE_COMPUTE
  };

  const char* stageNames[] = {
    [STAGE_VERTEX] = "vertex",
    [STAGE_FRAGMENT] = "fragment",
    [STAGE_COMPUTE] = "compute"
  };

  const char* prefix = ""
    "#version 460\n"
    "#extension GL_EXT_multiview : require\n"
    "#extension GL_GOOGLE_include_directive : require\n";

  const char* suffixes[] = {
    [STAGE_VERTEX] = ""
      "void main() {"
        "Color = PassColor * VertexColor;"
        "Normal = normalize(NormalMatrix * VertexNormal);"
        "UV = VertexUV;"
        "Position = lovrmain();"
      "}",
    [STAGE_FRAGMENT] = "void main() { PixelColors[0] = lovrmain(); }",
    [STAGE_COMPUTE] = "void main() { lovrmain(); }"
  };

  const char* strings[] = {
    prefix,
    (const char*) etc_shaders_lovr_glsl,
    "#line 1\n",
    source->data,
    suffixes[stage]
  };

  int lengths[] = {
    -1,
    etc_shaders_lovr_glsl_len,
    -1,
    source->size,
    -1
  };

  const glslang_resource_t* resource = glslang_default_resource();

  glslang_input_t input = {
    .language = GLSLANG_SOURCE_GLSL,
    .stage = stages[stage],
    .client = GLSLANG_CLIENT_VULKAN,
    .client_version = GLSLANG_TARGET_VULKAN_1_1,
    .target_language = GLSLANG_TARGET_SPV,
    .target_language_version = GLSLANG_TARGET_SPV_1_3,
    .strings = strings,
    .lengths = lengths,
    .string_count = COUNTOF(strings),
    .default_version = 460,
    .default_profile = GLSLANG_NO_PROFILE,
    .resource = resource
  };

  glslang_shader_t* shader = glslang_shader_create(&input);

  if (!glslang_shader_preprocess(shader, &input)) {
    lovrThrow("Could not preprocess %s shader:\n%s", stageNames[stage], glslang_shader_get_info_log(shader));
    return NULL;
  }

  if (!glslang_shader_parse(shader, &input)) {
    lovrThrow("Could not parse %s shader:\n%s", stageNames[stage], glslang_shader_get_info_log(shader));
    return NULL;
  }

  glslang_program_t* program = glslang_program_create();
  glslang_program_add_shader(program, shader);

  if (!glslang_program_link(program, 0)) {
    lovrThrow("Could not link shader:\n%s", glslang_program_get_info_log(program));
    return NULL;
  }

  glslang_program_SPIRV_generate(program, stages[stage]);

  void* words = glslang_program_SPIRV_get_ptr(program);
  size_t size = glslang_program_SPIRV_get_size(program) * 4;
  void* data = malloc(size);
  lovrAssert(data, "Out of memory");
  memcpy(data, words, size);
  Blob* blob = lovrBlobCreate(data, size, "SPIRV");

  glslang_program_delete(program);
  glslang_shader_delete(shader);

  return blob;
#else
  lovrThrow("Could not compile shader: No shader compiler available");
  return NULL;
#endif
}

static void lovrShaderInit(Shader* shader) {

  // Shaders store the full list of their flags so clones can override them, but they are reordered
  // to put overridden (active) ones first, so a contiguous list can be used to create pipelines
  for (uint32_t i = 0; i < shader->info.flagCount; i++) {
    ShaderFlag* flag = &shader->info.flags[i];
    uint32_t hash = flag->name ? (uint32_t) hash64(flag->name, strlen(flag->name)) : 0;
    for (uint32_t j = 0; j < shader->flagCount; j++) {
      if (hash ? (hash != shader->flagLookup[j]) : (flag->id != shader->flags[j].id)) continue;

      uint32_t index = shader->overrideCount++;

      if (index != j) {
        gpu_shader_flag temp = shader->flags[index];
        shader->flags[index] = shader->flags[j];
        shader->flags[j] = temp;

        uint32_t tempHash = shader->flagLookup[index];
        shader->flagLookup[index] = shader->flagLookup[j];
        shader->flagLookup[j] = tempHash;
      }

      shader->flags[index].value = flag->value;
    }
  }

  if (shader->info.type == SHADER_COMPUTE) {
    gpu_compute_pipeline_info pipelineInfo = {
      .shader = shader->gpu,
      .flags = shader->flags,
      .flagCount = shader->overrideCount
    };

    gpu_pipeline* pipeline = malloc(gpu_sizeof_pipeline());
    lovrAssert(pipeline, "Out of memory");
    gpu_pipeline_init_compute(pipeline, &pipelineInfo);
    shader->computePipeline = state.pipelines.length;
    arr_push(&state.pipelines, pipeline);
  }
}

Shader* lovrGraphicsGetDefaultShader(DefaultShader type) {
  if (state.defaultShaders[type]) {
    return state.defaultShaders[type];
  }

  ShaderInfo info = { .type = SHADER_GRAPHICS };

  switch (type) {
    case SHADER_UNLIT:
      info.stages[0] = lovrBlobCreate((void*) lovr_shader_unlit_vert, sizeof(lovr_shader_unlit_vert), "Unlit Vertex Shader");
      info.stages[1] = lovrBlobCreate((void*) lovr_shader_unlit_frag, sizeof(lovr_shader_unlit_frag), "Unlit Fragment Shader");
      info.label = "unlit";
      break;
    case SHADER_CUBE:
      info.stages[0] = lovrBlobCreate((void*) lovr_shader_cubemap_vert, sizeof(lovr_shader_cubemap_vert), "Cubemap Vertex Shader");
      info.stages[1] = lovrBlobCreate((void*) lovr_shader_cubemap_frag, sizeof(lovr_shader_cubemap_frag), "Cubemap Fragment Shader");
      info.label = "cubemap";
      break;
    case SHADER_PANO:
      info.stages[0] = lovrBlobCreate((void*) lovr_shader_cubemap_vert, sizeof(lovr_shader_cubemap_vert), "Cubemap Vertex Shader");
      info.stages[1] = lovrBlobCreate((void*) lovr_shader_equirect_frag, sizeof(lovr_shader_equirect_frag), "Equirect Fragment Shader");
      info.label = "equirect";
      break;
    case SHADER_FILL:
      info.stages[0] = lovrBlobCreate((void*) lovr_shader_fill_vert, sizeof(lovr_shader_fill_vert), "Fill Vertex Shader");
      info.stages[1] = lovrBlobCreate((void*) lovr_shader_unlit_frag, sizeof(lovr_shader_unlit_frag), "Unlit Fragment Shader");
      info.label = "fill";
      break;
    case SHADER_FONT:
      info.stages[0] = lovrBlobCreate((void*) lovr_shader_unlit_vert, sizeof(lovr_shader_unlit_vert), "Unlit Vertex Shader");
      info.stages[1] = lovrBlobCreate((void*) lovr_shader_font_frag, sizeof(lovr_shader_font_frag), "Font Fragment Shader");
      info.label = "font";
      break;
    default: lovrUnreachable();
  }

  state.defaultShaders[type] = lovrShaderCreate(&info);
  info.stages[0]->data = NULL;
  info.stages[1]->data = NULL;
  lovrRelease(info.stages[0], lovrBlobDestroy);
  lovrRelease(info.stages[1], lovrBlobDestroy);
  return state.defaultShaders[type];
}

Shader* lovrShaderCreate(ShaderInfo* info) {
  Shader* shader = calloc(1, sizeof(Shader) + gpu_sizeof_shader());
  lovrAssert(shader, "Out of memory");

  uint32_t stageCount = info->type == SHADER_GRAPHICS ? 2 : 1;
  uint32_t firstStage = info->type == SHADER_GRAPHICS ? GPU_STAGE_VERTEX : GPU_STAGE_COMPUTE;
  uint32_t userSet = info->type == SHADER_GRAPHICS ? 2 : 0;

  spv_result result;
  spv_info spv[2] = { 0 };
  for (uint32_t i = 0; i < stageCount; i++) {
    result = spv_parse(info->stages[i]->data, info->stages[i]->size, &spv[i]);
    lovrCheck(result == SPV_OK, "Failed to load Shader: %s\n", spv_result_to_string(result));
    lovrCheck(spv[i].version <= 0x00010300, "Invalid SPIR-V version (up to 1.3 is supported)");

    spv[i].features = tempAlloc(spv[i].featureCount * sizeof(uint32_t));
    spv[i].specConstants = tempAlloc(spv[i].specConstantCount * sizeof(spv_spec_constant));
    spv[i].pushConstants = tempAlloc(spv[i].pushConstantCount * sizeof(spv_push_constant));
    spv[i].attributes = tempAlloc(spv[i].attributeCount * sizeof(spv_attribute));
    spv[i].resources = tempAlloc(spv[i].resourceCount * sizeof(spv_resource));

    result = spv_parse(info->stages[i]->data, info->stages[i]->size, &spv[i]);
    lovrCheck(result == SPV_OK, "Failed to load Shader: %s\n", spv_result_to_string(result));

    checkShaderFeatures(spv[i].features, spv[i].featureCount);
  }

  uint32_t constantStage = spv[0].pushConstantSize > spv[1].pushConstantSize ? 0 : 1;
  uint32_t maxFlags = spv[0].specConstantCount + spv[1].specConstantCount;
  shader->constantCount = spv[constantStage].pushConstantCount;
  shader->attributeCount = spv[0].attributeCount;

  shader->constantSize = MAX(spv[0].pushConstantSize, spv[1].pushConstantSize);
  shader->constants = malloc(spv[constantStage].pushConstantCount * sizeof(ShaderConstant));
  shader->resources = malloc((spv[0].resourceCount + spv[1].resourceCount) * sizeof(ShaderResource));
  shader->attributes = malloc(spv[0].attributeCount * sizeof(ShaderAttribute));
  gpu_slot* slots = tempAlloc((spv[0].resourceCount + spv[1].resourceCount) * sizeof(gpu_slot));
  shader->flags = malloc(maxFlags * sizeof(gpu_shader_flag));
  shader->flagLookup = malloc(maxFlags * sizeof(uint32_t));
  lovrAssert(shader->constants && shader->resources && shader->attributes, "Out of memory");
  lovrAssert(shader->flags && shader->flagLookup, "Out of memory");

  // Push constants
  for (uint32_t i = 0; i < spv[constantStage].pushConstantCount; i++) {
    static const FieldType constantTypes[] = {
      [SPV_B32] = FIELD_U32,
      [SPV_I32] = FIELD_I32,
      [SPV_I32x2] = FIELD_I32x2,
      [SPV_I32x3] = FIELD_I32x3,
      [SPV_I32x4] = FIELD_I32x4,
      [SPV_U32] = FIELD_U32,
      [SPV_U32x2] = FIELD_U32x2,
      [SPV_U32x3] = FIELD_U32x3,
      [SPV_U32x4] = FIELD_U32x4,
      [SPV_F32] = FIELD_F32,
      [SPV_F32x2] = FIELD_F32x2,
      [SPV_F32x3] = FIELD_F32x3,
      [SPV_F32x4] = FIELD_F32x4,
      [SPV_MAT2] = FIELD_MAT2,
      [SPV_MAT3] = FIELD_MAT3,
      [SPV_MAT4] = FIELD_MAT4
    };

    spv_push_constant* constant = &spv[constantStage].pushConstants[i];

    shader->constants[i] = (ShaderConstant) {
      .hash = (uint32_t) hash64(constant->name, strlen(constant->name)),
      .offset = constant->offset,
      .type = constantTypes[constant->type]
    };
  }

  // Resources
  for (uint32_t s = 0; s < stageCount; s++) {
    for (uint32_t i = 0; i < spv[s].resourceCount; i++) {
      spv_resource* resource = &spv[s].resources[i];

      if (resource->set != userSet) {
        continue;
      }

      static const gpu_slot_type resourceTypes[] = {
        [SPV_UNIFORM_BUFFER] = GPU_SLOT_UNIFORM_BUFFER,
        [SPV_STORAGE_BUFFER] = GPU_SLOT_STORAGE_BUFFER,
        [SPV_SAMPLED_TEXTURE] = GPU_SLOT_SAMPLED_TEXTURE,
        [SPV_STORAGE_TEXTURE] = GPU_SLOT_STORAGE_TEXTURE,
        [SPV_SAMPLER] = GPU_SLOT_SAMPLER
      };

      uint32_t hash = (uint32_t) hash64(resource->name, strlen(resource->name));
      uint32_t stage = s == 0 ? firstStage : GPU_STAGE_FRAGMENT;
      bool append = true;

      if (s > 0) {
        for (uint32_t j = 0; j < shader->resourceCount; j++) {
          ShaderResource* other = &shader->resources[j];
          if (other->binding == resource->binding) {
            lovrCheck(other->type == resourceTypes[resource->type], "Shader variable (%d) does not use a consistent type", resource->binding);
            shader->resources[j].stageMask |= stage;
            append = false;
            break;
          }
        }
      }

      if (!append) {
        continue;
      }

      uint32_t index = shader->resourceCount++;

      if (shader->resourceCount > MAX_SHADER_RESOURCES) {
        lovrThrow("Shader resource count exceeds resourcesPerShader limit (%d)", MAX_SHADER_RESOURCES);
      }

      lovrCheck(resource->binding < 32, "Max resource binding number is %d", 32 - 1);

      slots[index] = (gpu_slot) {
        .number = resource->binding,
        .type = resourceTypes[resource->type],
        .stages = stage
      };

      shader->resources[index] = (ShaderResource) {
        .hash = hash,
        .binding = resource->binding,
        .stageMask = stage,
        .type = resourceTypes[resource->type]
      };

      bool buffer = resource->type == SPV_UNIFORM_BUFFER || resource->type == SPV_STORAGE_BUFFER;
      bool texture = resource->type == SPV_SAMPLED_TEXTURE || resource->type == SPV_STORAGE_TEXTURE;
      bool sampler = resource->type == SPV_SAMPLER;
      bool storage = resource->type == SPV_STORAGE_BUFFER || resource->type == SPV_STORAGE_TEXTURE;
      shader->bufferMask |= (buffer << resource->binding);
      shader->textureMask |= (texture << resource->binding);
      shader->samplerMask |= (sampler << resource->binding);
      shader->storageMask |= (storage << resource->binding);
    }
  }

  // Attributes
  for (uint32_t i = 0; i < spv[0].attributeCount; i++) {
    shader->attributes[i].location = spv[0].attributes[i].location;
    shader->attributes[i].hash = (uint32_t) hash64(spv[0].attributes[i].name, strlen(spv[0].attributes[i].name));
    shader->hasCustomAttributes |= shader->attributes[i].location < 10;
  }

  // Specialization constants
  for (uint32_t s = 0; s < stageCount; s++) {
    for (uint32_t i = 0; i < spv[s].specConstantCount; i++) {
      spv_spec_constant* constant = &spv[s].specConstants[i];

      bool append = true;

      if (s > 0) {
        for (uint32_t j = 0; j < spv[0].specConstantCount; j++) {
          spv_spec_constant* other = &spv[0].specConstants[j];
          if (other->id == constant->id) {
            lovrCheck(other->type == constant->type, "Shader flag (%d) does not use a consistent type", constant->id);
            lovrCheck(!strcmp(constant->name, other->name), "Shader flag (%d) does not use a consistent name", constant->id);
            append = false;
            break;
          }
        }
      }

      if (!append) {
        break;
      }

      static const gpu_flag_type flagTypes[] = {
        [SPV_B32] = GPU_FLAG_B32,
        [SPV_I32] = GPU_FLAG_I32,
        [SPV_U32] = GPU_FLAG_U32,
        [SPV_F32] = GPU_FLAG_F32
      };

      uint32_t index = shader->flagCount++;

      if (constant->name) {
        shader->flagLookup[index] = (uint32_t) hash64(constant->name, strlen(constant->name));
      } else {
        shader->flagLookup[index] = 0;
      }

      shader->flags[index] = (gpu_shader_flag) {
        .id = constant->id,
        .type = flagTypes[constant->type]
      };
    }
  }

  shader->ref = 1;
  shader->gpu = (gpu_shader*) (shader + 1);
  shader->info = *info;
  shader->layout = getLayout(slots, shader->resourceCount);

  gpu_shader_info gpu = {
    .stages[0] = { info->stages[0]->data, info->stages[0]->size },
    .pushConstantSize = shader->constantSize,
    .label = info->label
  };

  if (info->stages[1]) {
    gpu.stages[1] = (gpu_shader_stage) { info->stages[1]->data, info->stages[1]->size };
  }

  if (info->type == SHADER_GRAPHICS) {
    gpu.layouts[0] = state.layouts.data[state.builtinLayout].gpu;
    gpu.layouts[1] = state.layouts.data[state.materialLayout].gpu;
  }

  gpu.layouts[userSet] = shader->resourceCount > 0 ? state.layouts.data[shader->layout].gpu : NULL;

  gpu_shader_init(shader->gpu, &gpu);
  lovrShaderInit(shader);
  return shader;
}

Shader* lovrShaderClone(Shader* parent, ShaderFlag* flags, uint32_t count) {
  Shader* shader = calloc(1, sizeof(Shader) + gpu_sizeof_shader());
  lovrAssert(shader, "Out of memory");
  shader->ref = 1;
  lovrRetain(parent);
  shader->parent = parent;
  shader->gpu = parent->gpu;
  shader->info = parent->info;
  shader->info.flags = flags;
  shader->info.flagCount = count;
  shader->layout = parent->layout;
  shader->bufferMask = parent->bufferMask;
  shader->textureMask = parent->textureMask;
  shader->samplerMask = parent->samplerMask;
  shader->storageMask = parent->storageMask;
  shader->constantSize = parent->constantSize;
  shader->constantCount = parent->constantCount;
  shader->resourceCount = parent->resourceCount;
  shader->flagCount = parent->flagCount;
  shader->constants = parent->constants;
  shader->resources = parent->resources;
  shader->flags = malloc(shader->flagCount * sizeof(gpu_shader_flag));
  shader->flagLookup = malloc(shader->flagCount * sizeof(uint32_t));
  lovrAssert(shader->flags && shader->flagLookup, "Out of memory");
  memcpy(shader->flags, parent->flags, shader->flagCount * sizeof(gpu_shader_flag));
  memcpy(shader->flagLookup, parent->flagLookup, shader->flagCount * sizeof(uint32_t));
  lovrShaderInit(shader);
  return shader;
}

void lovrShaderDestroy(void* ref) {
  Shader* shader = ref;
  gpu_shader_destroy(shader->gpu);
  lovrRelease(shader->parent, lovrShaderDestroy);
  free(shader->constants);
  free(shader->resources);
  free(shader->attributes);
  free(shader->flags);
  free(shader->flagLookup);
  free(shader);
}

const ShaderInfo* lovrShaderGetInfo(Shader* shader) {
  return &shader->info;
}

bool lovrShaderHasStage(Shader* shader, ShaderStage stage) {
  switch (stage) {
    case STAGE_VERTEX: return shader->info.type == SHADER_GRAPHICS;
    case STAGE_FRAGMENT: return shader->info.type == SHADER_GRAPHICS;
    case STAGE_COMPUTE: return shader->info.type == SHADER_COMPUTE;
    default: return false;
  }
}

bool lovrShaderHasAttribute(Shader* shader, const char* name, uint32_t location) {
  uint32_t hash = name ? (uint32_t) hash64(name, strlen(name)) : 0;
  for (uint32_t i = 0; i < shader->attributeCount; i++) {
    ShaderAttribute* attribute = &shader->attributes[i];
    if (name ? (attribute->hash == hash) : (attribute->location == location)) {
      return true;
    }
  }
  return false;
}

// Material

Material* lovrMaterialCreate(MaterialInfo* info) {
  MaterialBlock* block = &state.materialBlocks.data[state.materialBlock];

  if (!block || block->head == ~0u || !gpu_finished(block->list[block->head].tick)) {
    bool found = false;

    for (size_t i = 0; i < state.materialBlocks.length; i++) {
      block = &state.materialBlocks.data[i];
      if (block->head != ~0u && gpu_finished(block->list[block->head].tick)) {
        state.materialBlock = i;
        found = true;
        break;
      }
    }

    if (!found) {
      arr_expand(&state.materialBlocks, 1);
      state.materialBlock = state.materialBlocks.length++;
      block = &state.materialBlocks.data[state.materialBlock];
      block->list = malloc(MATERIALS_PER_BLOCK * sizeof(Material));
      block->buffer = malloc(gpu_sizeof_buffer());
      block->bundlePool = malloc(gpu_sizeof_bundle_pool());
      block->bundles = malloc(MATERIALS_PER_BLOCK * gpu_sizeof_bundle());
      lovrAssert(block->list && block->buffer && block->bundlePool && block->bundles, "Out of memory");

      for (uint32_t i = 0; i < MATERIALS_PER_BLOCK; i++) {
        block->list[i].next = i + 1;
        block->list[i].tick = state.tick - 4;
        block->list[i].block = state.materialBlock;
        block->list[i].index = i;
        block->list[i].bundle = (gpu_bundle*) ((char*) block->bundles + i * gpu_sizeof_bundle());
      }
      block->list[MATERIALS_PER_BLOCK - 1].next = ~0u;
      block->tail = MATERIALS_PER_BLOCK - 1;
      block->head = 0;

      gpu_buffer_init(block->buffer, &(gpu_buffer_info) {
        .size = MATERIALS_PER_BLOCK * ALIGN(sizeof(MaterialData), state.limits.uniformBufferAlign),
        .pointer = &block->pointer,
        .label = "Material Block"
      });

      gpu_bundle_pool_info poolInfo = {
        .bundles = block->bundles,
        .layout = state.layouts.data[state.materialLayout].gpu,
        .count = MATERIALS_PER_BLOCK
      };

      gpu_bundle_pool_init(block->bundlePool, &poolInfo);
    }
  }

  Material* material = &block->list[block->head];
  block->head = material->next;
  material->next = ~0u;
  material->ref = 1;
  material->info = *info;

  MaterialData* data;
  uint32_t stride = ALIGN(sizeof(MaterialData), state.limits.uniformBufferAlign);

  if (block->pointer) {
    data = (MaterialData*) ((char*) block->pointer + material->index * stride);
  } else {
    beginFrame();
    uint32_t size = stride * MATERIALS_PER_BLOCK;
    gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
    data = gpu_map(scratchpad, size, 4, GPU_MAP_WRITE);
    gpu_copy_buffers(state.stream, scratchpad, block->buffer, 0, stride * material->index, stride);
    state.hasMaterialUpload = true;
  }

  memcpy(data, info, sizeof(MaterialData));

  gpu_buffer_binding buffer = {
    .object = block->buffer,
    .offset = material->index * stride,
    .extent = stride
  };

  gpu_binding bindings[8] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER, .buffer = buffer }
  };

  Texture* textures[] = {
    info->texture,
    info->glowTexture,
    info->occlusionTexture,
    info->metalnessTexture,
    info->roughnessTexture,
    info->clearcoatTexture,
    info->normalTexture
  };

  for (uint32_t i = 0; i < COUNTOF(textures); i++) {
    lovrRetain(textures[i]);
    Texture* texture = textures[i] ? textures[i] : state.defaultTexture;
    lovrCheck(i == 0 || texture->info.type == TEXTURE_2D, "Material textures must be 2D");
    bindings[i + 1] = (gpu_binding) { i + 1, GPU_SLOT_SAMPLED_TEXTURE, .texture = texture->gpu };
  }

  gpu_bundle_info bundleInfo = {
    .layout = state.layouts.data[state.materialLayout].gpu,
    .bindings = bindings,
    .count = COUNTOF(bindings)
  };

  gpu_bundle_write(&material->bundle, &bundleInfo, 1);

  return material;
}

void lovrMaterialDestroy(void* ref) {
  Material* material = ref;
  MaterialBlock* block = &state.materialBlocks.data[material->block];
  material->tick = state.tick;
  block->tail = material->index;
  if (block->head == ~0u) block->head = block->tail;
  lovrRelease(material->info.texture, lovrTextureDestroy);
  lovrRelease(material->info.glowTexture, lovrTextureDestroy);
  lovrRelease(material->info.occlusionTexture, lovrTextureDestroy);
  lovrRelease(material->info.metalnessTexture, lovrTextureDestroy);
  lovrRelease(material->info.roughnessTexture, lovrTextureDestroy);
  lovrRelease(material->info.clearcoatTexture, lovrTextureDestroy);
  lovrRelease(material->info.normalTexture, lovrTextureDestroy);
}

const MaterialInfo* lovrMaterialGetInfo(Material* material) {
  return &material->info;
}

// Font

Font* lovrFontCreate(FontInfo* info) {
  Font* font = calloc(1, sizeof(Font));
  lovrAssert(font, "Out of memory");
  font->ref = 1;
  font->info = *info;
  lovrRetain(info->rasterizer);
  arr_init(&font->glyphs, realloc);
  map_init(&font->glyphLookup, 36);
  map_init(&font->kerning, 36);

  font->pixelDensity = lovrRasterizerGetLeading(info->rasterizer);
  font->lineSpacing = 1.f;
  font->padding = (uint32_t) ceil(info->spread / 2.);

  // Initial atlas size must be big enough to hold any of the glyphs
  float box[4];
  font->atlasWidth = 1;
  font->atlasHeight = 1;
  lovrRasterizerGetBoundingBox(info->rasterizer, box);
  uint32_t maxWidth = (uint32_t) ceilf(box[2] - box[0]) + 2 * font->padding;
  uint32_t maxHeight = (uint32_t) ceilf(box[3] - box[1]) + 2 * font->padding;
  while (font->atlasWidth < 2 * maxWidth || font->atlasHeight < 2 * maxHeight) {
    font->atlasWidth <<= 1;
    font->atlasHeight <<= 1;
  }

  return font;
}

void lovrFontDestroy(void* ref) {
  Font* font = ref;
  lovrRelease(font->info.rasterizer, lovrRasterizerDestroy);
  lovrRelease(font->material, lovrMaterialDestroy);
  lovrRelease(font->atlas, lovrTextureDestroy);
  arr_free(&font->glyphs);
  map_free(&font->glyphLookup);
  map_free(&font->kerning);
  free(font);
}

const FontInfo* lovrFontGetInfo(Font* font) {
  return &font->info;
}

float lovrFontGetPixelDensity(Font* font) {
  return font->pixelDensity;
}

void lovrFontSetPixelDensity(Font* font, float pixelDensity) {
  font->pixelDensity = pixelDensity;
}

float lovrFontGetLineSpacing(Font* font) {
  return font->lineSpacing;
}

void lovrFontSetLineSpacing(Font* font, float spacing) {
  font->lineSpacing = spacing;
}

static Glyph* lovrFontGetGlyph(Font* font, uint32_t codepoint, bool* resized) {
  uint64_t hash = hash64(&codepoint, 4);
  uint64_t index = map_get(&font->glyphLookup, hash);

  if (index != MAP_NIL) {
    if (resized) *resized = false;
    return &font->glyphs.data[index];
  }

  arr_expand(&font->glyphs, 1);
  map_set(&font->glyphLookup, hash, font->glyphs.length);
  Glyph* glyph = &font->glyphs.data[font->glyphs.length++];

  glyph->codepoint = codepoint;
  glyph->advance = lovrRasterizerGetAdvance(font->info.rasterizer, codepoint);

  if (lovrRasterizerIsGlyphEmpty(font->info.rasterizer, codepoint)) {
    memset(glyph->box, 0, sizeof(glyph->box));
    if (resized) *resized = false;
    return glyph;
  }

  lovrRasterizerGetGlyphBoundingBox(font->info.rasterizer, codepoint, glyph->box);

  float width = glyph->box[2] - glyph->box[0];
  float height = glyph->box[3] - glyph->box[1];
  uint32_t pixelWidth = 2 * font->padding + (uint32_t) ceilf(width);
  uint32_t pixelHeight = 2 * font->padding + (uint32_t) ceilf(height);

  // If the glyph exceeds the width, start a new row
  if (font->atlasX + pixelWidth > font->atlasWidth) {
    font->atlasX = font->atlasWidth == font->atlasHeight ? 0 : font->atlasWidth >> 1;
    font->atlasY += font->rowHeight;
  }

  // If the glyph exceeds the height, expand the atlas
  if (font->atlasY + pixelHeight > font->atlasHeight) {
    if (font->atlasWidth == font->atlasHeight) {
      font->atlasX = font->atlasWidth;
      font->atlasY = 0;
      font->atlasWidth <<= 1;
      font->rowHeight = 0;
    } else {
      font->atlasX = 0;
      font->atlasY = font->atlasHeight;
      font->atlasHeight <<= 1;
      font->rowHeight = 0;
    }
  }

  glyph->x = font->atlasX + font->padding;
  glyph->y = font->atlasY + font->padding;
  glyph->uv[0] = (uint16_t) ((float) glyph->x / font->atlasWidth * 65535.f + .5f);
  glyph->uv[1] = (uint16_t) ((float) glyph->y / font->atlasHeight * 65535.f + .5f);
  glyph->uv[2] = (uint16_t) ((float) (glyph->x + width) / font->atlasWidth * 65535.f + .5f);
  glyph->uv[3] = (uint16_t) ((float) (glyph->y + height) / font->atlasHeight * 65535.f + .5f);

  font->atlasX += pixelWidth;
  font->rowHeight = MAX(font->rowHeight, pixelHeight);

  // Atlas resize
  if (!font->atlas || font->atlasWidth > font->atlas->info.width || font->atlasHeight > font->atlas->info.height) {
    lovrCheck(font->atlasWidth <= 65536, "Font atlas is way too big!");

    Texture* atlas = lovrTextureCreate(&(TextureInfo) {
      .type = TEXTURE_2D,
      .format = FORMAT_RGBA8,
      .width = font->atlasWidth,
      .height = font->atlasHeight,
      .depth = 1,
      .mipmaps = 1,
      .samples = 1,
      .usage = TEXTURE_SAMPLE | TEXTURE_TRANSFER,
      .label = "Font Atlas"
    });

    float clear[4] = { 0.f, 0.f, 0.f, 0.f };
    gpu_clear_texture(state.stream, atlas->gpu, clear, 0, ~0u, 0, ~0u);

    // This barrier serves 2 purposes:
    // - Ensure new atlas clear is finished/flushed before copying to it
    // - Ensure any unsynchronized pending uploads to old atlas finish before copying to new atlas
    gpu_barrier barrier;
    barrier.prev = GPU_PHASE_TRANSFER;
    barrier.next = GPU_PHASE_TRANSFER;
    barrier.flush = GPU_CACHE_TRANSFER_WRITE;
    barrier.clear = GPU_CACHE_TRANSFER_READ;
    gpu_sync(state.stream, &barrier, 1);

    if (font->atlas) {
      uint32_t srcOffset[4] = { 0, 0, 0, 0 };
      uint32_t dstOffset[4] = { 0, 0, 0, 0 };
      uint32_t extent[3] = { font->atlas->info.width, font->atlas->info.height, 1 };
      gpu_copy_textures(state.stream, font->atlas->gpu, atlas->gpu, srcOffset, dstOffset, extent);
      lovrRelease(font->atlas, lovrTextureDestroy);
    }

    font->atlas = atlas;

    // Material
    lovrRelease(font->material, lovrMaterialDestroy);
    font->material = lovrMaterialCreate(&(MaterialInfo) {
      .data.color = { 1.f, 1.f, 1.f, 1.f },
      .data.uvScale = { 1.f, 1.f },
      .data.sdfRange = { font->info.spread / font->atlasWidth, font->info.spread / font->atlasHeight },
      .texture = font->atlas
    });

    // Recompute all glyph uvs after atlas resize
    for (size_t i = 0; i < font->glyphs.length; i++) {
      Glyph* g = &font->glyphs.data[i];
      if (g->box[2] - g->box[0] > 0.f) {
        g->uv[0] = (uint16_t) ((float) g->x / font->atlasWidth * 65535.f + .5f);
        g->uv[1] = (uint16_t) ((float) g->y / font->atlasHeight * 65535.f + .5f);
        g->uv[2] = (uint16_t) ((float) (g->x + g->box[2] - g->box[0]) / font->atlasWidth * 65535.f + .5f);
        g->uv[3] = (uint16_t) ((float) (g->y + g->box[3] - g->box[1]) / font->atlasHeight * 65535.f + .5f);
      }
    }

    if (resized) *resized = true;
  }

  gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());

  uint32_t stack = tempPush();
  float* pixels = tempAlloc(pixelWidth * pixelHeight * 4 * sizeof(float));
  lovrRasterizerGetPixels(font->info.rasterizer, glyph->codepoint, pixels, pixelWidth, pixelHeight, font->info.spread);
  uint8_t* dst = gpu_map(scratchpad, pixelWidth * pixelHeight * 4 * sizeof(uint8_t), 4, GPU_MAP_WRITE);
  float* src = pixels;
  for (uint32_t y = 0; y < pixelHeight; y++) {
    for (uint32_t x = 0; x < pixelWidth; x++) {
      for (uint32_t c = 0; c < 4; c++) {
        float f = *src++; // CLAMP would evaluate this multiple times
        *dst++ = (uint8_t) (CLAMP(f, 0.f, 1.f) * 255.f + .5f);
      }
    }
  }
  uint32_t dstOffset[4] = { glyph->x - font->padding, glyph->y - font->padding, 0, 0 };
  uint32_t extent[3] = { pixelWidth, pixelHeight, 1 };
  gpu_copy_buffer_texture(state.stream, scratchpad, font->atlas->gpu, 0, dstOffset, extent);
  tempPop(stack);

  state.hasGlyphUpload = true;
  return glyph;
}

float lovrFontGetKerning(Font* font, uint32_t first, uint32_t second) {
  uint32_t codepoints[] = { first, second };
  uint64_t hash = hash64(codepoints, sizeof(codepoints));
  union { float f32; uint64_t u64; } kerning = { .u64 = map_get(&font->kerning, hash) };

  if (kerning.u64 == MAP_NIL) {
    kerning.f32 = lovrRasterizerGetKerning(font->info.rasterizer, first, second);
    map_set(&font->kerning, hash, kerning.u64);
  }

  return kerning.f32;
}

float lovrFontGetWidth(Font* font, ColoredString* strings, uint32_t count) {
  float x = 0.f;
  float maxWidth = 0.f;
  float space = lovrFontGetGlyph(font, ' ', NULL)->advance;

  for (uint32_t i = 0; i < count; i++) {
    size_t bytes;
    uint32_t codepoint;
    uint32_t previous = '\0';
    const char* str = strings[i].string;
    const char* end = strings[i].string + strings[i].length;
    while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {
      if (codepoint == ' ' || codepoint == '\t') {
        x += codepoint == '\t' ? space * 4.f : space;
        previous = '\0';
        str += bytes;
        continue;
      } else if (codepoint == '\n') {
        maxWidth = MAX(maxWidth, x);
        x = 0.f;
        previous = '\0';
        str += bytes;
        continue;
      } else if (codepoint == '\r') {
        str += bytes;
        continue;
      }

      Glyph* glyph = lovrFontGetGlyph(font, codepoint, NULL);

      if (previous) x += lovrFontGetKerning(font, previous, codepoint);
      previous = codepoint;

      x += glyph->advance;
      str += bytes;
    }
  }

  return MAX(maxWidth, x) / font->pixelDensity;
}

void lovrFontGetLines(Font* font, ColoredString* strings, uint32_t count, float wrap, void (*callback)(void* context, const char* string, size_t length), void* context) {
  size_t totalLength = 0;
  for (uint32_t i = 0; i < count; i++) {
    totalLength += strings[i].length;
  }

  uint32_t stack = tempPush();
  char* string = tempAlloc(totalLength + 1);
  string[totalLength] = '\0';

  for (uint32_t i = 0, cursor = 0; i < count; cursor += strings[i].length, i++) {
    memcpy(string + cursor, strings[i].string, strings[i].length);
  }

  float x = 0.f;
  float nextWordStartX = 0.f;
  wrap *= font->pixelDensity;

  size_t bytes;
  uint32_t codepoint;
  uint32_t previous = '\0';
  const char* lineStart = string;
  const char* wordStart = string;
  const char* end = string + totalLength;
  float space = lovrFontGetGlyph(font, ' ', NULL)->advance;
  while ((bytes = utf8_decode(string, end, &codepoint)) > 0) {
    if (codepoint == ' ' || codepoint == '\t') {
      x += codepoint == '\t' ? space * 4.f : space;
      nextWordStartX = x;
      previous = '\0';
      string += bytes;
      wordStart = string;
      continue;
    } else if (codepoint == '\n') {
      size_t length = string - lineStart;
      while (string[length] == ' ' || string[length] == '\t') length--;
      callback(context, lineStart, length);
      nextWordStartX = 0.f;
      x = 0.f;
      previous = '\0';
      string += bytes;
      lineStart = string;
      wordStart = string;
      continue;
    } else if (codepoint == '\r') {
      string += bytes;
      continue;
    }

    Glyph* glyph = lovrFontGetGlyph(font, codepoint, NULL);

    // Keming
    if (previous) x += lovrFontGetKerning(font, previous, codepoint);
    previous = codepoint;

    // Wrap
    if (wordStart != lineStart && x + glyph->advance > wrap) {
      size_t length = wordStart - lineStart;
      while (string[length] == ' ' || string[length] == '\t') length--;
      callback(context, lineStart, length);
      lineStart = wordStart;
      x -= nextWordStartX;
      nextWordStartX = 0.f;
      previous = '\0';
    }

    // Advance
    x += glyph->advance;
    string += bytes;
  }

  if (end - lineStart > 0) {
    callback(context, lineStart, end - lineStart);
  }

  tempPop(stack);
}

// Model

Model* lovrModelCreate(ModelInfo* info) {
  ModelData* data = info->data;
  Model* model = calloc(1, sizeof(Model));
  lovrAssert(model, "Out of memory");
  model->ref = 1;
  model->info = *info;
  lovrRetain(info->data);

  // Textures
  model->textures = malloc(data->imageCount * sizeof(Texture*));
  lovrAssert(model->textures, "Out of memory");
  for (uint32_t i = 0; i < data->imageCount; i++) {
    model->textures[i] = lovrTextureCreate(&(TextureInfo) {
      .type = TEXTURE_2D,
      .usage = TEXTURE_SAMPLE,
      .format = lovrImageGetFormat(data->images[i]),
      .width = lovrImageGetWidth(data->images[i], 0),
      .height = lovrImageGetHeight(data->images[i], 0),
      .depth = 1,
      .mipmaps = info->mipmaps || lovrImageGetLevelCount(data->images[i]) > 1 ? ~0u : 1,
      .samples = 1,
      .srgb = lovrImageIsSRGB(data->images[i]),
      .images = &data->images[i],
      .imageCount = 1
    });
  }

  // Materials
  model->materials = malloc(data->materialCount * sizeof(Material*));
  lovrAssert(model->materials, "Out of memory");
  for (uint32_t i = 0; i < data->materialCount; i++) {
    MaterialInfo material;
    ModelMaterial* properties = &data->materials[i];
    memcpy(&material.data, properties, sizeof(MaterialData));
    material.texture = properties->texture == ~0u ? NULL : model->textures[properties->texture];
    material.glowTexture = properties->glowTexture == ~0u ? NULL : model->textures[properties->glowTexture];
    material.occlusionTexture = properties->occlusionTexture == ~0u ? NULL : model->textures[properties->occlusionTexture];
    material.metalnessTexture = properties->metalnessTexture == ~0u ? NULL : model->textures[properties->metalnessTexture];
    material.roughnessTexture = properties->roughnessTexture == ~0u ? NULL : model->textures[properties->roughnessTexture];
    material.clearcoatTexture = properties->clearcoatTexture == ~0u ? NULL : model->textures[properties->clearcoatTexture];
    material.normalTexture = properties->normalTexture == ~0u ? NULL : model->textures[properties->normalTexture];
    model->materials[i] = lovrMaterialCreate(&material);
  }

  // Buffers
  char* vertices;
  char* indices;
  char* skinData;

  BufferInfo vertexBufferInfo = {
    .length = data->vertexCount,
    .stride = sizeof(ModelVertex),
    .fieldCount = 5,
    .fields[0] = { 0, 10, FIELD_F32x3, offsetof(ModelVertex, position) },
    .fields[1] = { 0, 11, FIELD_F32x3, offsetof(ModelVertex, normal) },
    .fields[2] = { 0, 12, FIELD_F32x2, offsetof(ModelVertex, uv) },
    .fields[3] = { 0, 13, FIELD_UN8x4, offsetof(ModelVertex, color) },
    .fields[4] = { 0, 14, FIELD_F32x3, offsetof(ModelVertex, tangent) }
  };

  model->vertexBuffer = lovrBufferCreate(&vertexBufferInfo, (void**) &vertices);

  if (data->skinnedVertexCount > 0) {
    model->skinBuffer = lovrBufferCreate(&(BufferInfo) {
      .length = data->skinnedVertexCount,
      .stride = 8,
      .fieldCount = 2,
      .fields[0] = { 0, 0, FIELD_UN8x4, 0 },
      .fields[1] = { 0, 0, FIELD_U8x4, 4 }
    }, (void**) &skinData);

    vertexBufferInfo.length = data->skinnedVertexCount;
    model->rawVertexBuffer = lovrBufferCreate(&vertexBufferInfo, NULL);

    beginFrame();
    gpu_buffer* src = model->vertexBuffer->gpu;
    gpu_buffer* dst = model->rawVertexBuffer->gpu;
    gpu_copy_buffers(state.stream, src, dst, 0, 0, data->skinnedVertexCount * sizeof(ModelVertex));

    gpu_barrier barrier;
    barrier.prev = GPU_PHASE_TRANSFER;
    barrier.next = GPU_PHASE_SHADER_COMPUTE;
    barrier.flush = GPU_CACHE_TRANSFER_WRITE;
    barrier.clear = GPU_CACHE_STORAGE_READ | GPU_CACHE_STORAGE_WRITE;
    gpu_sync(state.stream, &barrier, 1);
  }

  size_t indexSize = data->indexType == U32 ? 4 : 2;

  if (data->indexCount > 0) {
    model->indexBuffer = lovrBufferCreate(&(BufferInfo) {
      .length = data->indexCount,
      .stride = indexSize,
      .fieldCount = 1,
      .fields[0] = { 0, 0, data->indexType == U32 ? FIELD_U32 : FIELD_I32, 0 }
    }, (void**) &indices);
  }

  // Sort primitives by their skin, so there is a single contiguous region of skinned vertices
  uint32_t stack = tempPush();
  uint64_t* map = tempAlloc(data->primitiveCount * sizeof(uint64_t));

  for (uint32_t i = 0; i < data->primitiveCount; i++) {
    map[i] = ((uint64_t) data->primitives[i].skin << 32) | i;
  }

  qsort(map, data->primitiveCount, sizeof(uint64_t), u64cmp);

  // Draws
  model->draws = calloc(data->primitiveCount, sizeof(Draw));
  lovrAssert(model->draws, "Out of memory");
  for (uint32_t i = 0, vertexCursor = 0, indexCursor = 0; i < data->primitiveCount; i++) {
    ModelPrimitive* primitive = &data->primitives[map[i] & ~0u];
    Draw* draw = &model->draws[map[i] & ~0u];

    switch (primitive->mode) {
      case DRAW_POINTS: draw->mode = VERTEX_POINTS; break;
      case DRAW_LINES: draw->mode = VERTEX_LINES; break;
      case DRAW_TRIANGLES: draw->mode = VERTEX_TRIANGLES; break;
      default: lovrThrow("Model uses an unsupported draw mode (lineloop, linestrip, strip, fan)");
    }

    draw->material = primitive->material == ~0u ? NULL: model->materials[primitive->material];
    draw->vertex.buffer = model->vertexBuffer;
    draw->index.stride = indexSize;

    if (primitive->indices) {
      draw->index.buffer = model->indexBuffer;
      draw->start = indexCursor;
      draw->count = primitive->indices->count;
      draw->base = vertexCursor;
      indexCursor += draw->count;
    } else {
      draw->start = vertexCursor;
      draw->count = primitive->attributes[ATTR_POSITION]->count;
    }

    vertexCursor += primitive->attributes[ATTR_POSITION]->count;
  }

  // Vertices
  for (uint32_t i = 0; i < data->primitiveCount; i++) {
    ModelPrimitive* primitive = &data->primitives[map[i] & ~0u];
    ModelAttribute** attributes = primitive->attributes;
    uint32_t count = attributes[ATTR_POSITION]->count;
    size_t stride = sizeof(ModelVertex);

    lovrModelDataCopyAttribute(data, attributes[ATTR_POSITION], vertices + 0, F32, 3, false, count, stride, 0);
    lovrModelDataCopyAttribute(data, attributes[ATTR_NORMAL], vertices + 12, F32, 3, false, count, stride, 0);
    lovrModelDataCopyAttribute(data, attributes[ATTR_TEXCOORD], vertices + 24, F32, 2, false, count, stride, 0);
    lovrModelDataCopyAttribute(data, attributes[ATTR_COLOR], vertices + 32, U8, 4, true, count, stride, 255);
    lovrModelDataCopyAttribute(data, attributes[ATTR_TANGENT], vertices + 36, F32, 3, false, count, stride, 0);
    vertices += count * stride;

    if (data->skinnedVertexCount > 0 && primitive->skin != ~0u) {
      lovrModelDataCopyAttribute(data, attributes[ATTR_JOINTS], skinData + 0, U8, 4, false, count, 8, 0);
      lovrModelDataCopyAttribute(data, attributes[ATTR_WEIGHTS], skinData + 4, U8, 4, true, count, 8, 0);
      skinData += count * 8;
    }

    if (primitive->indices) {
      char* indexData = data->buffers[primitive->indices->buffer].data + primitive->indices->offset;
      memcpy(indices, indexData, primitive->indices->count * indexSize);
      indices += primitive->indices->count * indexSize;
    }
  }

  for (uint32_t i = 0; i < data->skinCount; i++) {
    lovrCheck(data->skins[i].jointCount <= 256, "Currently, the max number of joints per skin is 256");
  }

  model->localTransforms = malloc(sizeof(NodeTransform) * data->nodeCount);
  model->globalTransforms = malloc(16 * sizeof(float) * data->nodeCount);
  lovrAssert(model->localTransforms && model->globalTransforms, "Out of memory");
  lovrModelResetPose(model);
  tempPop(stack);

  return model;
}

void lovrModelDestroy(void* ref) {
  Model* model = ref;
  ModelData* data = model->info.data;
  for (uint32_t i = 0; i < data->materialCount; i++) {
    lovrRelease(model->materials[i], lovrMaterialDestroy);
  }
  for (uint32_t i = 0; i < data->imageCount; i++) {
    lovrRelease(model->textures[i], lovrTextureDestroy);
  }
  lovrRelease(model->rawVertexBuffer, lovrBufferDestroy);
  lovrRelease(model->vertexBuffer, lovrBufferDestroy);
  lovrRelease(model->indexBuffer, lovrBufferDestroy);
  lovrRelease(model->skinBuffer, lovrBufferDestroy);
  lovrRelease(model->info.data, lovrModelDataDestroy);
  free(model->localTransforms);
  free(model->globalTransforms);
  free(model->draws);
  free(model->materials);
  free(model->textures);
  free(model);
}

const ModelInfo* lovrModelGetInfo(Model* model) {
  return &model->info;
}

void lovrModelResetPose(Model* model) {
  ModelData* data = model->info.data;
  for (uint32_t i = 0; i < data->nodeCount; i++) {
    vec3 position = model->localTransforms[i].properties[PROP_TRANSLATION];
    quat orientation = model->localTransforms[i].properties[PROP_ROTATION];
    vec3 scale = model->localTransforms[i].properties[PROP_SCALE];
    if (data->nodes[i].matrix) {
      mat4_getPosition(data->nodes[i].transform.matrix, position);
      mat4_getOrientation(data->nodes[i].transform.matrix, orientation);
      mat4_getScale(data->nodes[i].transform.matrix, scale);
    } else {
      vec3_init(position, data->nodes[i].transform.properties.translation);
      quat_init(orientation, data->nodes[i].transform.properties.rotation);
      vec3_init(scale, data->nodes[i].transform.properties.scale);
    }
  }
  model->transformsDirty = true;
}

void lovrModelAnimate(Model* model, uint32_t animationIndex, float time, float alpha) {
  if (alpha <= 0.f) return;

  ModelData* data = model->info.data;
  lovrAssert(animationIndex < data->animationCount, "Invalid animation index '%d' (Model has %d animation%s)", animationIndex + 1, data->animationCount, data->animationCount == 1 ? "" : "s");
  ModelAnimation* animation = &data->animations[animationIndex];
  time = fmodf(time, animation->duration);

  for (uint32_t i = 0; i < animation->channelCount; i++) {
    ModelAnimationChannel* channel = &animation->channels[i];
    uint32_t node = channel->nodeIndex;
    NodeTransform* transform = &model->localTransforms[node];

    uint32_t keyframe = 0;
    while (keyframe < channel->keyframeCount && channel->times[keyframe] < time) {
      keyframe++;
    }

    float property[4];
    bool rotate = channel->property == PROP_ROTATION;
    size_t n = 3 + rotate;
    float* (*lerp)(float* a, float* b, float t) = rotate ? quat_slerp : vec3_lerp;

    // Handle the first/last keyframe case (no interpolation)
    if (keyframe == 0 || keyframe >= channel->keyframeCount) {
      size_t index = MIN(keyframe, channel->keyframeCount - 1);

      // For cubic interpolation, each keyframe has 3 parts, and the actual data is in the middle
      if (channel->smoothing == SMOOTH_CUBIC) {
        index = 3 * index + 1;
      }

      memcpy(property, channel->data + index * n, n * sizeof(float));
    } else {
      float t1 = channel->times[keyframe - 1];
      float t2 = channel->times[keyframe];
      float z = (time - t1) / (t2 - t1);

      switch (channel->smoothing) {
        case SMOOTH_STEP:
          memcpy(property, channel->data + (z >= .5f ? keyframe : keyframe - 1) * n, n * sizeof(float));
          break;
        case SMOOTH_LINEAR:
          memcpy(property, channel->data + (keyframe - 1) * n, n * sizeof(float));
          lerp(property, channel->data + keyframe * n, z);
          break;
        case SMOOTH_CUBIC: {
          size_t stride = 3 * n;
          float* p0 = channel->data + (keyframe - 1) * stride + 1 * n;
          float* m0 = channel->data + (keyframe - 1) * stride + 2 * n;
          float* p1 = channel->data + (keyframe - 0) * stride + 1 * n;
          float* m1 = channel->data + (keyframe - 0) * stride + 0 * n;
          float dt = t2 - t1;
          float z2 = z * z;
          float z3 = z2 * z;
          float a = 2.f * z3 - 3.f * z2 + 1.f;
          float b = 2.f * z3 - 3.f * z2 + 1.f;
          float c = -2.f * z3 + 3.f * z2;
          float d = (z3 * -z2) * dt;
          for (size_t j = 0; j < n; j++) {
            property[j] = a * p0[j] + b * m0[j] + c * p1[j] + d * m1[j];
          }
          break;
        }
        default: break;
      }
    }

    if (alpha >= 1.f) {
      memcpy(transform->properties[channel->property], property, n * sizeof(float));
    } else {
      lerp(transform->properties[channel->property], property, alpha);
    }
  }

  model->transformsDirty = true;
}

void lovrModelGetNodePose(Model* model, uint32_t node, float position[4], float rotation[4], CoordinateSpace space) {
  ModelData* data = model->info.data;
  lovrAssert(node < data->nodeCount, "Invalid node index '%d' (Model has %d node%s)", node, data->nodeCount, data->nodeCount == 1 ? "" : "s");
  if (space == SPACE_LOCAL) {
    vec3_init(position, model->localTransforms[node].properties[PROP_TRANSLATION]);
    quat_init(rotation, model->localTransforms[node].properties[PROP_ROTATION]);
  } else {
    if (model->transformsDirty) {
      updateModelTransforms(model, data->rootNode, (float[]) MAT4_IDENTITY);
      model->transformsDirty = false;
    }
    mat4_getPosition(model->globalTransforms + 16 * node, position);
    mat4_getOrientation(model->globalTransforms + 16 * node, rotation);
  }
}

void lovrModelSetNodePose(Model* model, uint32_t node, float position[4], float rotation[4], float alpha) {
  if (alpha <= 0.f) return;

  ModelData* data = model->info.data;
  lovrAssert(node < data->nodeCount, "Invalid node index '%d' (Model has %d node%s)", node, data->nodeCount, data->nodeCount == 1 ? "" : "s");
  NodeTransform* transform = &model->localTransforms[node];

  if (alpha >= 1.f) {
    vec3_init(transform->properties[PROP_TRANSLATION], position);
    quat_init(transform->properties[PROP_ROTATION], rotation);
  } else {
    vec3_lerp(transform->properties[PROP_TRANSLATION], position, alpha);
    quat_slerp(transform->properties[PROP_ROTATION], rotation, alpha);
  }

  model->transformsDirty = true;
}

Texture* lovrModelGetTexture(Model* model, uint32_t index) {
  ModelData* data = model->info.data;
  lovrAssert(index < data->imageCount, "Invalid texture index '%d' (Model has %d texture%s)", index, data->imageCount, data->imageCount == 1 ? "" : "s");
  return model->textures[index];
}

Material* lovrModelGetMaterial(Model* model, uint32_t index) {
  ModelData* data = model->info.data;
  lovrAssert(index < data->materialCount, "Invalid material index '%d' (Model has %d material%s)", index, data->materialCount, data->materialCount == 1 ? "" : "s");
  return model->materials[index];
}

Buffer* lovrModelGetVertexBuffer(Model* model) {
  return model->rawVertexBuffer;
}

Buffer* lovrModelGetIndexBuffer(Model* model) {
  return model->indexBuffer;
}

static void lovrModelReskin(Model* model) {
  ModelData* data = model->info.data;

  if (data->skinCount == 0 || model->lastReskin == state.tick) {
    return;
  }

  if (!state.animator) {
    Blob* source = lovrBlobCreate((void*) lovr_shader_animator_comp, sizeof(lovr_shader_animator_comp), NULL);
    state.animator = lovrShaderCreate(&(ShaderInfo) {
      .type = SHADER_COMPUTE,
      .stages[0] = source,
      .flags = &(ShaderFlag) { "local_size_x_id", 0, state.device.subgroupSize },
      .label = "Animator"
    });
    source->data = NULL;
    lovrRelease(source, lovrBlobDestroy);
  }

  gpu_pipeline* pipeline = state.pipelines.data[state.animator->computePipeline];
  gpu_layout* layout = state.layouts.data[state.animator->layout].gpu;
  gpu_shader* shader = state.animator->gpu;
  gpu_buffer* joints = tempAlloc(gpu_sizeof_buffer());

  uint32_t count = data->skinnedVertexCount;

  gpu_binding bindings[] = {
    { 0, GPU_SLOT_STORAGE_BUFFER, .buffer = { model->rawVertexBuffer->gpu, 0, count * sizeof(ModelVertex) } },
    { 1, GPU_SLOT_STORAGE_BUFFER, .buffer = { model->vertexBuffer->gpu, 0, count * sizeof(ModelVertex) } },
    { 2, GPU_SLOT_STORAGE_BUFFER, .buffer = { model->skinBuffer->gpu, 0, count * 8 } },
    { 3, GPU_SLOT_UNIFORM_BUFFER, .buffer = { joints, 0, 0 } } // Filled in for each skin
  };

  for (uint32_t i = 0, baseVertex = 0; i < data->skinCount; i++) {
    ModelSkin* skin = &data->skins[i];

    float transform[16];
    uint32_t size = bindings[3].buffer.extent = skin->jointCount * 16 * sizeof(float);
    float* joint = gpu_map(joints, size, state.limits.uniformBufferAlign, GPU_MAP_WRITE);
    for (uint32_t j = 0; j < skin->jointCount; j++) {
      mat4_init(transform, model->globalTransforms + 16 * skin->joints[j]);
      mat4_mul(transform, skin->inverseBindMatrices + 16 * j);
      memcpy(joint, transform, sizeof(transform));
      joint += 16;
    }

    gpu_bundle* bundle = getBundle(state.animator->layout);
    gpu_bundle_info bundleInfo = { layout, bindings, COUNTOF(bindings) };
    gpu_bundle_write(&bundle, &bundleInfo, 1);

    uint32_t constants[] = { baseVertex, skin->vertexCount };
    uint32_t subgroupSize = state.device.subgroupSize;

    gpu_compute_begin(state.stream);
    gpu_bind_pipeline(state.stream, pipeline, true);
    gpu_bind_bundle(state.stream, shader, 0, bundle, NULL, 0);
    gpu_push_constants(state.stream, shader, constants, sizeof(constants));
    gpu_compute(state.stream, (skin->vertexCount + subgroupSize - 1) / subgroupSize, 1, 1);
    gpu_compute_end(state.stream);
    baseVertex += skin->vertexCount;
    state.stats.pipelineSwitches++;
    state.stats.bundleSwitches++;
  }

  state.hasReskin = true;
}

// Tally

Tally* lovrTallyCreate(TallyInfo* info) {
  lovrCheck(info->count > 0, "Tally count must be greater than zero");
  lovrCheck(info->count <= 4096, "Maximum Tally count is 4096");
  lovrCheck(info->views <= state.limits.renderSize[2], "Tally view count can not exceed the maximum view count");
  Tally* tally = calloc(1, sizeof(Tally) + gpu_sizeof_tally());
  lovrAssert(tally, "Out of memory");
  tally->ref = 1;
  tally->tick = state.tick;
  tally->info = *info;
  tally->gpu = (gpu_tally*) (tally + 1);
  tally->masks = calloc((info->count + 63) / 64, sizeof(uint64_t));
  lovrAssert(tally->masks, "Out of memory");

  uint32_t total = info->count * (info->type == TALLY_TIMER ? 2 * info->views : 1);

  gpu_tally_init(tally->gpu, &(gpu_tally_info) {
    .type = (gpu_tally_type) info->type,
    .count = total
  });

  if (info->type == TALLY_TIMER) {
    tally->buffer = calloc(1, gpu_sizeof_buffer());
    lovrAssert(tally->buffer, "Out of memory");
    gpu_buffer_init(tally->buffer, &(gpu_buffer_info) {
      .size = info->count * 2 * info->views * sizeof(uint32_t)
    });
  }

  return tally;
}

void lovrTallyDestroy(void* ref) {
  Tally* tally = ref;
  gpu_tally_destroy(tally->gpu);
  if (tally->buffer) gpu_buffer_destroy(tally->buffer);
  free(tally->buffer);
  free(tally->masks);
  free(tally);
}

// Pass

Pass* lovrGraphicsGetPass(PassInfo* info) {
  beginFrame();
  Pass* pass = tempAlloc(sizeof(Pass));
  pass->ref = 1;
  pass->info = *info;
  pass->stream = gpu_stream_begin(info->label);
  arr_init(&pass->access, tempGrow);
  arr_push(&state.passes, pass);
  lovrRetain(pass);

  if (info->type == PASS_TRANSFER) {
    return pass;
  }

  if (info->type == PASS_COMPUTE) {
    memset(pass->constants, 0, sizeof(pass->constants));
    pass->constantsDirty = true;

    pass->bindingMask = 0;
    pass->bindingsDirty = true;

    pass->pipelineIndex = 0;
    pass->pipeline = &pass->pipelines[0];
    pass->pipeline->shader = NULL;
    pass->pipeline->dirty = true;

    gpu_compute_begin(pass->stream);

    return pass;
  }

  // Validation

  Canvas* canvas = &info->canvas;
  const TextureInfo* main = canvas->textures[0] ? &canvas->textures[0]->info : &canvas->depth.texture->info;
  lovrCheck(canvas->textures[0] || canvas->depth.texture, "Render pass must have at least one color or depth texture");
  lovrCheck(main->width <= state.limits.renderSize[0], "Render pass width (%d) exceeds the renderSize limit of this GPU (%d)", main->width, state.limits.renderSize[0]);
  lovrCheck(main->height <= state.limits.renderSize[1], "Render pass height (%d) exceeds the renderSize limit of this GPU (%d)", main->height, state.limits.renderSize[1]);
  lovrCheck(main->depth <= state.limits.renderSize[2], "Render pass view count (%d) exceeds the renderSize limit of this GPU (%d)", main->depth, state.limits.renderSize[2]);
  lovrCheck(canvas->samples == 1 || canvas->samples == 4, "Render pass sample count must be 1 or 4...for now");

  uint32_t colorTextureCount = 0;
  for (uint32_t i = 0; i < COUNTOF(canvas->textures) && canvas->textures[i]; i++, colorTextureCount++) {
    const TextureInfo* texture = &canvas->textures[i]->info;
    bool renderable = texture->format == GPU_FORMAT_SURFACE || (state.features.formats[texture->format] & GPU_FEATURE_RENDER);
    lovrCheck(renderable, "This GPU does not support rendering to the texture format used by Canvas texture #%d", i + 1);
    lovrCheck(texture->usage & TEXTURE_RENDER, "Texture must be created with the 'render' flag to render to it");
    lovrCheck(texture->width == main->width, "Render pass texture sizes must match");
    lovrCheck(texture->height == main->height, "Render pass texture sizes must match");
    lovrCheck(texture->depth == main->depth, "Render pass texture sizes must match");
    lovrCheck(texture->samples == main->samples, "Render pass texture sample counts must match");
  }

  if (canvas->depth.texture || canvas->depth.format) {
    TextureFormat format = canvas->depth.texture ? canvas->depth.texture->info.format : canvas->depth.format;
    bool renderable = state.features.formats[format] & GPU_FEATURE_RENDER;
    lovrCheck(format == FORMAT_D16 || format == FORMAT_D24S8 || format == FORMAT_D32F, "Depth buffer must use a depth format");
    lovrCheck(renderable, "This GPU does not support depth buffers with this texture format");
    if (canvas->depth.texture) {
      const TextureInfo* texture = &canvas->depth.texture->info;
      lovrCheck(texture->usage & TEXTURE_RENDER, "Texture must be created with the 'render' flag to render to it");
      lovrCheck(texture->width == main->width, "Render pass texture sizes must match");
      lovrCheck(texture->height == main->height, "Render pass texture sizes must match");
      lovrCheck(texture->depth == main->depth, "Render pass texture sizes must match");
      lovrCheck(texture->samples == main->samples, "Depth buffer sample count must match the main render pass sample count...for now");
    }
  }

  // Render target

  gpu_canvas target = {
    .size = { main->width, main->height }
  };

  for (uint32_t i = 0; i < colorTextureCount; i++) {
    if (main->samples == 1 && canvas->samples > 1) {
      lovrCheck(canvas->loads[i] != LOAD_KEEP, "When internal multisampling is used, render pass textures must be cleared");
      TextureFormat format = canvas->textures[i]->info.format;
      bool srgb = canvas->textures[i]->info.srgb;
      target.color[i].texture = getAttachment(target.size, main->depth, format, srgb, canvas->samples);
      target.color[i].resolve = canvas->textures[i]->renderView;
    } else {
      target.color[i].texture = canvas->textures[i]->renderView;
    }

    target.color[i].load = (gpu_load_op) canvas->loads[i];
    target.color[i].save = GPU_SAVE_OP_SAVE;
    target.color[i].clear[0] = lovrMathGammaToLinear(canvas->clears[i][0]);
    target.color[i].clear[1] = lovrMathGammaToLinear(canvas->clears[i][1]);
    target.color[i].clear[2] = lovrMathGammaToLinear(canvas->clears[i][2]);
    target.color[i].clear[3] = canvas->clears[i][2];

    if (info->canvas.mipmap && canvas->textures[i]->info.mipmaps > 1) {
      trackTexture(pass, canvas->textures[i], GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
    } else {
      gpu_cache cache = GPU_CACHE_COLOR_WRITE | (canvas->loads[i] == LOAD_KEEP ? GPU_CACHE_COLOR_READ : 0);
      trackTexture(pass, canvas->textures[i], GPU_PHASE_COLOR, cache);
    }
  }

  if (canvas->depth.texture) {
    target.depth.texture = canvas->depth.texture->renderView;
    if (info->canvas.mipmap && canvas->depth.texture->info.mipmaps > 1) {
      trackTexture(pass, canvas->depth.texture, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
    } else {
      gpu_phase phase = canvas->depth.load == LOAD_KEEP ? GPU_PHASE_DEPTH_EARLY : GPU_PHASE_DEPTH_LATE;
      gpu_cache cache = GPU_CACHE_DEPTH_WRITE | (canvas->depth.load == LOAD_KEEP ? GPU_CACHE_DEPTH_READ : 0);
      trackTexture(pass, canvas->depth.texture, phase, cache);
    }
  } else if (canvas->depth.format) {
    target.depth.texture = getAttachment(target.size, main->depth, canvas->depth.format, false, canvas->samples);
  }

  if (target.depth.texture) {
    target.depth.load = target.depth.stencilLoad = (gpu_load_op) canvas->depth.load;
    target.depth.save = canvas->depth.texture ? GPU_SAVE_OP_SAVE : GPU_SAVE_OP_DISCARD;
    target.depth.clear.depth = canvas->depth.clear;
  }

  // Begin render pass

  gpu_render_begin(pass->stream, &target);

  // The default Buffer (filled with zeros/ones) is always at slot #1, used for default vertex data
  gpu_buffer* buffers[] = { state.defaultBuffer->gpu, state.defaultBuffer->gpu };
  gpu_bind_vertex_buffers(pass->stream, buffers, NULL, 0, 2);

  // Reset state

  pass->transform = pass->transforms[0];
  mat4_identity(pass->transform);

  pass->pipelineIndex = 0;
  pass->pipeline = &pass->pipelines[0];
  pass->pipeline->info = (gpu_pipeline_info) {
    .colorCount = colorTextureCount,
    .depth.format = canvas->depth.texture ? canvas->depth.texture->info.format : canvas->depth.format,
    .multisample.count = canvas->samples,
    .viewCount = main->depth,
    .depth.test = GPU_COMPARE_LEQUAL,
    .depth.write = true
  };

  for (uint32_t i = 0; i < colorTextureCount; i++) {
    pass->pipeline->info.color[i].format = canvas->textures[i]->info.format;
    pass->pipeline->info.color[i].srgb = canvas->textures[i]->info.srgb;
    pass->pipeline->info.color[i].mask = 0xf;
  }

  float defaultColor[4] = { 1.f, 1.f, 1.f, 1.f };
  memcpy(pass->pipeline->color, defaultColor, sizeof(defaultColor));
  pass->pipeline->formatHash = 0;
  pass->pipeline->shader = NULL;
  pass->pipeline->drawMode = VERTEX_TRIANGLES;
  pass->pipeline->dirty = true;
  pass->pipeline->material = state.defaultMaterial;
  lovrRetain(pass->pipeline->material);
  pass->materialDirty = true;

  memset(pass->constants, 0, sizeof(pass->constants));
  pass->constantsDirty = true;

  pass->bindingMask = 0;
  pass->bindingsDirty = true;

  pass->cameraCount = main->depth;
  pass->cameras = tempAlloc(pass->cameraCount * sizeof(Camera));
  for (uint32_t i = 0; i < pass->cameraCount; i++) {
    mat4_identity(pass->cameras[i].view);
    mat4_perspective(pass->cameras[i].projection, .01f, 100.f, 1.0f, (float) main->width / main->height);
  }
  pass->cameraDirty = true;

  pass->drawCount = 0;

  gpu_buffer_binding cameras = { tempAlloc(gpu_sizeof_buffer()), 0, pass->cameraCount * sizeof(Camera) };
  gpu_buffer_binding draws = { tempAlloc(gpu_sizeof_buffer()), 0, 256 * sizeof(DrawData) };

  pass->builtins[0] = (gpu_binding) { 0, GPU_SLOT_UNIFORM_BUFFER, .buffer = cameras };
  pass->builtins[1] = (gpu_binding) { 1, GPU_SLOT_UNIFORM_BUFFER, .buffer = draws };
  pass->builtins[2] = (gpu_binding) { 2, GPU_SLOT_SAMPLER, .sampler = NULL };
  pass->pipeline->sampler = state.defaultSamplers[FILTER_LINEAR];
  pass->samplerDirty = true;
  lovrRetain(pass->pipeline->sampler);

  pass->vertexBuffer = NULL;
  pass->indexBuffer = NULL;

  float viewport[6] = { 0.f, 0.f, (float) main->width, (float) main->height, 0.f, 1.f };
  lovrPassSetViewport(pass, viewport, viewport + 4);

  uint32_t scissor[4] = { 0, 0, main->width, main->height };
  lovrPassSetScissor(pass, scissor);

  return pass;
}

void lovrPassDestroy(void* ref) {
  //
}

const PassInfo* lovrPassGetInfo(Pass* pass) {
  return &pass->info;
}

void lovrPassGetViewMatrix(Pass* pass, uint32_t index, float* viewMatrix) {
  lovrCheck(index < pass->cameraCount, "Invalid camera index '%d'", index);
  mat4_init(viewMatrix, pass->cameras[index].view);
}

void lovrPassSetViewMatrix(Pass* pass, uint32_t index, float* viewMatrix) {
  lovrCheck(index < pass->cameraCount, "Invalid camera index '%d'", index);
  mat4_init(pass->cameras[index].view, viewMatrix);
  pass->cameraDirty = true;
}

void lovrPassGetProjection(Pass* pass, uint32_t index, float* projection) {
  lovrCheck(index < pass->cameraCount, "Invalid camera index '%d'", index);
  mat4_init(projection, pass->cameras[index].projection);
}

void lovrPassSetProjection(Pass* pass, uint32_t index, float* projection) {
  lovrCheck(index < pass->cameraCount, "Invalid camera index '%d'", index);
  mat4_init(pass->cameras[index].projection, projection);
  pass->cameraDirty = true;
}

void lovrPassPush(Pass* pass, StackType stack) {
  switch (stack) {
    case STACK_TRANSFORM:
      pass->transform = pass->transforms[++pass->transformIndex];
      lovrCheck(pass->transformIndex < COUNTOF(pass->transforms), "%s stack overflow (more pushes than pops?)", "Transform");
      mat4_init(pass->transforms[pass->transformIndex], pass->transforms[pass->transformIndex - 1]);
      break;
    case STACK_PIPELINE:
      pass->pipeline = &pass->pipelines[++pass->pipelineIndex];
      lovrCheck(pass->pipelineIndex < COUNTOF(pass->pipelines), "%s stack overflow (more pushes than pops?)", "Pipeline");
      memcpy(pass->pipeline, &pass->pipelines[pass->pipelineIndex - 1], sizeof(Pipeline));
      lovrRetain(pass->pipeline->sampler);
      lovrRetain(pass->pipeline->shader);
      lovrRetain(pass->pipeline->material);
      break;
    default: break;
  }
}

void lovrPassPop(Pass* pass, StackType stack) {
  switch (stack) {
    case STACK_TRANSFORM:
      pass->transform = pass->transforms[--pass->transformIndex];
      lovrCheck(pass->transformIndex < COUNTOF(pass->transforms), "%s stack underflow (more pops than pushes?)", "Transform");
      break;
    case STACK_PIPELINE:
      lovrRelease(pass->pipeline->sampler, lovrSamplerDestroy);
      lovrRelease(pass->pipeline->shader, lovrShaderDestroy);
      lovrRelease(pass->pipeline->material, lovrMaterialDestroy);
      pass->pipeline = &pass->pipelines[--pass->pipelineIndex];
      lovrCheck(pass->pipelineIndex < COUNTOF(pass->pipelines), "%s stack underflow (more pops than pushes?)", "Pipeline");
      gpu_set_viewport(pass->stream, pass->pipeline->viewport, pass->pipeline->depthRange);
      gpu_set_scissor(pass->stream, pass->pipeline->scissor);
      pass->pipeline->dirty = true;
      pass->samplerDirty = true;
      pass->materialDirty = true;
      break;
    default: break;
  }
}

void lovrPassOrigin(Pass* pass) {
  mat4_identity(pass->transform);
}

void lovrPassTranslate(Pass* pass, vec3 translation) {
  mat4_translate(pass->transform, translation[0], translation[1], translation[2]);
}

void lovrPassRotate(Pass* pass, quat rotation) {
  mat4_rotateQuat(pass->transform, rotation);
}

void lovrPassScale(Pass* pass, vec3 scale) {
  mat4_scale(pass->transform, scale[0], scale[1], scale[2]);
}

void lovrPassTransform(Pass* pass, mat4 transform) {
  mat4_mul(pass->transform, transform);
}

void lovrPassSetAlphaToCoverage(Pass* pass, bool enabled) {
  pass->pipeline->dirty |= enabled != pass->pipeline->info.multisample.alphaToCoverage;
  pass->pipeline->info.multisample.alphaToCoverage = enabled;
}

void lovrPassSetBlendMode(Pass* pass, BlendMode mode, BlendAlphaMode alphaMode) {
  if (mode == BLEND_NONE) {
    pass->pipeline->dirty |= pass->pipeline->info.color[0].blend.enabled;
    memset(&pass->pipeline->info.color[0].blend, 0, sizeof(gpu_blend_state));
    return;
  }

  gpu_blend_state* blend = &pass->pipeline->info.color[0].blend;

  switch (mode) {
    case BLEND_ALPHA:
      blend->color.src = GPU_BLEND_SRC_ALPHA;
      blend->color.dst = GPU_BLEND_ONE_MINUS_SRC_ALPHA;
      blend->color.op = GPU_BLEND_ADD;
      blend->alpha.src = GPU_BLEND_ONE;
      blend->alpha.dst = GPU_BLEND_ONE_MINUS_SRC_ALPHA;
      blend->alpha.op = GPU_BLEND_ADD;
      break;
    case BLEND_ADD:
      blend->color.src = GPU_BLEND_SRC_ALPHA;
      blend->color.dst = GPU_BLEND_ONE;
      blend->color.op = GPU_BLEND_ADD;
      blend->alpha.src = GPU_BLEND_ZERO;
      blend->alpha.dst = GPU_BLEND_ONE;
      blend->alpha.op = GPU_BLEND_ADD;
      break;
    case BLEND_SUBTRACT:
      blend->color.src = GPU_BLEND_SRC_ALPHA;
      blend->color.dst = GPU_BLEND_ONE;
      blend->color.op = GPU_BLEND_RSUB;
      blend->alpha.src = GPU_BLEND_ZERO;
      blend->alpha.dst = GPU_BLEND_ONE;
      blend->alpha.op = GPU_BLEND_RSUB;
      break;
    case BLEND_MULTIPLY:
      blend->color.src = GPU_BLEND_DST_COLOR;
      blend->color.dst = GPU_BLEND_ZERO;
      blend->color.op = GPU_BLEND_ADD;
      blend->alpha.src = GPU_BLEND_DST_COLOR;
      blend->alpha.dst = GPU_BLEND_ZERO;
      blend->alpha.op = GPU_BLEND_ADD;
      break;
    case BLEND_LIGHTEN:
      blend->color.src = GPU_BLEND_SRC_ALPHA;
      blend->color.dst = GPU_BLEND_ZERO;
      blend->color.op = GPU_BLEND_MAX;
      blend->alpha.src = GPU_BLEND_ONE;
      blend->alpha.dst = GPU_BLEND_ZERO;
      blend->alpha.op = GPU_BLEND_MAX;
      break;
    case BLEND_DARKEN:
      blend->color.src = GPU_BLEND_SRC_ALPHA;
      blend->color.dst = GPU_BLEND_ZERO;
      blend->color.op = GPU_BLEND_MIN;
      blend->alpha.src = GPU_BLEND_ONE;
      blend->alpha.dst = GPU_BLEND_ZERO;
      blend->alpha.op = GPU_BLEND_MIN;
      break;
    case BLEND_SCREEN:
      blend->color.src = GPU_BLEND_SRC_ALPHA;
      blend->color.dst = GPU_BLEND_ONE_MINUS_SRC_COLOR;
      blend->color.op = GPU_BLEND_ADD;
      blend->alpha.src = GPU_BLEND_ONE;
      blend->alpha.dst = GPU_BLEND_ONE_MINUS_SRC_COLOR;
      blend->alpha.op = GPU_BLEND_ADD;
      break;
    default: lovrUnreachable();
  };

  if (alphaMode == BLEND_PREMULTIPLIED && mode != BLEND_MULTIPLY) {
    blend->color.src = GPU_BLEND_ONE;
  }

  blend->enabled = true;
  pass->pipeline->dirty = true;
}

void lovrPassSetColor(Pass* pass, float color[4]) {
  pass->pipeline->color[0] = lovrMathGammaToLinear(color[0]);
  pass->pipeline->color[1] = lovrMathGammaToLinear(color[1]);
  pass->pipeline->color[2] = lovrMathGammaToLinear(color[2]);
  pass->pipeline->color[3] = color[3];
}

void lovrPassSetColorWrite(Pass* pass, bool r, bool g, bool b, bool a) {
  uint8_t mask = (r << 0) | (g << 1) | (b << 2) | (a << 3);
  pass->pipeline->dirty |= pass->pipeline->info.color[0].mask != mask;
  pass->pipeline->info.color[0].mask = mask;
}

void lovrPassSetCullMode(Pass* pass, CullMode mode) {
  pass->pipeline->dirty |= pass->pipeline->info.rasterizer.cullMode != (gpu_cull_mode) mode;
  pass->pipeline->info.rasterizer.cullMode = (gpu_cull_mode) mode;
}

void lovrPassSetDepthTest(Pass* pass, CompareMode test) {
  pass->pipeline->dirty |= pass->pipeline->info.depth.test != (gpu_compare_mode) test;
  pass->pipeline->info.depth.test = (gpu_compare_mode) test;
}

void lovrPassSetDepthWrite(Pass* pass, bool write) {
  pass->pipeline->dirty |= pass->pipeline->info.depth.write != write;
  pass->pipeline->info.depth.write = write;
}

void lovrPassSetDepthOffset(Pass* pass, float offset, float sloped) {
  pass->pipeline->info.rasterizer.depthOffset = offset;
  pass->pipeline->info.rasterizer.depthOffsetSloped = sloped;
  pass->pipeline->dirty = true;
}

void lovrPassSetDepthClamp(Pass* pass, bool clamp) {
  if (state.features.depthClamp) {
    pass->pipeline->dirty |= pass->pipeline->info.rasterizer.depthClamp != clamp;
    pass->pipeline->info.rasterizer.depthClamp = clamp;
  }
}

void lovrPassSetMaterial(Pass* pass, Material* material, Texture* texture) {
  if (texture) {
    material = lovrTextureGetMaterial(texture);
  }

  material = material ? material : state.defaultMaterial;

  if (pass->pipeline->material != material) {
    lovrRetain(material);
    lovrRelease(pass->pipeline->material, lovrMaterialDestroy);
    pass->pipeline->material = material;
    pass->materialDirty = true;
  }
}

void lovrPassSetSampler(Pass* pass, Sampler* sampler) {
  if (sampler != pass->pipeline->sampler) {
    lovrRetain(sampler);
    lovrRelease(pass->pipeline->sampler, lovrSamplerDestroy);
    pass->pipeline->sampler = sampler;
    pass->samplerDirty = true;
  }
}

void lovrPassSetScissor(Pass* pass, uint32_t scissor[4]) {
  gpu_set_scissor(pass->stream, scissor);
  memcpy(pass->pipeline->scissor, scissor, 4 * sizeof(uint32_t));
}

void lovrPassSetShader(Pass* pass, Shader* shader) {
  Shader* previous = pass->pipeline->shader;
  if (shader == previous) return;

  // Clear any bindings for resources that share the same slot but have different types
  if (shader) {
    if (previous) {
      for (uint32_t i = 0, j = 0; i < previous->resourceCount && j < shader->resourceCount;) {
        if (previous->resources[i].binding < shader->resources[j].binding) {
          i++;
        } else if (previous->resources[i].binding > shader->resources[j].binding) {
          j++;
        } else {
          if (previous->resources[i].type != shader->resources[j].type) {
            pass->bindingMask &= ~(1u << shader->resources[j].binding);
          }
          i++;
          j++;
        }
      }
    }

    uint32_t shaderSlots = (shader->bufferMask | shader->textureMask | shader->samplerMask);
    uint32_t missingResources = shaderSlots & ~pass->bindingMask;

    // Assign default bindings to any slots used by the shader that are missing resources
    if (missingResources) {
      for (uint32_t i = 0; i < 32; i++) { // TODO biterationtrinsics
        uint32_t bit = (1u << i);

        if (~missingResources & bit) {
          continue;
        }

        pass->bindings[i].number = i;
        pass->bindings[i].type = shader->resources[i].type;

        if (shader->bufferMask & bit) {
          pass->bindings[i].buffer.object = state.defaultBuffer->gpu;
          pass->bindings[i].buffer.offset = 0;
          pass->bindings[i].buffer.extent = state.defaultBuffer->size;
        } else if (shader->textureMask & bit) {
          pass->bindings[i].texture = state.defaultTexture->gpu;
        } else if (shader->samplerMask & bit) {
          pass->bindings[i].sampler = state.defaultSamplers[FILTER_LINEAR]->gpu;
        }

        pass->bindingMask |= bit;
      }

      pass->bindingsDirty = true;
    }

    pass->pipeline->info.shader = shader->gpu;
    pass->pipeline->info.flags = shader->flags;
    pass->pipeline->info.flagCount = shader->overrideCount;
  }

  lovrRetain(shader);
  lovrRelease(previous, lovrShaderDestroy);
  pass->pipeline->shader = shader;
  pass->pipeline->dirty = true;
}

void lovrPassSetStencilTest(Pass* pass, CompareMode test, uint8_t value, uint8_t mask) {
  bool hasReplace = false;
  hasReplace |= pass->pipeline->info.stencil.failOp == GPU_STENCIL_REPLACE;
  hasReplace |= pass->pipeline->info.stencil.depthFailOp == GPU_STENCIL_REPLACE;
  hasReplace |= pass->pipeline->info.stencil.passOp == GPU_STENCIL_REPLACE;
  if (hasReplace && test != COMPARE_NONE) {
    lovrCheck(value == pass->pipeline->info.stencil.value, "When stencil write is 'replace' and stencil test is active, their values must match");
  }
  switch (test) { // (Reversed compare mode)
    case COMPARE_NONE: default: pass->pipeline->info.stencil.test = GPU_COMPARE_NONE; break;
    case COMPARE_EQUAL: pass->pipeline->info.stencil.test = GPU_COMPARE_EQUAL; break;
    case COMPARE_NEQUAL: pass->pipeline->info.stencil.test = GPU_COMPARE_NEQUAL; break;
    case COMPARE_LESS: pass->pipeline->info.stencil.test = GPU_COMPARE_GREATER; break;
    case COMPARE_LEQUAL: pass->pipeline->info.stencil.test = GPU_COMPARE_GEQUAL; break;
    case COMPARE_GREATER: pass->pipeline->info.stencil.test = GPU_COMPARE_LESS; break;
    case COMPARE_GEQUAL: pass->pipeline->info.stencil.test = GPU_COMPARE_LEQUAL; break;
  }
  pass->pipeline->info.stencil.testMask = mask;
  if (test != COMPARE_NONE) pass->pipeline->info.stencil.value = value;
  pass->pipeline->dirty = true;
}

void lovrPassSetStencilWrite(Pass* pass, StencilAction actions[3], uint8_t value, uint8_t mask) {
  bool hasReplace = actions[0] == STENCIL_REPLACE || actions[1] == STENCIL_REPLACE || actions[2] == STENCIL_REPLACE;
  if (hasReplace && pass->pipeline->info.stencil.test != GPU_COMPARE_NONE) {
    lovrCheck(value == pass->pipeline->info.stencil.value, "When stencil write is 'replace' and stencil test is active, their values must match");
  }
  pass->pipeline->info.stencil.failOp = (gpu_stencil_op) actions[0];
  pass->pipeline->info.stencil.depthFailOp = (gpu_stencil_op) actions[1];
  pass->pipeline->info.stencil.passOp = (gpu_stencil_op) actions[2];
  pass->pipeline->info.stencil.writeMask = mask;
  if (hasReplace) pass->pipeline->info.stencil.value = value;
  pass->pipeline->dirty = true;
}

void lovrPassSetVertexMode(Pass* pass, VertexMode mode) {
  pass->pipeline->drawMode = mode;
}

void lovrPassSetViewport(Pass* pass, float viewport[4], float depthRange[2]) {
  gpu_set_viewport(pass->stream, viewport, depthRange);
  memcpy(pass->pipeline->viewport, viewport, 4 * sizeof(float));
  memcpy(pass->pipeline->depthRange, depthRange, 2 * sizeof(float));
}

void lovrPassSetWinding(Pass* pass, Winding winding) {
  pass->pipeline->dirty |= pass->pipeline->info.rasterizer.winding != (gpu_winding) winding;
  pass->pipeline->info.rasterizer.winding = (gpu_winding) winding;
}

void lovrPassSetWireframe(Pass* pass, bool wireframe) {
  if (state.features.wireframe) {
    pass->pipeline->dirty |= pass->pipeline->info.rasterizer.wireframe != (gpu_winding) wireframe;
    pass->pipeline->info.rasterizer.wireframe = wireframe;
  }
}

void lovrPassSendBuffer(Pass* pass, const char* name, size_t length, uint32_t slot, Buffer* buffer, uint32_t offset, uint32_t extent) {
  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader, "A Shader must be active to send resources");
  ShaderResource* resource = findShaderResource(shader, name, length, slot);
  slot = resource->binding;

  lovrCheck(shader->bufferMask & (1u << slot), "Trying to send a Buffer to slot %d, but the active Shader doesn't have a Buffer in that slot");
  lovrCheck(offset < buffer->size, "Buffer offset is past the end of the Buffer");

  uint32_t limit;

  if (shader->storageMask & (1u << slot)) {
    lovrCheck(!lovrBufferIsTemporary(buffer), "Temporary buffers can not be sent to storage buffer variables", slot + 1);
    lovrCheck((offset & (state.limits.storageBufferAlign - 1)) == 0, "Storage buffer offset (%d) is not aligned to storageBufferAlign limit (%d)", offset, state.limits.storageBufferAlign);
    limit = state.limits.storageBufferRange;
  } else {
    lovrCheck((offset & (state.limits.uniformBufferAlign - 1)) == 0, "Uniform buffer offset (%d) is not aligned to uniformBufferAlign limit (%d)", offset, state.limits.uniformBufferAlign);
    limit = state.limits.uniformBufferRange;
  }

  if (extent == 0) {
    extent = MIN(buffer->size - offset, limit);
  } else {
    lovrCheck(offset + extent <= buffer->size, "Buffer range goes past the end of the Buffer");
    lovrCheck(extent <= limit, "Buffer range exceeds storageBufferRange/uniformBufferRange limit");
  }

  pass->bindings[slot].buffer.object = buffer->gpu;
  pass->bindings[slot].buffer.offset = offset;
  pass->bindings[slot].buffer.extent = extent;
  pass->bindingMask |= (1u << slot);
  pass->bindingsDirty = true;

  gpu_phase phase = 0;
  gpu_cache cache = 0;

  if (pass->info.type == PASS_RENDER) {
    if (resource->stageMask & GPU_STAGE_VERTEX) phase |= GPU_PHASE_SHADER_VERTEX;
    if (resource->stageMask & GPU_STAGE_FRAGMENT) phase |= GPU_PHASE_SHADER_FRAGMENT;
    cache = (shader->storageMask & (1u << slot)) ? GPU_CACHE_STORAGE_READ : GPU_CACHE_UNIFORM;
  } else {
    phase = GPU_PHASE_SHADER_COMPUTE;
    cache = (shader->storageMask & (1u << slot)) ? GPU_CACHE_STORAGE_WRITE : GPU_CACHE_UNIFORM; // TODO readonly
  }

  trackBuffer(pass, buffer, phase, cache);
}

void lovrPassSendTexture(Pass* pass, const char* name, size_t length, uint32_t slot, Texture* texture) {
  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader, "A Shader must be active to send resources");
  ShaderResource* resource = findShaderResource(shader, name, length, slot);
  slot = resource->binding;

  lovrCheck(shader->textureMask & (1u << slot), "Trying to send a Texture to slot %d, but the active Shader doesn't have a Texture in that slot");

  if (shader->storageMask & (1u << slot)) {
    lovrCheck(texture->info.usage & TEXTURE_STORAGE, "Textures must be created with the 'storage' usage to send them to image variables in shaders");
  } else {
    lovrCheck(texture->info.usage & TEXTURE_SAMPLE, "Textures must be created with the 'sample' usage to send them to sampler variables in shaders");
  }

  pass->bindings[slot].texture = texture->gpu;
  pass->bindingMask |= (1u << slot);
  pass->bindingsDirty = true;

  gpu_phase phase = 0;
  gpu_cache cache = 0;

  if (pass->info.type == PASS_RENDER) {
    if (resource->stageMask & GPU_STAGE_VERTEX) phase |= GPU_PHASE_SHADER_VERTEX;
    if (resource->stageMask & GPU_STAGE_FRAGMENT) phase |= GPU_PHASE_SHADER_FRAGMENT;
    cache = (shader->storageMask & (1u << slot)) ? GPU_CACHE_STORAGE_READ : GPU_CACHE_TEXTURE;
  } else {
    phase = GPU_PHASE_SHADER_COMPUTE;
    cache = (shader->storageMask & (1u << slot)) ? GPU_CACHE_STORAGE_WRITE : GPU_CACHE_TEXTURE; // TODO readonly
  }

  trackTexture(pass, texture, phase, cache);
}

void lovrPassSendSampler(Pass* pass, const char* name, size_t length, uint32_t slot, Sampler* sampler) {
  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader, "A Shader must be active to send resources");
  ShaderResource* resource = findShaderResource(shader, name, length, slot);
  slot = resource->binding;

  lovrCheck(shader->samplerMask & (1u << slot), "Trying to send a Sampler to slot %d, but the active Shader doesn't have a Sampler in that slot");

  pass->bindings[slot].sampler = sampler->gpu;
  pass->bindingMask |= (1u << slot);
  pass->bindingsDirty = true;
}

void lovrPassSendValue(Pass* pass, const char* name, size_t length, void** data, FieldType* type) {
  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader, "A Shader must be active to send resources");

  uint32_t hash = (uint32_t) hash64(name, length);
  for (uint32_t i = 0; i < shader->constantCount; i++) {
    if (shader->constants[i].hash == hash) {
      *data = (char*) pass->constants + shader->constants[i].offset;
      *type = shader->constants[i].type;
      pass->constantsDirty = true;
      return;
    }
  }

  lovrThrow("Shader has no push constant named '%s'", name);
}

static void flushPipeline(Pass* pass, Draw* draw, Shader* shader) {
  Pipeline* pipeline = pass->pipeline;

  if (pipeline->info.drawMode != (gpu_draw_mode) draw->mode) {
    pipeline->info.drawMode = (gpu_draw_mode) draw->mode;
    pipeline->dirty = true;
  }

  if (!pipeline->shader && pipeline->info.shader != shader->gpu) {
    pipeline->info.shader = shader->gpu;
    pipeline->info.flags = NULL;
    pipeline->info.flagCount = 0;
    pipeline->dirty = true;
  }

  // Builtin vertex format
  if (!draw->vertex.buffer && pipeline->formatHash != 1 + draw->vertex.format) {
    pipeline->formatHash = 1 + draw->vertex.format;
    pipeline->info.vertex = state.vertexFormats[draw->vertex.format];
    pipeline->dirty = true;

    if (shader->hasCustomAttributes) {
      for (uint32_t i = 0; i < shader->attributeCount; i++) {
        if (shader->attributes[i].location < 10) {
          pipeline->info.vertex.attributes[pipeline->info.vertex.attributeCount++] = (gpu_attribute) {
            .buffer = 1,
            .location = shader->attributes[i].location,
            .type = GPU_TYPE_F32x4,
            .offset = shader->attributes[i].location == LOCATION_COLOR ? 16 : 0
          };
        }
      }
    }
  }

  // Custom vertex format
  if (draw->vertex.buffer && pipeline->formatHash != draw->vertex.buffer->hash) {
    pipeline->formatHash = draw->vertex.buffer->hash;
    pipeline->info.vertex.bufferCount = 2;
    pipeline->info.vertex.attributeCount = shader->attributeCount;
    pipeline->info.vertex.bufferStrides[0] = draw->vertex.buffer->info.stride;
    pipeline->info.vertex.bufferStrides[1] = 0;
    pipeline->dirty = true;

    for (uint32_t i = 0; i < shader->attributeCount; i++) {
      ShaderAttribute* attribute = &shader->attributes[i];

      bool found = false;
      for (uint32_t j = 0; j < draw->vertex.buffer->info.fieldCount; j++) {
        BufferField field = draw->vertex.buffer->info.fields[j];
        if (field.hash ? (field.hash == attribute->hash) : (field.location == attribute->location)) {
          pipeline->info.vertex.attributes[i] = (gpu_attribute) {
            .buffer = 0,
            .location = attribute->location,
            .offset = field.offset,
            .type = field.type
          };
          found = true;
          break;
        }
      }

      if (!found) {
        pipeline->info.vertex.attributes[i] = (gpu_attribute) {
          .buffer = 1,
          .location = attribute->location,
          .offset = attribute->location == LOCATION_COLOR ? 16 : 0,
          .type = GPU_TYPE_F32x4
        };
      }
    }
  }

  if (!pipeline->dirty) {
    return;
  }

  uint64_t hash = hash64(&pipeline->info, sizeof(pipeline->info));
  uint64_t index = map_get(&state.pipelineLookup, hash);

  if (index == MAP_NIL) {
    gpu_pipeline* gpu = malloc(gpu_sizeof_pipeline());
    lovrAssert(gpu, "Out of memory");
    gpu_pipeline_init_graphics(gpu, &pipeline->info);
    index = state.pipelines.length;
    arr_push(&state.pipelines, gpu);
    map_set(&state.pipelineLookup, hash, index);
  }

  gpu_bind_pipeline(pass->stream, state.pipelines.data[index], false);
  state.stats.pipelineSwitches++;
  pipeline->dirty = false;
}

static void flushConstants(Pass* pass, Shader* shader) {
  if (pass->constantsDirty && shader->constantSize > 0) {
    gpu_push_constants(pass->stream, shader->gpu, pass->constants, shader->constantSize);
    pass->constantsDirty = false;
  }
}

static void flushBindings(Pass* pass, Shader* shader) {
  if (!pass->bindingsDirty || shader->resourceCount == 0) {
    return;
  }

  uint32_t set = pass->info.type == PASS_RENDER ? 2 : 0;
  gpu_binding* bindings = tempAlloc(shader->resourceCount * sizeof(gpu_binding));

  for (uint32_t i = 0; i < shader->resourceCount; i++) {
    bindings[i] = pass->bindings[shader->resources[i].binding];
  }

  gpu_bundle_info info = {
    .layout = state.layouts.data[shader->layout].gpu,
    .bindings = bindings,
    .count = shader->resourceCount
  };

  gpu_bundle* bundle = getBundle(shader->layout);
  gpu_bundle_write(&bundle, &info, 1);
  gpu_bind_bundle(pass->stream, shader->gpu, set, bundle, NULL, 0);
  state.stats.bundleSwitches++;
}

static void flushBuiltins(Pass* pass, Draw* draw, Shader* shader) {
  bool rebind = false;

  if (pass->cameraDirty) {
    for (uint32_t i = 0; i < pass->cameraCount; i++) {
      mat4_init(pass->cameras[i].viewProjection, pass->cameras[i].projection);
      mat4_init(pass->cameras[i].inverseProjection, pass->cameras[i].projection);
      mat4_mul(pass->cameras[i].viewProjection, pass->cameras[i].view);
      mat4_invert(pass->cameras[i].inverseProjection);
    }

    uint32_t size = pass->cameraCount * sizeof(Camera);
    void* data = gpu_map(pass->builtins[0].buffer.object, size, state.limits.uniformBufferAlign, GPU_MAP_WRITE);
    memcpy(data, pass->cameras, size);
    pass->cameraDirty = false;
    rebind = true;
  }

  if (pass->drawCount % 256 == 0) {
    uint32_t size = 256 * sizeof(DrawData);
    pass->drawData = gpu_map(pass->builtins[1].buffer.object, size, state.limits.uniformBufferAlign, GPU_MAP_WRITE);
    rebind = true;
  }

  if (pass->samplerDirty) {
    pass->builtins[2].sampler = pass->pipeline->sampler->gpu;
    pass->samplerDirty = false;
    rebind = true;
  }

  if (rebind) {
    gpu_bundle_info bundleInfo = {
      .layout = state.layouts.data[state.builtinLayout].gpu,
      .bindings = pass->builtins,
      .count = COUNTOF(pass->builtins)
    };

    gpu_bundle* bundle = getBundle(state.builtinLayout);
    gpu_bundle_write(&bundle, &bundleInfo, 1);
    gpu_bind_bundle(pass->stream, shader->gpu, 0, bundle, NULL, 0);
    state.stats.bundleSwitches++;
  }

  float m[16];
  float* transform;
  if (draw->transform) {
    transform = mat4_mul(mat4_init(m, pass->transform), draw->transform);
  } else {
    transform = pass->transform;
  }

  float cofactor[16];
  mat4_init(cofactor, transform);
  mat4_cofactor(cofactor);

  memcpy(pass->drawData->transform, transform, 64);
  memcpy(pass->drawData->cofactor, cofactor, 64);
  memcpy(pass->drawData->color, pass->pipeline->color, 16);
  pass->drawData++;
}

static void flushMaterial(Pass* pass, Draw* draw, Shader* shader) {
  if (draw->material && draw->material != pass->pipeline->material) {
    gpu_bind_bundle(pass->stream, shader->gpu, 1, draw->material->bundle, NULL, 0);
    state.stats.bundleSwitches++;
    pass->materialDirty = true;
  } else if (pass->materialDirty) {
    gpu_bind_bundle(pass->stream, shader->gpu, 1, pass->pipeline->material->bundle, NULL, 0);
    state.stats.bundleSwitches++;
    pass->materialDirty = false;
  }
}

static void flushBuffers(Pass* pass, Draw* draw) {
  if (!draw->vertex.buffer && draw->vertex.count > 0) {
    lovrCheck(draw->vertex.count < UINT16_MAX, "This draw has too many vertices (max is 65534), try splitting it up into multiple draws or using a Buffer");
    uint32_t stride = state.vertexFormats[draw->vertex.format].bufferStrides[0];
    uint32_t size = draw->vertex.count * stride;

    gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
    void* pointer = gpu_map(scratchpad, size, stride, GPU_MAP_WRITE);

    if (draw->vertex.pointer) {
      *draw->vertex.pointer = pointer;
    } else {
      memcpy(pointer, draw->vertex.data, size);
    }

    gpu_bind_vertex_buffers(pass->stream, &scratchpad, NULL, 0, 1);
    pass->vertexBuffer = NULL;
  } else if (draw->vertex.buffer && draw->vertex.buffer->gpu != pass->vertexBuffer) {
    lovrCheck(draw->vertex.buffer->info.stride <= state.limits.vertexBufferStride, "Vertex buffer stride exceeds vertexBufferStride limit");
    gpu_bind_vertex_buffers(pass->stream, &draw->vertex.buffer->gpu, NULL, 0, 1);
    pass->vertexBuffer = draw->vertex.buffer->gpu;
    trackBuffer(pass, draw->vertex.buffer, GPU_PHASE_INPUT_VERTEX, GPU_CACHE_VERTEX);
  }

  if (!draw->index.buffer && draw->index.count > 0) {
    uint32_t stride = draw->index.stride ? draw->index.stride : sizeof(uint16_t);
    uint32_t size = draw->index.count * stride;

    gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
    void* pointer = gpu_map(scratchpad, size, stride, GPU_MAP_WRITE);

    if (draw->index.pointer) {
      *draw->index.pointer = pointer;
    } else {
      memcpy(pointer, draw->index.data, size);
    }

    gpu_index_type type = stride == 4 ? GPU_INDEX_U32 : GPU_INDEX_U16;
    gpu_bind_index_buffer(pass->stream, scratchpad, 0, type);
    pass->indexBuffer = NULL;
  } else if (draw->index.buffer && draw->index.buffer->gpu != pass->indexBuffer) {
    gpu_index_type type = draw->index.buffer->info.stride == 4 ? GPU_INDEX_U32 : GPU_INDEX_U16;
    gpu_bind_index_buffer(pass->stream, draw->index.buffer->gpu, 0, type);
    pass->indexBuffer = draw->index.buffer->gpu;
    trackBuffer(pass, draw->index.buffer, GPU_PHASE_INPUT_INDEX, GPU_CACHE_INDEX);
  }
}

static void lovrPassDraw(Pass* pass, Draw* draw) {
  lovrCheck(pass->info.type == PASS_RENDER, "This function can only be called on a render pass");
  Shader* shader = pass->pipeline->shader ? pass->pipeline->shader : lovrGraphicsGetDefaultShader(draw->shader);

  flushPipeline(pass, draw, shader);
  flushConstants(pass, shader);
  flushBindings(pass, shader);
  flushBuiltins(pass, draw, shader);
  flushMaterial(pass, draw, shader);
  flushBuffers(pass, draw);

  uint32_t defaultCount = draw->index.count > 0 ? draw->index.count : draw->vertex.count;
  uint32_t count = draw->count > 0 ? draw->count : defaultCount;
  uint32_t instances = MAX(draw->instances, 1);
  uint32_t id = pass->drawCount & 0xff;

  if (draw->index.buffer || draw->index.count > 0) {
    gpu_draw_indexed(pass->stream, count, instances, draw->start, draw->base, id);
  } else {
    gpu_draw(pass->stream, count, instances, draw->start, id);
  }

  pass->drawCount++;
}

void lovrPassPoints(Pass* pass, uint32_t count, float** points) {
  lovrPassDraw(pass, &(Draw) {
    .mode = VERTEX_POINTS,
    .vertex.format = VERTEX_POINT,
    .vertex.pointer = (void**) points,
    .vertex.count = count
  });
}

void lovrPassLine(Pass* pass, uint32_t count, float** points) {
  uint16_t* indices;

  lovrPassDraw(pass, &(Draw) {
    .mode = VERTEX_LINES,
    .vertex.format = VERTEX_POINT,
    .vertex.pointer = (void**) points,
    .vertex.count = count,
    .index.pointer = (void**) &indices,
    .index.count = 2 * (count - 1)
  });

  for (uint32_t i = 0; i < count - 1; i++) {
    indices[2 * i + 0] = i;
    indices[2 * i + 1] = i + 1;
  }
}

void lovrPassPlane(Pass* pass, float* transform, DrawStyle style, uint32_t cols, uint32_t rows) {
  ShapeVertex* vertices;
  uint16_t* indices;

  uint32_t vertexCount = (cols + 1) * (rows + 1);
  uint32_t indexCount;

  if (style == STYLE_LINE) {
    indexCount = 2 * (rows + 1) + 2 * (cols + 1);

    lovrPassDraw(pass, &(Draw) {
      .mode = VERTEX_LINES,
      .transform = transform,
      .vertex.pointer = (void**) &vertices,
      .vertex.count = vertexCount,
      .index.pointer = (void**) &indices,
      .index.count = indexCount
    });
  } else {
    indexCount = (cols * rows) * 6;

    lovrPassDraw(pass, &(Draw) {
      .mode = VERTEX_TRIANGLES,
      .transform = transform,
      .vertex.pointer = (void**) &vertices,
      .vertex.count = vertexCount,
      .index.pointer = (void**) &indices,
      .index.count = indexCount
    });
  }

  for (uint32_t y = 0; y <= rows; y++) {
    float v = y * (1.f / rows);
    for (uint32_t x = 0; x <= cols; x++) {
      float u = x * (1.f / cols);
      *vertices++ = (ShapeVertex) {
        .position = { u - .5f, .5f - v, 0.f },
        .normal = { 0.f, 0.f, 1.f },
        .uv = { u, v }
      };
    }
  }

  if (style == STYLE_LINE) {
    for (uint32_t y = 0; y <= rows; y++) {
      uint16_t a = y * (cols + 1);
      uint16_t b = a + cols;
      uint16_t line[] = { a, b };
      memcpy(indices, line, sizeof(line));
      indices += COUNTOF(line);
    }

    for (uint32_t x = 0; x <= cols; x++) {
      uint16_t a = x;
      uint16_t b = x + ((cols + 1) * rows);
      uint16_t line[] = { a, b };
      memcpy(indices, line, sizeof(line));
      indices += COUNTOF(line);
    }
  } else {
    for (uint32_t y = 0; y < rows; y++) {
      for (uint32_t x = 0; x < cols; x++) {
        uint16_t a = (y * (cols + 1)) + x;
        uint16_t b = a + 1;
        uint16_t c = a + cols + 1;
        uint16_t d = a + cols + 2;
        uint16_t cell[] = { a, c, b, b, c, d };
        memcpy(indices, cell, sizeof(cell));
        indices += COUNTOF(cell);
      }
    }
  }
}

void lovrPassBox(Pass* pass, float* transform, DrawStyle style) {
  if (style == STYLE_LINE) {
    static ShapeVertex vertices[] = {
      { { -.5f,  .5f, -.5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } }, // Front
      { {  .5f,  .5f, -.5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { {  .5f, -.5f, -.5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { { -.5f, -.5f, -.5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { { -.5f,  .5f,  .5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } }, // Back
      { {  .5f,  .5f,  .5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { {  .5f, -.5f,  .5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { { -.5f, -.5f,  .5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } }
    };

    static uint16_t indices[] = {
      0, 1, 1, 2, 2, 3, 3, 0, // Front
      4, 5, 5, 6, 6, 7, 7, 4, // Back
      0, 4, 1, 5, 2, 6, 3, 7  // Connections
    };

    lovrPassDraw(pass, &(Draw) {
      .mode = VERTEX_LINES,
      .transform = transform,
      .vertex.data = vertices,
      .vertex.count = COUNTOF(vertices),
      .index.data = indices,
      .index.count = COUNTOF(indices)
    });
  } else {
    ShapeVertex vertices[] = {
      { { -.5f, -.5f, -.5f }, {  0.f,  0.f, -1.f }, { 0.f, 0.f } }, // Front
      { { -.5f,  .5f, -.5f }, {  0.f,  0.f, -1.f }, { 0.f, 1.f } },
      { {  .5f, -.5f, -.5f }, {  0.f,  0.f, -1.f }, { 1.f, 0.f } },
      { {  .5f,  .5f, -.5f }, {  0.f,  0.f, -1.f }, { 1.f, 1.f } },
      { {  .5f,  .5f, -.5f }, {  1.f,  0.f,  0.f }, { 0.f, 1.f } }, // Right
      { {  .5f,  .5f,  .5f }, {  1.f,  0.f,  0.f }, { 1.f, 1.f } },
      { {  .5f, -.5f, -.5f }, {  1.f,  0.f,  0.f }, { 0.f, 0.f } },
      { {  .5f, -.5f,  .5f }, {  1.f,  0.f,  0.f }, { 1.f, 0.f } },
      { {  .5f, -.5f,  .5f }, {  0.f,  0.f,  1.f }, { 0.f, 0.f } }, // Back
      { {  .5f,  .5f,  .5f }, {  0.f,  0.f,  1.f }, { 0.f, 1.f } },
      { { -.5f, -.5f,  .5f }, {  0.f,  0.f,  1.f }, { 1.f, 0.f } },
      { { -.5f,  .5f,  .5f }, {  0.f,  0.f,  1.f }, { 1.f, 1.f } },
      { { -.5f,  .5f,  .5f }, { -1.f,  0.f,  0.f }, { 0.f, 1.f } }, // Left
      { { -.5f,  .5f, -.5f }, { -1.f,  0.f,  0.f }, { 1.f, 1.f } },
      { { -.5f, -.5f,  .5f }, { -1.f,  0.f,  0.f }, { 0.f, 0.f } },
      { { -.5f, -.5f, -.5f }, { -1.f,  0.f,  0.f }, { 1.f, 0.f } },
      { { -.5f, -.5f, -.5f }, {  0.f, -1.f,  0.f }, { 0.f, 0.f } }, // Bottom
      { {  .5f, -.5f, -.5f }, {  0.f, -1.f,  0.f }, { 1.f, 0.f } },
      { { -.5f, -.5f,  .5f }, {  0.f, -1.f,  0.f }, { 0.f, 1.f } },
      { {  .5f, -.5f,  .5f }, {  0.f, -1.f,  0.f }, { 1.f, 1.f } },
      { { -.5f,  .5f, -.5f }, {  0.f,  1.f,  0.f }, { 0.f, 1.f } }, // Top
      { { -.5f,  .5f,  .5f }, {  0.f,  1.f,  0.f }, { 0.f, 0.f } },
      { {  .5f,  .5f, -.5f }, {  0.f,  1.f,  0.f }, { 1.f, 1.f } },
      { {  .5f,  .5f,  .5f }, {  0.f,  1.f,  0.f }, { 1.f, 0.f } }
    };

    uint16_t indices[] = {
      0,  1,   2,  2,  1,  3,
      4,  5,   6,  6,  5,  7,
      8,  9,  10, 10,  9, 11,
      12, 13, 14, 14, 13, 15,
      16, 17, 18, 18, 17, 19,
      20, 21, 22, 22, 21, 23
    };

    lovrPassDraw(pass, &(Draw) {
      .mode = VERTEX_TRIANGLES,
      .transform = transform,
      .vertex.data = vertices,
      .vertex.count = COUNTOF(vertices),
      .index.data = indices,
      .index.count = COUNTOF(indices)
    });
  }
}

void lovrPassCircle(Pass* pass, float* transform, DrawStyle style, float angle1, float angle2, uint32_t segments) {
  if (fabsf(angle1 - angle2) >= 2.f * (float) M_PI) {
    angle1 = 0.f;
    angle2 = 2.f * (float) M_PI;
  }

  ShapeVertex* vertices;
  uint16_t* indices;

  if (style == STYLE_LINE) {
    uint32_t vertexCount = segments + 1;
    uint32_t indexCount = segments * 2;

    lovrPassDraw(pass, &(Draw) {
      .mode = VERTEX_LINES,
      .transform = transform,
      .vertex.pointer = (void**) &vertices,
      .vertex.count = vertexCount,
      .index.pointer = (void**) &indices,
      .index.count = indexCount
    });
  } else {
    uint32_t vertexCount = segments + 2;
    uint32_t indexCount = segments * 3;

    lovrPassDraw(pass, &(Draw) {
      .mode = VERTEX_TRIANGLES,
      .transform = transform,
      .vertex.pointer = (void**) &vertices,
      .vertex.count = vertexCount,
      .index.pointer = (void**) &indices,
      .index.count = indexCount
    });

    // Center
    *vertices++ = (ShapeVertex) { { 0.f, 0.f, 0.f }, { 0.f, 0.f, 1.f }, { .5f, .5f } };
  }

  float angleShift = (angle2 - angle1) / segments;
  for (uint32_t i = 0; i <= segments; i++) {
    float theta = angle1 + i * angleShift;
    float x = cosf(theta);
    float y = sinf(theta);
    *vertices++ = (ShapeVertex) { { x, y, 0.f }, { 0.f, 0.f, 1.f }, { x + .5f, .5f - y } };
  }

  if (style == STYLE_LINE) {
    for (uint32_t i = 0; i < segments; i++) {
      uint16_t segment[] = { i, i + 1 };
      memcpy(indices, segment, sizeof(segment));
      indices += COUNTOF(segment);
    }
  } else {
    for (uint32_t i = 0; i < segments; i++) {
      uint16_t wedge[] = { 0, i + 1, i + 2 };
      memcpy(indices, wedge, sizeof(wedge));
      indices += COUNTOF(wedge);
    }
  }
}

void lovrPassSphere(Pass* pass, float* transform, uint32_t segmentsH, uint32_t segmentsV) {
  uint32_t vertexCount = 2 + (segmentsH + 1) * (segmentsV - 1);
  uint32_t indexCount = 2 * 3 * segmentsH + segmentsH * (segmentsV - 2) * 6;
  ShapeVertex* vertices;
  uint16_t* indices;

  lovrPassDraw(pass, &(Draw) {
    .mode = VERTEX_TRIANGLES,
    .transform = transform,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  });

  // Top
  *vertices++ = (ShapeVertex) { { 0.f, 1.f, 0.f }, { 0.f, 1.f, 0.f }, { .5f, 0.f } };

  // Rings
  for (uint32_t i = 1; i < segmentsV; i++) {
    float v = i / (float) segmentsV;
    float phi = v * (float) M_PI;
    float sinphi = sinf(phi);
    float cosphi = cosf(phi);
    for (uint32_t j = 0; j <= segmentsH; j++) {
      float u = j / (float) segmentsH;
      float theta = u * 2.f * (float) M_PI;
      float sintheta = sinf(theta);
      float costheta = cosf(theta);
      float x = sintheta * sinphi;
      float y = cosphi;
      float z = -costheta * sinphi;
      *vertices++ = (ShapeVertex) { { x, y, z }, { x, y, z }, { u, v } };
    }
  }

  // Bottom
  *vertices++ = (ShapeVertex) { { 0.f, -1.f, 0.f }, { 0.f, -1.f, 0.f }, { .5f, 1.f } };

  // Top
  for (uint32_t i = 0; i < segmentsH; i++) {
    uint16_t wedge[] = { 0, i + 2, i + 1 };
    memcpy(indices, wedge, sizeof(wedge));
    indices += COUNTOF(wedge);
  }

  // Rings
  for (uint32_t i = 0; i < segmentsV - 2; i++) {
    for (uint32_t j = 0; j < segmentsH; j++) {
      uint16_t a = 1 + i * (segmentsH + 1) + 0 + j;
      uint16_t b = 1 + i * (segmentsH + 1) + 1 + j;
      uint16_t c = 1 + i * (segmentsH + 1) + 0 + segmentsH + 1 + j;
      uint16_t d = 1 + i * (segmentsH + 1) + 1 + segmentsH + 1 + j;
      uint16_t quad[] = { a, b, c, c, b, d };
      memcpy(indices, quad, sizeof(quad));
      indices += COUNTOF(quad);
    }
  }

  // Bottom
  for (uint32_t i = 0; i < segmentsH; i++) {
    uint16_t wedge[] = { vertexCount - 1, vertexCount - 1 - (i + 2), vertexCount - 1 - (i + 1) };
    memcpy(indices, wedge, sizeof(wedge));
    indices += COUNTOF(wedge);
  }
}

void lovrPassCylinder(Pass* pass, float* transform, bool capped, float angle1, float angle2, uint32_t segments) {
  if (fabsf(angle1 - angle2) >= 2.f * (float) M_PI) {
    angle1 = 0.f;
    angle2 = 2.f * (float) M_PI;
  }

  uint32_t vertexCount = 2 * (segments + 1);
  uint32_t indexCount = 6 * segments;
  ShapeVertex* vertices;
  uint16_t* indices;

  if (capped) {
    vertexCount *= 2;
    vertexCount += 2;
    indexCount += 3 * segments * 2;
  }

  lovrPassDraw(pass, &(Draw) {
    .mode = VERTEX_TRIANGLES,
    .transform = transform,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  });

  float angleShift = (angle2 - angle1) / segments;

  // Tube
  for (uint32_t i = 0; i <= segments; i++) {
    float theta = angle1 + i * angleShift;
    float x = cosf(theta);
    float y = sinf(theta);
    *vertices++ = (ShapeVertex) { { x, y, -.5f }, { x, y, 0.f }, { x + .5f, .5f - y } };
    *vertices++ = (ShapeVertex) { { x, y,  .5f }, { x, y, 0.f }, { x + .5f, .5f - y } };
  }

  // Tube quads
  for (uint32_t i = 0; i < segments; i++) {
    uint16_t a = i * 2 + 0;
    uint16_t b = i * 2 + 1;
    uint16_t c = i * 2 + 2;
    uint16_t d = i * 2 + 3;
    uint16_t quad[] = { a, b, c, c, b, d };
    memcpy(indices, quad, sizeof(quad));
    indices += COUNTOF(quad);
  }

  if (capped) {
    // Cap centers
    *vertices++ = (ShapeVertex) { { 0.f, 0.f, -.5f }, { 0.f, 0.f, -.5f }, { .5f, .5f } };
    *vertices++ = (ShapeVertex) { { 0.f, 0.f,  .5f }, { 0.f, 0.f,  .5f }, { .5f, .5f } };

    // Caps
    for (uint32_t i = 0; i <= segments; i++) {
      float theta = angle1 + i * angleShift;
      float x = cosf(theta);
      float y = sinf(theta);
      *vertices++ = (ShapeVertex) { { x, y, -.5f }, { 0.f, 0.f, -.5f }, { x + .5f, y - .5f } };
      *vertices++ = (ShapeVertex) { { x, y,  .5f }, { 0.f, 0.f,  .5f }, { x + .5f, y - .5f } };
    }

    // Cap wedges
    uint16_t base = 2 * (segments + 1);
    for (uint32_t i = 0; i < segments; i++) {
      uint16_t a = base + 0;
      uint16_t b = base + (i + 1) * 2;
      uint16_t c = base + (i + 2) * 2;
      uint16_t wedge1[] = { a + 0, b + 0, c + 0 };
      uint16_t wedge2[] = { a + 1, b + 1, c + 1 };
      memcpy(indices + 0, wedge1, sizeof(wedge1));
      memcpy(indices + 3, wedge2, sizeof(wedge2));
      indices += 6;
    }
  }
}

void lovrPassCapsule(Pass* pass, float* transform, uint32_t segments) {
  float sx = vec3_length(transform + 0);
  float sy = vec3_length(transform + 4);
  float sz = vec3_length(transform + 8);
  vec3_scale(transform + 0, 1.f / sx);
  vec3_scale(transform + 4, 1.f / sy);
  vec3_scale(transform + 8, 1.f / sz);
  float radius = sx;
  float length = sz * .5f;

  uint32_t rings = segments / 2;
  uint32_t vertexCount = 2 * (1 + rings * (segments + 1));
  uint32_t indexCount = 2 * (3 * segments + 6 * segments * (rings - 1)) + 6 * segments;
  ShapeVertex* vertices;
  uint16_t* indices;

  lovrPassDraw(pass, &(Draw) {
    .mode = VERTEX_TRIANGLES,
    .transform = transform,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  });

  float tip = length + radius;
  uint32_t h = vertexCount / 2;
  vertices[0] = (ShapeVertex) { { 0.f, 0.f, -tip }, { 0.f, 0.f, -1.f }, { .5f, 0.f } };
  vertices[h] = (ShapeVertex) { { 0.f, 0.f,  tip }, { 0.f, 0.f,  1.f }, { .5f, 1.f } };
  vertices++;

  for (uint32_t i = 1; i <= rings; i++) {
    float v = i / (float) rings;
    float phi = v * (float) M_PI / 2.f;
    float sinphi = sinf(phi);
    float cosphi = cosf(phi);
    for (uint32_t j = 0; j <= segments; j++) {
      float u = j / (float) segments;
      float theta = u * (float) M_PI * 2.f;
      float sintheta = sinf(theta);
      float costheta = cosf(theta);
      float x = costheta * sinphi;
      float y = sintheta * sinphi;
      float z = cosphi;
      vertices[0] = (ShapeVertex) { { x * radius, y * radius, -(length + z * radius) }, { x, y, -z }, { u, v } };
      vertices[h] = (ShapeVertex) { { x * radius, y * radius,  (length + z * radius) }, { x, y,  z }, { u, 1.f - v } };
      vertices++;
    }
  }

  uint16_t* i1 = indices;
  uint16_t* i2 = indices + (indexCount - 6 * segments) / 2;
  for (uint32_t i = 0; i < segments; i++) {
    uint16_t wedge1[] = { 0, 0 + i + 2, 0 + i + 1 };
    uint16_t wedge2[] = { h, h + i + 1, h + i + 2 };
    memcpy(i1, wedge1, sizeof(wedge1));
    memcpy(i2, wedge2, sizeof(wedge2));
    i1 += COUNTOF(wedge1);
    i2 += COUNTOF(wedge2);
  }

  for (uint32_t i = 0; i < rings - 1; i++) {
    for (uint32_t j = 0; j < segments; j++) {
      uint16_t a = 1 + i * (segments + 1) + 0 + j;
      uint16_t b = 1 + i * (segments + 1) + 1 + j;
      uint16_t c = 1 + i * (segments + 1) + 0 + segments + 1 + j;
      uint16_t d = 1 + i * (segments + 1) + 1 + segments + 1 + j;
      uint16_t quad1[] = { a, b, c, c, b, d };
      uint16_t quad2[] = { h + a, h + c, h + b, h + b, h + c, h + d };
      memcpy(i1, quad1, sizeof(quad1));
      memcpy(i2, quad2, sizeof(quad2));
      i1 += COUNTOF(quad1);
      i2 += COUNTOF(quad2);
    }
  }

  for (uint32_t i = 0; i < segments; i++) {
    uint16_t a = h - segments - 1 + i;
    uint16_t b = h - segments - 1 + i + 1;
    uint16_t c = vertexCount - segments - 1 + i;
    uint16_t d = vertexCount - segments - 1 + i + 1;
    uint16_t quad[] = { a, b, c, c, b, d };
    memcpy(i2, quad, sizeof(quad));
    i2 += COUNTOF(quad);
  }
}

void lovrPassTorus(Pass* pass, float* transform, uint32_t segmentsT, uint32_t segmentsP) {
  float sx = vec3_length(transform + 0);
  float sy = vec3_length(transform + 4);
  float sz = vec3_length(transform + 8);
  vec3_scale(transform + 0, 1.f / sx);
  vec3_scale(transform + 4, 1.f / sy);
  vec3_scale(transform + 8, 1.f / sz);
  float radius = sx * .5f;
  float thickness = sz * .5f;

  uint32_t vertexCount = segmentsT * segmentsP;
  uint32_t indexCount = segmentsT * segmentsP * 6;
  ShapeVertex* vertices;
  uint16_t* indices;

  lovrPassDraw(pass, &(Draw) {
    .mode = VERTEX_TRIANGLES,
    .transform = transform,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  });

  // T and P stand for toroidal and poloidal, or theta and phi
  float dt = (2.f * (float) M_PI) / segmentsT;
  float dp = (2.f * (float) M_PI) / segmentsP;
  for (uint32_t t = 0; t < segmentsT; t++) {
    float theta = t * dt;
    float tx = cosf(theta);
    float ty = sinf(theta);
    for (uint32_t p = 0; p < segmentsP; p++) {
      float phi = p * dp;
      float nx = cosf(phi) * tx;
      float ny = cosf(phi) * ty;
      float nz = sinf(phi);

      *vertices++ = (ShapeVertex) {
        .position = { tx * radius + nx * thickness, ty * radius + ny * thickness, nz * thickness },
        .normal = { nx, ny, nz }
      };

      uint16_t a = (t + 0) * segmentsP + p;
      uint16_t b = (t + 1) % segmentsT * segmentsP + p;
      uint16_t c = (t + 0) * segmentsP + (p + 1) % segmentsP;
      uint16_t d = (t + 1) % segmentsT * segmentsP + (p + 1) % segmentsP;
      uint16_t quad[] = { a, b, c, b, c, d };
      memcpy(indices, quad, sizeof(quad));
      indices += COUNTOF(quad);
    }
  }
}

static void aline(GlyphVertex* vertices, uint32_t head, uint32_t tail, float width, HorizontalAlign align) {
  if (align == ALIGN_LEFT) return;
  float shift = align / 2.f * width;
  for (uint32_t i = head; i < tail; i++) {
    vertices[i].position.x -= shift;
  }
}

void lovrPassText(Pass* pass, Font* font, ColoredString* strings, uint32_t count, float* transform, float wrap, HorizontalAlign halign, VerticalAlign valign) {
  font = font ? font : lovrGraphicsGetDefaultFont();
  float space = lovrFontGetGlyph(font, ' ', NULL)->advance;

  size_t totalLength = 0;
  for (uint32_t i = 0; i < count; i++) {
    totalLength += strings[i].length;
  }

  uint32_t stack = tempPush();
  GlyphVertex* vertices = tempAlloc(totalLength * 4 * sizeof(GlyphVertex));
  uint32_t vertexCount = 0;
  uint32_t glyphCount = 0;
  uint32_t lineCount = 1;
  uint32_t lineStart = 0;
  uint32_t wordStart = 0;

  float x = 0.f;
  float y = 0.f;
  float wordStartX = 0.f;
  float prevWordEndX = 0.f;
  float leading = lovrRasterizerGetLeading(font->info.rasterizer) * font->lineSpacing;
  float ascent = lovrRasterizerGetAscent(font->info.rasterizer);
  float scale = 1.f / font->pixelDensity;
  wrap /= scale;

  for (uint32_t i = 0; i < count; i++) {
    size_t bytes;
    uint32_t codepoint;
    uint32_t previous = '\0';
    const char* str = strings[i].string;
    const char* end = strings[i].string + strings[i].length;
    uint8_t r = (uint8_t) (CLAMP(strings[i].color[0], 0.f, 1.f) * 255.f);
    uint8_t g = (uint8_t) (CLAMP(strings[i].color[1], 0.f, 1.f) * 255.f);
    uint8_t b = (uint8_t) (CLAMP(strings[i].color[2], 0.f, 1.f) * 255.f);
    uint8_t a = (uint8_t) (CLAMP(strings[i].color[3], 0.f, 1.f) * 255.f);

    while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {
      if (codepoint == ' ' || codepoint == '\t') {
        if (previous) prevWordEndX = x;
        wordStart = vertexCount;
        x += codepoint == '\t' ? space * 4.f : space;
        wordStartX = x;
        previous = '\0';
        str += bytes;
        continue;
      } else if (codepoint == '\n') {
        aline(vertices, lineStart, vertexCount, x, halign);
        lineStart = vertexCount;
        wordStart = vertexCount;
        x = 0.f;
        y -= leading;
        wordStartX = 0.f;
        prevWordEndX = 0.f;
        lineCount++;
        previous = '\0';
        str += bytes;
        continue;
      } else if (codepoint == '\r') {
        str += bytes;
        continue;
      }

      bool resized;
      Glyph* glyph = lovrFontGetGlyph(font, codepoint, &resized);

      if (resized) {
        tempPop(stack);
        lovrPassText(pass, font, strings, count, transform, wrap, halign, valign);
        return;
      }

      // Keming
      if (previous) x += lovrFontGetKerning(font, previous, codepoint);
      previous = codepoint;

      // Wrap
      if (wrap > 0.f && x + glyph->advance > wrap && wordStart != lineStart) {
        float dx = wordStartX;
        float dy = leading;

        // Shift the vertices of the overflowing word down a line and back to the beginning
        for (uint32_t v = wordStart; v < vertexCount; v++) {
          vertices[v].position.x -= dx;
          vertices[v].position.y -= dy;
        }

        aline(vertices, lineStart, wordStart, prevWordEndX, halign);
        lineStart = wordStart;
        wordStartX = 0.f;
        lineCount++;
        x -= dx;
        y -= dy;
      }

      // Vertices
      float* bb = glyph->box;
      uint16_t* uv = glyph->uv;
      vertices[vertexCount++] = (GlyphVertex) { { x + bb[0], y + bb[3] }, { uv[0], uv[1] }, { r, g, b, a } };
      vertices[vertexCount++] = (GlyphVertex) { { x + bb[2], y + bb[3] }, { uv[2], uv[1] }, { r, g, b, a } };
      vertices[vertexCount++] = (GlyphVertex) { { x + bb[0], y + bb[1] }, { uv[0], uv[3] }, { r, g, b, a } };
      vertices[vertexCount++] = (GlyphVertex) { { x + bb[2], y + bb[1] }, { uv[2], uv[3] }, { r, g, b, a } };
      glyphCount++;

      // Advance
      x += glyph->advance;
      str += bytes;
    }
  }

  // Align last line
  aline(vertices, lineStart, vertexCount, x, halign);

  mat4_scale(transform, scale, scale, scale);
  mat4_translate(transform, 0.f, -ascent + valign / 2.f * (leading * lineCount), 0.f);

  uint16_t* indices;
  lovrPassDraw(pass, &(Draw) {
    .mode = VERTEX_TRIANGLES,
    .shader = SHADER_FONT,
    .material = font->material,
    .transform = transform,
    .vertex.format = VERTEX_GLYPH,
    .vertex.count = vertexCount,
    .vertex.data = vertices,
    .index.count = glyphCount * 6,
    .index.pointer = (void**) &indices
  });

  for (uint32_t i = 0; i < vertexCount; i += 4) {
    uint16_t quad[] = { i + 0, i + 2, i + 1, i + 1, i + 2, i + 3 };
    memcpy(indices, quad, sizeof(quad));
    indices += COUNTOF(quad);
  }

  tempPop(stack);
}

void lovrPassSkybox(Pass* pass, Texture* texture) {
  if (texture->info.type == TEXTURE_2D) {
    lovrPassDraw(pass, &(Draw) {
      .mode = VERTEX_TRIANGLES,
      .shader = SHADER_PANO,
      .material = texture ? lovrTextureGetMaterial(texture) : NULL,
      .vertex.format = VERTEX_EMPTY,
      .count = 6
    });
  } else {
    lovrPassDraw(pass, &(Draw) {
      .mode = VERTEX_TRIANGLES,
      .shader = SHADER_CUBE,
      .material = texture ? lovrTextureGetMaterial(texture) : NULL,
      .vertex.format = VERTEX_EMPTY,
      .count = 6
    });
  }
}

void lovrPassFill(Pass* pass, Texture* texture) {
  lovrPassDraw(pass, &(Draw) {
    .mode = VERTEX_TRIANGLES,
    .shader = SHADER_FILL,
    .material = texture ? lovrTextureGetMaterial(texture) : NULL,
    .vertex.format = VERTEX_EMPTY,
    .count = 3
  });
}

void lovrPassMonkey(Pass* pass, float* transform) {
  uint32_t vertexCount = COUNTOF(monkey_vertices) / 6;

  ShapeVertex* vertices;
  lovrPassDraw(pass, &(Draw) {
    .mode = VERTEX_TRIANGLES,
    .vertex.count = vertexCount,
    .vertex.pointer = (void**) &vertices,
    .index.count = COUNTOF(monkey_indices),
    .index.data = monkey_indices,
    .transform = transform
  });

  // Manual vertex format conversion to avoid another format (and sn8x3 isn't always supported)
  for (uint32_t i = 0; i < vertexCount; i++) {
    vertices[i] = (ShapeVertex) {
      .position.x = monkey_vertices[6 * i + 0] / 255.f * monkey_size[0] + monkey_offset[0],
      .position.y = monkey_vertices[6 * i + 1] / 255.f * monkey_size[1] + monkey_offset[1],
      .position.z = monkey_vertices[6 * i + 2] / 255.f * monkey_size[2] + monkey_offset[2],
      .normal.x = monkey_vertices[6 * i + 3] / 255.f * 2.f - 1.f,
      .normal.y = monkey_vertices[6 * i + 4] / 255.f * 2.f - 1.f,
      .normal.z = monkey_vertices[6 * i + 5] / 255.f * 2.f - 1.f,
    };
  }
}

static void renderNode(Pass* pass, Model* model, uint32_t index, bool recurse, uint32_t instances) {
  ModelNode* node = &model->info.data->nodes[index];
  mat4 globalTransform = model->globalTransforms + 16 * index;

  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    Draw draw = model->draws[node->primitiveIndex + i];
    if (node->skin == ~0u) draw.transform = globalTransform;
    draw.instances = instances;
    lovrPassDraw(pass, &draw);
  }

  if (recurse) {
    for (uint32_t i = 0; i < node->childCount; i++) {
      renderNode(pass, model, node->children[i], true, instances);
    }
  }
}

void lovrPassDrawModel(Pass* pass, Model* model, float* transform, uint32_t node, bool recurse, uint32_t instances) {
  if (model->transformsDirty) {
    updateModelTransforms(model, model->info.data->rootNode, (float[]) MAT4_IDENTITY);
    lovrModelReskin(model);
    model->transformsDirty = false;
  }

  if (node == ~0u) {
    node = model->info.data->rootNode;
  }

  lovrPassPush(pass, STACK_TRANSFORM);
  lovrPassTransform(pass, transform);
  renderNode(pass, model, node, recurse, instances);
  lovrPassPop(pass, STACK_TRANSFORM);
}

void lovrPassMesh(Pass* pass, Buffer* vertices, Buffer* indices, float* transform, uint32_t start, uint32_t count, uint32_t instances) {
  if (count == ~0u) {
    if (indices || vertices) {
      count = (indices ? indices : vertices)->info.length - start;
    } else {
      count = 0;
    }
  }

  if (indices) {
    lovrCheck(count <= indices->info.length - start, "Mesh draw range exceeds index buffer size");
  } else if (vertices) {
    lovrCheck(count <= vertices->info.length - start, "Mesh draw range exceeds vertex buffer size");
  }

  lovrPassDraw(pass, &(Draw) {
    .mode = pass->pipeline->drawMode,
    .vertex.buffer = vertices,
    .index.buffer = indices,
    .transform = transform,
    .start = start,
    .count = count,
    .instances = instances
  });
}

void lovrPassMultimesh(Pass* pass, Buffer* vertices, Buffer* indices, Buffer* draws, uint32_t count, uint32_t offset, uint32_t stride) {
  lovrCheck(pass->info.type == PASS_RENDER, "This function can only be called on a render pass");
  lovrCheck(offset % 4 == 0, "Multimesh draw buffer offset must be a multiple of 4");
  uint32_t commandSize = indices ? 20 : 16;
  stride = stride ? stride : commandSize;
  uint32_t totalSize = stride * (count - 1) + commandSize;
  lovrCheck(offset + totalSize < draws->size, "Multimesh draw range exceeds size of draw buffer");

  Draw draw = (Draw) {
    .mode = pass->pipeline->drawMode,
    .vertex.buffer = vertices,
    .index.buffer = indices
  };

  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader, "A custom Shader must be bound to draw a multimesh");

  flushPipeline(pass, &draw, shader);
  flushConstants(pass, shader);
  flushBindings(pass, shader);
  flushBuiltins(pass, &draw, shader);
  flushBuffers(pass, &draw);

  if (indices) {
    gpu_draw_indirect_indexed(pass->stream, draws->gpu, offset, count, stride);
  } else {
    gpu_draw_indirect(pass->stream, draws->gpu, offset, count, stride);
  }

  trackBuffer(pass, draws, GPU_PHASE_INDIRECT, GPU_CACHE_INDIRECT);
}

void lovrPassCompute(Pass* pass, uint32_t x, uint32_t y, uint32_t z, Buffer* indirect, uint32_t offset) {
  lovrCheck(pass->info.type == PASS_COMPUTE, "This function can only be called on a compute pass");

  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader && shader->info.type == SHADER_COMPUTE, "Tried to run a compute shader, but no compute shader is bound");
  lovrCheck(x <= state.limits.computeDispatchCount[0], "Compute %s count exceeds computeDispatchCount limit", "x");
  lovrCheck(y <= state.limits.computeDispatchCount[1], "Compute %s count exceeds computeDispatchCount limit", "y");
  lovrCheck(z <= state.limits.computeDispatchCount[2], "Compute %s count exceeds computeDispatchCount limit", "z");

  gpu_pipeline* pipeline = state.pipelines.data[shader->computePipeline];

  if (pass->pipeline->dirty) {
    gpu_bind_pipeline(pass->stream, pipeline, true);
    state.stats.pipelineSwitches++;
    pass->pipeline->dirty = false;
  }

  flushConstants(pass, shader);
  flushBindings(pass, shader);

  if (indirect) {
    lovrCheck(offset % 4 == 0, "Indirect compute offset must be a multiple of 4");
    lovrCheck(offset <= indirect->size - 12, "Indirect compute offset overflows the Buffer");
    trackBuffer(pass, indirect, GPU_PHASE_INDIRECT, GPU_CACHE_INDIRECT);
    gpu_compute_indirect(pass->stream, indirect->gpu, offset);
  } else {
    gpu_compute(pass->stream, x, y, z);
  }
}

void lovrPassClearBuffer(Pass* pass, Buffer* buffer, uint32_t offset, uint32_t extent) {
  if (extent == 0) return;
  if (extent == ~0u) extent = buffer->size - offset;
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!lovrBufferIsTemporary(buffer), "Temporary buffers can not be cleared");
  lovrCheck(offset % 4 == 0, "Buffer clear offset must be a multiple of 4");
  lovrCheck(extent % 4 == 0, "Buffer clear extent must be a multiple of 4");
  lovrCheck(offset + extent <= buffer->size, "Buffer clear range goes past the end of the Buffer");
  gpu_clear_buffer(pass->stream, buffer->gpu, offset, extent);
  trackBuffer(pass, buffer, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
}

void lovrPassClearTexture(Pass* pass, Texture* texture, float value[4], uint32_t layer, uint32_t layerCount, uint32_t level, uint32_t levelCount) {
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!texture->info.parent, "Texture views can not be cleared");
  lovrCheck(texture->info.usage & TEXTURE_TRANSFER, "Texture must be created with 'transfer' usage to clear it");
  lovrCheck(texture->info.type == TEXTURE_3D || layer + layerCount <= texture->info.depth, "Texture clear range exceeds texture layer count");
  lovrCheck(level + levelCount <= texture->info.mipmaps, "Texture clear range exceeds texture mipmap count");
  gpu_clear_texture(pass->stream, texture->gpu, value, layer, layerCount, level, levelCount);
  trackTexture(pass, texture, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
}

void* lovrPassCopyDataToBuffer(Pass* pass, Buffer* buffer, uint32_t offset, uint32_t extent) {
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!lovrBufferIsTemporary(buffer), "Temporary buffers can not be copied to, use Buffer:setData");
  lovrCheck(offset + extent <= buffer->size, "Buffer copy range goes past the end of the Buffer");
  gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
  void* pointer = gpu_map(scratchpad, extent, 4, GPU_MAP_WRITE);
  gpu_copy_buffers(pass->stream, scratchpad, buffer->gpu, 0, offset, extent);
  trackBuffer(pass, buffer, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
  return pointer;
}

void lovrPassCopyBufferToBuffer(Pass* pass, Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t extent) {
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!lovrBufferIsTemporary(dst), "Temporary buffers can not be copied to");
  lovrCheck(srcOffset + extent <= src->size, "Buffer copy range goes past the end of the source Buffer");
  lovrCheck(dstOffset + extent <= dst->size, "Buffer copy range goes past the end of the destination Buffer");
  gpu_copy_buffers(pass->stream, src->gpu, dst->gpu, srcOffset, dstOffset, extent);
  trackBuffer(pass, src, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_READ);
  trackBuffer(pass, dst, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
}

void lovrPassCopyTallyToBuffer(Pass* pass, Tally* tally, Buffer* buffer, uint32_t srcIndex, uint32_t dstOffset, uint32_t count) {
  if (count == ~0u) count = tally->info.count;
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!lovrBufferIsTemporary(buffer), "Temporary buffers can not be copied to");
  lovrCheck(srcIndex + count <= tally->info.count, "Tally copy range exceeds the number of slots in the Tally");
  lovrCheck(dstOffset + count * 4 <= buffer->size, "Buffer copy range goes past the end of the destination Buffer");
  lovrCheck(dstOffset % 4 == 0, "Buffer copy offset must be a multiple of 4");

  for (uint32_t i = 0; i < count; i++) {
    uint32_t index = srcIndex + i;
    lovrCheck(tally->masks[index / 64] & (1 << (index % 64)), "Trying to copy Tally slot %d, but it hasn't been marked yet", index + 1);
  }

  if (tally->info.type == TALLY_TIMER) {
    gpu_copy_tally_buffer(pass->stream, tally->gpu, tally->buffer, srcIndex, 0, count, 4);

    // Wait for transfer to finish, then dispatch a compute shader to fixup timestamps
    gpu_sync(pass->stream, &(gpu_barrier) {
      .prev = GPU_PHASE_TRANSFER,
      .next = GPU_PHASE_SHADER_COMPUTE,
      .flush = GPU_CACHE_TRANSFER_WRITE,
      .clear = GPU_CACHE_STORAGE_READ
    }, 1);

    if (!state.timeWizard) {
      Blob* source = lovrBlobCreate((void*) lovr_shader_timewizard_comp, sizeof(lovr_shader_timewizard_comp), NULL);
      state.timeWizard = lovrShaderCreate(&(ShaderInfo) {
        .type = SHADER_COMPUTE,
        .stages[0] = source,
        .label = "Chronophage"
      });
      source->data = NULL;
      lovrRelease(source, lovrBlobDestroy);
    }

    gpu_pipeline* pipeline = state.pipelines.data[state.timeWizard->computePipeline];
    gpu_layout* layout = state.layouts.data[state.timeWizard->layout].gpu;
    gpu_shader* shader = state.timeWizard->gpu;

    gpu_binding bindings[] = {
      [0] = { 0, GPU_SLOT_STORAGE_BUFFER, .buffer = { tally->buffer, 0, ~0u } },
      [1] = { 1, GPU_SLOT_STORAGE_BUFFER, .buffer = { buffer->gpu, dstOffset, count * sizeof(uint32_t) } }
    };

    gpu_bundle* bundle = getBundle(state.timeWizard->layout);
    gpu_bundle_info bundleInfo = { layout, bindings, COUNTOF(bindings) };
    gpu_bundle_write(&bundle, &bundleInfo, 1);

    struct { uint32_t first, count, views; float period; } constants = {
      .first = srcIndex,
      .count = count,
      .views = tally->info.views,
      .period = state.limits.timestampPeriod
    };

    gpu_compute_begin(pass->stream);
    gpu_bind_pipeline(pass->stream, pipeline, true);
    gpu_bind_bundle(pass->stream, shader, 0, bundle, NULL, 0);
    gpu_push_constants(pass->stream, shader, &constants, sizeof(constants));
    gpu_compute(pass->stream, (count + 31) / 32, 1, 1);
    gpu_compute_end(pass->stream);
    state.stats.pipelineSwitches++;
    state.stats.bundleSwitches++;

    trackBuffer(pass, buffer, GPU_PHASE_SHADER_COMPUTE, GPU_CACHE_STORAGE_WRITE);
  } else {
    gpu_copy_tally_buffer(pass->stream, tally->gpu, buffer->gpu, srcIndex, dstOffset, count, 4);
    trackBuffer(pass, buffer, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
  }
}

void lovrPassCopyImageToTexture(Pass* pass, Image* image, Texture* texture, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t extent[4]) {
  if (extent[0] == ~0u) extent[0] = MIN(texture->info.width - dstOffset[0], lovrImageGetWidth(image, srcOffset[3]) - srcOffset[0]);
  if (extent[1] == ~0u) extent[1] = MIN(texture->info.height - dstOffset[1], lovrImageGetHeight(image, srcOffset[3]) - srcOffset[1]);
  if (extent[2] == ~0u) extent[2] = MIN(texture->info.depth - dstOffset[2], lovrImageGetLayerCount(image) - srcOffset[2]);
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(texture->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to copy to it");
  lovrCheck(!texture->info.parent, "Texture views can not be written to");
  lovrCheck(texture->info.samples == 1, "Multisampled Textures can not be written to");
  lovrCheck(lovrImageGetFormat(image) == texture->info.format, "Image and Texture formats must match");
  lovrCheck(srcOffset[0] + extent[0] <= lovrImageGetWidth(image, srcOffset[3]), "Image copy region exceeds its %s", "width");
  lovrCheck(srcOffset[1] + extent[1] <= lovrImageGetHeight(image, srcOffset[3]), "Image copy region exceeds its %s", "height");
  lovrCheck(srcOffset[2] + extent[2] <= lovrImageGetLayerCount(image), "Image copy region exceeds its %s", "layer count");
  lovrCheck(srcOffset[3] < lovrImageGetLevelCount(image), "Image copy region exceeds its %s", "mipmap count");
  checkTextureBounds(&texture->info, dstOffset, extent);
  size_t rowSize = measureTexture(texture->info.format, extent[0], 1, 1);
  size_t totalSize = measureTexture(texture->info.format, extent[0], extent[1], 1) * extent[2];
  size_t layerOffset = measureTexture(texture->info.format, extent[0], srcOffset[1], 1);
  layerOffset += measureTexture(texture->info.format, srcOffset[0], 1, 1);
  size_t pitch = measureTexture(texture->info.format, lovrImageGetWidth(image, srcOffset[3]), 1, 1);
  gpu_buffer* buffer = tempAlloc(gpu_sizeof_buffer());
  char* dst = gpu_map(buffer, totalSize, 64, GPU_MAP_WRITE);
  for (uint32_t z = 0; z < extent[2]; z++) {
    const char* src = (char*) lovrImageGetLayerData(image, srcOffset[3], z) + layerOffset;
    for (uint32_t y = 0; y < extent[1]; y++) {
      memcpy(dst, src, rowSize);
      dst += rowSize;
      src += pitch;
    }
  }
  gpu_copy_buffer_texture(pass->stream, buffer, texture->gpu, 0, dstOffset, extent);
  trackTexture(pass, texture, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
}

void lovrPassCopyTextureToTexture(Pass* pass, Texture* src, Texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t extent[3]) {
  if (extent[0] == ~0u) extent[0] = MIN(src->info.width - srcOffset[0], dst->info.width - dstOffset[0]);
  if (extent[1] == ~0u) extent[1] = MIN(src->info.height - srcOffset[1], dst->info.height - dstOffset[0]);
  if (extent[2] == ~0u) extent[2] = MIN(src->info.depth - srcOffset[2], dst->info.depth - dstOffset[0]);
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(src->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to copy %s it", "from");
  lovrCheck(dst->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to copy %s it", "to");
  lovrCheck(!src->info.parent && !dst->info.parent, "Can not copy texture views");
  lovrCheck(src->info.format == dst->info.format, "Copying between Textures requires them to have the same format");
  lovrCheck(src->info.samples == dst->info.samples, "Texture sample counts must match to copy between them");
  checkTextureBounds(&src->info, srcOffset, extent);
  checkTextureBounds(&dst->info, dstOffset, extent);
  gpu_copy_textures(pass->stream, src->gpu, dst->gpu, srcOffset, dstOffset, extent);
  trackTexture(pass, src, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_READ);
  trackTexture(pass, dst, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
}

void lovrPassBlit(Pass* pass, Texture* src, Texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t srcExtent[3], uint32_t dstExtent[3], FilterMode filter) {
  if (srcExtent[0] == ~0u) srcExtent[0] = src->info.width - srcOffset[0];
  if (srcExtent[1] == ~0u) srcExtent[1] = src->info.height - srcOffset[1];
  if (srcExtent[2] == ~0u) srcExtent[2] = src->info.depth - srcOffset[2];
  if (dstExtent[0] == ~0u) dstExtent[0] = dst->info.width - dstOffset[0];
  if (dstExtent[1] == ~0u) dstExtent[1] = dst->info.height - dstOffset[1];
  if (dstExtent[2] == ~0u) dstExtent[2] = dst->info.depth - dstOffset[2];
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!src->info.parent && !dst->info.parent, "Can not blit Texture views");
  lovrCheck(src->info.samples == 1 && dst->info.samples == 1, "Multisampled textures can not be used for blits");
  lovrCheck(src->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to blit %s it", "from");
  lovrCheck(dst->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to blit %s it", "to");
  lovrCheck(state.features.formats[src->info.format] & GPU_FEATURE_BLIT_SRC, "This GPU does not support blitting from the source texture's format");
  lovrCheck(state.features.formats[dst->info.format] & GPU_FEATURE_BLIT_DST, "This GPU does not support blitting to the destination texture's format");
  lovrCheck(src->info.format == dst->info.format, "Texture formats must match to blit between them");
  // FIXME if src or dst is 3D you can only blit 1 layer or something!
  checkTextureBounds(&src->info, srcOffset, srcExtent);
  checkTextureBounds(&dst->info, dstOffset, dstExtent);
  gpu_blit(pass->stream, src->gpu, dst->gpu, srcOffset, dstOffset, srcExtent, dstExtent, (gpu_filter) filter);
  trackTexture(pass, src, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_READ);
  trackTexture(pass, dst, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
}

void lovrPassMipmap(Pass* pass, Texture* texture, uint32_t base, uint32_t count) {
  if (count == ~0u) count = texture->info.mipmaps - (base + 1);
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!texture->info.parent, "Can not mipmap a Texture view");
  lovrCheck(texture->info.samples == 1, "Can not mipmap a multisampled texture");
  lovrCheck(texture->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to mipmap %s it", "from");
  lovrCheck(state.features.formats[texture->info.format] & GPU_FEATURE_BLIT_SRC, "This GPU does not support blitting %s the source texture's format, which is required for mipmapping", "from");
  lovrCheck(state.features.formats[texture->info.format] & GPU_FEATURE_BLIT_DST, "This GPU does not support blitting %s the source texture's format, which is required for mipmapping", "to");
  lovrCheck(base + count < texture->info.mipmaps, "Trying to generate too many mipmaps");
  mipmapTexture(pass->stream, texture, base, count);
  trackTexture(pass, texture, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_READ | GPU_CACHE_TRANSFER_WRITE);
}

void lovrPassTick(Pass* pass, Tally* tally, uint32_t index) {
  lovrCheck(index < tally->info.count, "Trying to use tally slot #%d, but the tally only has %d slots", index + 1, tally->info.count);
  lovrCheck(~tally->masks[index / 64] & (1 << (index % 64)), "Tally slot #%d has already been used", index + 1);

  if (tally->tick != state.tick) {
    gpu_clear_tally(state.stream, tally->gpu, 0, tally->info.count * 2 * tally->info.views);
    memset(tally->masks, 0, (tally->info.count + 63) / 64 * sizeof(uint64_t));
    tally->tick = state.tick;
  }

  if (tally->info.type == TALLY_TIMER) {
    gpu_tally_mark(pass->stream, tally->gpu, index * 2 * tally->info.views);
  } else {
    gpu_tally_begin(pass->stream, tally->gpu, index);
  }
}

void lovrPassTock(Pass* pass, Tally* tally, uint32_t index) {
  lovrCheck(index < tally->info.count, "Trying to use tally slot #%d, but the tally only has %d slots", index + 1, tally->info.count);
  lovrCheck(tally->masks[index / 64] & (1 << (index % 64)), "Tally slot #%d has not been started yet", index + 1);

  if (tally->info.type == TALLY_TIMER) {
    gpu_tally_mark(pass->stream, tally->gpu, index * 2 * tally->info.views + tally->info.views);
  } else {
    gpu_tally_end(pass->stream, tally->gpu, index);
  }
}

// Helpers

static void* tempAlloc(size_t size) {
  while (state.allocator.cursor + size > state.allocator.length) {
    lovrAssert(state.allocator.length << 1 <= MAX_FRAME_MEMORY, "Out of memory");
    os_vm_commit(state.allocator.memory + state.allocator.length, state.allocator.length);
    state.allocator.length <<= 1;
  }

  uint32_t cursor = ALIGN(state.allocator.cursor, 8);
  state.allocator.cursor = cursor + size;
  return state.allocator.memory + cursor;
}

static void* tempGrow(void* p, size_t size) {
  if (size == 0) return NULL;
  void* new = tempAlloc(size);
  if (!p) return new;
  return memcpy(new, p, size >> 1);
}

static uint32_t tempPush(void) {
  return state.allocator.cursor;
}

static void tempPop(uint32_t stack) {
  state.allocator.cursor = stack;
}

static int u64cmp(const void* a, const void* b) {
  uint64_t x = *(uint64_t*) a, y = *(uint64_t*) b;
  return (x > y) - (x < y);
}

static void beginFrame(void) {
  if (state.active) {
    return;
  }

  state.active = true;
  state.tick = gpu_begin();
  state.stream = gpu_stream_begin("Internal uploads");
  state.allocator.cursor = 0;
}

// Clean up ALL passes created during the frame, even unsubmitted ones
static void cleanupPasses(void) {
  for (size_t i = 0; i < state.passes.length; i++) {
    Pass* pass = state.passes.data[i];

    for (size_t j = 0; j < pass->access.length; j++) {
      Access* access = &pass->access.data[j];
      lovrRelease(access->buffer, lovrBufferDestroy);
      lovrRelease(access->texture, lovrTextureDestroy);
    }

    for (size_t j = 0; j <= pass->pipelineIndex; j++) {
      lovrRelease(pass->pipelines[j].sampler, lovrSamplerDestroy);
      lovrRelease(pass->pipelines[j].shader, lovrShaderDestroy);
      lovrRelease(pass->pipelines[j].material, lovrMaterialDestroy);
      pass->pipelines[j].sampler = NULL;
      pass->pipelines[j].shader = NULL;
      pass->pipelines[j].material = NULL;
    }
  }
}

static uint32_t getLayout(gpu_slot* slots, uint32_t count) {
  uint64_t hash = hash64(slots, count * sizeof(gpu_slot));

  uint32_t index;
  for (uint32_t index = 0; index < state.layouts.length; index++) {
    if (state.layouts.data[index].hash == hash) {
      return index;
    }
  }

  gpu_layout_info info = {
    .slots = slots,
    .count = count
  };

  gpu_layout* handle = malloc(gpu_sizeof_layout());
  lovrAssert(handle, "Out of memory");
  gpu_layout_init(handle, &info);

  Layout layout = {
    .hash = hash,
    .gpu = handle
  };

  index = state.layouts.length;
  arr_push(&state.layouts, layout);
  return index;
}

static gpu_bundle* getBundle(uint32_t layoutIndex) {
  Layout* layout = &state.layouts.data[layoutIndex];
  BundlePool* pool = layout->head;
  const uint32_t POOL_SIZE = 512;

  if (pool) {
    if (pool->cursor < POOL_SIZE) {
      return (gpu_bundle*) ((char*) pool->bundles + gpu_sizeof_bundle() * pool->cursor++);
    }

    // If the pool's closed, move it to the end of the list and try to use the next pool
    layout->tail->next = pool;
    layout->tail = pool;
    layout->head = pool->next;
    pool->next = NULL;
    pool->tick = state.tick;
    pool = layout->head;

    if (pool && gpu_finished(pool->tick)) {
      pool->cursor = 1;
      return pool->bundles;
    }
  }

  // If no pool was available, make a new one
  pool = malloc(sizeof(BundlePool));
  gpu_bundle_pool* gpu = malloc(gpu_sizeof_bundle_pool());
  gpu_bundle* bundles = malloc(POOL_SIZE * gpu_sizeof_bundle());
  lovrAssert(pool && gpu && bundles, "Out of memory");
  pool->gpu = gpu;
  pool->bundles = bundles;
  pool->cursor = 1;
  pool->next = layout->head;

  gpu_bundle_pool_info info = {
    .bundles = pool->bundles,
    .layout = layout->gpu,
    .count = POOL_SIZE
  };

  gpu_bundle_pool_init(pool->gpu, &info);

  layout->head = pool;
  if (!layout->tail) layout->tail = pool;
  return pool->bundles;
}

// Note that we are technically not doing synchronization correctly for temporary attachments.  It
// is very unlikely to be a problem in practice, though it should still be fixed for correctness.
static gpu_texture* getAttachment(uint32_t size[2], uint32_t layers, TextureFormat format, bool srgb, uint32_t samples) {
  uint16_t key[] = { size[0], size[1], layers, format, srgb, samples };
  uint32_t hash = (uint32_t) hash64(key, sizeof(key));

  // Find a matching attachment that hasn't been used this frame
  for (uint32_t i = 0; i < state.attachments.length; i++) {
    if (state.attachments.data[i].hash == hash && state.attachments.data[i].tick != state.tick) {
      return state.attachments.data[i].texture;
    }
  }

  // Find something to evict
  TempAttachment* attachment = NULL;
  for (uint32_t i = 0; i < state.attachments.length; i++) {
    if (state.tick - state.attachments.data[i].tick > 16) {
      attachment = &state.attachments.data[i];
      break;
    }
  }

  if (attachment) {
    gpu_texture_destroy(attachment->texture);
  } else {
    arr_expand(&state.attachments, 1);
    attachment = &state.attachments.data[state.attachments.length++];
    attachment->texture = calloc(1, gpu_sizeof_texture());
    lovrAssert(attachment->texture, "Out of memory");
  }

  gpu_texture_info info = {
    .type = GPU_TEXTURE_ARRAY,
    .format = (gpu_texture_format) format,
    .size[0] = size[0],
    .size[1] = size[1],
    .size[2] = layers,
    .mipmaps = 1,
    .samples = samples,
    .usage = GPU_TEXTURE_RENDER | GPU_TEXTURE_TRANSIENT,
    .srgb = srgb
  };

  lovrAssert(gpu_texture_init(attachment->texture, &info), "Failed to create scratch texture");
  attachment->hash = hash;
  attachment->tick = state.tick;
  return attachment->texture;
}

// Returns number of bytes of a 3D texture region of a given format
static size_t measureTexture(TextureFormat format, uint16_t w, uint16_t h, uint16_t d) {
  switch (format) {
    case FORMAT_R8: return w * h * d;
    case FORMAT_RG8:
    case FORMAT_R16:
    case FORMAT_R16F:
    case FORMAT_RGB565:
    case FORMAT_RGB5A1:
    case FORMAT_D16: return w * h * d * 2;
    case FORMAT_RGBA8:
    case FORMAT_RG16:
    case FORMAT_RG16F:
    case FORMAT_R32F:
    case FORMAT_RG11B10F:
    case FORMAT_RGB10A2:
    case FORMAT_D24S8:
    case FORMAT_D32F: return w * h * d * 4;
    case FORMAT_RGBA16:
    case FORMAT_RGBA16F:
    case FORMAT_RG32F: return w * h * d * 8;
    case FORMAT_RGBA32F: return w * h * d * 16;
    case FORMAT_BC1:
    case FORMAT_BC2:
    case FORMAT_BC3:
    case FORMAT_BC4U:
    case FORMAT_BC4S:
    case FORMAT_BC5U:
    case FORMAT_BC5S:
    case FORMAT_BC6UF:
    case FORMAT_BC6SF:
    case FORMAT_BC7:
    case FORMAT_ASTC_4x4: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_ASTC_5x4: return ((w + 4) / 5) * ((h + 3) / 4) * d * 16;
    case FORMAT_ASTC_5x5: return ((w + 4) / 5) * ((h + 4) / 5) * d * 16;
    case FORMAT_ASTC_6x5: return ((w + 5) / 6) * ((h + 4) / 5) * d * 16;
    case FORMAT_ASTC_6x6: return ((w + 5) / 6) * ((h + 5) / 6) * d * 16;
    case FORMAT_ASTC_8x5: return ((w + 7) / 8) * ((h + 4) / 5) * d * 16;
    case FORMAT_ASTC_8x6: return ((w + 7) / 8) * ((h + 5) / 6) * d * 16;
    case FORMAT_ASTC_8x8: return ((w + 7) / 8) * ((h + 7) / 8) * d * 16;
    case FORMAT_ASTC_10x5: return ((w + 9) / 10) * ((h + 4) / 5) * d * 16;
    case FORMAT_ASTC_10x6: return ((w + 9) / 10) * ((h + 5) / 6) * d * 16;
    case FORMAT_ASTC_10x8: return ((w + 9) / 10) * ((h + 7) / 8) * d * 16;
    case FORMAT_ASTC_10x10: return ((w + 9) / 10) * ((h + 9) / 10) * d * 16;
    case FORMAT_ASTC_12x10: return ((w + 11) / 12) * ((h + 9) / 10) * d * 16;
    case FORMAT_ASTC_12x12: return ((w + 11) / 12) * ((h + 11) / 12) * d * 16;
    default: lovrUnreachable();
  }
}

// Errors if a 3D texture region exceeds the texture's bounds
static void checkTextureBounds(const TextureInfo* info, uint32_t offset[4], uint32_t extent[3]) {
  uint32_t maxWidth = MAX(info->width >> offset[3], 1);
  uint32_t maxHeight = MAX(info->height >> offset[3], 1);
  uint32_t maxDepth = info->type == TEXTURE_3D ? MAX(info->depth >> offset[3], 1) : info->depth;
  lovrCheck(offset[0] + extent[0] <= maxWidth, "Texture x range [%d,%d] exceeds width (%d)", offset[0], offset[0] + extent[0], maxWidth);
  lovrCheck(offset[1] + extent[1] <= maxHeight, "Texture y range [%d,%d] exceeds height (%d)", offset[1], offset[1] + extent[1], maxHeight);
  lovrCheck(offset[2] + extent[2] <= maxDepth, "Texture z range [%d,%d] exceeds depth (%d)", offset[2], offset[2] + extent[2], maxDepth);
  lovrCheck(offset[3] < info->mipmaps, "Texture mipmap %d exceeds its mipmap count (%d)", offset[3] + 1, info->mipmaps);
}

static void mipmapTexture(gpu_stream* stream, Texture* texture, uint32_t base, uint32_t count) {
  if (count == ~0u) count = texture->info.mipmaps - (base + 1);
  bool volumetric = texture->info.type == TEXTURE_3D;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t level = base + i + 1;
    uint32_t srcOffset[4] = { 0, 0, 0, level - 1 };
    uint32_t dstOffset[4] = { 0, 0, 0, level };
    uint32_t srcExtent[3] = {
      MAX(texture->info.width >> (level - 1), 1),
      MAX(texture->info.height >> (level - 1), 1),
      volumetric ? MAX(texture->info.depth >> (level - 1), 1) : 1
    };
    uint32_t dstExtent[3] = {
      MAX(texture->info.width >> level, 1),
      MAX(texture->info.height >> level, 1),
      volumetric ? MAX(texture->info.depth >> level, 1) : 1
    };
    gpu_blit(stream, texture->gpu, texture->gpu, srcOffset, dstOffset, srcExtent, dstExtent, GPU_FILTER_LINEAR);
    gpu_sync(stream, &(gpu_barrier) {
      .prev = GPU_PHASE_TRANSFER,
      .next = GPU_PHASE_TRANSFER,
      .flush = GPU_CACHE_TRANSFER_WRITE,
      .clear = GPU_CACHE_TRANSFER_READ
    }, 1);
  }
}

static ShaderResource* findShaderResource(Shader* shader, const char* name, size_t length, uint32_t slot) {
  if (name) {
    uint32_t hash = (uint32_t) hash64(name, length);
    for (uint32_t i = 0; i < shader->resourceCount; i++) {
      if (shader->resources[i].hash == hash) {
        return &shader->resources[i];
      }
    }
    lovrThrow("Shader has no variable named '%s'", name);
  } else {
    for (uint32_t i = 0; i < shader->resourceCount; i++) {
      if (shader->resources[i].binding == slot) {
        return &shader->resources[i];
      }
    }
    lovrThrow("Shader has no variable in slot '%d'", slot);
  }
}

static void trackBuffer(Pass* pass, Buffer* buffer, gpu_phase phase, gpu_cache cache) {
  if (lovrBufferIsTemporary(buffer)) {
    return; // Scratch buffers are write-only from CPU and read-only from GPU, no sync needed
  }

  Access access = {
    .buffer = buffer,
    .sync = &buffer->sync,
    .phase = phase,
    .cache = cache
  };

  arr_push(&pass->access, access);
  lovrRetain(buffer);
}

static void trackTexture(Pass* pass, Texture* texture, gpu_phase phase, gpu_cache cache) {
  if (texture == state.window) {
    return;
  }

  if (texture->info.parent) {
    texture = texture->info.parent;
  }

  if (texture->info.usage == TEXTURE_SAMPLE) {
    return; // If the texture is sample-only, no sync needed (initial upload is handled manually)
  }

  Access access = {
    .texture = texture,
    .sync = &texture->sync,
    .phase = phase,
    .cache = cache
  };

  arr_push(&pass->access, access);
  lovrRetain(texture);
}

static void updateModelTransforms(Model* model, uint32_t nodeIndex, float* parent) {
  mat4 global = model->globalTransforms + 16 * nodeIndex;
  NodeTransform* local = &model->localTransforms[nodeIndex];
  vec3 T = local->properties[PROP_TRANSLATION];
  quat R = local->properties[PROP_ROTATION];
  vec3 S = local->properties[PROP_SCALE];

  mat4_init(global, parent);
  mat4_translate(global, T[0], T[1], T[2]);
  mat4_rotateQuat(global, R);
  mat4_scale(global, S[0], S[1], S[2]);

  ModelNode* node = &model->info.data->nodes[nodeIndex];
  for (uint32_t i = 0; i < node->childCount; i++) {
    updateModelTransforms(model, node->children[i], global);
  }
}

// Only an explicit set of SPIR-V capabilities are allowed
// Some capabilities require a GPU feature to be supported
// Some common unsupported capabilities are checked directly, to provide better error messages
static void checkShaderFeatures(uint32_t* features, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    switch (features[i]) {
      case 0: break; // Matrix
      case 1: break; // Shader
      case 2: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "geometry shading");
      case 3: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "tessellation shading");
      case 5: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "linkage");
      case 9: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "half floats");
      case 10: lovrCheck(state.features.float64, "GPU does not support shader feature #%d: %s", features[i], "64 bit floats"); break;
      case 11: lovrCheck(state.features.int64, "GPU does not support shader feature #%d: %s", features[i], "64 bit integers"); break;
      case 12: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "64 bit atomics");
      case 22: lovrCheck(state.features.int16, "GPU does not support shader feature #%d: %s", features[i], "16 bit integers"); break;
      case 23: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "tessellation shading");
      case 24: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "geometry shading");
      case 25: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "extended image gather");
      case 27: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "multisample storage textures");
      case 32: lovrCheck(state.limits.clipDistances > 0, "GPU does not support shader feature #%d: %s", features[i], "clip distance"); break;
      case 33: lovrCheck(state.limits.cullDistances > 0, "GPU does not support shader feature #%d: %s", features[i], "cull distance"); break;
      case 34: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "cubemap array textures");
      case 35: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "sample rate shading");
      case 36: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "rectangle textures");
      case 37: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "rectangle textures");
      case 39: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "8 bit integers");
      case 40: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "input attachments");
      case 41: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "sparse residency");
      case 42: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "min LOD");
      case 43: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "1D textures");
      case 44: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "1D textures");
      case 45: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "cubemap array textures");
      case 46: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "texel buffers");
      case 47: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "texel buffers");
      case 48: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "multisampled storage textures");
      case 49: break; // StorageImageExtendedFormats (?)
      case 50: break; // ImageQuery
      case 51: break; // DerivativeControl
      case 52: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "sample rate shading");
      case 53: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "transform feedback");
      case 54: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "geometry shading");
      case 55: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "autoformat storage textures");
      case 56: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "autoformat storage textures");
      case 57: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "multiviewport");
      case 69: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "layered rendering");
      case 70: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "multiviewport");
      case 4427: break; // ShaderDrawParameters
      case 4437: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "multigpu");
      case 4439: lovrCheck(state.limits.renderSize[2] > 1, "GPU does not support shader feature #%d: %s", features[i], "multiview"); break;
      case 5301: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "non-uniform indexing");
      case 5306: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "non-uniform indexing");
      case 5307: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "non-uniform indexing");
      case 5308: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "non-uniform indexing");
      case 5309: lovrThrow("Shader uses unsupported feature #%d: %s", features[i], "non-uniform indexing");
      default: lovrThrow("Shader uses unknown feature #%d", features[i]);
    }
  }
}

static void onMessage(void* context, const char* message, bool severe) {
  if (severe) {
    lovrThrow("GPU error: %s", message);
  } else {
    lovrLog(LOG_DEBUG, "GPU", message);
  }
}
