#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "data/modelData.h"
#include "data/rasterizer.h"
#include "event/event.h"
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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef LOVR_USE_GLSLANG
#include "glslang_c_interface.h"
#include "resource_limits_c.h"
#endif

uint32_t os_vk_create_surface(void* instance, void** surface);
const char** os_vk_get_instance_extensions(uint32_t* count);

#define MAX_TRANSFORMS 16
#define MAX_PIPELINES 4
#define MAX_SHADER_RESOURCES 32
#define FLOAT_BITS(f) ((union { float f; uint32_t u; }) { f }).u

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
  uint32_t tick;
  Sync sync;
};

struct Texture {
  uint32_t ref;
  uint32_t xrTick;
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
  size_t layout;
  size_t computePipelineIndex;
  uint32_t workgroupSize[3];
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
  bool hasWritableTexture;
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
  VERTEX_FORMAX
} VertexFormat;

typedef struct {
  uint64_t hash;
  MeshMode mode;
  DefaultShader shader;
  Material* material;
  float* transform;
  struct {
    Buffer* buffer;
    VertexFormat format;
    uint32_t count;
    void** pointer;
  } vertex;
  struct {
    Buffer* buffer;
    uint32_t count;
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

struct Readback {
  uint32_t ref;
  uint32_t tick;
  uint32_t size;
  Readback* next;
  ReadbackInfo info;
  gpu_buffer* buffer;
  void* pointer;
  Image* image;
  Blob* blob;
  void* data;
};

struct Tally {
  uint32_t ref;
  uint32_t tick;
  TallyInfo info;
  gpu_tally* gpu;
  gpu_buffer* buffer;
};

typedef struct {
  float resolution[2];
  float time;
} Globals;

typedef struct {
  float view[16];
  float projection[16];
  float viewProjection[16];
  float inverseProjection[16];
} Camera;

typedef struct {
  bool dirty;
  MeshMode mode;
  float color[4];
  float viewport[4];
  float depthRange[2];
  uint32_t scissor[4];
  uint64_t formatHash;
  gpu_pipeline_info info;
  Material* material;
  Sampler* sampler;
  Shader* shader;
  Font* font;
} Pipeline;

typedef struct {
  struct { float x, y, z; } position;
  struct { float x, y, z; } normal;
  struct { float u, v; } uv;
} ShapeVertex;

typedef struct {
  struct { float x, y, z; } position;
  struct { float x, y, z; } normal;
  struct { float u, v; } uv;
  struct { uint8_t r, g, b, a; } color;
  struct { float x, y, z; } tangent;
} ModelVertex;

enum {
  SHAPE_PLANE,
  SHAPE_BOX,
  SHAPE_CIRCLE,
  SHAPE_SPHERE,
  SHAPE_CYLINDER,
  SHAPE_CONE,
  SHAPE_CAPSULE,
  SHAPE_TORUS,
  SHAPE_MONKEY
};

typedef struct {
  uint64_t hash;
  gpu_buffer* vertices;
  gpu_buffer* indices;
} Shape;

typedef struct {
  Sync* sync;
  Buffer* buffer;
  Texture* texture;
  gpu_phase phase;
  gpu_cache cache;
} Access;

struct Pass {
  uint32_t ref;
  uint32_t tick;
  PassInfo info;
  uint32_t width;
  uint32_t height;
  uint32_t viewCount;
  gpu_stream* stream;
  float* transform;
  uint32_t transformIndex;
  Pipeline* pipeline;
  uint32_t pipelineIndex;
  bool samplerDirty;
  bool materialDirty;
  char* constants;
  bool constantsDirty;
  gpu_binding bindings[32];
  uint32_t bindingMask;
  bool bindingsDirty;
  Camera* cameras;
  bool cameraDirty;
  DrawData* drawData;
  uint32_t drawCount;
  gpu_binding builtins[4];
  gpu_buffer* vertexBuffer;
  gpu_buffer* indexBuffer;
  Shape shapeCache[16];
  arr_t(Readback*) readbacks;
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
  gpu_texture* texture;
  uint32_t hash;
  uint32_t tick;
} ScratchTexture;

typedef struct {
  char* memory;
  size_t cursor;
  size_t length;
  size_t limit;
} Allocator;

static struct {
  bool initialized;
  bool active;
  bool presentable;
  GraphicsConfig config;
  gpu_device_info device;
  gpu_features features;
  gpu_limits limits;
  gpu_stream* stream;
  uint32_t tick;
  bool hasTextureUpload;
  bool hasMaterialUpload;
  bool hasGlyphUpload;
  bool hasReskin;
  float background[4];
  TextureFormat depthFormat;
  Texture* window;
  Pass* windowPass;
  Font* defaultFont;
  Buffer* defaultBuffer;
  Texture* defaultTexture;
  Sampler* defaultSamplers[2];
  Shader* animator;
  Shader* timeWizard;
  Shader* defaultShaders[DEFAULT_SHADER_COUNT];
  gpu_vertex_format vertexFormats[VERTEX_FORMAX];
  Readback* oldestReadback;
  Readback* newestReadback;
  Material* defaultMaterial;
  size_t materialBlock;
  arr_t(MaterialBlock) materialBlocks;
  Pass passes[63];
  uint32_t passCount;
  size_t scratchBufferIndex;
  arr_t(Buffer*) scratchBuffers;
  arr_t(gpu_buffer*) scratchBufferHandles;
  arr_t(ScratchTexture) scratchTextures;
  map_t pipelineLookup;
  arr_t(gpu_pipeline*) pipelines;
  arr_t(Layout) layouts;
  size_t builtinLayout;
  size_t materialLayout;
  Allocator allocator;
} state;

// Helpers

static void* tempAlloc(size_t size);
static size_t tempPush(void);
static void tempPop(size_t stack);
static int u64cmp(const void* a, const void* b);
static void beginFrame(void);
static void releasePassResources(void);
static void processReadbacks(void);
static size_t getLayout(gpu_slot* slots, uint32_t count);
static gpu_bundle* getBundle(size_t layout);
static gpu_texture* getScratchTexture(gpu_texture_info* info);
static bool isDepthFormat(TextureFormat format);
static uint32_t measureTexture(TextureFormat format, uint32_t w, uint32_t h, uint32_t d);
static void checkTextureBounds(const TextureInfo* info, uint32_t offset[4], uint32_t extent[3]);
static void mipmapTexture(gpu_stream* stream, Texture* texture, uint32_t base, uint32_t count);
static ShaderResource* findShaderResource(Shader* shader, const char* name, size_t length, uint32_t slot);
static void trackBuffer(Pass* pass, Buffer* buffer, gpu_phase phase, gpu_cache cache);
static void trackTexture(Pass* pass, Texture* texture, gpu_phase phase, gpu_cache cache);
static void trackMaterial(Pass* pass, Material* material, gpu_phase phase, gpu_cache cache);
static void updateModelTransforms(Model* model, uint32_t nodeIndex, float* parent);
static void checkShaderFeatures(uint32_t* features, uint32_t count);
static void onResize(uint32_t width, uint32_t height);
static void onMessage(void* context, const char* message, bool severe);

// Entry

bool lovrGraphicsInit(GraphicsConfig* config) {
  if (state.initialized) return false;

  state.config = *config;

  gpu_config gpu = {
    .debug = config->debug,
    .callback = onMessage,
    .engineName = "LOVR",
    .engineVersion = { LOVR_VERSION_MAJOR, LOVR_VERSION_MINOR, LOVR_VERSION_PATCH },
    .device = &state.device,
    .features = &state.features,
    .limits = &state.limits
  };

#ifdef LOVR_VK
  gpu.vk.cacheData = config->cacheData;
  gpu.vk.cacheSize = config->cacheSize;

  if (os_window_is_open()) {
    os_on_resize(onResize);
    gpu.vk.getInstanceExtensions = os_vk_get_instance_extensions;
    gpu.vk.createSurface = os_vk_create_surface;
    gpu.vk.surface = true;
    gpu.vk.vsync = config->vsync;
  }
#endif

#if defined LOVR_VK && !defined LOVR_DISABLE_HEADSET
  if (lovrHeadsetInterface) {
    gpu.vk.getPhysicalDevice = lovrHeadsetInterface->getVulkanPhysicalDevice;
    gpu.vk.createInstance = lovrHeadsetInterface->createVulkanInstance;
    gpu.vk.createDevice = lovrHeadsetInterface->createVulkanDevice;
    if (lovrHeadsetInterface->driverType != DRIVER_DESKTOP) {
      gpu.vk.vsync = false;
    }
  }
#endif

  if (!gpu_init(&gpu)) {
    lovrThrow("Failed to initialize GPU");
  }

  lovrAssert(state.limits.uniformBufferRange >= 65536, "LÖVR requires the GPU to support a uniform buffer range of at least 64KB");

  // Temporary frame memory uses a large 1GB virtual memory allocation, committing pages as needed
  state.allocator.length = 1 << 14;
  state.allocator.limit = 1 << 30;
  state.allocator.memory = os_vm_init(state.allocator.limit);
  os_vm_commit(state.allocator.memory, state.allocator.length);

  map_init(&state.pipelineLookup, 64);
  arr_init(&state.pipelines, realloc);
  arr_init(&state.layouts, realloc);
  arr_init(&state.materialBlocks, realloc);
  arr_init(&state.scratchBuffers, realloc);
  arr_init(&state.scratchBufferHandles, realloc);
  arr_init(&state.scratchTextures, realloc);

  for (uint32_t i = 0; i < COUNTOF(state.passes); i++) {
    arr_init(&state.passes[i].readbacks, realloc);
    arr_init(&state.passes[i].access, realloc);
  }

  gpu_slot builtinSlots[] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER, GPU_STAGE_ALL }, // Globals
    { 1, GPU_SLOT_UNIFORM_BUFFER, GPU_STAGE_ALL }, // Cameras
    { 2, GPU_SLOT_UNIFORM_BUFFER, GPU_STAGE_ALL }, // Draw data
    { 3, GPU_SLOT_SAMPLER, GPU_STAGE_ALL }, // Default sampler
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
  float* pointer = gpu_map(scratchpad, sizeof(data), 4, GPU_MAP_STAGING);
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
    .layers = 1,
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
      .wrap = { WRAP_REPEAT, WRAP_REPEAT, WRAP_REPEAT },
      .range = { 0.f, -1.f }
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
    .data.uvScale = { 1.f, 1.f },
    .data.metalness = 0.f,
    .data.roughness = 1.f,
    .data.normalScale = 1.f,
    .texture = state.defaultTexture
  });

  if (gpu.vk.surface) {
    state.window = malloc(sizeof(Texture));
    lovrAssert(state.window, "Out of memory");

    state.window->ref = 1;
    state.window->gpu = NULL;
    state.window->renderView = NULL;
    state.window->info = (TextureInfo) {
      .type = TEXTURE_2D,
      .format = GPU_FORMAT_SURFACE,
      .layers = 1,
      .mipmaps = 1,
      .samples = 1,
      .usage = TEXTURE_RENDER,
      .srgb = true
    };

    os_window_get_size(&state.window->info.width, &state.window->info.height);

    state.depthFormat = config->stencil ? FORMAT_D32FS8 : FORMAT_D32F;

    if (config->stencil && !lovrGraphicsIsFormatSupported(state.depthFormat, TEXTURE_FEATURE_RENDER)) {
      state.depthFormat = FORMAT_D24S8; // Guaranteed to be supported if the other one isn't
    }
  }

  float16Init();
  glslang_initialize_process();
  state.initialized = true;
  return true;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
#ifndef LOVR_DISABLE_HEADSET
  // If there's an active headset session it needs to be stopped so it can clean up its Pass and
  // swapchain textures before gpu_destroy is called.  This is really hacky and should be solved
  // with module-level refcounting in the future.
  if (lovrHeadsetInterface && lovrHeadsetInterface->stop) {
    lovrHeadsetInterface->stop();
  }
#endif
  for (Readback* readback = state.oldestReadback; readback; readback = readback->next) {
    lovrRelease(readback, lovrReadbackDestroy);
  }
  releasePassResources();
  for (uint32_t i = 0; i < COUNTOF(state.passes); i++) {
    arr_free(&state.passes[i].readbacks);
    arr_free(&state.passes[i].access);
  }
  lovrRelease(state.window, lovrTextureDestroy);
  lovrRelease(state.windowPass, lovrPassDestroy);
  lovrRelease(state.defaultFont, lovrFontDestroy);
  lovrRelease(state.defaultBuffer, lovrBufferDestroy);
  lovrRelease(state.defaultTexture, lovrTextureDestroy);
  lovrRelease(state.defaultSamplers[0], lovrSamplerDestroy);
  lovrRelease(state.defaultSamplers[1], lovrSamplerDestroy);
  lovrRelease(state.animator, lovrShaderDestroy);
  lovrRelease(state.timeWizard, lovrShaderDestroy);
  for (size_t i = 0; i < COUNTOF(state.defaultShaders); i++) {
    lovrRelease(state.defaultShaders[i], lovrShaderDestroy);
  }
  lovrRelease(state.defaultMaterial, lovrMaterialDestroy);
  for (size_t i = 0; i < state.materialBlocks.length; i++) {
    MaterialBlock* block = &state.materialBlocks.data[i];
    gpu_buffer_destroy(block->buffer);
    gpu_bundle_pool_destroy(block->bundlePool);
    free(block->list);
    free(block->buffer);
    free(block->bundlePool);
    free(block->bundles);
  }
  arr_free(&state.materialBlocks);
  for (size_t i = 0; i < state.scratchBuffers.length; i++) {
    free(state.scratchBuffers.data[i]);
    free(state.scratchBufferHandles.data[i]);
  }
  arr_free(&state.scratchBuffers);
  arr_free(&state.scratchBufferHandles);
  for (size_t i = 0; i < state.scratchTextures.length; i++) {
    gpu_texture_destroy(state.scratchTextures.data[i].texture);
    free(state.scratchTextures.data[i].texture);
  }
  arr_free(&state.scratchTextures);
  for (size_t i = 0; i < state.pipelines.length; i++) {
    gpu_pipeline_destroy(state.pipelines.data[i]);
    free(state.pipelines.data[i]);
  }
  map_free(&state.pipelineLookup);
  arr_free(&state.pipelines);
  for (size_t i = 0; i < state.layouts.length; i++) {
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
  os_vm_free(state.allocator.memory, state.allocator.limit);
  memset(&state, 0, sizeof(state));
}

bool lovrGraphicsIsInitialized() {
  return state.initialized;
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
  memcpy(limits->workgroupCount, state.limits.workgroupCount, 3 * sizeof(uint32_t));
  memcpy(limits->workgroupSize, state.limits.workgroupSize, 3 * sizeof(uint32_t));
  limits->totalWorkgroupSize = state.limits.totalWorkgroupSize;
  limits->computeSharedMemory = state.limits.computeSharedMemory;
  limits->shaderConstantSize = state.limits.pushConstantSize;
  limits->indirectDrawCount = state.limits.indirectDrawCount;
  limits->instances = state.limits.instances;
  limits->anisotropy = state.limits.anisotropy;
  limits->pointSize = state.limits.pointSize;
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

void lovrGraphicsGetShaderCache(void* data, size_t* size) {
  gpu_pipeline_get_cache(data, size);
}

void lovrGraphicsGetBackgroundColor(float background[4]) {
  background[0] = lovrMathLinearToGamma(state.background[0]);
  background[1] = lovrMathLinearToGamma(state.background[1]);
  background[2] = lovrMathLinearToGamma(state.background[2]);
  background[3] = state.background[3];
}

void lovrGraphicsSetBackgroundColor(float background[4]) {
  state.background[0] = lovrMathGammaToLinear(background[0]);
  state.background[1] = lovrMathGammaToLinear(background[1]);
  state.background[2] = lovrMathGammaToLinear(background[2]);
  state.background[3] = background[3];
}

void lovrGraphicsSubmit(Pass** passes, uint32_t count) {
  beginFrame();

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

  // Finish passes
  for (uint32_t i = 0; i < count; i++) {
    Pass* pass = passes[i];
    lovrAssert(passes[i]->tick == state.tick, "Trying to submit a Pass that wasn't recorded this frame");

    for (uint32_t j = 0; j < i; j++) {
      lovrCheck(passes[j] != passes[i], "Using a Pass twice in the same submit is not allowed");
    }

    streams[i + 1] = pass->stream;

    state.presentable |= pass == state.windowPass;

    switch (pass->info.type) {
      case PASS_RENDER:
        gpu_render_end(pass->stream);

        Canvas* canvas = &pass->info.canvas;

        if (canvas->mipmap) {
          bool depth = canvas->depth.texture && canvas->depth.texture->info.mipmaps > 1;

          // Waits for the external subpass dependency layout transition to finish before mipmapping
          gpu_barrier barrier = {
            .prev = GPU_PHASE_ALL,
            .next = GPU_PHASE_TRANSFER,
            .flush = 0,
            .clear = GPU_CACHE_TRANSFER_READ
          };

          gpu_sync(pass->stream, &barrier, 1);

          for (uint32_t t = 0; t < canvas->count; t++) {
            if (canvas->textures[t]->info.mipmaps > 1) {
              mipmapTexture(pass->stream, canvas->textures[t], 0, ~0u);
            }
          }

          if (depth) {
            mipmapTexture(pass->stream, canvas->depth.texture, 0, ~0u);
          }
        }
        break;
      case PASS_COMPUTE:
        gpu_compute_end(pass->stream);
        break;
      case PASS_TRANSFER:
        for (uint32_t j = 0; j < pass->readbacks.length; j++) {
          Readback* readback = pass->readbacks.data[j];

          if (!state.oldestReadback) {
            state.oldestReadback = readback;
          }

          if (state.newestReadback) {
            state.newestReadback->next = readback;
          }

          state.newestReadback = readback;
          lovrRetain(readback);
        }
        break;
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

      // Only the first write in a pass is used for inter-stream barriers
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

      uint32_t read = access->cache & GPU_CACHE_READ_MASK;
      uint32_t write = access->cache & GPU_CACHE_WRITE_MASK;
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

    // For automipmapping, the final write to the target is a transfer, not an attachment write
    if (pass->info.type == PASS_RENDER && pass->info.canvas.mipmap) {
      Canvas* canvas = &pass->info.canvas;

      for (uint32_t t = 0; t < canvas->count; t++) {
        if (canvas->textures[t]->info.mipmaps > 1) {
          canvas->textures[t]->sync.writePhase = GPU_PHASE_TRANSFER;
          canvas->textures[t]->sync.pendingWrite = GPU_CACHE_TRANSFER_WRITE;
        }
      }

      if (canvas->depth.texture && canvas->depth.texture->info.mipmaps > 1) {
        canvas->depth.texture->sync.writePhase = GPU_PHASE_TRANSFER;
        canvas->depth.texture->sync.pendingWrite = GPU_CACHE_TRANSFER_WRITE;
      }
    }
  }

  for (uint32_t i = 0; i < count; i++) {
    gpu_sync(streams[i], &barriers[i], 1);
  }

  for (uint32_t i = 0; i < count; i++) {
    for (uint32_t j = 0; j < passes[i]->access.length; j++) {
      passes[i]->access.data[j].sync->lastWriteIndex = 0;

      // OpenXR swapchain texture layout transitions >__>

      Texture* texture = passes[i]->access.data[j].texture;

      if (texture && texture->info.xr && texture->xrTick != state.tick) {
        gpu_xr_acquire(streams[0], texture->gpu);
        gpu_xr_release(streams[total - 1], texture->gpu);
        texture->xrTick = state.tick;
      }
    }
  }

  for (uint32_t i = 0; i < total; i++) {
    gpu_stream_end(streams[i]);
  }

  gpu_submit(streams, total);

  state.stream = NULL;
  state.active = false;
  releasePassResources();
}

void lovrGraphicsPresent() {
  if (state.presentable) {
    state.window->gpu = NULL;
    state.window->renderView = NULL;
    state.presentable = false;
    gpu_present();
  }
}

void lovrGraphicsWait() {
  gpu_wait_idle();
}

// Buffer

Buffer* lovrGraphicsGetBuffer(BufferInfo* info, void** data) {
  uint32_t size = info->length * info->stride;
  lovrCheck(size > 0, "Buffer size can not be zero");
  lovrCheck(size <= 1 << 30, "Max buffer size is 1GB");
  const uint32_t BUFFERS_PER_CHUNK = 64;

  if (state.scratchBufferIndex >= state.scratchBuffers.length * BUFFERS_PER_CHUNK) {
    Buffer* buffers = malloc(BUFFERS_PER_CHUNK * sizeof(Buffer));
    gpu_buffer* handles = malloc(BUFFERS_PER_CHUNK * gpu_sizeof_buffer());
    lovrAssert(buffers && handles, "Out of memory");

    for (uint32_t i = 0; i < BUFFERS_PER_CHUNK; i++) {
      buffers[i].gpu = (gpu_buffer*) ((char*) handles + gpu_sizeof_buffer() * i);
    }

    arr_push(&state.scratchBuffers, buffers);
    arr_push(&state.scratchBufferHandles, handles);
  }

  size_t index = state.scratchBufferIndex++;
  Buffer* buffer = &state.scratchBuffers.data[index / BUFFERS_PER_CHUNK][index % BUFFERS_PER_CHUNK];

  buffer->ref = 1;
  buffer->size = size;
  buffer->info = *info;
  buffer->hash = hash64(info->fields, info->fieldCount * sizeof(BufferField));

  beginFrame();
  buffer->pointer = gpu_map(buffer->gpu, size, state.limits.uniformBufferAlign, GPU_MAP_STREAM);
  buffer->tick = state.tick;

  if (data) {
    *data = buffer->pointer;
  }

  return buffer;
}

Buffer* lovrBufferCreate(const BufferInfo* info, void** data) {
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
    *data = gpu_map(scratchpad, size, 4, GPU_MAP_STAGING);
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

bool lovrBufferIsValid(Buffer* buffer) {
  return !lovrBufferIsTemporary(buffer) || buffer->tick == state.tick;
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
  if (!state.window->gpu) {
    beginFrame();

    state.window->gpu = gpu_surface_acquire();
    state.window->renderView = state.window->gpu;

    // Window texture may be unavailable during a resize
    if (!state.window->gpu) {
      return NULL;
    }
  }

  return state.window;
}

Texture* lovrTextureCreate(const TextureInfo* info) {
  uint32_t limits[] = {
    [TEXTURE_2D] = state.limits.textureSize2D,
    [TEXTURE_3D] = state.limits.textureSize3D,
    [TEXTURE_CUBE] = state.limits.textureSizeCube,
    [TEXTURE_ARRAY] = state.limits.textureSize2D
  };

  uint32_t limit = limits[info->type];
  uint32_t mipmapCap = log2(MAX(MAX(info->width, info->height), (info->type == TEXTURE_3D ? info->layers : 1))) + 1;
  uint32_t mipmaps = CLAMP(info->mipmaps, 1, mipmapCap);
  uint8_t supports = state.features.formats[info->format];

  lovrCheck(info->width > 0, "Texture width must be greater than zero");
  lovrCheck(info->height > 0, "Texture height must be greater than zero");
  lovrCheck(info->layers > 0, "Texture layer count must be greater than zero");
  lovrCheck(info->width <= limit, "Texture %s exceeds the limit for this texture type (%d)", "width", limit);
  lovrCheck(info->height <= limit, "Texture %s exceeds the limit for this texture type (%d)", "height", limit);
  lovrCheck(info->layers <= limit || info->type != TEXTURE_3D, "Texture %s exceeds the limit for this texture type (%d)", "layer count", limit);
  lovrCheck(info->layers <= state.limits.textureLayers || info->type != TEXTURE_ARRAY, "Texture %s exceeds the limit for this texture type (%d)", "layer count", limit);
  lovrCheck(info->layers == 1 || info->type != TEXTURE_2D, "2D textures must have a layer count of 1");
  lovrCheck(info->layers == 6 || info->type != TEXTURE_CUBE, "Cubemaps must have a layer count of 6");
  lovrCheck(info->width == info->height || info->type != TEXTURE_CUBE, "Cubemaps must be square");
  lovrCheck(measureTexture(info->format, info->width, info->height, info->layers) < 1 << 30, "Memory for a Texture can not exceed 1GB"); // TODO mip?
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

  beginFrame();

  if (info->imageCount > 0) {
    levelCount = lovrImageGetLevelCount(info->images[0]);
    lovrCheck(info->type != TEXTURE_3D || levelCount == 1, "Images used to initialize 3D textures can not have mipmaps");

    uint32_t total = 0;
    for (uint32_t level = 0; level < levelCount; level++) {
      levelOffsets[level] = total;
      uint32_t width = MAX(info->width >> level, 1);
      uint32_t height = MAX(info->height >> level, 1);
      levelSizes[level] = measureTexture(info->format, width, height, info->layers);
      total += levelSizes[level];
    }

    scratchpad = tempAlloc(gpu_sizeof_buffer());
    char* data = gpu_map(scratchpad, total, 64, GPU_MAP_STAGING);

    for (uint32_t level = 0; level < levelCount; level++) {
      for (uint32_t layer = 0; layer < info->layers; layer++) {
        Image* image = info->imageCount == 1 ? info->images[0] : info->images[layer];
        uint32_t slice = info->imageCount == 1 ? layer : 0;
        size_t size = lovrImageGetLayerSize(image, level);
        lovrCheck(size == levelSizes[level] / info->layers, "Texture/Image size mismatch!");
        void* pixels = lovrImageGetLayerData(image, level, slice);
        memcpy(data, pixels, size);
        data += size;
      }
    }
  }

  gpu_texture_init(texture->gpu, &(gpu_texture_info) {
    .type = (gpu_texture_type) info->type,
    .format = (gpu_texture_format) info->format,
    .size = { info->width, info->height, info->layers },
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
      .generateMipmaps = levelCount > 0 && levelCount < mipmaps
    }
  });

  // Automatically create a renderable view for renderable non-volume textures
  if ((info->usage & TEXTURE_RENDER) && info->type != TEXTURE_3D && info->layers <= state.limits.renderSize[2]) {
    if (info->mipmaps == 1) {
      texture->renderView = texture->gpu;
    } else {
      gpu_texture_view_info view = {
        .source = texture->gpu,
        .type = GPU_TEXTURE_ARRAY,
        .layerCount = info->layers,
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

Texture* lovrTextureCreateView(const TextureViewInfo* view) {
  const TextureInfo* info = &view->parent->info;
  uint32_t maxLayers = info->type == TEXTURE_3D ? MAX(info->layers >> view->levelIndex, 1) : info->layers;
  lovrCheck(!info->parent, "Can't nest texture views");
  lovrCheck(view->type != TEXTURE_3D, "Texture views may not be volume textures");
  lovrCheck(view->layerCount > 0, "Texture view must have at least one layer");
  lovrCheck(view->layerIndex + view->layerCount <= maxLayers, "Texture view layer range exceeds layer count of parent texture");
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
  texture->info.mipmaps = view->levelCount ? view->levelCount : info->mipmaps;
  texture->info.width = MAX(info->width >> view->levelIndex, 1);
  texture->info.height = MAX(info->height >> view->levelIndex, 1);
  texture->info.layers = view->layerCount;

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

Sampler* lovrSamplerCreate(const SamplerInfo* info) {
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

ShaderSource lovrGraphicsCompileShader(ShaderStage stage, ShaderSource* source) {
  uint32_t magic = 0x07230203;

  if (source->size % 4 == 0 && source->size >= 4 && !memcmp(source->code, &magic, 4)) {
    return *source;
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

  const char* strings[] = {
    prefix,
    (const char*) etc_shaders_lovr_glsl,
    "#line 1\n",
    source->code
  };

  lovrCheck(source->size <= INT_MAX, "Shader is way too big");

  int lengths[] = {
    -1,
    etc_shaders_lovr_glsl_len,
    -1,
    (int) source->size
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

  int options = 0;
  options |= GLSLANG_SHADER_AUTO_MAP_BINDINGS;

  glslang_shader_set_options(shader, options);

  if (!glslang_shader_preprocess(shader, &input)) {
    lovrThrow("Could not preprocess %s shader:\n%s", stageNames[stage], glslang_shader_get_info_log(shader));
    return (ShaderSource) { NULL, 0 };
  }

  if (!glslang_shader_parse(shader, &input)) {
    lovrThrow("Could not parse %s shader:\n%s", stageNames[stage], glslang_shader_get_info_log(shader));
    return (ShaderSource) { NULL, 0 };
  }

  glslang_program_t* program = glslang_program_create();
  glslang_program_add_shader(program, shader);

  if (!glslang_program_link(program, 0)) {
    lovrThrow("Could not link shader:\n%s", glslang_program_get_info_log(program));
    return (ShaderSource) { NULL, 0 };
  }

  glslang_program_SPIRV_generate(program, stages[stage]);

  void* words = glslang_program_SPIRV_get_ptr(program);
  size_t size = glslang_program_SPIRV_get_size(program) * 4;

  void* data = malloc(size);
  lovrAssert(data, "Out of memory");
  memcpy(data, words, size);

  glslang_program_delete(program);
  glslang_shader_delete(shader);

  return (ShaderSource) { data, size };
#else
  lovrThrow("Could not compile shader: No shader compiler available");
  return (ShaderSource) { NULL, 0 };
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
    shader->computePipelineIndex = state.pipelines.length;
    arr_push(&state.pipelines, pipeline);
  }
}

ShaderSource lovrGraphicsGetDefaultShaderSource(DefaultShader type, ShaderStage stage) {
  if (stage == STAGE_COMPUTE) {
    return (ShaderSource) { NULL, 0 };
  }

  const ShaderSource sources[][2] = {
    [SHADER_UNLIT] = {
      { lovr_shader_unlit_vert, sizeof(lovr_shader_unlit_vert) },
      { lovr_shader_unlit_frag, sizeof(lovr_shader_unlit_frag) }
    },
    [SHADER_NORMAL] = {
      { lovr_shader_unlit_vert, sizeof(lovr_shader_unlit_vert) },
      { lovr_shader_normal_frag, sizeof(lovr_shader_normal_frag) }
    },
    [SHADER_FONT] = {
      { lovr_shader_unlit_vert, sizeof(lovr_shader_unlit_vert) },
      { lovr_shader_font_frag, sizeof(lovr_shader_font_frag) }
    },
    [SHADER_CUBEMAP] = {
      { lovr_shader_cubemap_vert, sizeof(lovr_shader_cubemap_vert) },
      { lovr_shader_cubemap_frag, sizeof(lovr_shader_cubemap_frag) }
    },
    [SHADER_EQUIRECT] = {
      { lovr_shader_cubemap_vert, sizeof(lovr_shader_cubemap_vert) },
      { lovr_shader_equirect_frag, sizeof(lovr_shader_equirect_frag) }
    },
    [SHADER_FILL] = {
      { lovr_shader_fill_vert, sizeof(lovr_shader_fill_vert) },
      { lovr_shader_unlit_frag, sizeof(lovr_shader_unlit_frag) }
    },
    [SHADER_FILL_ARRAY] = {
      { lovr_shader_fill_vert, sizeof(lovr_shader_fill_vert) },
      { lovr_shader_fill_array_frag, sizeof(lovr_shader_fill_array_frag) }
    },
    [SHADER_FILL_LAYER] = {
      { lovr_shader_fill_vert, sizeof(lovr_shader_fill_vert) },
      { lovr_shader_fill_layer_frag, sizeof(lovr_shader_fill_layer_frag) }
    },
    [SHADER_LOGO] = {
      { lovr_shader_unlit_vert, sizeof(lovr_shader_unlit_vert) },
      { lovr_shader_logo_frag, sizeof(lovr_shader_logo_frag) }
    }
  };

  return sources[type][stage];
}

Shader* lovrGraphicsGetDefaultShader(DefaultShader type) {
  if (state.defaultShaders[type]) {
    return state.defaultShaders[type];
  }

  ShaderInfo info = {
    .type = SHADER_GRAPHICS,
    .source[0] = lovrGraphicsGetDefaultShaderSource(type, STAGE_VERTEX),
    .source[1] = lovrGraphicsGetDefaultShaderSource(type, STAGE_FRAGMENT)
  };

  return state.defaultShaders[type] = lovrShaderCreate(&info);
}

Shader* lovrShaderCreate(const ShaderInfo* info) {
  Shader* shader = calloc(1, sizeof(Shader) + gpu_sizeof_shader());
  lovrAssert(shader, "Out of memory");

  uint32_t stageCount = info->type == SHADER_GRAPHICS ? 2 : 1;
  uint32_t firstStage = info->type == SHADER_GRAPHICS ? GPU_STAGE_VERTEX : GPU_STAGE_COMPUTE;
  uint32_t userSet = info->type == SHADER_GRAPHICS ? 2 : 0;

  spv_result result;
  spv_info spv[2] = { 0 };
  for (uint32_t i = 0; i < stageCount; i++) {
    result = spv_parse(info->source[i].code, info->source[i].size, &spv[i]);
    lovrCheck(result == SPV_OK, "Failed to load Shader: %s\n", spv_result_to_string(result));
    lovrCheck(spv[i].version <= 0x00010300, "Invalid SPIR-V version (up to 1.3 is supported)");

    spv[i].features = tempAlloc(spv[i].featureCount * sizeof(uint32_t));
    spv[i].specConstants = tempAlloc(spv[i].specConstantCount * sizeof(spv_spec_constant));
    spv[i].pushConstants = tempAlloc(spv[i].pushConstantCount * sizeof(spv_push_constant));
    spv[i].attributes = tempAlloc(spv[i].attributeCount * sizeof(spv_attribute));
    spv[i].resources = tempAlloc(spv[i].resourceCount * sizeof(spv_resource));

    result = spv_parse(info->source[i].code, info->source[i].size, &spv[i]);
    lovrCheck(result == SPV_OK, "Failed to load Shader: %s\n", spv_result_to_string(result));

    checkShaderFeatures(spv[i].features, spv[i].featureCount);
  }

  if (info->type == SHADER_COMPUTE) {
    memcpy(shader->workgroupSize, spv[0].workgroupSize, 3 * sizeof(uint32_t));
    lovrCheck(shader->workgroupSize[0] <= state.limits.workgroupSize[0], "Shader workgroup size exceeds the 'workgroupSize' limit");
    lovrCheck(shader->workgroupSize[1] <= state.limits.workgroupSize[1], "Shader workgroup size exceeds the 'workgroupSize' limit");
    lovrCheck(shader->workgroupSize[2] <= state.limits.workgroupSize[2], "Shader workgroup size exceeds the 'workgroupSize' limit");
    uint32_t totalWorkgroupSize = shader->workgroupSize[0] * shader->workgroupSize[1] * shader->workgroupSize[2];
    lovrCheck(totalWorkgroupSize <= state.limits.totalWorkgroupSize, "Shader workgroup size exceeds the 'totalWorkgroupSize' limit");
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

  lovrCheck(shader->constantSize <= state.limits.pushConstantSize, "Shader push constants block is too big");

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

      // Flag names can start with flag_ which will be ignored for matching purposes
      if (constant->name) {
        size_t length = strlen(constant->name);
        size_t offset = length > 5 && !memcmp(constant->name, "flag_", 5) ? 5 : 0;
        shader->flagLookup[index] = (uint32_t) hash64(constant->name + offset, length - offset);
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
    .stages[0] = { info->source[0].code, info->source[0].size },
    .pushConstantSize = shader->constantSize,
    .label = info->label
  };

  if (info->source[1].code) {
    gpu.stages[1] = (gpu_shader_stage) { info->source[1].code, info->source[1].size };
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

void lovrShaderGetWorkgroupSize(Shader* shader, uint32_t size[3]) {
  memcpy(size, shader->workgroupSize, 3 * sizeof(uint32_t));
}

// Material

Material* lovrMaterialCreate(const MaterialInfo* info) {
  MaterialBlock* block = &state.materialBlocks.data[state.materialBlock];
  const uint32_t MATERIALS_PER_BLOCK = 256;

  if (!block || block->head == ~0u || !gpu_is_complete(block->list[block->head].tick)) {
    bool found = false;

    for (size_t i = 0; i < state.materialBlocks.length; i++) {
      block = &state.materialBlocks.data[i];
      if (block->head != ~0u && gpu_is_complete(block->list[block->head].tick)) {
        state.materialBlock = i;
        found = true;
        break;
      }
    }

    if (!found) {
      arr_expand(&state.materialBlocks, 1);
      lovrAssert(state.materialBlocks.length < UINT16_MAX, "Out of memory");
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
        block->list[i].block = (uint16_t) state.materialBlock;
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
    data = gpu_map(scratchpad, size, 4, GPU_MAP_STAGING);
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
    info->metalnessTexture,
    info->roughnessTexture,
    info->clearcoatTexture,
    info->occlusionTexture,
    info->normalTexture
  };

  for (uint32_t i = 0; i < COUNTOF(textures); i++) {
    lovrRetain(textures[i]);
    Texture* texture = textures[i] ? textures[i] : state.defaultTexture;
    lovrCheck(i == 0 || texture->info.type == TEXTURE_2D, "Material textures must be 2D");
    lovrCheck(texture->info.usage & TEXTURE_SAMPLE, "Textures must be created with the 'sample' usage to use them in Materials");
    bindings[i + 1] = (gpu_binding) { i + 1, GPU_SLOT_SAMPLED_TEXTURE, .texture = texture->gpu };
    material->hasWritableTexture |= texture->info.usage != TEXTURE_SAMPLE;
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
  lovrRelease(material->info.metalnessTexture, lovrTextureDestroy);
  lovrRelease(material->info.roughnessTexture, lovrTextureDestroy);
  lovrRelease(material->info.clearcoatTexture, lovrTextureDestroy);
  lovrRelease(material->info.occlusionTexture, lovrTextureDestroy);
  lovrRelease(material->info.normalTexture, lovrTextureDestroy);
}

const MaterialInfo* lovrMaterialGetInfo(Material* material) {
  return &material->info;
}

// Font

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

Font* lovrFontCreate(const FontInfo* info) {
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
  glyph->uv[1] = (uint16_t) ((float) (glyph->y + height) / font->atlasHeight * 65535.f + .5f);
  glyph->uv[2] = (uint16_t) ((float) (glyph->x + width) / font->atlasWidth * 65535.f + .5f);
  glyph->uv[3] = (uint16_t) ((float) glyph->y / font->atlasHeight * 65535.f + .5f);

  font->atlasX += pixelWidth;
  font->rowHeight = MAX(font->rowHeight, pixelHeight);

  beginFrame();

  // Atlas resize
  if (!font->atlas || font->atlasWidth > font->atlas->info.width || font->atlasHeight > font->atlas->info.height) {
    lovrCheck(font->atlasWidth <= 65536, "Font atlas is way too big!");

    Texture* atlas = lovrTextureCreate(&(TextureInfo) {
      .type = TEXTURE_2D,
      .format = FORMAT_RGBA8,
      .width = font->atlasWidth,
      .height = font->atlasHeight,
      .layers = 1,
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
        g->uv[1] = (uint16_t) ((float) (g->y + g->box[3] - g->box[1]) / font->atlasHeight * 65535.f + .5f);
        g->uv[2] = (uint16_t) ((float) (g->x + g->box[2] - g->box[0]) / font->atlasWidth * 65535.f + .5f);
        g->uv[3] = (uint16_t) ((float) g->y / font->atlasHeight * 65535.f + .5f);
      }
    }

    if (resized) *resized = true;
  }

  gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());

  size_t stack = tempPush();
  float* pixels = tempAlloc(pixelWidth * pixelHeight * 4 * sizeof(float));
  lovrRasterizerGetPixels(font->info.rasterizer, glyph->codepoint, pixels, pixelWidth, pixelHeight, font->info.spread);
  uint8_t* dst = gpu_map(scratchpad, pixelWidth * pixelHeight * 4 * sizeof(uint8_t), 4, GPU_MAP_STAGING);
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

  beginFrame();
  size_t stack = tempPush();
  char* string = tempAlloc(totalLength + 1);
  string[totalLength] = '\0';

  size_t cursor = 0;
  for (uint32_t i = 0; i < count; cursor += strings[i].length, i++) {
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

static void aline(GlyphVertex* vertices, uint32_t head, uint32_t tail, float width, HorizontalAlign align) {
  if (align == ALIGN_LEFT) return;
  float shift = align / 2.f * width;
  for (uint32_t i = head; i < tail; i++) {
    vertices[i].position.x -= shift;
  }
}

void lovrFontGetVertices(Font* font, ColoredString* strings, uint32_t count, float wrap, HorizontalAlign halign, VerticalAlign valign, GlyphVertex* vertices, uint32_t* glyphCount, uint32_t* lineCount, Material** material, bool flip) {
  uint32_t vertexCount = 0;
  uint32_t lineStart = 0;
  uint32_t wordStart = 0;
  *glyphCount = 0;
  *lineCount = 1;

  float x = 0.f;
  float y = 0.f;
  float wordStartX = 0.f;
  float prevWordEndX = 0.f;
  float leading = lovrRasterizerGetLeading(font->info.rasterizer) * font->lineSpacing;
  float space = lovrFontGetGlyph(font, ' ', NULL)->advance;

  for (uint32_t i = 0; i < count; i++) {
    size_t bytes;
    uint32_t codepoint;
    uint32_t previous = '\0';
    const char* str = strings[i].string;
    const char* end = strings[i].string + strings[i].length;
    uint8_t r = (uint8_t) (CLAMP(lovrMathGammaToLinear(strings[i].color[0]), 0.f, 1.f) * 255.f);
    uint8_t g = (uint8_t) (CLAMP(lovrMathGammaToLinear(strings[i].color[1]), 0.f, 1.f) * 255.f);
    uint8_t b = (uint8_t) (CLAMP(lovrMathGammaToLinear(strings[i].color[2]), 0.f, 1.f) * 255.f);
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
        (*lineCount)++;
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
        lovrFontGetVertices(font, strings, count, wrap, halign, valign, vertices, glyphCount, lineCount, material, flip);
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
        (*lineCount)++;
        x -= dx;
        y -= dy;
      }

      // Vertices
      float* bb = glyph->box;
      uint16_t* uv = glyph->uv;
      if (flip) {
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[0], -(y + bb[1]) }, { uv[0], uv[3] }, { r, g, b, a } };
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[2], -(y + bb[1]) }, { uv[2], uv[3] }, { r, g, b, a } };
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[0], -(y + bb[3]) }, { uv[0], uv[1] }, { r, g, b, a } };
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[2], -(y + bb[3]) }, { uv[2], uv[1] }, { r, g, b, a } };
      } else {
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[0], y + bb[3] }, { uv[0], uv[1] }, { r, g, b, a } };
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[2], y + bb[3] }, { uv[2], uv[1] }, { r, g, b, a } };
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[0], y + bb[1] }, { uv[0], uv[3] }, { r, g, b, a } };
        vertices[vertexCount++] = (GlyphVertex) { { x + bb[2], y + bb[1] }, { uv[2], uv[3] }, { r, g, b, a } };
      }
      (*glyphCount)++;

      // Advance
      x += glyph->advance;
      str += bytes;
    }
  }

  // Align last line
  aline(vertices, lineStart, vertexCount, x, halign);

  *material = font->material;
}

// Model

Model* lovrModelCreate(const ModelInfo* info) {
  ModelData* data = info->data;
  Model* model = calloc(1, sizeof(Model));
  lovrAssert(model, "Out of memory");
  model->ref = 1;
  model->info = *info;
  lovrRetain(info->data);

  // Materials and Textures
  model->textures = calloc(data->imageCount, sizeof(Texture*));
  model->materials = malloc(data->materialCount * sizeof(Material*));
  lovrAssert(model->textures && model->materials, "Out of memory");
  for (uint32_t i = 0; i < data->materialCount; i++) {
    MaterialInfo material;
    ModelMaterial* properties = &data->materials[i];
    memcpy(&material.data, properties, sizeof(MaterialData));

    struct { uint32_t index; Texture** texture; } textures[] = {
      { properties->texture, &material.texture },
      { properties->glowTexture, &material.glowTexture },
      { properties->metalnessTexture, &material.metalnessTexture },
      { properties->roughnessTexture, &material.roughnessTexture },
      { properties->clearcoatTexture, &material.clearcoatTexture },
      { properties->occlusionTexture, &material.occlusionTexture },
      { properties->normalTexture, &material.normalTexture }
    };

    for (uint32_t t = 0; t < COUNTOF(textures); t++) {
      uint32_t index = textures[t].index;
      Texture** texture = textures[t].texture;

      if (index == ~0u) {
        *texture = NULL;
      } else {
        if (!model->textures[index]) {
          model->textures[index] = lovrTextureCreate(&(TextureInfo) {
            .type = TEXTURE_2D,
            .usage = TEXTURE_SAMPLE,
            .format = lovrImageGetFormat(data->images[index]),
            .width = lovrImageGetWidth(data->images[index], 0),
            .height = lovrImageGetHeight(data->images[index], 0),
            .layers = 1,
            .mipmaps = info->mipmaps || lovrImageGetLevelCount(data->images[index]) > 1 ? ~0u : 1,
            .samples = 1,
            .srgb = texture == &material.texture || texture == &material.glowTexture,
            .images = &data->images[index],
            .imageCount = 1
          });
        }

        *texture = model->textures[index];
      }
    }

    model->materials[i] = lovrMaterialCreate(&material);
  }

  // Buffers
  char* vertices = NULL;
  char* indices = NULL;
  char* skinData = NULL;

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

  uint32_t indexSize = data->indexType == U32 ? 4 : 2;

  if (data->indexCount > 0) {
    model->indexBuffer = lovrBufferCreate(&(BufferInfo) {
      .length = data->indexCount,
      .stride = indexSize,
      .fieldCount = 1,
      .fields[0] = { 0, 0, data->indexType == U32 ? FIELD_INDEX32 : FIELD_INDEX16, 0 }
    }, (void**) &indices);
  }

  // Sort primitives by their skin, so there is a single contiguous region of skinned vertices
  size_t stack = tempPush();
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
      case DRAW_POINTS: draw->mode = MESH_POINTS; break;
      case DRAW_LINES: draw->mode = MESH_LINES; break;
      case DRAW_TRIANGLES: draw->mode = MESH_TRIANGLES; break;
      default: lovrThrow("Model uses an unsupported draw mode (lineloop, linestrip, strip, fan)");
    }

    draw->material = primitive->material == ~0u ? NULL: model->materials[primitive->material];
    draw->vertex.buffer = model->vertexBuffer;

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
    lovrModelDataCopyAttribute(data, attributes[ATTR_UV], vertices + 24, F32, 2, false, count, stride, 0);
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
  lovrModelResetNodeTransforms(model);
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

uint32_t lovrModelGetNodeDrawCount(Model* model, uint32_t node) {
  ModelData* data = model->info.data;
  return data->nodes[node].primitiveCount;
}

void lovrModelGetNodeDraw(Model* model, uint32_t node, uint32_t index, ModelDraw* mesh) {
  ModelData* data = model->info.data;
  lovrCheck(index < data->nodes[node].primitiveCount, "Invalid model node draw index %d", index + 1);
  Draw* draw = &model->draws[data->nodes[node].primitiveIndex + index];
  mesh->mode = draw->mode;
  mesh->material = draw->material;
  mesh->start = draw->start;
  mesh->count = draw->count;
  mesh->base = draw->base;
  mesh->indexed = draw->index.buffer;
}

void lovrModelResetNodeTransforms(Model* model) {
  ModelData* data = model->info.data;
  for (uint32_t i = 0; i < data->nodeCount; i++) {
    vec3 position = model->localTransforms[i].properties[PROP_TRANSLATION];
    quat orientation = model->localTransforms[i].properties[PROP_ROTATION];
    vec3 scale = model->localTransforms[i].properties[PROP_SCALE];
    if (data->nodes[i].hasMatrix) {
      mat4_getPosition(data->nodes[i].transform.matrix, position);
      mat4_getOrientation(data->nodes[i].transform.matrix, orientation);
      mat4_getScale(data->nodes[i].transform.matrix, scale);
    } else {
      vec3_init(position, data->nodes[i].transform.translation);
      quat_init(orientation, data->nodes[i].transform.rotation);
      vec3_init(scale, data->nodes[i].transform.scale);
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

void lovrModelGetNodeTransform(Model* model, uint32_t node, float position[4], float scale[4], float rotation[4], OriginType origin) {
  if (origin == ORIGIN_PARENT) {
    vec3_init(position, model->localTransforms[node].properties[PROP_TRANSLATION]);
    vec3_init(scale, model->localTransforms[node].properties[PROP_SCALE]);
    quat_init(rotation, model->localTransforms[node].properties[PROP_ROTATION]);
  } else {
    if (model->transformsDirty) {
      updateModelTransforms(model, model->info.data->rootNode, (float[]) MAT4_IDENTITY);
      model->transformsDirty = false;
    }
    mat4_getPosition(model->globalTransforms + 16 * node, position);
    mat4_getScale(model->globalTransforms + 16 * node, scale);
    mat4_getOrientation(model->globalTransforms + 16 * node, rotation);
  }
}

void lovrModelSetNodeTransform(Model* model, uint32_t node, float position[4], float scale[4], float rotation[4], float alpha) {
  if (alpha <= 0.f) return;

  NodeTransform* transform = &model->localTransforms[node];

  if (alpha >= 1.f) {
    if (position) vec3_init(transform->properties[PROP_TRANSLATION], position);
    if (scale) vec3_init(transform->properties[PROP_SCALE], scale);
    if (rotation) quat_init(transform->properties[PROP_ROTATION], rotation);
  } else {
    if (position) vec3_lerp(transform->properties[PROP_TRANSLATION], position, alpha);
    if (scale) vec3_lerp(transform->properties[PROP_SCALE], scale, alpha);
    if (rotation) quat_slerp(transform->properties[PROP_ROTATION], rotation, alpha);
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
    state.animator = lovrShaderCreate(&(ShaderInfo) {
      .type = SHADER_COMPUTE,
      .source[0] = { lovr_shader_animator_comp, sizeof(lovr_shader_animator_comp) },
      .flags = &(ShaderFlag) { NULL, 0, state.device.subgroupSize },
      .flagCount = 1,
      .label = "animator"
    });
  }

  gpu_pipeline* pipeline = state.pipelines.data[state.animator->computePipelineIndex];
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
    float* joint = gpu_map(joints, size, state.limits.uniformBufferAlign, GPU_MAP_STREAM);
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
    gpu_bind_bundles(state.stream, shader, &bundle, 0, 1, NULL, 0);
    gpu_push_constants(state.stream, shader, constants, sizeof(constants));
    gpu_compute(state.stream, (skin->vertexCount + subgroupSize - 1) / subgroupSize, 1, 1);
    gpu_compute_end(state.stream);
    baseVertex += skin->vertexCount;
  }

  model->lastReskin = state.tick;
  state.hasReskin = true;
}

// Readback

Readback* lovrReadbackCreate(const ReadbackInfo* info) {
  Readback* readback = calloc(1, sizeof(Readback) + gpu_sizeof_buffer());
  lovrAssert(readback, "Out of memory");
  readback->ref = 1;
  readback->tick = state.tick;
  readback->info = *info;
  readback->buffer = (gpu_buffer*) (readback + 1);

  switch (info->type) {
    case READBACK_BUFFER:
      lovrRetain(info->buffer.object);
      readback->size = info->buffer.extent;
      readback->data = malloc(readback->size);
      lovrAssert(readback->data, "Out of memory");
      readback->blob = lovrBlobCreate(readback->data, readback->size, "Readback");
      break;
    case READBACK_TEXTURE:
      lovrRetain(info->texture.object);
      TextureFormat format = info->texture.object->info.format;
      readback->size = measureTexture(format, info->texture.extent[0], info->texture.extent[1], 1);
      readback->image = lovrImageCreateRaw(info->texture.extent[0], info->texture.extent[1], format);
      break;
    case READBACK_TALLY:
      lovrRetain(info->tally.object);
      uint32_t stride = info->tally.object->info.type == TALLY_SHADER ? 16 : 4;
      readback->size = info->tally.count * stride;
      readback->data = malloc(readback->size);
      lovrAssert(readback->data, "Out of memory");
      readback->blob = lovrBlobCreate(readback->data, readback->size, "Readback");
      break;
  }

  readback->pointer = gpu_map(readback->buffer, readback->size, 16, GPU_MAP_READBACK);
  return readback;
}

void lovrReadbackDestroy(void* ref) {
  Readback* readback = ref;
  switch (readback->info.type) {
    case READBACK_BUFFER: lovrRelease(readback->info.buffer.object, lovrBufferDestroy); break;
    case READBACK_TEXTURE: lovrRelease(readback->info.texture.object, lovrTextureDestroy); break;
    case READBACK_TALLY: lovrRelease(readback->info.tally.object, lovrTallyDestroy); break;
  }
  lovrRelease(readback->image, lovrImageDestroy);
  lovrRelease(readback->blob, lovrBlobDestroy);
  free(readback);
}

const ReadbackInfo* lovrReadbackGetInfo(Readback* readback) {
  return &readback->info;
}

bool lovrReadbackIsComplete(Readback* readback) {
  return gpu_is_complete(readback->tick);
}

bool lovrReadbackWait(Readback* readback) {
  if ((state.tick == readback->tick && state.active) || lovrReadbackIsComplete(readback)) {
    return false;
  }

  beginFrame();

  bool waited = gpu_wait_tick(readback->tick);

  if (waited) {
    processReadbacks();
  }

  return waited;
}

void* lovrReadbackGetData(Readback* readback) {
  return lovrReadbackIsComplete(readback) ? readback->data : NULL;
}

Blob* lovrReadbackGetBlob(Readback* readback) {
  return lovrReadbackIsComplete(readback) ? readback->blob : NULL;
}

Image* lovrReadbackGetImage(Readback* readback) {
  return lovrReadbackIsComplete(readback) ? readback->image : NULL;
}

// Tally

Tally* lovrTallyCreate(const TallyInfo* info) {
  lovrCheck(info->count > 0, "Tally count must be greater than zero");
  lovrCheck(info->count <= 4096, "Maximum Tally count is 4096");
  lovrCheck(info->views <= state.limits.renderSize[2], "Tally view count can not exceed the maximum view count");
  lovrCheck(info->type != TALLY_SHADER || state.features.shaderTally, "This GPU does not support the 'shader' Tally type");
  Tally* tally = calloc(1, sizeof(Tally) + gpu_sizeof_tally());
  lovrAssert(tally, "Out of memory");
  tally->ref = 1;
  tally->tick = state.tick - 1;
  tally->info = *info;
  tally->gpu = (gpu_tally*) (tally + 1);

  uint32_t total = info->count * (info->type == TALLY_TIME ? 2 * info->views : 1);

  gpu_tally_init(tally->gpu, &(gpu_tally_info) {
    .type = (gpu_tally_type) info->type,
    .count = total
  });

  if (info->type == TALLY_TIME) {
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
  free(tally);
}

const TallyInfo* lovrTallyGetInfo(Tally* tally) {
  return &tally->info;
}

// Tally timestamps aren't very usable in their raw state, since they use unspecified units, aren't
// durations, and when using multiview there's one per view.  To make them easier to work with, copy
// them to a temporary buffer, then dispatch a compute shader to subtract pairs and convert to ns,
// writing the final friendly values to a destination Buffer.
static void lovrTallyResolve(Tally* tally, uint32_t index, uint32_t count, gpu_buffer* buffer, uint32_t offset, gpu_stream* stream) {
  gpu_copy_tally_buffer(stream, tally->gpu, tally->buffer, index, 0, count * 2, 4);

  gpu_sync(stream, &(gpu_barrier) {
    .prev = GPU_PHASE_TRANSFER,
    .next = GPU_PHASE_SHADER_COMPUTE,
    .flush = GPU_CACHE_TRANSFER_WRITE,
    .clear = GPU_CACHE_STORAGE_READ
  }, 1);

  if (!state.timeWizard) {
    state.timeWizard = lovrShaderCreate(&(ShaderInfo) {
      .type = SHADER_COMPUTE,
      .source[0] = { lovr_shader_timewizard_comp, sizeof(lovr_shader_timewizard_comp) },
      .label = "timewizard"
    });
  }

  gpu_pipeline* pipeline = state.pipelines.data[state.timeWizard->computePipelineIndex];
  gpu_layout* layout = state.layouts.data[state.timeWizard->layout].gpu;
  gpu_shader* shader = state.timeWizard->gpu;

  gpu_binding bindings[] = {
    [0] = { 0, GPU_SLOT_STORAGE_BUFFER, .buffer = { tally->buffer, 0, count * 2 * tally->info.views * sizeof(uint32_t) } },
    [1] = { 1, GPU_SLOT_STORAGE_BUFFER, .buffer = { buffer, offset, count * sizeof(uint32_t) } }
  };

  gpu_bundle* bundle = getBundle(state.timeWizard->layout);
  gpu_bundle_info bundleInfo = { layout, bindings, COUNTOF(bindings) };
  gpu_bundle_write(&bundle, &bundleInfo, 1);

  struct { uint32_t first, count, views; float period; } constants = {
    .first = index,
    .count = count,
    .views = tally->info.views,
    .period = state.limits.timestampPeriod
  };

  gpu_compute_begin(stream);
  gpu_bind_pipeline(stream, pipeline, true);
  gpu_bind_bundles(stream, shader, &bundle, 0, 1, NULL, 0);
  gpu_push_constants(stream, shader, &constants, sizeof(constants));
  gpu_compute(stream, (count + 31) / 32, 1, 1);
  gpu_compute_end(stream);
}

// Pass

static void lovrPassCheckValid(Pass* pass) {
  lovrCheck(pass->tick == state.tick, "Passes can only be used for a single frame (unable to use this Pass again because lovr.graphics.submit has been called since it was created)");
}

Pass* lovrGraphicsGetWindowPass() {
  if (!state.windowPass && state.window) {
    Texture* window = lovrGraphicsGetWindowTexture();

    // The window texture (and therefore the window pass) may become unavailable during a resize
    if (!window) {
      return NULL;
    }

    PassInfo info = {
      .type = PASS_RENDER,
      .canvas.count = 1,
      .canvas.textures[0] = window,
      .canvas.depth.format = state.depthFormat,
      .canvas.samples = state.config.antialias ? 4 : 1,
      .label = "Window"
    };

    lovrGraphicsGetBackgroundColor(info.canvas.clears[0]);

    state.windowPass = lovrGraphicsGetPass(&info);
  }

  return state.windowPass;
}

Pass* lovrGraphicsGetPass(PassInfo* info) {
  lovrCheck(state.passCount < COUNTOF(state.passes), "Too many passes, sorry... you can submit multiple smaller groups of passes");

  beginFrame();

  Pass* pass = &state.passes[state.passCount++];
  pass->ref = 1;
  pass->tick = state.tick;
  pass->info = *info;
  pass->stream = gpu_stream_begin(pass->info.label);

  pass->transformIndex = 0;
  pass->transform = tempAlloc(MAX_TRANSFORMS * 16 * sizeof(float));
  mat4_identity(pass->transform);

  pass->pipelineIndex = 0;
  pass->pipeline = tempAlloc(MAX_PIPELINES * sizeof(Pipeline));
  pass->pipeline->material = NULL;
  pass->pipeline->sampler = NULL;
  pass->pipeline->shader = NULL;
  pass->pipeline->font = NULL;
  pass->pipeline->dirty = true;

  pass->bindingMask = 0;
  pass->bindingsDirty = true;

  pass->width = 0;
  pass->height = 0;
  pass->viewCount = 0;

  arr_clear(&pass->readbacks);
  arr_clear(&pass->access);

  if (pass->info.type == PASS_TRANSFER) {
    return pass;
  }

  pass->constants = tempAlloc(state.limits.pushConstantSize);
  pass->constantsDirty = true;

  if (pass->info.type == PASS_COMPUTE) {
    gpu_compute_begin(pass->stream);
    return pass;
  }

  // Validation

  Canvas* canvas = &info->canvas;
  DepthInfo* depth = &canvas->depth;
  const TextureInfo* t = canvas->count > 0 ? &canvas->textures[0]->info : &depth->texture->info;
  lovrCheck(canvas->count > 0 || depth->texture, "Render pass must have at least one color or depth texture");
  lovrCheck(t->width <= state.limits.renderSize[0], "Render pass width (%d) exceeds the renderSize limit of this GPU (%d)", t->width, state.limits.renderSize[0]);
  lovrCheck(t->height <= state.limits.renderSize[1], "Render pass height (%d) exceeds the renderSize limit of this GPU (%d)", t->height, state.limits.renderSize[1]);
  lovrCheck(t->layers <= state.limits.renderSize[2], "Pass view count (%d) exceeds the renderSize limit of this GPU (%d)", t->layers, state.limits.renderSize[2]);
  lovrCheck(canvas->samples == 1 || canvas->samples == 4, "Render pass sample count must be 1 or 4...for now");
  lovrCheck(!canvas->mipmap || t->samples == 1, "Unable to mipmap multisampled textures");

  for (uint32_t i = 0; i < canvas->count; i++) {
    const TextureInfo* texture = &canvas->textures[i]->info;
    bool renderable = texture->format == GPU_FORMAT_SURFACE || (state.features.formats[texture->format] & GPU_FEATURE_RENDER);
    lovrCheck(!isDepthFormat(texture->format), "Unable to use a depth texture as a color target");
    lovrCheck(renderable, "This GPU does not support rendering to the texture format used by color target #%d", i + 1);
    lovrCheck(texture->usage & TEXTURE_RENDER, "Texture must be created with the 'render' flag to render to it");
    lovrCheck(texture->width == t->width, "Render pass texture sizes must match");
    lovrCheck(texture->height == t->height, "Render pass texture sizes must match");
    lovrCheck(texture->layers == t->layers, "Render pass texture layer counts must match");
    lovrCheck(texture->samples == t->samples, "Render pass texture sample counts must match");
    lovrCheck(texture->samples == 1 || texture->samples == canvas->samples, "Render pass texture sample count must be 1 or match the pass sample count");
    lovrCheck(!canvas->mipmap || texture->mipmaps == 1 || texture->usage & TEXTURE_TRANSFER, "Texture must have 'transfer' flag to mipmap it after a pass");
    lovrCheck(canvas->samples == 1 || texture->samples > 1 || canvas->loads[i] != LOAD_KEEP, "When doing multisample resolves to a texture, it must be cleared");
  }

  if (depth->texture || depth->format) {
    TextureFormat format = depth->texture ? depth->texture->info.format : depth->format;
    bool renderable = state.features.formats[format] & GPU_FEATURE_RENDER;
    lovrCheck(isDepthFormat(format), "Unable to use a color texture as a depth target");
    lovrCheck(renderable, "This GPU does not support depth buffers with this texture format");
    if (depth->texture) {
      const TextureInfo* texture = &depth->texture->info;
      lovrCheck(texture->usage & TEXTURE_RENDER, "Texture must be created with the 'render' flag to render to it");
      lovrCheck(texture->width == t->width, "Render pass texture sizes must match");
      lovrCheck(texture->height == t->height, "Render pass texture sizes must match");
      lovrCheck(texture->layers == t->layers, "Render pass texture layer counts must match");
      lovrCheck(texture->samples == canvas->samples, "Sorry, resolving depth textures is not supported yet!");
      lovrCheck(!canvas->mipmap || texture->mipmaps == 1 || texture->usage & TEXTURE_TRANSFER, "Texture must have 'transfer' flag to mipmap it after a pass");
    } else {
      lovrCheck(depth->load != LOAD_KEEP, "Must clear depth when not using a depth texture in a pass");
    }
  }

  // Render target

  pass->width = t->width;
  pass->height = t->height;
  pass->viewCount = t->layers;

  gpu_canvas target = { 0 };

  target.size[0] = pass->width;
  target.size[1] = pass->height;

  gpu_texture_info scratchTextureInfo = {
    .type = GPU_TEXTURE_ARRAY,
    .size = { t->width, t->height, t->layers },
    .mipmaps = 1,
    .samples = canvas->samples,
    .usage = GPU_TEXTURE_RENDER | GPU_TEXTURE_TRANSIENT
  };

  for (uint32_t i = 0; i < canvas->count; i++) {
    if (t->samples == 1 && canvas->samples > 1) {
      scratchTextureInfo.format = canvas->textures[i]->info.format;
      scratchTextureInfo.srgb = canvas->textures[i]->info.srgb;
      target.color[i].texture = getScratchTexture(&scratchTextureInfo);
      target.color[i].resolve = canvas->textures[i]->renderView;
    } else {
      target.color[i].texture = canvas->textures[i]->renderView;
    }

    target.color[i].load = (gpu_load_op) canvas->loads[i];
    target.color[i].save = GPU_SAVE_OP_KEEP;
    target.color[i].clear[0] = lovrMathGammaToLinear(canvas->clears[i][0]);
    target.color[i].clear[1] = lovrMathGammaToLinear(canvas->clears[i][1]);
    target.color[i].clear[2] = lovrMathGammaToLinear(canvas->clears[i][2]);
    target.color[i].clear[3] = canvas->clears[i][3];

    gpu_cache cache = GPU_CACHE_COLOR_WRITE | (canvas->loads[i] == LOAD_KEEP ? GPU_CACHE_COLOR_READ : 0);
    trackTexture(pass, canvas->textures[i], GPU_PHASE_COLOR, cache);
  }

  if (depth->texture) {
    target.depth.texture = depth->texture->renderView;
    gpu_phase phase = depth->load == LOAD_KEEP ? GPU_PHASE_DEPTH_EARLY : GPU_PHASE_DEPTH_LATE;
    gpu_cache cache = GPU_CACHE_DEPTH_WRITE | (depth->load == LOAD_KEEP ? GPU_CACHE_DEPTH_READ : 0);
    trackTexture(pass, depth->texture, phase, cache);
  } else if (depth->format) {
    scratchTextureInfo.format = depth->format;
    scratchTextureInfo.srgb = false;
    target.depth.texture = getScratchTexture(&scratchTextureInfo);
  }

  if (target.depth.texture) {
    target.depth.load = target.depth.stencilLoad = (gpu_load_op) depth->load;
    target.depth.save = target.depth.stencilSave = depth->texture ? GPU_SAVE_OP_KEEP : GPU_SAVE_OP_DISCARD;
    target.depth.clear.depth = depth->clear;
  }

  gpu_render_begin(pass->stream, &target);

  // Reset state

  float color[4] = { 1.f, 1.f, 1.f, 1.f };
  float viewport[4] = { 0.f, 0.f, (float) pass->width, (float) pass->height };
  float depthRange[2] = { 0.f, 1.f };
  uint32_t scissor[4] = { 0, 0, pass->width, pass->height };

  pass->pipeline->mode = MESH_TRIANGLES;
  memcpy(pass->pipeline->color, color, sizeof(color));
  memcpy(pass->pipeline->viewport, viewport, sizeof(viewport));
  memcpy(pass->pipeline->depthRange, depthRange, sizeof(depthRange));
  memcpy(pass->pipeline->scissor, scissor, sizeof(scissor));
  pass->pipeline->formatHash = 0;

  pass->pipeline->info = (gpu_pipeline_info) {
    .attachmentCount = canvas->count,
    .multisample.count = canvas->samples,
    .viewCount = pass->viewCount,
    .depth.format = depth->texture ? depth->texture->info.format : depth->format,
    .depth.test = GPU_COMPARE_GEQUAL,
    .depth.write = true,
    .stencil.testMask = 0xff,
    .stencil.writeMask = 0xff
  };

  for (uint32_t i = 0; i < pass->info.canvas.count; i++) {
    pass->pipeline->info.color[i].format = canvas->textures[i]->info.format;
    pass->pipeline->info.color[i].srgb = canvas->textures[i]->info.srgb;
    pass->pipeline->info.color[i].mask = 0xf;
  }

  lovrPassSetBlendMode(pass, BLEND_ALPHA, BLEND_ALPHA_MULTIPLY);

  pass->materialDirty = true;
  pass->samplerDirty = true;

  pass->cameras = tempAlloc(pass->viewCount * sizeof(Camera));
  pass->cameraDirty = true;

  float aspect = (float) pass->width / pass->height;

  for (uint32_t i = 0; i < pass->viewCount; i++) {
    mat4_identity(pass->cameras[i].view);
    mat4_perspective(pass->cameras[i].projection, 1.f / aspect, aspect, .01f, 0.f);
  }

  gpu_buffer_binding globals = { tempAlloc(gpu_sizeof_buffer()), 0, sizeof(Globals) };
  gpu_buffer_binding cameras = { tempAlloc(gpu_sizeof_buffer()), 0, pass->viewCount * sizeof(Camera) };
  gpu_buffer_binding draws = { tempAlloc(gpu_sizeof_buffer()), 0, 256 * sizeof(DrawData) };
  pass->drawCount = 0;

  pass->builtins[0] = (gpu_binding) { 0, GPU_SLOT_UNIFORM_BUFFER, .buffer = globals };
  pass->builtins[1] = (gpu_binding) { 1, GPU_SLOT_UNIFORM_BUFFER, .buffer = cameras };
  pass->builtins[2] = (gpu_binding) { 2, GPU_SLOT_UNIFORM_BUFFER, .buffer = draws };
  pass->builtins[3] = (gpu_binding) { 3, GPU_SLOT_SAMPLER, .sampler = NULL };

  Globals* global = gpu_map(pass->builtins[0].buffer.object, sizeof(Globals), state.limits.uniformBufferAlign, GPU_MAP_STREAM);

  global->resolution[0] = pass->width;
  global->resolution[1] = pass->height;

#ifndef LOVR_DISABLE_HEADSET
  global->time = lovrHeadsetInterface ? lovrHeadsetInterface->getDisplayTime() : os_get_time();
#else
  global->time = os_get_time();
#endif

  pass->vertexBuffer = NULL;
  pass->indexBuffer = NULL;

  memset(pass->shapeCache, 0, sizeof(pass->shapeCache));

  lovrPassSetViewport(pass, pass->pipeline->viewport, pass->pipeline->depthRange);
  lovrPassSetScissor(pass, pass->pipeline->scissor);

  // The default vertex buffer is always in the second slot, used for default attribute values
  gpu_buffer* buffers[] = { state.defaultBuffer->gpu, state.defaultBuffer->gpu };
  gpu_bind_vertex_buffers(pass->stream, buffers, NULL, 0, 2);

  return pass;
}

void lovrPassDestroy(void* ref) {
  //
}

const PassInfo* lovrPassGetInfo(Pass* pass) {
  return &pass->info;
}

uint32_t lovrPassGetWidth(Pass* pass) {
  return pass->width;
}

uint32_t lovrPassGetHeight(Pass* pass) {
  return pass->height;
}

uint32_t lovrPassGetViewCount(Pass* pass) {
  return pass->viewCount;
}

uint32_t lovrPassGetSampleCount(Pass* pass) {
  return pass->info.canvas.samples;
}

void lovrPassGetTarget(Pass* pass, Texture* color[4], Texture** depth, uint32_t* count) {
  memcpy(color, pass->info.canvas.textures, pass->info.canvas.count * sizeof(Texture*));
  *depth = pass->info.canvas.depth.texture;
  *count = pass->info.canvas.count;
}

void lovrPassGetClear(Pass* pass, float color[4][4], float* depth, uint8_t* stencil, uint32_t* count) {
  for (uint32_t i = 0; i < pass->info.canvas.count; i++) {
    color[i][0] = lovrMathLinearToGamma(pass->info.canvas.clears[i][0]);
    color[i][1] = lovrMathLinearToGamma(pass->info.canvas.clears[i][1]);
    color[i][2] = lovrMathLinearToGamma(pass->info.canvas.clears[i][2]);
    color[i][3] = pass->info.canvas.clears[i][3];
  }
  *depth = pass->info.canvas.depth.clear;
  *stencil = 0;
  *count = pass->info.canvas.count;
}

void lovrPassReset(Pass* pass) {
}

void lovrPassGetViewMatrix(Pass* pass, uint32_t index, float viewMatrix[16]) {
  lovrCheck(index < pass->viewCount, "Trying to use view '%d', but Pass view count is %d", index + 1, pass->viewCount);
  mat4_init(viewMatrix, pass->cameras[index].view);
}

void lovrPassSetViewMatrix(Pass* pass, uint32_t index, float viewMatrix[16]) {
  lovrCheck(index < pass->viewCount, "Trying to use view '%d', but Pass view count is %d", index + 1, pass->viewCount);
  mat4_init(pass->cameras[index].view, viewMatrix);
  pass->cameraDirty = true;
}

void lovrPassGetProjection(Pass* pass, uint32_t index, float projection[16]) {
  lovrCheck(index < pass->viewCount, "Trying to use view '%d', but Pass view count is %d", index + 1, pass->viewCount);
  mat4_init(projection, pass->cameras[index].projection);
}

void lovrPassSetProjection(Pass* pass, uint32_t index, float projection[16]) {
  lovrCheck(index < pass->viewCount, "Trying to use view '%d', but Pass view count is %d", index + 1, pass->viewCount);
  mat4_init(pass->cameras[index].projection, projection);
  pass->cameraDirty = true;

  // If the handedness of the projection changes, flip the winding
  if (index == 0 && ((projection[5] > 0.f) != (pass->cameras[0].projection[5] > 0.f))) {
    pass->pipeline->info.rasterizer.winding = !pass->pipeline->info.rasterizer.winding;
    pass->pipeline->dirty = true;
  }
}

void lovrPassPush(Pass* pass, StackType stack) {
  switch (stack) {
    case STACK_TRANSFORM:
      lovrCheck(++pass->transformIndex < MAX_TRANSFORMS, "%s stack overflow (more pushes than pops?)", "Transform");
      mat4_init(pass->transform + 16, pass->transform);
      pass->transform += 16;
      break;
    case STACK_STATE:
      lovrCheck(++pass->pipelineIndex < MAX_PIPELINES, "%s stack overflow (more pushes than pops?)", "Pipeline");
      memcpy(pass->pipeline + 1, pass->pipeline, sizeof(Pipeline));
      pass->pipeline++;
      lovrRetain(pass->pipeline->font);
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
      lovrCheck(--pass->transformIndex < MAX_TRANSFORMS, "%s stack underflow (more pops than pushes?)", "Transform");
      pass->transform -= 16;
      break;
    case STACK_STATE:
      lovrRelease(pass->pipeline->font, lovrFontDestroy);
      lovrRelease(pass->pipeline->sampler, lovrSamplerDestroy);
      lovrRelease(pass->pipeline->shader, lovrShaderDestroy);
      lovrRelease(pass->pipeline->material, lovrMaterialDestroy);
      lovrCheck(--pass->pipelineIndex < MAX_PIPELINES, "%s stack underflow (more pops than pushes?)", "Pipeline");
      pass->pipeline--;
      lovrPassSetViewport(pass, pass->pipeline->viewport, pass->pipeline->depthRange);
      lovrPassSetScissor(pass, pass->pipeline->scissor);
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

void lovrPassSetFont(Pass* pass, Font* font) {
  if (pass->pipeline->font != font) {
    lovrRetain(font);
    lovrRelease(pass->pipeline->font, lovrFontDestroy);
    pass->pipeline->font = font;
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

void lovrPassSetMeshMode(Pass* pass, MeshMode mode) {
  pass->pipeline->mode = mode;
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
  if (pass->info.type == PASS_RENDER) gpu_set_scissor(pass->stream, scissor);
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

  // If shaders have different push constant ranges, descriptor sets need to be rebound
  if ((shader ? shader->constantSize : 0) != (previous ? previous->constantSize : 0)) {
    pass->materialDirty = true;
    pass->samplerDirty = true;
  }
}

void lovrPassSetStencilTest(Pass* pass, CompareMode test, uint8_t value, uint8_t mask) {
  TextureFormat depthFormat = pass->info.canvas.depth.texture ? pass->info.canvas.depth.texture->info.format : pass->info.canvas.depth.format;
  lovrCheck(depthFormat == FORMAT_D32FS8 || depthFormat == FORMAT_D24S8, "Trying to set stencil test when no stencil buffer exists");
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
  TextureFormat depthFormat = pass->info.canvas.depth.texture ? pass->info.canvas.depth.texture->info.format : pass->info.canvas.depth.format;
  lovrCheck(depthFormat == FORMAT_D32FS8 || depthFormat == FORMAT_D24S8, "Trying to write to the stencil buffer when no stencil buffer exists");
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

void lovrPassSetViewport(Pass* pass, float viewport[4], float depthRange[2]) {
  if (pass->info.type == PASS_RENDER) gpu_set_viewport(pass->stream, viewport, depthRange);
  memcpy(pass->pipeline->viewport, viewport, 4 * sizeof(float));
  memcpy(pass->pipeline->depthRange, depthRange, 2 * sizeof(float));
}

void lovrPassSetWinding(Pass* pass, Winding winding) {
  if (pass->viewCount > 0 && pass->cameras[0].projection[5] > 0.f) { // Handedness change needs winding flip
    winding = !winding;
  }

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

static void bindPipeline(Pass* pass, Draw* draw, Shader* shader) {
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

  // Vertex formats
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
        lovrCheck(field.type < FIELD_MAT2, "Currently, matrix and index types can not be used in vertex buffers");
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
  } else if (!draw->vertex.buffer && pipeline->formatHash != 1 + draw->vertex.format) {
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
  pipeline->dirty = false;
}

static void bindBundles(Pass* pass, Draw* draw, Shader* shader) {
  size_t stack = tempPush();

  gpu_bundle* bundles[3];
  uint32_t bundleMask = 0;

  // Set 0 - Builtins
  if (pass->info.type == PASS_RENDER) {
    bool builtinsDirty = false;

    if (pass->cameraDirty) {
      for (uint32_t i = 0; i < pass->viewCount; i++) {
        mat4_init(pass->cameras[i].viewProjection, pass->cameras[i].projection);
        mat4_init(pass->cameras[i].inverseProjection, pass->cameras[i].projection);
        mat4_mul(pass->cameras[i].viewProjection, pass->cameras[i].view);
        mat4_invert(pass->cameras[i].inverseProjection);
      }

      uint32_t size = pass->viewCount * sizeof(Camera);
      void* data = gpu_map(pass->builtins[1].buffer.object, size, state.limits.uniformBufferAlign, GPU_MAP_STREAM);
      memcpy(data, pass->cameras, size);
      pass->cameraDirty = false;
      builtinsDirty = true;
    }

    if (pass->drawCount % 256 == 0) {
      uint32_t size = 256 * sizeof(DrawData);
      pass->drawData = gpu_map(pass->builtins[2].buffer.object, size, state.limits.uniformBufferAlign, GPU_MAP_STREAM);
      builtinsDirty = true;
    }

    if (pass->samplerDirty) {
      Sampler* sampler = pass->pipeline->sampler ? pass->pipeline->sampler : state.defaultSamplers[FILTER_LINEAR];
      pass->builtins[3].sampler = sampler->gpu;
      pass->samplerDirty = false;
      builtinsDirty = true;
    }

    if (builtinsDirty) {
      gpu_bundle_info bundleInfo = {
        .layout = state.layouts.data[state.builtinLayout].gpu,
        .bindings = pass->builtins,
        .count = COUNTOF(pass->builtins)
      };

      bundles[0] = getBundle(state.builtinLayout);
      gpu_bundle_write(&bundles[0], &bundleInfo, 1);
      bundleMask |= (1 << 0);
    }

    // Draw data

    float m[16];
    float* transform;
    if (draw->transform) {
      transform = mat4_mul(mat4_init(m, pass->transform), draw->transform);
    } else {
      transform = pass->transform;
    }

    float cofactor[16];
    mat4_init(cofactor, transform);
    cofactor[12] = 0.f;
    cofactor[13] = 0.f;
    cofactor[14] = 0.f;
    cofactor[15] = 1.f;
    mat4_cofactor(cofactor);

    memcpy(pass->drawData->transform, transform, 64);
    memcpy(pass->drawData->cofactor, cofactor, 64);
    memcpy(pass->drawData->color, pass->pipeline->color, 16);
    pass->drawData++;
  }

  // Set 1 - Material
  if (pass->info.type == PASS_RENDER) {
    if (draw->material && draw->material != pass->pipeline->material) {
      trackMaterial(pass, draw->material, GPU_PHASE_SHADER_VERTEX | GPU_PHASE_SHADER_FRAGMENT, GPU_CACHE_TEXTURE);
      pass->materialDirty = true;
      bundles[1] = draw->material->bundle;
      bundleMask |= (1 << 1);
    } else if (pass->materialDirty) {
      Material* material = pass->pipeline->material ? pass->pipeline->material : state.defaultMaterial;
      trackMaterial(pass, material, GPU_PHASE_SHADER_VERTEX | GPU_PHASE_SHADER_FRAGMENT, GPU_CACHE_TEXTURE);
      pass->materialDirty = false;
      bundles[1] = material->bundle;
      bundleMask |= (1 << 1);
    } else {
      bundles[1] = (pass->pipeline->material ? pass->pipeline->material : state.defaultMaterial)->bundle;
    }
  }

  // Set 2 - Resources
  if (pass->bindingsDirty && shader->resourceCount > 0) {
    gpu_binding* bindings = tempAlloc(shader->resourceCount * sizeof(gpu_binding));

    for (uint32_t i = 0; i < shader->resourceCount; i++) {
      bindings[i] = pass->bindings[shader->resources[i].binding];
      bindings[i].type = shader->resources[i].type;
    }

    gpu_bundle_info info = {
      .layout = state.layouts.data[shader->layout].gpu,
      .bindings = bindings,
      .count = shader->resourceCount
    };

    gpu_bundle* bundle = getBundle(shader->layout);
    gpu_bundle_write(&bundle, &info, 1);
    pass->bindingsDirty = false;

    uint32_t set = pass->info.type == PASS_RENDER ? 2 : 0;
    bundleMask |= (1 << set);
    bundles[set] = bundle;
  }

  // Bind
  if (bundleMask) {
    uint32_t first = 0;
    while (~bundleMask & 0x1) {
      bundleMask >>= 1;
      first++;
    }

    uint32_t count = 0;
    while (bundleMask) {
      bundleMask >>= 1;
      count++;
    }

    gpu_bind_bundles(pass->stream, shader->gpu, bundles + first, first, count, NULL, 0);
  }

  tempPop(stack);
}

static void bindBuffers(Pass* pass, Draw* draw) {
  Shape* cache = NULL;

  if (draw->hash) {
    cache = &pass->shapeCache[draw->hash & (COUNTOF(pass->shapeCache) - 1)];
    if (cache->hash == draw->hash) {
      if (pass->vertexBuffer != cache->vertices) {
        gpu_bind_vertex_buffers(pass->stream, &cache->vertices, NULL, 0, 1);
        pass->vertexBuffer = cache->vertices;
      }

      if (pass->indexBuffer != cache->indices) {
        gpu_bind_index_buffer(pass->stream, cache->indices, 0, GPU_INDEX_U16);
        pass->indexBuffer = cache->indices;
      }

      *draw->vertex.pointer = NULL;
      *draw->index.pointer = NULL;
      return;
    }
  }

  if (!draw->vertex.buffer && draw->vertex.count > 0) {
    lovrCheck(draw->vertex.count < UINT16_MAX, "This draw has too many vertices (max is 65534), try splitting it up into multiple draws or using a Buffer");
    uint32_t stride = state.vertexFormats[draw->vertex.format].bufferStrides[0];
    uint32_t size = draw->vertex.count * stride;

    gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
    *draw->vertex.pointer = gpu_map(scratchpad, size, stride, GPU_MAP_STREAM);

    gpu_bind_vertex_buffers(pass->stream, &scratchpad, NULL, 0, 1);
    pass->vertexBuffer = scratchpad;
  } else if (draw->vertex.buffer && draw->vertex.buffer->gpu != pass->vertexBuffer) {
    lovrCheck(draw->vertex.buffer->info.stride <= state.limits.vertexBufferStride, "Vertex buffer stride exceeds vertexBufferStride limit");
    gpu_bind_vertex_buffers(pass->stream, &draw->vertex.buffer->gpu, NULL, 0, 1);
    pass->vertexBuffer = draw->vertex.buffer->gpu;
    trackBuffer(pass, draw->vertex.buffer, GPU_PHASE_INPUT_VERTEX, GPU_CACHE_VERTEX);
  }

  if (!draw->index.buffer && draw->index.count > 0) {
    uint32_t size = draw->index.count * sizeof(uint16_t);

    gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
    *draw->index.pointer = gpu_map(scratchpad, size, sizeof(uint16_t), GPU_MAP_STREAM);

    gpu_bind_index_buffer(pass->stream, scratchpad, 0, GPU_INDEX_U16);
    pass->indexBuffer = scratchpad;
  } else if (draw->index.buffer && draw->index.buffer->gpu != pass->indexBuffer) {
    gpu_index_type type = draw->index.buffer->info.stride == 4 ? GPU_INDEX_U32 : GPU_INDEX_U16;
    gpu_bind_index_buffer(pass->stream, draw->index.buffer->gpu, 0, type);
    pass->indexBuffer = draw->index.buffer->gpu;
    trackBuffer(pass, draw->index.buffer, GPU_PHASE_INPUT_INDEX, GPU_CACHE_INDEX);
  }

  if (cache) {
    cache->hash = draw->hash;
    cache->vertices = pass->vertexBuffer;
    cache->indices = pass->indexBuffer;
  }
}

static void pushConstants(Pass* pass, Shader* shader) {
  if (pass->constantsDirty && shader->constantSize > 0) {
    gpu_push_constants(pass->stream, shader->gpu, pass->constants, shader->constantSize);
    pass->constantsDirty = false;
  }
}

static void lovrPassDraw(Pass* pass, Draw* draw) {
  lovrPassCheckValid(pass);
  lovrCheck(pass->info.type == PASS_RENDER, "This function can only be called on a render pass");
  Shader* shader = pass->pipeline->shader ? pass->pipeline->shader : lovrGraphicsGetDefaultShader(draw->shader);

  bindPipeline(pass, draw, shader);
  bindBundles(pass, draw, shader);
  bindBuffers(pass, draw);
  pushConstants(pass, shader);

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
    .mode = MESH_POINTS,
    .vertex.format = VERTEX_POINT,
    .vertex.pointer = (void**) points,
    .vertex.count = count
  });
}

void lovrPassLine(Pass* pass, uint32_t count, float** points) {
  lovrCheck(count >= 2, "Need at least 2 points to make a line");

  uint16_t* indices;

  lovrPassDraw(pass, &(Draw) {
    .mode = MESH_LINES,
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
  uint32_t key[] = { SHAPE_PLANE, style, cols, rows };
  ShapeVertex* vertices;
  uint16_t* indices;

  uint32_t vertexCount = (cols + 1) * (rows + 1);
  uint32_t indexCount;

  if (style == STYLE_LINE) {
    indexCount = 2 * (rows + 1) + 2 * (cols + 1);

    lovrPassDraw(pass, &(Draw) {
      .hash = hash64(key, sizeof(key)),
      .mode = MESH_LINES,
      .transform = transform,
      .vertex.pointer = (void**) &vertices,
      .vertex.count = vertexCount,
      .index.pointer = (void**) &indices,
      .index.count = indexCount
    });
  } else {
    indexCount = (cols * rows) * 6;

    lovrPassDraw(pass, &(Draw) {
      .hash = hash64(key, sizeof(key)),
      .mode = MESH_TRIANGLES,
      .transform = transform,
      .vertex.pointer = (void**) &vertices,
      .vertex.count = vertexCount,
      .index.pointer = (void**) &indices,
      .index.count = indexCount
    });
  }

  if (!vertices) {
    return;
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
  uint32_t key[] = { SHAPE_BOX, style };
  ShapeVertex* vertices;
  uint16_t* indices;

  if (style == STYLE_LINE) {
    static ShapeVertex vertexData[] = {
      { { -.5f,  .5f, -.5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } }, // Front
      { {  .5f,  .5f, -.5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { {  .5f, -.5f, -.5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { { -.5f, -.5f, -.5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { { -.5f,  .5f,  .5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } }, // Back
      { {  .5f,  .5f,  .5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { {  .5f, -.5f,  .5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
      { { -.5f, -.5f,  .5f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f } }
    };

    static uint16_t indexData[] = {
      0, 1, 1, 2, 2, 3, 3, 0, // Front
      4, 5, 5, 6, 6, 7, 7, 4, // Back
      0, 4, 1, 5, 2, 6, 3, 7  // Connections
    };

    lovrPassDraw(pass, &(Draw) {
      .hash = hash64(key, sizeof(key)),
      .mode = MESH_LINES,
      .transform = transform,
      .vertex.pointer = (void**) &vertices,
      .vertex.count = COUNTOF(vertexData),
      .index.pointer = (void**) &indices,
      .index.count = COUNTOF(indexData)
    });

    if (vertices) {
      memcpy(vertices, vertexData, sizeof(vertexData));
      memcpy(indices, indexData, sizeof(indexData));
    }
  } else {
    static ShapeVertex vertexData[] = {
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

    static uint16_t indexData[] = {
      0,  1,   2,  2,  1,  3,
      4,  5,   6,  6,  5,  7,
      8,  9,  10, 10,  9, 11,
      12, 13, 14, 14, 13, 15,
      16, 17, 18, 18, 17, 19,
      20, 21, 22, 22, 21, 23
    };

    lovrPassDraw(pass, &(Draw) {
      .hash = hash64(key, sizeof(key)),
      .mode = MESH_TRIANGLES,
      .transform = transform,
      .vertex.pointer = (void**) &vertices,
      .vertex.count = COUNTOF(vertexData),
      .index.pointer = (void**) &indices,
      .index.count = COUNTOF(indexData)
    });

    if (vertices) {
      memcpy(vertices, vertexData, sizeof(vertexData));
      memcpy(indices, indexData, sizeof(indexData));
    }
  }
}

void lovrPassCircle(Pass* pass, float* transform, DrawStyle style, float angle1, float angle2, uint32_t segments) {
  if (fabsf(angle1 - angle2) >= 2.f * (float) M_PI) {
    angle1 = 0.f;
    angle2 = 2.f * (float) M_PI;
  }

  uint32_t key[] = { SHAPE_CIRCLE, style, FLOAT_BITS(angle1), FLOAT_BITS(angle2), segments };
  ShapeVertex* vertices;
  uint16_t* indices;

  if (style == STYLE_LINE) {
    uint32_t vertexCount = segments + 1;
    uint32_t indexCount = segments * 2;

    lovrPassDraw(pass, &(Draw) {
      .hash = hash64(key, sizeof(key)),
      .mode = MESH_LINES,
      .transform = transform,
      .vertex.pointer = (void**) &vertices,
      .vertex.count = vertexCount,
      .index.pointer = (void**) &indices,
      .index.count = indexCount
    });

    if (!vertices) {
      return;
    }
  } else {
    uint32_t vertexCount = segments + 2;
    uint32_t indexCount = segments * 3;

    lovrPassDraw(pass, &(Draw) {
      .hash = hash64(key, sizeof(key)),
      .mode = MESH_TRIANGLES,
      .transform = transform,
      .vertex.pointer = (void**) &vertices,
      .vertex.count = vertexCount,
      .index.pointer = (void**) &indices,
      .index.count = indexCount
    });

    if (!vertices) {
      return;
    }

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

  uint32_t key[] = { SHAPE_SPHERE, segmentsH, segmentsV };

  lovrPassDraw(pass, &(Draw) {
    .hash = hash64(key, sizeof(key)),
    .mode = MESH_TRIANGLES,
    .transform = transform,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount,
  });

  if (!vertices) {
    return;
  }

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

  uint32_t key[] = { SHAPE_CYLINDER, capped, FLOAT_BITS(angle1), FLOAT_BITS(angle2), segments };

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
    .hash = hash64(key, sizeof(key)),
    .mode = MESH_TRIANGLES,
    .transform = transform,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  });

  if (!vertices) {
    return;
  }

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
    uint16_t quad[] = { a, c, b, b, c, d };
    memcpy(indices, quad, sizeof(quad));
    indices += COUNTOF(quad);
  }

  if (capped) {
    // Cap centers
    *vertices++ = (ShapeVertex) { { 0.f, 0.f, -.5f }, { 0.f, 0.f, -1.f }, { .5f, .5f } };
    *vertices++ = (ShapeVertex) { { 0.f, 0.f,  .5f }, { 0.f, 0.f,  1.f }, { .5f, .5f } };

    // Caps
    for (uint32_t i = 0; i <= segments; i++) {
      float theta = angle1 + i * angleShift;
      float x = cosf(theta);
      float y = sinf(theta);
      *vertices++ = (ShapeVertex) { { x, y, -.5f }, { 0.f, 0.f, -1.f }, { x + .5f, y - .5f } };
      *vertices++ = (ShapeVertex) { { x, y,  .5f }, { 0.f, 0.f,  1.f }, { x + .5f, y - .5f } };
    }

    // Cap wedges
    uint16_t base = 2 * (segments + 1);
    for (uint32_t i = 0; i < segments; i++) {
      uint16_t a = base + 0;
      uint16_t b = base + (i + 1) * 2;
      uint16_t c = base + (i + 2) * 2;
      uint16_t wedge1[] = { a + 0, c + 0, b + 0 };
      uint16_t wedge2[] = { a + 1, b + 1, c + 1 };
      memcpy(indices + 0, wedge1, sizeof(wedge1));
      memcpy(indices + 3, wedge2, sizeof(wedge2));
      indices += 6;
    }
  }
}

void lovrPassCone(Pass* pass, float* transform, uint32_t segments) {
  uint32_t key[] = { SHAPE_CONE, segments };
  uint32_t vertexCount = 2 * segments + 1;
  uint32_t indexCount = 3 * (segments - 2) + 3 * segments;
  ShapeVertex* vertices;
  uint16_t* indices;

  lovrPassDraw(pass, &(Draw) {
    .hash = hash64(key, sizeof(key)),
    .mode = MESH_TRIANGLES,
    .transform = transform,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  });

  if (!vertices) {
    return;
  }

  for (uint32_t i = 0; i < segments; i++) {
    float theta = i * 2.f * (float) M_PI / segments;
    float x = cosf(theta);
    float y = sinf(theta);
    float rsqrt3 = .57735f;
    float nx = cosf(theta) * rsqrt3;
    float ny = sinf(theta) * rsqrt3;
    float nz = -rsqrt3;
    float u = x + .5f;
    float v = .5f - y;
    vertices[segments * 0] = (ShapeVertex) { { x, y, 0.f }, { 0.f, 0.f, 1.f }, { u, v } };
    vertices[segments * 1] = (ShapeVertex) { { x, y, 0.f }, { nx, ny, nz }, { u, v } };
    vertices++;
  }

  vertices[segments] = (ShapeVertex) { { 0.f, 0.f, -1.f }, { 0.f, 0.f, 0.f }, { .5f, .5f } };

  // Base
  for (uint32_t i = 0; i < segments - 2; i++) {
    uint16_t tri[] = { 0, i + 1, i + 2 };
    memcpy(indices, tri, sizeof(tri));
    indices += COUNTOF(tri);
  }

  // Sides
  for (uint32_t i = 0; i < segments; i++) {
    uint16_t tri[] = { segments + (i + 1) % segments, segments + i, vertexCount - 1 };
    memcpy(indices, tri, sizeof(tri));
    indices += COUNTOF(tri);
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

  uint32_t key[] = { SHAPE_CAPSULE, FLOAT_BITS(radius), FLOAT_BITS(length), segments };

  uint32_t rings = segments / 2;
  uint32_t vertexCount = 2 * (1 + rings * (segments + 1));
  uint32_t indexCount = 2 * (3 * segments + 6 * segments * (rings - 1)) + 6 * segments;
  ShapeVertex* vertices;
  uint16_t* indices;

  lovrPassDraw(pass, &(Draw) {
    .hash = hash64(key, sizeof(key)),
    .mode = MESH_TRIANGLES,
    .transform = transform,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  });

  if (!vertices) {
    return;
  }

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

  uint32_t key[] = { SHAPE_TORUS, FLOAT_BITS(radius), FLOAT_BITS(thickness), segmentsT, segmentsP };
  uint32_t vertexCount = segmentsT * segmentsP;
  uint32_t indexCount = segmentsT * segmentsP * 6;
  ShapeVertex* vertices;
  uint16_t* indices;

  lovrPassDraw(pass, &(Draw) {
    .hash = hash64(key, sizeof(key)),
    .mode = MESH_TRIANGLES,
    .transform = transform,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  });

  if (!vertices) {
    return;
  }

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
      uint16_t quad[] = { a, b, c, c, b, d };
      memcpy(indices, quad, sizeof(quad));
      indices += COUNTOF(quad);
    }
  }
}

void lovrPassText(Pass* pass, ColoredString* strings, uint32_t count, float* transform, float wrap, HorizontalAlign halign, VerticalAlign valign) {
  Font* font = pass->pipeline->font ? pass->pipeline->font : lovrGraphicsGetDefaultFont();

  size_t totalLength = 0;
  for (uint32_t i = 0; i < count; i++) {
    totalLength += strings[i].length;
  }

  size_t stack = tempPush();
  GlyphVertex* vertices = tempAlloc(totalLength * 4 * sizeof(GlyphVertex));
  uint32_t glyphCount;
  uint32_t lineCount;

  float leading = lovrRasterizerGetLeading(font->info.rasterizer) * font->lineSpacing;
  float ascent = lovrRasterizerGetAscent(font->info.rasterizer);
  float scale = 1.f / font->pixelDensity;
  wrap /= scale;

  Material* material;
  bool flip = pass->cameras[0].projection[5] > 0.f;
  lovrFontGetVertices(font, strings, count, wrap, halign, valign, vertices, &glyphCount, &lineCount, &material, flip);

  mat4_scale(transform, scale, scale, scale);
  float offset = -ascent + valign / 2.f * (leading * lineCount);
  mat4_translate(transform, 0.f, flip ? -offset : offset, 0.f);

  GlyphVertex* vertexPointer;
  uint16_t* indices;
  lovrPassDraw(pass, &(Draw) {
    .mode = MESH_TRIANGLES,
    .shader = SHADER_FONT,
    .material = font->material,
    .transform = transform,
    .vertex.format = VERTEX_GLYPH,
    .vertex.pointer = (void**) &vertexPointer,
    .vertex.count = glyphCount * 4,
    .index.pointer = (void**) &indices,
    .index.count = glyphCount * 6
  });

  memcpy(vertexPointer, vertices, glyphCount * 4 * sizeof(GlyphVertex));

  for (uint32_t i = 0; i < glyphCount * 4; i += 4) {
    uint16_t quad[] = { i + 0, i + 2, i + 1, i + 1, i + 2, i + 3 };
    memcpy(indices, quad, sizeof(quad));
    indices += COUNTOF(quad);
  }

  tempPop(stack);
}

void lovrPassSkybox(Pass* pass, Texture* texture) {
  if (texture->info.type == TEXTURE_2D) {
    lovrPassDraw(pass, &(Draw) {
      .mode = MESH_TRIANGLES,
      .shader = SHADER_EQUIRECT,
      .material = texture ? lovrTextureGetMaterial(texture) : NULL,
      .vertex.format = VERTEX_EMPTY,
      .count = 6
    });
  } else {
    lovrPassDraw(pass, &(Draw) {
      .mode = MESH_TRIANGLES,
      .shader = SHADER_CUBEMAP,
      .material = texture ? lovrTextureGetMaterial(texture) : NULL,
      .vertex.format = VERTEX_EMPTY,
      .count = 6
    });
  }
}

void lovrPassFill(Pass* pass, Texture* texture) {
  DefaultShader shader;

  if (!texture || texture->info.layers == 1) {
    shader = SHADER_FILL;
  } else if (pass->viewCount > 1 && texture->info.layers > 1) {
    lovrCheck(texture->info.layers == pass->viewCount, "Texture layer counts must match to fill between them");
    shader = SHADER_FILL_ARRAY;
  } else if (pass->viewCount == 1 && texture->info.layers > 1) {
    shader = SHADER_FILL_LAYER;
  } else {
    lovrUnreachable();
  }

  lovrPassDraw(pass, &(Draw) {
    .mode = MESH_TRIANGLES,
    .shader = shader,
    .material = texture ? lovrTextureGetMaterial(texture) : NULL,
    .vertex.format = VERTEX_EMPTY,
    .count = 3
  });
}

void lovrPassMonkey(Pass* pass, float* transform) {
  uint32_t key[] = { SHAPE_MONKEY };
  uint32_t vertexCount = COUNTOF(monkey_vertices) / 6;
  ShapeVertex* vertices;
  uint16_t* indices;

  lovrPassDraw(pass, &(Draw) {
    .hash = hash64(key, sizeof(key)),
    .mode = MESH_TRIANGLES,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = COUNTOF(monkey_indices),
    .transform = transform
  });

  if (!vertices) {
    return;
  }

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

  memcpy(indices, monkey_indices, sizeof(monkey_indices));
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

void lovrPassMesh(Pass* pass, Buffer* vertices, Buffer* indices, float* transform, uint32_t start, uint32_t count, uint32_t instances, uint32_t base) {
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
    .mode = pass->pipeline->mode,
    .vertex.buffer = vertices,
    .index.buffer = indices,
    .transform = transform,
    .start = start,
    .count = count,
    .instances = instances,
    .base = base
  });
}

void lovrPassMeshIndirect(Pass* pass, Buffer* vertices, Buffer* indices, Buffer* draws, uint32_t count, uint32_t offset, uint32_t stride) {
  lovrPassCheckValid(pass);
  lovrCheck(pass->info.type == PASS_RENDER, "This function can only be called on a render pass");
  lovrCheck(offset % 4 == 0, "Buffer offset must be a multiple of 4 when sourcing draws from a Buffer");
  uint32_t commandSize = indices ? 20 : 16;
  stride = stride ? stride : commandSize;
  uint32_t totalSize = stride * (count - 1) + commandSize;
  lovrCheck(offset + totalSize < draws->size, "Draw buffer range exceeds the size of the buffer");

  Draw draw = (Draw) {
    .mode = pass->pipeline->mode,
    .vertex.buffer = vertices,
    .index.buffer = indices
  };

  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader, "A custom Shader must be bound to source draws from a Buffer");

  bindPipeline(pass, &draw, shader);
  bindBundles(pass, &draw, shader);
  bindBuffers(pass, &draw);
  pushConstants(pass, shader);

  if (indices) {
    gpu_draw_indirect_indexed(pass->stream, draws->gpu, offset, count, stride);
  } else {
    gpu_draw_indirect(pass->stream, draws->gpu, offset, count, stride);
  }

  trackBuffer(pass, draws, GPU_PHASE_INDIRECT, GPU_CACHE_INDIRECT);
}

void lovrPassCompute(Pass* pass, uint32_t x, uint32_t y, uint32_t z, Buffer* indirect, uint32_t offset) {
  lovrPassCheckValid(pass);
  lovrCheck(pass->info.type == PASS_COMPUTE, "This function can only be called on a compute pass");

  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader && shader->info.type == SHADER_COMPUTE, "Tried to run a compute shader, but no compute shader is bound");
  lovrCheck(x <= state.limits.workgroupCount[0], "Compute %s count exceeds workgroupCount limit", "x");
  lovrCheck(y <= state.limits.workgroupCount[1], "Compute %s count exceeds workgroupCount limit", "y");
  lovrCheck(z <= state.limits.workgroupCount[2], "Compute %s count exceeds workgroupCount limit", "z");

  gpu_pipeline* pipeline = state.pipelines.data[shader->computePipelineIndex];

  if (pass->pipeline->dirty) {
    gpu_bind_pipeline(pass->stream, pipeline, true);
    pass->pipeline->dirty = false;
  }

  bindBundles(pass, NULL, shader);
  pushConstants(pass, shader);

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
  lovrPassCheckValid(pass);
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!lovrBufferIsTemporary(buffer), "Temporary buffers can not be cleared");
  lovrCheck(offset % 4 == 0, "Buffer clear offset must be a multiple of 4");
  lovrCheck(extent % 4 == 0, "Buffer clear extent must be a multiple of 4");
  lovrCheck(offset + extent <= buffer->size, "Buffer clear range goes past the end of the Buffer");
  gpu_clear_buffer(pass->stream, buffer->gpu, offset, extent);
  trackBuffer(pass, buffer, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
}

void lovrPassClearTexture(Pass* pass, Texture* texture, float value[4], uint32_t layer, uint32_t layerCount, uint32_t level, uint32_t levelCount) {
  lovrPassCheckValid(pass);
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!texture->info.parent, "Texture views can not be cleared");
  lovrCheck(texture->info.usage & TEXTURE_TRANSFER, "Texture must be created with 'transfer' usage to clear it");
  lovrCheck(texture->info.type == TEXTURE_3D || layer + layerCount <= texture->info.layers, "Texture clear range exceeds texture layer count");
  lovrCheck(level + levelCount <= texture->info.mipmaps, "Texture clear range exceeds texture mipmap count");
  gpu_clear_texture(pass->stream, texture->gpu, value, layer, layerCount, level, levelCount);
  trackTexture(pass, texture, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
}

void* lovrPassCopyDataToBuffer(Pass* pass, Buffer* buffer, uint32_t offset, uint32_t extent) {
  lovrPassCheckValid(pass);
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!lovrBufferIsTemporary(buffer), "Temporary buffers can not be copied to, use Buffer:setData");
  lovrCheck(offset + extent <= buffer->size, "Buffer copy range goes past the end of the Buffer");
  gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
  void* pointer = gpu_map(scratchpad, extent, 4, GPU_MAP_STAGING);
  gpu_copy_buffers(pass->stream, scratchpad, buffer->gpu, 0, offset, extent);
  trackBuffer(pass, buffer, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
  return pointer;
}

void lovrPassCopyBufferToBuffer(Pass* pass, Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t extent) {
  lovrPassCheckValid(pass);
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!lovrBufferIsTemporary(dst), "Temporary buffers can not be copied to");
  lovrCheck(srcOffset + extent <= src->size, "Buffer copy range goes past the end of the source Buffer");
  lovrCheck(dstOffset + extent <= dst->size, "Buffer copy range goes past the end of the destination Buffer");
  gpu_copy_buffers(pass->stream, src->gpu, dst->gpu, srcOffset, dstOffset, extent);
  trackBuffer(pass, src, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_READ);
  trackBuffer(pass, dst, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
}

void lovrPassCopyTallyToBuffer(Pass* pass, Tally* tally, Buffer* buffer, uint32_t srcIndex, uint32_t dstOffset, uint32_t count) {
  lovrPassCheckValid(pass);
  if (count == ~0u) count = tally->info.count;
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!lovrBufferIsTemporary(buffer), "Temporary buffers can not be copied to");
  lovrCheck(srcIndex + count <= tally->info.count, "Tally copy range exceeds the number of slots in the Tally");
  lovrCheck(dstOffset + count * 4 <= buffer->size, "Buffer copy range goes past the end of the destination Buffer");
  lovrCheck(dstOffset % 4 == 0, "Buffer copy offset must be a multiple of 4");

  if (tally->info.type == TALLY_TIME) {
    lovrTallyResolve(tally, srcIndex, count, buffer->gpu, dstOffset, pass->stream);
    trackBuffer(pass, buffer, GPU_PHASE_SHADER_COMPUTE, GPU_CACHE_STORAGE_WRITE);
  } else {
    uint32_t stride = tally->info.type == TALLY_SHADER ? 16 : 4;
    gpu_copy_tally_buffer(pass->stream, tally->gpu, buffer->gpu, srcIndex, dstOffset, count, stride);
    trackBuffer(pass, buffer, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
  }
}

void lovrPassCopyImageToTexture(Pass* pass, Image* image, Texture* texture, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t extent[3]) {
  if (extent[0] == ~0u) extent[0] = MIN(texture->info.width - dstOffset[0], lovrImageGetWidth(image, srcOffset[3]) - srcOffset[0]);
  if (extent[1] == ~0u) extent[1] = MIN(texture->info.height - dstOffset[1], lovrImageGetHeight(image, srcOffset[3]) - srcOffset[1]);
  if (extent[2] == ~0u) extent[2] = MIN(texture->info.layers - dstOffset[2], lovrImageGetLayerCount(image) - srcOffset[2]);
  lovrPassCheckValid(pass);
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
  uint32_t rowSize = measureTexture(texture->info.format, extent[0], 1, 1);
  uint32_t totalSize = measureTexture(texture->info.format, extent[0], extent[1], 1) * extent[2];
  uint32_t layerOffset = measureTexture(texture->info.format, extent[0], srcOffset[1], 1);
  layerOffset += measureTexture(texture->info.format, srcOffset[0], 1, 1);
  uint32_t pitch = measureTexture(texture->info.format, lovrImageGetWidth(image, srcOffset[3]), 1, 1);
  gpu_buffer* buffer = tempAlloc(gpu_sizeof_buffer());
  char* dst = gpu_map(buffer, totalSize, 64, GPU_MAP_STAGING);
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
  if (extent[2] == ~0u) extent[2] = MIN(src->info.layers - srcOffset[2], dst->info.layers - dstOffset[0]);
  lovrPassCheckValid(pass);
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
  if (srcExtent[2] == ~0u) srcExtent[2] = src->info.layers - srcOffset[2];
  if (dstExtent[0] == ~0u) dstExtent[0] = dst->info.width - dstOffset[0];
  if (dstExtent[1] == ~0u) dstExtent[1] = dst->info.height - dstOffset[1];
  if (dstExtent[2] == ~0u) dstExtent[2] = dst->info.layers - dstOffset[2];
  lovrPassCheckValid(pass);
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!src->info.parent && !dst->info.parent, "Can not blit Texture views");
  lovrCheck(src->info.samples == 1 && dst->info.samples == 1, "Multisampled textures can not be used for blits");
  lovrCheck(src->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to blit %s it", "from");
  lovrCheck(dst->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to blit %s it", "to");
  lovrCheck(state.features.formats[src->info.format] & GPU_FEATURE_BLIT_SRC, "This GPU does not support blitting from the source texture's format");
  lovrCheck(state.features.formats[dst->info.format] & GPU_FEATURE_BLIT_DST, "This GPU does not support blitting to the destination texture's format");
  lovrCheck(src->info.format == dst->info.format, "Texture formats must match to blit between them");
  lovrCheck(((src->info.type == TEXTURE_3D) ^ (dst->info.type == TEXTURE_3D)) == false, "3D textures can only be blitted with other 3D textures");
  lovrCheck(src->info.type == TEXTURE_3D || srcExtent[2] == dstExtent[2], "When blitting between non-3D textures, blit layer counts must match");
  checkTextureBounds(&src->info, srcOffset, srcExtent);
  checkTextureBounds(&dst->info, dstOffset, dstExtent);
  gpu_blit(pass->stream, src->gpu, dst->gpu, srcOffset, dstOffset, srcExtent, dstExtent, (gpu_filter) filter);
  trackTexture(pass, src, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_READ);
  trackTexture(pass, dst, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_WRITE);
}

void lovrPassMipmap(Pass* pass, Texture* texture, uint32_t base, uint32_t count) {
  if (count == ~0u) count = texture->info.mipmaps - (base + 1);
  lovrPassCheckValid(pass);
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!texture->info.parent, "Can not mipmap a Texture view");
  lovrCheck(texture->info.samples == 1, "Can not mipmap a multisampled texture");
  lovrCheck(texture->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to mipmap it");
  lovrCheck(state.features.formats[texture->info.format] & GPU_FEATURE_BLIT_SRC, "This GPU does not support blitting %s the source texture's format, which is required for mipmapping", "from");
  lovrCheck(state.features.formats[texture->info.format] & GPU_FEATURE_BLIT_DST, "This GPU does not support blitting %s the source texture's format, which is required for mipmapping", "to");
  lovrCheck(base + count < texture->info.mipmaps, "Trying to generate too many mipmaps");
  mipmapTexture(pass->stream, texture, base, count);
  trackTexture(pass, texture, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_READ | GPU_CACHE_TRANSFER_WRITE);
}

Readback* lovrPassReadBuffer(Pass* pass, Buffer* buffer, uint32_t offset, uint32_t extent) {
  lovrPassCheckValid(pass);
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!lovrBufferIsTemporary(buffer), "Unable to read back a temporary buffer");
  lovrCheck(offset + extent <= buffer->size, "Tried to read past the end of the Buffer");
  Readback* readback = lovrReadbackCreate(&(ReadbackInfo) {
    .type = READBACK_BUFFER,
    .buffer.object = buffer,
    .buffer.offset = offset,
    .buffer.extent = extent
  });
  gpu_copy_buffers(pass->stream, buffer->gpu, readback->buffer, offset, 0, extent);
  trackBuffer(pass, buffer, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_READ);
  arr_push(&pass->readbacks, readback);
  return readback;
}

Readback* lovrPassReadTexture(Pass* pass, Texture* texture, uint32_t offset[4], uint32_t extent[3]) {
  if (extent[0] == ~0u) extent[0] = texture->info.width - offset[0];
  if (extent[1] == ~0u) extent[1] = texture->info.height - offset[1];
  lovrPassCheckValid(pass);
  lovrCheck(extent[2] == 1, "Currently, only one layer can be read from a Texture");
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!texture->info.parent, "Can not read from a Texture view");
  lovrCheck(texture->info.samples == 1, "Can not read from a multisampled texture");
  lovrCheck(texture->info.usage & TEXTURE_TRANSFER, "Texture must be created with the 'transfer' usage to read from it");
  checkTextureBounds(&texture->info, offset, extent);
  Readback* readback = lovrReadbackCreate(&(ReadbackInfo) {
    .type = READBACK_TEXTURE,
    .texture.object = texture,
    .texture.offset = { offset[0], offset[1], offset[2], offset[3] },
    .texture.extent = { extent[0], extent[1] }
  });
  gpu_copy_texture_buffer(pass->stream, texture->gpu, readback->buffer, offset, 0, extent);
  trackTexture(pass, texture, GPU_PHASE_TRANSFER, GPU_CACHE_TRANSFER_READ);
  arr_push(&pass->readbacks, readback);
  return readback;
}

Readback* lovrPassReadTally(Pass* pass, Tally* tally, uint32_t index, uint32_t count) {
  lovrPassCheckValid(pass);
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(index + count <= tally->info.count, "Tally read range exceeds the number of slots in the Tally");

  Readback* readback = lovrReadbackCreate(&(ReadbackInfo) {
    .type = READBACK_TALLY,
    .tally.object = tally,
    .tally.index = index,
    .tally.count = count
  });

  if (tally->info.type == TALLY_TIME) {
    lovrTallyResolve(tally, index, count, readback->buffer, 0, pass->stream);
  } else {
    uint32_t stride = tally->info.type == TALLY_SHADER ? 16 : 4;
    gpu_copy_tally_buffer(pass->stream, tally->gpu, readback->buffer, index, 0, count, stride);
  }

  arr_push(&pass->readbacks, readback);
  return readback;
}

void lovrPassTick(Pass* pass, Tally* tally, uint32_t index) {
  lovrCheck(tally->info.views == pass->viewCount, "Tally view count does not match Pass view count");
  lovrCheck(index < tally->info.count, "Trying to use tally slot #%d, but the tally only has %d slots", index + 1, tally->info.count);

  if (tally->tick != state.tick) {
    uint32_t multiplier = tally->info.type == TALLY_TIME ? 2 * tally->info.count * tally->info.views : 1;
    gpu_clear_tally(state.stream, tally->gpu, 0, tally->info.count * multiplier);
    tally->tick = state.tick;
  }

  if (tally->info.type == TALLY_TIME) {
    gpu_tally_mark(pass->stream, tally->gpu, index * 2 * tally->info.views);
  } else {
    gpu_tally_begin(pass->stream, tally->gpu, index);
  }
}

void lovrPassTock(Pass* pass, Tally* tally, uint32_t index) {
  lovrCheck(tally->info.views == pass->viewCount, "Tally view count does not match Pass view count");
  lovrCheck(index < tally->info.count, "Trying to use tally slot #%d, but the tally only has %d slots", index + 1, tally->info.count);

  if (tally->info.type == TALLY_TIME) {
    gpu_tally_mark(pass->stream, tally->gpu, index * 2 * tally->info.views + tally->info.views);
  } else {
    gpu_tally_end(pass->stream, tally->gpu, index);
  }
}

// Helpers

static void* tempAlloc(size_t size) {
  while (state.allocator.cursor + size > state.allocator.length) {
    lovrAssert(state.allocator.length << 1 <= state.allocator.limit, "Out of memory");
    os_vm_commit(state.allocator.memory + state.allocator.length, state.allocator.length);
    state.allocator.length <<= 1;
  }

  uint32_t cursor = ALIGN(state.allocator.cursor, 8);
  state.allocator.cursor = cursor + size;
  return state.allocator.memory + cursor;
}

static size_t tempPush(void) {
  return state.allocator.cursor;
}

static void tempPop(size_t stack) {
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
  state.stream = gpu_stream_begin("Internal");
  state.scratchBufferIndex = 0;
  state.allocator.cursor = 0;
  processReadbacks();
}

static void releasePassResources(void) {
  for (uint32_t i = 0; i < state.passCount; i++) {
    Pass* pass = &state.passes[i];

    for (size_t j = 0; j < pass->access.length; j++) {
      Access* access = &pass->access.data[j];
      lovrRelease(access->buffer, lovrBufferDestroy);
      lovrRelease(access->texture, lovrTextureDestroy);
    }

    if (pass->info.type == PASS_RENDER || pass->info.type == PASS_COMPUTE) {
      for (size_t j = 0; j <= pass->pipelineIndex; j++) {
        Pipeline* pipeline = pass->pipeline - j;
        lovrRelease(pipeline->font, lovrFontDestroy);
        lovrRelease(pipeline->sampler, lovrSamplerDestroy);
        lovrRelease(pipeline->shader, lovrShaderDestroy);
        lovrRelease(pipeline->material, lovrMaterialDestroy);
        pipeline->font = NULL;
        pipeline->sampler = NULL;
        pipeline->shader = NULL;
        pipeline->material = NULL;
      }
    }
  }

  state.passCount = 0;
  state.windowPass = NULL;
}

static void processReadbacks(void) {
  while (state.oldestReadback && gpu_is_complete(state.oldestReadback->tick)) {
    Readback* readback = state.oldestReadback;

    if (readback->image) {
      size_t size = lovrImageGetLayerSize(readback->image, 0);
      void* data = lovrImageGetLayerData(readback->image, 0, 0);
      memcpy(data, readback->pointer, size);
    } else {
      memcpy(readback->data, readback->pointer, readback->size);
    }

    Readback* next = readback->next;
    lovrRelease(readback, lovrReadbackDestroy);
    state.oldestReadback = next;
  }

  if (!state.oldestReadback) {
    state.newestReadback = NULL;
  }
}

static size_t getLayout(gpu_slot* slots, uint32_t count) {
  uint64_t hash = hash64(slots, count * sizeof(gpu_slot));

  size_t index;
  for (size_t index = 0; index < state.layouts.length; index++) {
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

static gpu_bundle* getBundle(size_t layoutIndex) {
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

    if (pool && gpu_is_complete(pool->tick)) {
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

static gpu_texture* getScratchTexture(gpu_texture_info* info) {
  uint16_t key[] = { info->size[0], info->size[1], info->size[2], info->format, info->srgb, info->samples };
  uint32_t hash = (uint32_t) hash64(key, sizeof(key));

  // Find a matching scratch texture that hasn't been used this frame
  for (uint32_t i = 0; i < state.scratchTextures.length; i++) {
    if (state.scratchTextures.data[i].hash == hash && state.scratchTextures.data[i].tick != state.tick) {
      return state.scratchTextures.data[i].texture;
    }
  }

  // Find something to evict
  ScratchTexture* scratch = NULL;
  for (uint32_t i = 0; i < state.scratchTextures.length; i++) {
    if (state.tick - state.scratchTextures.data[i].tick > 16) {
      scratch = &state.scratchTextures.data[i];
      break;
    }
  }

  if (scratch) {
    gpu_texture_destroy(scratch->texture);
  } else {
    arr_expand(&state.scratchTextures, 1);
    scratch = &state.scratchTextures.data[state.scratchTextures.length++];
    scratch->texture = calloc(1, gpu_sizeof_texture());
    lovrAssert(scratch->texture, "Out of memory");
  }

  lovrAssert(gpu_texture_init(scratch->texture, info), "Failed to create scratch texture");
  scratch->hash = hash;
  scratch->tick = state.tick;
  return scratch->texture;
}

static bool isDepthFormat(TextureFormat format) {
  return format == FORMAT_D16 || format == FORMAT_D32F || format == FORMAT_D24S8 || format == FORMAT_D32FS8;
}

// Returns number of bytes of a 3D texture region of a given format
static uint32_t measureTexture(TextureFormat format, uint32_t w, uint32_t h, uint32_t d) {
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
    case FORMAT_D32FS8: return w * h * d * 5;
    case FORMAT_RGBA16:
    case FORMAT_RGBA16F:
    case FORMAT_RG32F: return w * h * d * 8;
    case FORMAT_RGBA32F: return w * h * d * 16;
    case FORMAT_BC1: return ((w + 3) / 4) * ((h + 3) / 4) * d * 8;
    case FORMAT_BC2: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_BC3: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_BC4U: return ((w + 3) / 4) * ((h + 3) / 4) * d * 8;
    case FORMAT_BC4S: return ((w + 3) / 4) * ((h + 3) / 4) * d * 8;
    case FORMAT_BC5U: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_BC5S: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_BC6UF: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_BC6SF: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
    case FORMAT_BC7: return ((w + 3) / 4) * ((h + 3) / 4) * d * 16;
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
  uint32_t maxLayers = info->type == TEXTURE_3D ? MAX(info->layers >> offset[3], 1) : info->layers;
  lovrCheck(offset[0] + extent[0] <= maxWidth, "Texture x range [%d,%d] exceeds width (%d)", offset[0], offset[0] + extent[0], maxWidth);
  lovrCheck(offset[1] + extent[1] <= maxHeight, "Texture y range [%d,%d] exceeds height (%d)", offset[1], offset[1] + extent[1], maxHeight);
  lovrCheck(offset[2] + extent[2] <= maxLayers, "Texture layer range [%d,%d] exceeds layer count (%d)", offset[2], offset[2] + extent[2], maxLayers);
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
      volumetric ? MAX(texture->info.layers >> (level - 1), 1) : 1
    };
    uint32_t dstExtent[3] = {
      MAX(texture->info.width >> level, 1),
      MAX(texture->info.height >> level, 1),
      volumetric ? MAX(texture->info.layers >> level, 1) : 1
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
  if (!texture || texture == state.window) {
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

static void trackMaterial(Pass* pass, Material* material, gpu_phase phase, gpu_cache cache) {
  if (!material->hasWritableTexture) {
    return;
  }

  trackTexture(pass, material->info.texture, phase, cache);
  trackTexture(pass, material->info.glowTexture, phase, cache);
  trackTexture(pass, material->info.metalnessTexture, phase, cache);
  trackTexture(pass, material->info.roughnessTexture, phase, cache);
  trackTexture(pass, material->info.clearcoatTexture, phase, cache);
  trackTexture(pass, material->info.occlusionTexture, phase, cache);
  trackTexture(pass, material->info.normalTexture, phase, cache);
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

static void onResize(uint32_t width, uint32_t height) {
  state.window->info.width = width;
  state.window->info.height = height;

  gpu_surface_resize(width, height);

  lovrEventPush((Event) {
    .type = EVENT_RESIZE,
    .data.resize.width = width,
    .data.resize.height = height
  });
}

static void onMessage(void* context, const char* message, bool severe) {
  if (severe) {
    lovrThrow("GPU error: %s", message);
  } else {
    lovrLog(LOG_DEBUG, "GPU", message);
  }
}
