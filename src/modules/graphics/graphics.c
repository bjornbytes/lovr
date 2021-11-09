#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "data/modelData.h"
#include "data/rasterizer.h"
#include "headset/headset.h"
#include "math/math.h"
#include "core/gpu.h"
#include "core/maf.h"
#include "core/os.h"
#include "core/util.h"
#include "resources/shaders.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define MAX_BUNCHES 256
#define BUNDLES_PER_BUNCH 1024
#define MAX_LAYOUTS 64
#define MAX_MATERIAL_BLOCKS 16
#define MATERIALS_PER_BLOCK 1024
#define MAX_DETAIL 8

typedef struct {
  gpu_buffer* gpu;
  char* data;
  uint32_t index;
  uint32_t offset;
} Megaview;

struct Buffer {
  uint32_t ref;
  uint32_t size;
  Megaview mega;
  BufferInfo info;
  gpu_vertex_format format;
  uint32_t mask;
  uint64_t hash;
  gpu_phase readPhase;
  gpu_phase writePhase;
  gpu_cache pendingReads;
  gpu_cache pendingWrite;
  uint32_t lastWrite;
  bool transient;
};

struct Texture {
  uint32_t ref;
  gpu_texture* gpu;
  gpu_texture* renderView;
  Sampler* sampler;
  TextureInfo info;
  gpu_phase readPhase;
  gpu_phase writePhase;
  gpu_cache pendingReads;
  gpu_cache pendingWrite;
  uint32_t lastWrite;
};

struct Sampler {
  uint32_t ref;
  gpu_sampler* gpu;
  SamplerInfo info;
};

typedef struct {
  uint32_t size;
  uint32_t count;
  uint32_t names[16];
  uint16_t offsets[16];
  uint8_t types[16];
  uint16_t scalars;
  uint16_t vectors;
  uint16_t colors;
  uint16_t scales;
  uint32_t textureCount;
  uint8_t textureSlots[16];
  uint32_t textureNames[16];
} MaterialFormat;

typedef struct {
  uint32_t constantSize;
  uint32_t constantCount;
  uint32_t constantLookup[32];
  uint8_t constantOffsets[32];
  uint8_t constantTypes[32];
  gpu_slot slots[3][32];
  uint32_t slotNames[32];
  uint32_t flagNames[32];
  gpu_shader_flag flags[32];
  uint32_t flagCount;
  uint32_t attributeMask;
  MaterialFormat material;
} ReflectionInfo;

struct Shader {
  uint32_t ref;
  ShaderInfo info;
  gpu_shader* gpu;
  uint32_t layout;
  uint32_t material;
  uint32_t computePipelineIndex;
  uint32_t constantSize;
  uint32_t constantCount;
  uint32_t constantLookup[32];
  uint8_t constantOffsets[32];
  uint8_t constantTypes[32];
  uint32_t resourceCount;
  uint32_t bufferMask;
  uint32_t textureMask;
  uint32_t samplerMask;
  uint32_t storageMask;
  uint8_t slotStages[32];
  uint8_t resourceSlots[32];
  uint32_t resourceLookup[32];
  uint32_t flagCount;
  uint32_t activeFlagCount;
  uint32_t flagLookup[32];
  gpu_shader_flag flags[32];
  uint32_t attributeMask;
};

struct Material {
  uint32_t ref;
  uint32_t next;
  uint32_t block;
  uint32_t index;
  uint32_t tick;
  Texture** textures;
};

typedef struct {
  MaterialFormat format;
  Material* instances;
  gpu_bunch* bunch;
  gpu_bundle* bundles;
  Megaview buffer;
  uint32_t layout;
  uint32_t next;
  uint32_t last;
} MaterialBlock;

enum {
  FLAG_VERTEX = (1 << 0),
  FLAG_INDEX = (1 << 1),
  FLAG_INDEX32 = (1 << 2)
};

typedef struct {
  float depth;
  uint16_t pipeline;
  uint16_t bundle;
  uint16_t material;
  uint8_t vertexBuffer;
  uint8_t indexBuffer;
  uint32_t flags;
  uint32_t start;
  uint32_t count;
  uint32_t instances;
  uint32_t baseVertex;
} BatchDraw;

enum {
  DIRTY_PIPELINE = (1 << 0),
  DIRTY_VERTEX = (1 << 1),
  DIRTY_INDEX = (1 << 2),
  DIRTY_CHUNK = (1 << 3),
  DIRTY_BUNDLE = (1 << 4)
};

typedef struct {
  uint16_t count;
  uint16_t dirty;
} BatchGroup;

typedef struct {
  Buffer* buffer;
  gpu_phase phase;
  gpu_cache cache;
} BufferAccess;

typedef struct {
  Texture* texture;
  gpu_phase phase;
  gpu_cache cache;
} TextureAccess;

typedef arr_t(BufferAccess) arr_bufferaccess_t;
typedef arr_t(TextureAccess) arr_textureaccess_t;

struct Batch {
  uint32_t ref;
  BatchInfo info;
  gpu_pass* pass;
  BatchDraw* draws;
  uint32_t drawCount;
  BatchGroup* groups;
  uint32_t groupCount;
  uint32_t groupedCount;
  uint32_t* activeDraws;
  uint32_t activeDrawCount;
  float* origins;
  gpu_bundle** bundles;
  gpu_bundle_info* bundleInfo;
  gpu_bunch* bunch;
  uint32_t bundleCount;
  uint32_t lastBundleCount;
  Megaview drawBuffer;
  Megaview stash;
  uint32_t stashCursor;
  arr_bufferaccess_t buffers;
  arr_textureaccess_t textures;
};

typedef struct {
  float properties[3][4];
} NodeTransform;

struct Model {
  uint32_t ref;
  uint32_t material;
  ModelData* data;
  DrawInfo* draws;
  Buffer* vertexBuffer;
  Buffer* indexBuffer;
  Texture** textures;
  Material** materials;
  float* vertices;
  uint32_t* indices;
  uint32_t vertexCount;
  uint32_t indexCount;
  NodeTransform* localTransforms;
  float* globalTransforms;
  bool transformsDirty;
};

struct Font {
  uint32_t ref;
  Rasterizer* rasterizer;
};

typedef struct {
  PassType type;
  uint32_t order;
  gpu_stream* stream;
  arr_bufferaccess_t buffers;
  arr_textureaccess_t textures;
  gpu_barrier barrier;
} Pass;

typedef struct {
  float color[4];
  Shader* shader;
  uint64_t format;
  gpu_pipeline_info info;
  uint16_t index;
  bool dirty;
} Pipeline;

typedef struct {
  float view[16];
  float projection[16];
  float viewProjection[16];
  float inverseViewProjection[16];
} Camera;

typedef struct {
  float transform[16];
  float normalMatrix[16];
  float color[4];
} DrawData;

typedef struct {
  void (*callback)(void*, uint32_t, void*);
  void* userdata;
  void* data;
  uint32_t size;
  uint32_t tick;
} Reader;

typedef struct {
  uint32_t head;
  uint32_t tail;
  Reader list[16];
} ReaderPool;

typedef struct {
  char* pointer;
  gpu_buffer* gpu;
  uint32_t size;
  uint32_t next;
  uint32_t tick;
  uint32_t refs;
} Megabuffer;

typedef struct {
  Megabuffer list[256];
  uint32_t active[3];
  uint32_t oldest[3];
  uint32_t newest[3];
  uint32_t cursor[3];
  uint32_t count;
} BufferPool;

typedef struct Bunch {
  gpu_bunch* gpu;
  gpu_bundle* bundles;
  struct Bunch* next;
  uint32_t cursor;
  uint32_t tick;
} Bunch;

typedef struct {
  Bunch list[256];
  Bunch* head[MAX_LAYOUTS];
  Bunch* tail[MAX_LAYOUTS];
  uint32_t count;
} BunchPool;

typedef struct {
  gpu_texture* handle;
  uint32_t hash;
  uint32_t tick;
} ScratchTexture;

typedef struct {
  char* memory;
  uint32_t cursor;
  uint32_t length;
  uint32_t limit;
} Allocator;

typedef struct {
  struct { float x, y, z; } position;
  struct { unsigned nx: 10, ny: 10, nz: 10, pad: 2; } normal;
  struct { uint16_t u, v; } uv;
} ShapeVertex;

typedef struct {
  struct { float x, y, z; } position;
  struct { unsigned nx: 10, ny: 10, nz: 10, pad: 2; } normal;
  struct { float u, v; } uv;
  struct { uint8_t r, g, b, a; } color;
  struct { unsigned x: 10, y: 10, z: 10, handedness: 2; } tangent;
} ModelVertex;

enum {
  SHAPE_GRID,
  SHAPE_CUBE,
  SHAPE_CONE,
  SHAPE_TUBE,
  SHAPE_BALL,
  SHAPE_MAX
};

typedef struct {
  uint32_t start[SHAPE_MAX][MAX_DETAIL];
  uint32_t count[SHAPE_MAX][MAX_DETAIL];
  uint32_t base[SHAPE_MAX];
  Buffer* vertices;
  Buffer* indices;
} Geometry;

static struct {
  bool initialized;
  bool active;
  uint32_t tick;
  uint32_t passCount;
  Pass passes[32];
  Pass* pass;
  Pass* uploads;
  Batch* batch;
  float background[4];
  Camera cameras[6];
  uint32_t viewCount;
  bool cameraDirty;
  float* matrix;
  Pipeline* pipeline;
  uint32_t matrixIndex;
  uint32_t pipelineIndex;
  float matrixStack[16][16];
  Pipeline pipelineStack[4];
  gpu_binding bindings[32];
  uint32_t emptyBindingMask;
  bool bindingsDirty;
  char* constantData;
  bool constantsDirty;
  Megaview cameraBuffer;
  Megaview drawBuffer;
  uint32_t drawCursor;
  gpu_pipeline* boundPipeline;
  gpu_bundle* boundBundle;
  Material* boundMaterial;
  gpu_buffer* boundVertexBuffer;
  gpu_buffer* boundIndexBuffer;
  gpu_index_type boundIndexType;
  Geometry geometry;
  Megaview zeros;
  Texture* window;
  Texture* defaultTexture;
  Shader* defaultShaders[DEFAULT_SHADER_COUNT];
  Sampler* defaultSamplers[DEFAULT_SAMPLER_COUNT];
  gpu_vertex_format formats[VERTEX_FORMAT_COUNT];
  uint32_t formatMask[VERTEX_FORMAT_COUNT];
  uint64_t formatHash[VERTEX_FORMAT_COUNT];
  ScratchTexture attachmentCache[16][4];
  ReaderPool readers;
  BufferPool buffers;
  BunchPool bunches;
  uint32_t pipelineCount;
  uint64_t pipelineLookup[4096];
  gpu_pipeline* pipelines[4096];
  uint32_t gpuPassCount;
  uint64_t passKeys[256];
  gpu_pass* gpuPasses[256];
  uint64_t layoutLookup[MAX_LAYOUTS];
  gpu_layout* layouts[MAX_LAYOUTS];
  uint64_t materialLookup[MAX_MATERIAL_BLOCKS];
  MaterialBlock materials[MAX_MATERIAL_BLOCKS];
  uint32_t blockSize;
  gpu_hardware hardware;
  gpu_features features;
  gpu_limits limits;
  GraphicsStats stats;
  Allocator allocator;
} state;

// Helpers

uint32_t os_vk_create_surface(void* instance, void** surface);
const char** os_vk_get_instance_extensions(uint32_t* count);

static void* talloc(size_t size);
static void* tgrow(void* p, size_t n);
static Megaview allocateBuffer(gpu_memory_type type, uint32_t size, uint32_t align);
static void recycleBuffer(uint8_t index, gpu_memory_type type);
static gpu_bundle* allocateBundle(uint32_t layout);
static gpu_pass* lookupPass(Canvas* canvas);
static uint32_t lookupLayout(gpu_slot* slots, uint32_t count);
static uint32_t lookupMaterialBlock(MaterialFormat* format);
static void generateGeometry(void);
static void clearState(gpu_pass* pass);
static void onMessage(void* context, const char* message, int severe);
static bool getInstanceExtensions(char* buffer, uint32_t size);
static bool isDepthFormat(TextureFormat format);
static size_t measureTexture(TextureFormat format, uint16_t w, uint16_t h, uint16_t d);
static void checkTextureBounds(const TextureInfo* info, uint16_t offset[4], uint16_t extent[3]);
static bool parseSpirv(const void* source, uint32_t size, uint8_t stage, ReflectionInfo* reflection);
static gpu_texture* getScratchTexture(uint32_t size[2], uint32_t layers, TextureFormat format, bool srgb, uint32_t samples);
static void updateModelTransforms(Model* model, uint32_t nodeIndex, float* parent);

// Entry

bool lovrGraphicsInit(bool debug, bool vsync, uint32_t blockSize) {
  lovrCheck(blockSize <= 1 << 30, "Block size can not exceed 1GB");
  state.blockSize = blockSize;

  // GPU

  gpu_config config = {
    .debug = debug,
    .hardware = &state.hardware,
    .features = &state.features,
    .limits = &state.limits,
    .callback = onMessage
  };

#ifdef LOVR_VK
  config.vk.getInstanceExtensions = getInstanceExtensions;

#ifndef LOVR_DISABLE_HEADSET
  if (lovrHeadsetDisplayDriver) {
    config.vk.getDeviceExtensions = lovrHeadsetDisplayDriver->getVulkanDeviceExtensions;
    config.vk.getPhysicalDevice = lovrHeadsetDisplayDriver->getVulkanPhysicalDevice;
    config.vk.createInstance = lovrHeadsetDisplayDriver->createVulkanInstance;
    config.vk.createDevice = lovrHeadsetDisplayDriver->createVulkanDevice;
  }
#endif

  if (os_window_is_open()) {
    config.vk.surface = true;
    config.vk.vsync = vsync;
    config.vk.createSurface = os_vk_create_surface;
  }
#endif

  if (!gpu_init(&config)) {
    lovrThrow("Failed to initialize GPU");
  }

  // Heaps

  state.allocator.length = 1 << 14;
  state.allocator.limit = 1 << 30;
  state.allocator.memory = os_vm_init(state.allocator.limit);
  os_vm_commit(state.allocator.memory, state.allocator.length);

  memset(state.buffers.active, 0xff, sizeof(state.buffers.active));
  memset(state.buffers.oldest, 0xff, sizeof(state.buffers.oldest));
  memset(state.buffers.newest, 0xff, sizeof(state.buffers.newest));

  state.buffers.list[0].gpu = malloc(COUNTOF(state.buffers.list) * gpu_sizeof_buffer());
  state.bunches.list[0].gpu = malloc(COUNTOF(state.bunches.list) * gpu_sizeof_bunch());
  state.pipelines[0] = malloc(COUNTOF(state.pipelines) * gpu_sizeof_pipeline());
  state.gpuPasses[0] = malloc(COUNTOF(state.gpuPasses) * gpu_sizeof_pass());
  state.layouts[0] = malloc(COUNTOF(state.layouts) * gpu_sizeof_layout());
  lovrAssert(state.buffers.list[0].gpu && state.bunches.list[0].gpu && state.pipelines[0] && state.gpuPasses[0] && state.layouts[0], "Out of memory");

  for (uint32_t i = 1; i < COUNTOF(state.buffers.list); i++) {
    state.buffers.list[i].gpu = (gpu_buffer*) ((char*) state.buffers.list[0].gpu + i * gpu_sizeof_buffer());
  }

  for (uint32_t i = 1; i < COUNTOF(state.bunches.list); i++) {
    state.bunches.list[i].gpu = (gpu_bunch*) ((char*) state.bunches.list[0].gpu + i * gpu_sizeof_bunch());
  }

  for (uint32_t i = 1; i < COUNTOF(state.pipelines); i++) {
    state.pipelines[i] = (gpu_pipeline*) ((char*) state.pipelines[0] + i * gpu_sizeof_pipeline());
  }

  for (uint32_t i = 1; i < COUNTOF(state.gpuPasses); i++) {
    state.gpuPasses[i] = (gpu_pass*) ((char*) state.gpuPasses[0] + i * gpu_sizeof_pass());
  }

  for (uint32_t i = 1; i < COUNTOF(state.layouts); i++) {
    state.layouts[i] = (gpu_layout*) ((char*) state.layouts[0] + i * gpu_sizeof_layout());
  }

  // Builtins

  state.zeros = allocateBuffer(GPU_MEMORY_GPU, 4096, 4);

  if (state.zeros.data) {
    memset(state.zeros.data, 0, 4096);
  } else {
    lovrGraphicsPrepare();
    gpu_clear_buffer(state.uploads->stream, state.zeros.gpu, state.zeros.offset, 4096);
    arr_push(&state.uploads->buffers, (BufferAccess) { 0 }); // TODO maybe better way to ensure upload stream gets submitted
  }

  for (uint32_t i = 0; i < DEFAULT_SAMPLER_COUNT; i++) {
    state.defaultSamplers[i] = lovrSamplerCreate(&(SamplerInfo) {
      .min = i == SAMPLER_NEAREST ? FILTER_NEAREST : FILTER_LINEAR,
      .mag = i == SAMPLER_NEAREST ? FILTER_NEAREST : FILTER_LINEAR,
      .mip = i >= SAMPLER_TRILINEAR ? FILTER_LINEAR : FILTER_NEAREST,
      .wrap = { WRAP_REPEAT, WRAP_REPEAT, WRAP_REPEAT },
      .anisotropy = i == SAMPLER_ANISOTROPIC ? state.limits.anisotropy : 0.f
    });
  }

  gpu_slot defaultBindings[] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER_DYNAMIC, GPU_STAGE_GRAPHICS, 1 }, // Cameras
    { 1, GPU_SLOT_UNIFORM_BUFFER_DYNAMIC, GPU_STAGE_GRAPHICS, 1 }, // Draws
    { 2, GPU_SLOT_SAMPLER, GPU_STAGE_GRAPHICS, 1 }, // Nearest
    { 3, GPU_SLOT_SAMPLER, GPU_STAGE_GRAPHICS, 1 }, // Bilinear
    { 4, GPU_SLOT_SAMPLER, GPU_STAGE_GRAPHICS, 1 }, // Trilinear
    { 5, GPU_SLOT_SAMPLER, GPU_STAGE_GRAPHICS, 1 } // Anisotropic
  };

  lookupLayout(defaultBindings, COUNTOF(defaultBindings));

  MaterialFormat basicMaterial = {
    .size = ALIGN(32, state.limits.uniformBufferAlign),
    .count = 3,
    .names = {
      hash32("color", strlen("color")),
      hash32("uvShift", strlen("uvShift")),
      hash32("uvScale", strlen("uvScale"))
    },
    .offsets = { 0, 16, 24 },
    .types = { FIELD_F32x4, FIELD_F32x2, FIELD_F32x2 },
    .vectors = (1 << 0) | (1 << 1) | (1 << 2),
    .colors = (1 << 0),
    .scales = (1 << 2),
    .textureCount = 1,
    .textureSlots[0] = 1,
    .textureNames[0] = hash32("colorTexture", strlen("colorTexture"))
  };

  MaterialFormat physicalMaterial = {
    .size = ALIGN(48, state.limits.uniformBufferAlign),
    .count = 5,
    .names = {
      hash32("color", strlen("color")),
      hash32("uvShift", strlen("uvShift")),
      hash32("uvScale", strlen("uvScale")),
      hash32("metalness", strlen("metalness")),
      hash32("roughness", strlen("roughness"))
    },
    .offsets = { 0, 16, 24, 32, 36 },
    .types = { FIELD_F32x4, FIELD_F32x2, FIELD_F32x2, FIELD_F32, FIELD_F32 },
    .scalars = (1 << 3) | (1 << 4),
    .vectors = (1 << 0) | (1 << 1) | (1 << 2),
    .colors = (1 << 0),
    .scales = (1 << 2),
    .textureCount = 1,
    .textureSlots[0] = 1,
    .textureNames = {
      hash32("colorTexture", strlen("colorTexture"))
      // ...
    }
  };

  MaterialFormat cubemapMaterial = {
    .textureCount = 1,
    .textureSlots[0] = 1,
    .textureNames[0] = hash32("cubemap", strlen("cubemap"))
  };

  lookupMaterialBlock(&basicMaterial);
  lookupMaterialBlock(&physicalMaterial);
  lookupMaterialBlock(&cubemapMaterial);

  state.formats[VERTEX_SHAPE] = (gpu_vertex_format) {
    .bufferCount = 1,
    .attributeCount = 3,
    .bufferStrides[0] = sizeof(ShapeVertex),
    .attributes[0] = { 0, 0, offsetof(ShapeVertex, position), GPU_TYPE_F32x3 },
    .attributes[1] = { 0, 1, offsetof(ShapeVertex, normal), GPU_TYPE_U10Nx3 },
    .attributes[2] = { 0, 2, offsetof(ShapeVertex, uv), GPU_TYPE_U16Nx2 }
  };

  state.formats[VERTEX_MODEL] = (gpu_vertex_format) {
    .bufferCount = 1,
    .attributeCount = 5,
    .bufferStrides[0] = sizeof(ModelVertex),
    .attributes[0] = { 0, 0, offsetof(ModelVertex, position), GPU_TYPE_F32x3 },
    .attributes[1] = { 0, 1, offsetof(ModelVertex, normal), GPU_TYPE_U10Nx3 },
    .attributes[2] = { 0, 2, offsetof(ModelVertex, uv), GPU_TYPE_F32x2 },
    .attributes[3] = { 0, 3, offsetof(ModelVertex, color), GPU_TYPE_U8Nx4 },
    .attributes[4] = { 0, 4, offsetof(ModelVertex, tangent), GPU_TYPE_U10Nx3 }
  };

  state.formats[VERTEX_POINT] = (gpu_vertex_format) {
    .bufferCount = 1,
    .attributeCount = 1,
    .bufferStrides[0] = 12,
    .attributes[0] = { 0, 0, 0, GPU_TYPE_F32x3 }
  };

  state.formats[VERTEX_EMPTY] = (gpu_vertex_format) { 0 };

  for (uint32_t i = 0; i < VERTEX_FORMAT_COUNT; i++) {
    for (uint32_t j = 0; j < state.formats[i].attributeCount; j++) {
      state.formatMask[i] |= (1 << state.formats[i].attributes[j].location);
    }
    state.formatHash[i] = hash64(&state.formats[i], sizeof(state.formats[i]));
  }

  generateGeometry();
  state.constantData = malloc(state.limits.pushConstantSize);
  lovrAssert(state.constantData, "Out of memory");
  clearState(NULL);
  return state.initialized = true;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  for (uint32_t i = 0; i < state.buffers.count; i++) {
    gpu_buffer_destroy(state.buffers.list[i].gpu);
  }
  for (uint32_t i = 0; i < state.bunches.count; i++) {
    gpu_bunch_destroy(state.bunches.list[i].gpu);
    free(state.bunches.list[i].bundles);
  }
  for (uint32_t i = 0; i < state.pipelineCount; i++) {
    gpu_pipeline_destroy(state.pipelines[i]);
  }
  for (uint32_t i = 0; i < COUNTOF(state.gpuPasses) && state.passKeys[i]; i++) {
    gpu_pass_destroy(state.gpuPasses[i]);
  }
  for (uint32_t i = 0; i < COUNTOF(state.layouts) && state.layoutLookup[i]; i++) {
    gpu_layout_destroy(state.layouts[i]);
  }
  for (uint32_t i = 0; i < COUNTOF(state.materials) && state.materialLookup[i]; i++) {
    gpu_bunch_destroy(state.materials[i].bunch);
    free(state.materials[i].bunch);
    free(state.materials[i].bundles);
    free(state.materials[i].instances);
  }
  for (uint32_t i = 0; i < COUNTOF(state.attachmentCache); i++) {
    for (uint32_t j = 0; j < COUNTOF(state.attachmentCache[0]); j++) {
      if (state.attachmentCache[i][j].handle) {
        gpu_texture_destroy(state.attachmentCache[i][j].handle);
        free(state.attachmentCache[i][j].handle);
      }
    }
  }
  for (uint32_t i = 0; i < COUNTOF(state.defaultShaders); i++) {
    lovrRelease(state.defaultShaders[i], lovrShaderDestroy);
  }
  for (uint32_t i = 0; i < COUNTOF(state.defaultSamplers); i++) {
    lovrRelease(state.defaultSamplers[i], lovrSamplerDestroy);
  }
  lovrRelease(state.defaultTexture, lovrTextureDestroy);
  lovrRelease(state.window, lovrTextureDestroy);
  gpu_destroy();
  free(state.layouts[0]);
  free(state.gpuPasses[0]);
  free(state.pipelines[0]);
  free(state.bunches.list[0].gpu);
  free(state.buffers.list[0].gpu);
  free(state.constantData);
  os_vm_free(state.allocator.memory, state.allocator.limit);
  memset(&state, 0, sizeof(state));
}

void lovrGraphicsGetHardware(GraphicsHardware* hardware) {
  hardware->vendorId = state.hardware.vendorId;
  hardware->deviceId = state.hardware.deviceId;
  hardware->deviceName = state.hardware.deviceName;
  hardware->driverMajor = state.hardware.driverMajor;
  hardware->driverMinor = state.hardware.driverMinor;
  hardware->driverPatch = state.hardware.driverPatch;
  hardware->subgroupSize = state.hardware.subgroupSize;
  hardware->discrete = state.hardware.discrete;
#ifdef LOVR_VK
  hardware->renderer = "vulkan";
#endif
}

void lovrGraphicsGetFeatures(GraphicsFeatures* features) {
  features->bptc = state.features.bptc;
  features->astc = state.features.astc;
  features->wireframe = state.features.wireframe;
  features->depthClamp = state.features.depthClamp;
  features->clipDistance = state.features.clipDistance;
  features->cullDistance = state.features.cullDistance;
  features->fullIndexBufferRange = state.features.fullIndexBufferRange;
  features->indirectDrawFirstInstance = state.features.indirectDrawFirstInstance;
  features->dynamicIndexing = state.features.dynamicIndexing;
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
  limits->renderSize[2] = MIN(state.limits.renderSize[2], COUNTOF(state.cameras));
  limits->uniformBufferRange = state.limits.uniformBufferRange;
  limits->storageBufferRange = state.limits.storageBufferRange;
  limits->uniformBufferAlign = state.limits.uniformBufferAlign;
  limits->storageBufferAlign = state.limits.storageBufferAlign;
  limits->vertexAttributes = state.limits.vertexAttributes;
  limits->vertexBufferStride = state.limits.vertexBufferStride;
  limits->vertexShaderOutputs = state.limits.vertexShaderOutputs;
  memcpy(limits->computeDispatchCount, state.limits.computeDispatchCount, 3 * sizeof(uint32_t));
  memcpy(limits->computeWorkgroupSize, state.limits.computeWorkgroupSize, 3 * sizeof(uint32_t));
  limits->computeWorkgroupVolume = state.limits.computeWorkgroupVolume;
  limits->computeSharedMemory = state.limits.computeSharedMemory;
  limits->shaderConstantSize = state.limits.pushConstantSize;
  limits->indirectDrawCount = state.limits.indirectDrawCount;
  limits->instances = state.limits.instances;
  limits->anisotropy = state.limits.anisotropy;
  limits->pointSize = state.limits.pointSize;
}

void lovrGraphicsGetStats(GraphicsStats* stats) {
  state.stats.blocks = state.buffers.count / (float) COUNTOF(state.buffers.list);
  state.stats.canvases = state.gpuPassCount / (float) COUNTOF(state.gpuPasses);
  state.stats.pipelines = state.pipelineCount / (float) COUNTOF(state.pipelines);
  state.stats.bunches = state.bunches.count / (float) COUNTOF(state.bunches.list);
  *stats = state.stats;
}

bool lovrGraphicsIsFormatSupported(uint32_t format, uint32_t usage) {
  uint32_t features = state.features.formats[format];
  if (!usage) return features;
  if ((usage & TEXTURE_FEATURE_SAMPLE) && !(features & GPU_FEATURE_SAMPLE)) return false;
  if ((usage & TEXTURE_FEATURE_FILTER) && !(features & GPU_FEATURE_FILTER)) return false;
  if ((usage & TEXTURE_FEATURE_RENDER) && !(features & GPU_FEATURE_RENDER)) return false;
  if ((usage & TEXTURE_FEATURE_BLEND) && !(features & GPU_FEATURE_BLEND)) return false;
  if ((usage & TEXTURE_FEATURE_STORAGE) && !(features & GPU_FEATURE_STORAGE)) return false;
  if ((usage & TEXTURE_FEATURE_BLIT) && !(features & GPU_FEATURE_BLIT)) return false;
  return true;
}

void lovrGraphicsPrepare() {
  if (state.active) return;
  state.active = true;
  state.allocator.cursor = 0;

  state.tick = gpu_begin();
  state.passCount = 0;

  Pass* syncPass = &state.passes[state.passCount++];
  syncPass->type = PASS_TRANSFER;
  syncPass->order = 0;
  memset(&syncPass->barrier, 0, sizeof(gpu_barrier));

  state.uploads = &state.passes[state.passCount++];
  state.uploads->type = PASS_TRANSFER;
  state.uploads->order = 1;
  state.uploads->stream = gpu_stream_begin();
  arr_init(&state.uploads->buffers, tgrow);
  arr_init(&state.uploads->textures, tgrow);

  state.stats.scratchMemory = 0;
  state.stats.renderPasses = 0;
  state.stats.computePasses = 0;
  state.stats.transferPasses = 0;
  state.stats.pipelineBinds = 0;
  state.stats.bundleBinds = 0;
  state.stats.drawCalls = 0;
  state.stats.dispatches = 0;
  state.stats.workgroups = 0;
  state.stats.copies = 0;

  state.cameraDirty = true;

  // Process any finished readbacks
  ReaderPool* readers = &state.readers;
  while (readers->tail != readers->head && gpu_finished(readers->list[readers->tail & 0xf].tick)) {
    Reader* reader = &readers->list[readers->tail++ & 0xf];
    reader->callback(reader->data, reader->size, reader->userdata);
  }
}

static int passcmp(const void* a, const void* b) {
  const Pass* p = a, *q = b;
  return (p->order > q->order) - (p->order < q->order);
}

static int buffercmp(const void* a, const void* b) {
  const BufferAccess* x = a, *y = b;
  const uintptr_t ba = (uintptr_t) (x->buffer), bb = (uintptr_t) (y->buffer);
  return (ba > bb) - (ba < bb);
}

static int texturecmp(const void* a, const void* b) {
  const TextureAccess* x = a, *y = b;
  const uintptr_t ta = (uintptr_t) (x->texture), tb = (uintptr_t) (y->texture);
  return (ta > tb) - (ta < tb);
}

void lovrGraphicsSubmit() {
  if (!state.active) return;
  if (state.pass) lovrGraphicsFinish();
  state.active = false;

  if (state.window) {
    state.window->gpu = NULL;
    state.window->renderView = NULL;
  }

  qsort(&state.passes, state.passCount, sizeof(Pass), passcmp);

  for (uint32_t i = 0; i < state.passCount; i++) {
    Pass* pass = &state.passes[i];

    qsort(pass->buffers.data, pass->buffers.length, sizeof(BufferAccess), buffercmp);

    for (uint32_t j = 0; j < pass->buffers.length; j++) {
      BufferAccess* access = &pass->buffers.data[j];
      Buffer* buffer = access->buffer;
      if (!buffer) continue; // TODO Maybe better way to ensure gets submitted

      Pass* writer = &state.passes[buffer->lastWrite];
      gpu_barrier* barrier = &writer->barrier;

      while (pass->buffers.data[j + 1].buffer == buffer) {
        access->cache |= pass->buffers.data[j + 1].cache;
        access->phase |= pass->buffers.data[j + 1].phase;
        j++;
      }

      uint32_t read = access->cache & GPU_CACHE_READ;
      uint32_t write = access->cache & GPU_CACHE_WRITE;
      uint32_t newReads = read & ~buffer->pendingReads;
      bool hasNewReads = newReads || (access->phase & ~buffer->readPhase);

      // Read after write (memory + execution dependency, merges readers, only unsynced reads)
      if (read && buffer->pendingWrite && hasNewReads) {
        barrier->prev |= buffer->writePhase;
        barrier->next |= access->phase;
        barrier->flush |= buffer->pendingWrite;
        barrier->invalidate |= newReads;
        buffer->readPhase |= access->phase;
        buffer->pendingReads |= read;
      }

      // Write after write (memory + execution dependency, resets writer)
      if (write && buffer->pendingWrite && !buffer->pendingReads) {
        barrier->prev |= buffer->writePhase;
        barrier->next |= access->phase;
        barrier->flush |= buffer->pendingWrite;
        barrier->invalidate |= write;
        buffer->writePhase = access->phase;
        buffer->pendingWrite = write;
        buffer->lastWrite = i;
      }

      // Write after read (execution dependency, resets writer and clears readers)
      if (write && buffer->pendingReads) {
        barrier->prev |= buffer->readPhase;
        barrier->next |= access->phase;
        buffer->readPhase = 0;
        buffer->pendingReads = 0;
        buffer->writePhase = access->phase;
        buffer->pendingWrite = write;
        buffer->lastWrite = i;
      }
    }

    qsort(pass->textures.data, pass->textures.length, sizeof(TextureAccess), texturecmp);

    for (uint32_t j = 0; j < pass->textures.length; j++) {
      TextureAccess* access = &pass->textures.data[j];
      Texture* texture = access->texture;
      if (!texture) continue; // TODO Maybe better way to ensure gets submitted

      Pass* writer = &state.passes[texture->lastWrite];
      gpu_barrier* barrier = &writer->barrier;

      while (pass->textures.data[j + 1].texture == texture) {
        access->cache |= pass->textures.data[j + 1].cache;
        access->phase |= pass->textures.data[j + 1].phase;
        j++;
      }

      uint32_t read = access->cache & GPU_CACHE_READ;
      uint32_t write = access->cache & GPU_CACHE_WRITE;
      uint32_t newReads = read & ~texture->pendingReads;
      bool hasNewReads = newReads || (access->phase & ~texture->readPhase);

      // Read after write (memory + execution dependency, merges readers, only unsynced reads)
      if (read && texture->pendingWrite && hasNewReads) {
        barrier->prev |= texture->writePhase;
        barrier->next |= access->phase;
        barrier->flush |= texture->pendingWrite;
        barrier->invalidate |= newReads;
        texture->readPhase |= access->phase;
        texture->pendingReads |= read;
      }

      // Write after write (memory + execution dependency, resets writer)
      if (write && texture->pendingWrite && !texture->pendingReads) {
        barrier->prev |= texture->writePhase;
        barrier->next |= access->phase;
        barrier->flush |= texture->pendingWrite;
        barrier->invalidate |= write;
        texture->writePhase = access->phase;
        texture->pendingWrite = write;
        texture->lastWrite = i;
      }

      // Write after read (execution dependency, resets writer and clears readers)
      if (write && texture->pendingReads) {
        barrier->prev |= texture->readPhase;
        barrier->next |= access->phase;
        texture->readPhase = 0;
        texture->pendingReads = 0;
        texture->writePhase = access->phase;
        texture->pendingWrite = write;
        texture->lastWrite = i;
      }
    }
  }

  // If something needs to sync against work in the previous frame, set up the sync pass
  if (state.passes[0].barrier.prev || state.passes[0].barrier.next) {
    state.passes[0].stream = gpu_stream_begin();
  }

  // If there weren't any uploads, don't submit the upload pass
  if (state.uploads->buffers.length == 0 && state.uploads->textures.length == 0) {
    state.uploads->stream = NULL;
  }

  // Sync, finish, and submit streams
  uint32_t count = 0;
  gpu_stream* streams[32];
  for (uint32_t i = 0; i < state.passCount; i++) {
    if (!state.passes[i].stream) continue;
    Pass* pass = &state.passes[i];
    gpu_sync(pass->stream, &pass->barrier, 1);
    gpu_stream_end(pass->stream);
    streams[count++] = pass->stream;
  }

  gpu_submit(streams, count);

  // Release all tracked resources
  for (uint32_t i = 0; i < state.passCount; i++) {
    for (size_t j = 0; j < state.passes[i].buffers.length; j++) {
      Buffer* buffer = state.passes[i].buffers.data[j].buffer;
      if (!buffer) continue; // (TODO)
      buffer->lastWrite = 0;
      lovrRelease(buffer, lovrBufferDestroy);
    }
    for (size_t j = 0; j < state.passes[i].textures.length; j++) {
      Texture* texture = state.passes[i].textures.data[j].texture;
      if (!texture) continue; // (TODO)
      texture->lastWrite = 0;
      lovrRelease(texture, lovrTextureDestroy);
    }
  }
}

void lovrGraphicsWait() {
  gpu_wait();
}

void lovrGraphicsBeginRender(Canvas* canvas, uint32_t order) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!state.pass, "Can not start a new pass while one is already active");
  lovrCheck(state.passCount < COUNTOF(state.passes), "Too many passes, try combining passes or breaking work into multiple submissions");

  // Validate Canvas
  const TextureInfo* main = canvas->textures[0] ? &canvas->textures[0]->info : &canvas->depth.texture->info;
  lovrCheck(canvas->textures[0] || canvas->depth.texture, "Canvas must have at least one color or depth texture");
  lovrCheck(main->width <= state.limits.renderSize[0], "Canvas width (%d) exceeds the renderSize limit of this GPU (%d)", main->width, state.limits.renderSize[0]);
  lovrCheck(main->height <= state.limits.renderSize[1], "Canvas height (%d) exceeds the renderSize limit of this GPU (%d)", main->height, state.limits.renderSize[1]);
  lovrCheck(main->depth <= state.limits.renderSize[2], "Canvas view count (%d) exceeds the renderSize limit of this GPU (%d)", main->depth, state.limits.renderSize[2]);
  lovrCheck(canvas->samples == 1 || canvas->samples == 4, "Currently, Canvas sample count must be 1 or 4");

  // Validate color attachments
  uint32_t colorTextureCount = 0;
  for (uint32_t i = 0; i < COUNTOF(canvas->textures) && canvas->textures[i]; i++, colorTextureCount++) {
    const TextureInfo* info = &canvas->textures[i]->info;
    bool renderable = info->format == ~0u || (state.features.formats[info->format] & GPU_FEATURE_RENDER_COLOR);
    lovrCheck(renderable, "This GPU does not support rendering to the texture format used by Canvas texture #%d", i + 1);
    lovrCheck(info->usage & TEXTURE_RENDER, "Texture must be created with the 'render' flag to render to it");
    lovrCheck(info->width == main->width, "Canvas texture sizes must match");
    lovrCheck(info->height == main->height, "Canvas texture sizes must match");
    lovrCheck(info->depth == main->depth, "Canvas texture depths must match");
    lovrCheck(info->samples == main->samples, "Canvas texture sample counts must match");
  }

  // Validate depth attachment
  if (canvas->depth.texture || canvas->depth.format) {
    TextureFormat format = canvas->depth.texture ? canvas->depth.texture->info.format : canvas->depth.format;
    bool renderable = state.features.formats[format] & GPU_FEATURE_RENDER_DEPTH;
    lovrCheck(renderable, "This GPU does not support rendering to the Canvas depth buffer's format");
    if (canvas->depth.texture) {
      const TextureInfo* info = &canvas->depth.texture->info;
      lovrCheck(info->usage & TEXTURE_RENDER, "Textures must be created with the 'render' flag to attach them to a Canvas");
      lovrCheck(info->width == main->width, "Canvas texture sizes must match");
      lovrCheck(info->height == main->height, "Canvas texture sizes must match");
      lovrCheck(info->depth == main->depth, "Canvas texture depths must match");
      lovrCheck(info->samples == canvas->samples, "Currently, Canvas depth buffer sample count must match its main sample count");
    }
  }

  // Set up render target
  gpu_canvas target = {
    .pass = lookupPass(canvas),
    .size = { main->width, main->height }
  };

  for (uint32_t i = 0; i < colorTextureCount; i++) {
    if (main->samples == 1 && canvas->samples > 1) {
      TextureFormat format = canvas->textures[i]->info.format;
      bool srgb = canvas->textures[i]->info.srgb;
      target.color[i].texture = getScratchTexture(target.size, main->depth, format, srgb, canvas->samples);
      target.color[i].resolve = canvas->textures[i]->renderView;
    } else {
      target.color[i].texture = canvas->textures[i]->renderView;
    }

    target.color[i].clear[0] = lovrMathGammaToLinear(canvas->clears[i][0]);
    target.color[i].clear[1] = lovrMathGammaToLinear(canvas->clears[i][1]);
    target.color[i].clear[2] = lovrMathGammaToLinear(canvas->clears[i][2]);
    target.color[i].clear[3] = canvas->clears[i][3];
  }

  if (canvas->depth.texture) {
    target.depth.texture = canvas->depth.texture->renderView;
  } else if (canvas->depth.format) {
    target.depth.texture = getScratchTexture(target.size, main->depth, canvas->depth.format, false, canvas->samples);
  }

  target.depth.clear.depth = canvas->depth.clear;

  state.viewCount = main->depth;

  order = (CLAMP(order, 1, 100) << 16) | state.passCount;
  state.pass = &state.passes[state.passCount++];
  state.pass->type = PASS_RENDER;
  state.pass->order = order;
  state.pass->stream = gpu_stream_begin();
  arr_init(&state.pass->buffers, tgrow);
  arr_init(&state.pass->textures, tgrow);
  gpu_render_begin(state.pass->stream, &target);

  float viewport[4] = { 0.f, 0.f, (float) main->width, (float) main->height };
  float depthRange[2] = { 0.f, 1.f };
  gpu_set_viewport(state.pass->stream, viewport, depthRange);

  uint32_t scissor[4] = { 0, 0, main->width, main->height };
  gpu_set_scissor(state.pass->stream, scissor);

  clearState(target.pass);
  state.stats.renderPasses++;
}

void lovrGraphicsBeginCompute(uint32_t order) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!state.pass, "Can not start a new pass while one is already active");
  lovrCheck(state.passCount < COUNTOF(state.passes), "Too many passes, try combining passes or breaking work into multiple submissions");
  order = (CLAMP(order, 1, 100) << 16) | state.passCount;
  state.pass = &state.passes[state.passCount++];
  state.pass->type = PASS_COMPUTE;
  state.pass->order = order;
  state.pass->stream = gpu_stream_begin();
  arr_init(&state.pass->buffers, tgrow);
  arr_init(&state.pass->textures, tgrow);
  state.emptyBindingMask = ~0u;
  state.bindingsDirty = true;
  state.boundPipeline = NULL;
  state.boundBundle = NULL;
  gpu_compute_begin(state.pass->stream);
  state.stats.computePasses++;
}

void lovrGraphicsBeginTransfer(uint32_t order) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!state.pass, "Can not start a new pass while one is already active");
  lovrCheck(state.passCount < COUNTOF(state.passes), "Too many passes, try combining passes or breaking work into multiple submissions");
  order = (CLAMP(order, 1, 100) << 16) | state.passCount;
  state.pass = &state.passes[state.passCount++];
  state.pass->type = PASS_TRANSFER;
  state.pass->order = order;
  state.pass->stream = gpu_stream_begin();
  arr_init(&state.pass->buffers, tgrow);
  arr_init(&state.pass->textures, tgrow);
  state.stats.transferPasses++;
}

void lovrGraphicsBeginBatch(Batch* batch) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!state.pass, "Can not start a new pass while one is already active");
  lovrCheck(state.passCount < COUNTOF(state.passes), "Too many passes, try combining passes or breaking work into multiple submissions");
  uint32_t order = state.passCount;
  state.pass = &state.passes[state.passCount++];
  state.pass->type = PASS_BATCH;
  state.pass->order = order;
  state.pass->stream = batch->info.transient ? NULL : gpu_stream_begin();
  arr_init(&state.pass->buffers, tgrow);
  arr_init(&state.pass->textures, tgrow);
  state.batch = batch;
  lovrRetain(batch);
  clearState(batch->pass);
  if (batch->info.transient) {
    lovrBatchReset(batch);
    batch->drawBuffer = allocateBuffer(GPU_MEMORY_CPU_WRITE, batch->info.capacity * sizeof(DrawData), state.limits.uniformBufferAlign);
    state.drawBuffer = batch->drawBuffer;
  }
}

void lovrGraphicsFinish() {
  lovrCheck(state.pass, "No pass is active");
  for (uint32_t i = 0; i <= state.pipelineIndex; i++) {
    lovrRelease(state.pipelineStack[i].shader, lovrShaderDestroy);
    state.pipelineStack[i].shader = NULL;
  }
  switch (state.pass->type) {
    case PASS_RENDER: gpu_render_end(state.pass->stream); break;
    case PASS_COMPUTE: gpu_compute_end(state.pass->stream); break;
    case PASS_TRANSFER: break;
    case PASS_BATCH: {
      Batch* batch = state.batch;

      // Allocate bundles
      if (batch->bundleCount > 0) {
        if (batch->info.transient) {
          for (uint32_t i = 0; i < batch->bundleCount; i++) {
            uint32_t layoutIndex = ((char*) batch->bundleInfo[i].layout - (char*) state.layouts[0]) / gpu_sizeof_layout();
            batch->bundles[i] = allocateBundle(layoutIndex);
          }
        } else {
          gpu_bunch_info info = {
            .bundles = batch->bundles[0],
            .contents = batch->bundleInfo,
            .count = batch->bundleCount
          };

          lovrAssert(gpu_bunch_init(batch->bunch, &info), "Failed to initialize bunch");
        }

        gpu_bundle_write(batch->bundles, batch->bundleInfo, batch->bundleCount);
      }

      if (!batch->info.transient) {
        // Add stash/ubos to pass's sync arrays with BUFFER_COPY_DST usage
      }

      lovrRelease(batch, lovrBatchDestroy);
      state.batch = NULL;
      break;
    }
    default: break;
  }
  state.pass = NULL;
}

void lovrGraphicsGetBackground(float background[4]) {
  memcpy(background, state.background, 4 * sizeof(float));
}

void lovrGraphicsSetBackground(float background[4]) {
  memcpy(state.background, background, 4 * sizeof(float));
}

void lovrGraphicsGetViewMatrix(uint32_t index, float* view) {
  lovrCheck(index < COUNTOF(state.cameras), "Invalid view index %d", index);
  mat4_init(view, state.cameras[index].view);
}

void lovrGraphicsSetViewMatrix(uint32_t index, float* view) {
  lovrCheck(index < COUNTOF(state.cameras), "Invalid view index %d", index);
  mat4_init(state.cameras[index].view, view);
}

void lovrGraphicsGetProjection(uint32_t index, float* projection) {
  lovrCheck(index < COUNTOF(state.cameras), "Invalid view index %d", index);
  mat4_init(projection, state.cameras[index].projection);
}

void lovrGraphicsSetProjection(uint32_t index, float* projection) {
  lovrCheck(index < COUNTOF(state.cameras), "Invalid view index %d", index);
  mat4_init(state.cameras[index].projection, projection);
}

void lovrGraphicsSetViewport(float viewport[4], float depthRange[2]) {
  lovrCheck(state.pass && state.pass->type == PASS_RENDER, "The viewport can only be changed during a render pass");
  lovrCheck(viewport[2] > 0.f && viewport[3] > 0.f, "Viewport dimensions must be greater than zero");
  lovrCheck(depthRange[0] >= 0.f && depthRange[0] <= 1.f, "Depth range values must be between 0 and 1");
  lovrCheck(depthRange[1] >= 0.f && depthRange[1] <= 1.f, "Depth range values must be between 0 and 1");
  gpu_set_viewport(state.pass->stream, viewport, depthRange);
}

void lovrGraphicsSetScissor(uint32_t scissor[4]) {
  lovrCheck(state.pass && state.pass->type == PASS_RENDER, "The scissor can only be changed during a render pass");
  lovrCheck(scissor[2] > 0 && scissor[3] > 0, "Scissor dimensions must be greater than zero");
  gpu_set_scissor(state.pass->stream, scissor);
}

void lovrGraphicsPush(StackType type, const char* label) {
  switch (type) {
    case STACK_TRANSFORM:
      state.matrix = state.matrixStack[++state.matrixIndex];
      lovrCheck(state.matrixIndex < COUNTOF(state.matrixStack), "Transform stack overflow (more pushes than pops?)");
      mat4_init(state.matrix, state.matrixStack[state.matrixIndex - 1]);
      break;
    case STACK_PIPELINE:
      state.pipeline = &state.pipelineStack[++state.pipelineIndex];
      lovrCheck(state.pipelineIndex < COUNTOF(state.pipelineStack), "Pipeline stack overflow (more pushes than pops?)");
      memcpy(state.pipeline, &state.pipelineStack[state.pipelineIndex - 1], sizeof(Pipeline));
      lovrRetain(state.pipeline->shader);
      break;
    case STACK_LABEL:
      lovrCheck(state.pass, "A pass must be active to push labels");
      gpu_label_push(state.pass->stream, label);
    default: break;
  }
}

void lovrGraphicsPop(StackType type) {
  switch (type) {
    case STACK_TRANSFORM:
      state.matrix = state.matrixStack[--state.matrixIndex];
      lovrCheck(state.matrixIndex < COUNTOF(state.matrixStack), "Transform stack underflow (more pops than pushes?)");
      break;
    case STACK_PIPELINE:
      lovrRelease(state.pipeline->shader, lovrShaderDestroy);
      state.pipeline = &state.pipelineStack[--state.pipelineIndex];
      lovrCheck(state.pipelineIndex < COUNTOF(state.pipelineStack), "Pipeline stack underflow (more pops than pushes?)");
      break;
    case STACK_LABEL:
      lovrCheck(state.pass, "A pass must be active to pop labels");
      gpu_label_pop(state.pass->stream);
    default: break;
  }
}

void lovrGraphicsOrigin(void) {
  mat4_identity(state.matrix);
}

void lovrGraphicsTranslate(vec3 translation) {
  mat4_translate(state.matrix, translation[0], translation[1], translation[2]);
}

void lovrGraphicsRotate(quat rotation) {
  mat4_rotateQuat(state.matrix, rotation);
}

void lovrGraphicsScale(vec3 scale) {
  mat4_scale(state.matrix, scale[0], scale[1], scale[2]);
}

void lovrGraphicsTransform(mat4 transform) {
  mat4_mul(state.matrix, transform);
}

void lovrGraphicsSetAlphaToCoverage(bool enabled) {
  state.pipeline->dirty |= enabled != state.pipeline->info.alphaToCoverage;
  state.pipeline->info.alphaToCoverage = enabled;
}

void lovrGraphicsSetBlendMode(BlendMode mode, BlendAlphaMode alphaMode) {
  if (mode == BLEND_NONE) {
    state.pipeline->dirty |= state.pipeline->info.blend.enabled;
    memset(&state.pipeline->info.blend, 0, sizeof(gpu_blend_state));
    return;
  }

  gpu_blend_state blendModes[] = {
    [BLEND_ALPHA] = {
      .color = { .src = GPU_BLEND_SRC_ALPHA, .dst = GPU_BLEND_ONE_MINUS_SRC_ALPHA, .op = GPU_BLEND_ADD },
      .alpha = { .src = GPU_BLEND_ONE, .dst = GPU_BLEND_ONE_MINUS_SRC_ALPHA, .op = GPU_BLEND_ADD }
    },
    [BLEND_ADD] = {
      .color = { .src = GPU_BLEND_SRC_ALPHA, .dst = GPU_BLEND_ONE, .op = GPU_BLEND_ADD },
      .alpha = { .src = GPU_BLEND_ZERO, .dst = GPU_BLEND_ONE, .op = GPU_BLEND_ADD }
    },
    [BLEND_SUBTRACT] = {
      .color = { .src = GPU_BLEND_SRC_ALPHA, .dst = GPU_BLEND_ONE, .op = GPU_BLEND_RSUB },
      .alpha = { .src = GPU_BLEND_ZERO, .dst = GPU_BLEND_ONE, .op = GPU_BLEND_RSUB }
    },
    [BLEND_MULTIPLY] = {
      .color = { .src = GPU_BLEND_DST_COLOR, .dst = GPU_BLEND_ZERO, .op = GPU_BLEND_ADD },
      .alpha = { .src = GPU_BLEND_DST_COLOR, .dst = GPU_BLEND_ZERO, .op = GPU_BLEND_ADD },
    },
    [BLEND_LIGHTEN] = {
      .color = { .src = GPU_BLEND_SRC_ALPHA, .dst = GPU_BLEND_ZERO, .op = GPU_BLEND_MAX },
      .alpha = { .src = GPU_BLEND_ONE, .dst = GPU_BLEND_ZERO, .op = GPU_BLEND_MAX }
    },
    [BLEND_DARKEN] = {
      .color = { .src = GPU_BLEND_SRC_ALPHA, .dst = GPU_BLEND_ZERO, .op = GPU_BLEND_MIN },
      .alpha = { .src = GPU_BLEND_ONE, .dst = GPU_BLEND_ZERO, .op = GPU_BLEND_MIN }
    },
    [BLEND_SCREEN] = {
      .color = { .src = GPU_BLEND_SRC_ALPHA, .dst = GPU_BLEND_ONE_MINUS_SRC_COLOR, .op = GPU_BLEND_ADD },
      .alpha = { .src = GPU_BLEND_ONE, .dst = GPU_BLEND_ONE_MINUS_SRC_COLOR, .op = GPU_BLEND_ADD }
    }
  };

  state.pipeline->info.blend = blendModes[mode];

  if (alphaMode == BLEND_PREMULTIPLIED && mode != BLEND_MULTIPLY) {
    state.pipeline->info.blend.color.src = GPU_BLEND_ONE;
  }

  state.pipeline->info.blend.enabled = true;
  state.pipeline->dirty = true;
}

void lovrGraphicsSetColorMask(bool r, bool g, bool b, bool a) {
  uint8_t mask = (r << 0) | (g << 1) | (b << 2) | (a << 3);
  state.pipeline->dirty |= state.pipeline->info.colorMask != mask;
  state.pipeline->info.colorMask = mask;
}

void lovrGraphicsSetCullMode(CullMode mode) {
  state.pipeline->dirty |= state.pipeline->info.rasterizer.cullMode != (gpu_cull_mode) mode;
  state.pipeline->info.rasterizer.cullMode = (gpu_cull_mode) mode;
}

void lovrGraphicsSetDepthTest(CompareMode test) {
  state.pipeline->dirty |= state.pipeline->info.depth.test != (gpu_compare_mode) test;
  state.pipeline->info.depth.test = (gpu_compare_mode) test;
}

void lovrGraphicsSetDepthWrite(bool write) {
  state.pipeline->dirty |= state.pipeline->info.depth.write != write;
  state.pipeline->info.depth.write = write;
}

void lovrGraphicsSetDepthOffset(float offset, float sloped) {
  state.pipeline->info.rasterizer.depthOffset = offset;
  state.pipeline->info.rasterizer.depthOffsetSloped = sloped;
  state.pipeline->dirty = true;
}

void lovrGraphicsSetDepthClamp(bool clamp) {
  if (state.features.depthClamp) {
    state.pipeline->dirty |= state.pipeline->info.rasterizer.depthClamp != clamp;
    state.pipeline->info.rasterizer.depthClamp = clamp;
  }
}

void lovrGraphicsSetShader(Shader* shader) {
  Shader* previous = state.pipeline->shader;
  if (shader == previous) return;

  // Clear any bindings for resources that share the same slot but have different types
  if (previous) {
    for (uint32_t i = 0, j = 0; i < previous->resourceCount && j < shader->resourceCount;) {
      if (previous->resourceSlots[i] < shader->resourceSlots[j]) {
        i++;
      } else if (previous->resourceSlots[i] > shader->resourceSlots[j]) {
        j++;
      } else {
        // Either the bufferMask or textureMask will be set, so only need to check one
        uint32_t mask = 1 << shader->resourceSlots[j];
        bool differentType = 0;
        differentType |= (previous->bufferMask & mask) != (shader->bufferMask & mask);
        differentType |= (previous->textureMask & mask) != (shader->textureMask & mask);
        differentType |= (previous->samplerMask & mask) != (shader->samplerMask & mask);
        bool differentStorage = !(previous->storageMask & mask) == !(shader->storageMask & mask);
        state.emptyBindingMask |= (differentType || differentStorage) << shader->resourceSlots[j];
        i++;
        j++;
      }
    }

    // Material format mismatch requires the material to be rebound
    if (shader->material != previous->material) {
      state.boundMaterial = NULL;
    }
  }

  uint32_t empties = (shader->bufferMask | shader->textureMask) & state.emptyBindingMask;

  // Assign default bindings to any empty slots used by the shader (TODO biterationtrinsics)
  if (empties) {
    for (uint32_t i = 0; i < 32; i++) {
      if (~empties & (1 << i)) continue;

      if (shader->bufferMask) {
        state.bindings[i].buffer = (gpu_buffer_binding) {
          .object = state.zeros.gpu,
          .offset = 0,
          .extent = 4096
        };
      } else {
        Texture* texture = lovrGraphicsGetDefaultTexture();
        state.bindings[i].texture = texture->gpu;
      }

      state.emptyBindingMask &= ~(1 << i);
    }

    state.bindingsDirty = true;
  }

  lovrRetain(shader);
  lovrRelease(previous, lovrShaderDestroy);

  state.pipeline->shader = shader;
  state.pipeline->info.shader = shader ? shader->gpu : NULL;
  state.pipeline->info.flags = shader ? shader->flags : NULL;
  state.pipeline->info.flagCount = shader ? shader->activeFlagCount : 0;
  state.pipeline->dirty = true;
}

void lovrGraphicsSetStencilTest(CompareMode test, uint8_t value, uint8_t mask) {
  bool hasReplace = false;
  hasReplace |= state.pipeline->info.stencil.failOp == GPU_STENCIL_REPLACE;
  hasReplace |= state.pipeline->info.stencil.depthFailOp == GPU_STENCIL_REPLACE;
  hasReplace |= state.pipeline->info.stencil.passOp == GPU_STENCIL_REPLACE;
  if (hasReplace && test != COMPARE_NONE) {
    lovrCheck(value == state.pipeline->info.stencil.value, "When stencil write is 'replace' and stencil test is active, their values must match");
  }
  switch (test) { // (Reversed compare mode)
    case COMPARE_NONE: default: state.pipeline->info.stencil.test = GPU_COMPARE_NONE; break;
    case COMPARE_EQUAL: state.pipeline->info.stencil.test = GPU_COMPARE_EQUAL; break;
    case COMPARE_NEQUAL: state.pipeline->info.stencil.test = GPU_COMPARE_NEQUAL; break;
    case COMPARE_LESS: state.pipeline->info.stencil.test = GPU_COMPARE_GREATER; break;
    case COMPARE_LEQUAL: state.pipeline->info.stencil.test = GPU_COMPARE_GEQUAL; break;
    case COMPARE_GREATER: state.pipeline->info.stencil.test = GPU_COMPARE_LESS; break;
    case COMPARE_GEQUAL: state.pipeline->info.stencil.test = GPU_COMPARE_LEQUAL; break;
  }
  state.pipeline->info.stencil.testMask = mask;
  if (test != COMPARE_NONE) state.pipeline->info.stencil.value = value;
  state.pipeline->dirty = true;
}

void lovrGraphicsSetStencilWrite(StencilAction actions[3], uint8_t value, uint8_t mask) {
  bool hasReplace = actions[0] == STENCIL_REPLACE || actions[1] == STENCIL_REPLACE || actions[2] == STENCIL_REPLACE;
  if (hasReplace && state.pipeline->info.stencil.test != GPU_COMPARE_NONE) {
    lovrCheck(value == state.pipeline->info.stencil.value, "When stencil write is 'replace' and stencil test is active, their values must match");
  }
  state.pipeline->info.stencil.failOp = (gpu_stencil_op) actions[0];
  state.pipeline->info.stencil.depthFailOp = (gpu_stencil_op) actions[1];
  state.pipeline->info.stencil.passOp = (gpu_stencil_op) actions[2];
  state.pipeline->info.stencil.writeMask = mask;
  if (hasReplace) state.pipeline->info.stencil.value = value;
  state.pipeline->dirty = true;
}

void lovrGraphicsSetWinding(Winding winding) {
  state.pipeline->dirty |= state.pipeline->info.rasterizer.winding != (gpu_winding) winding;
  state.pipeline->info.rasterizer.winding = (gpu_winding) winding;
}

void lovrGraphicsSetWireframe(bool wireframe) {
  if (state.features.wireframe) {
    state.pipeline->dirty |= state.pipeline->info.rasterizer.wireframe != (gpu_winding) wireframe;
    state.pipeline->info.rasterizer.wireframe = wireframe;
  }
}

void lovrGraphicsSetBuffer(const char* name, size_t length, uint32_t slot, Buffer* buffer, uint32_t offset, uint32_t extent) {
  Shader* shader = state.pipeline->shader;
  lovrCheck(shader, "A Shader must be active to bind resources");

  if (name) {
    slot = ~0u;
    uint32_t hash = hash32(name, length);
    for (uint32_t i = 0; i < shader->resourceCount; i++) {
      if (shader->resourceLookup[i] == hash) {
        slot = shader->resourceSlots[i];
        break;
      }
    }
    lovrCheck(slot != ~0u, "Shader has no resource named '%s'", name);
  }

  bool storage = shader->storageMask & (1 << slot);
  lovrCheck(shader->bufferMask & (1 << slot), "Shader slot %d is not a Buffer", slot + 1);
  lovrCheck(offset < buffer->size, "Buffer bind offset is past the end of the Buffer");

  if (storage) {
    lovrCheck(buffer->info.type == BUFFER_COMPUTE, "Bad Buffer type for slot #%d (expected compute)", slot + 1);
    lovrCheck((offset & (state.limits.storageBufferAlign - 1)) == 0, "Storage buffer offset (%d) is not aligned to storageBufferAlign limit (%d)", offset, state.limits.storageBufferAlign);
  } else {
    lovrCheck(buffer->info.type == BUFFER_UNIFORM, "Bad Buffer type for slot #%d (expected uniform)", slot + 1);
    lovrCheck((offset & (state.limits.uniformBufferAlign - 1)) == 0, "Uniform buffer offset (%d) is not aligned to uniformBufferAlign limit (%d)", offset, state.limits.uniformBufferAlign);
  }

  uint32_t limit = storage ? state.limits.storageBufferRange : state.limits.uniformBufferRange;

  if (extent == 0) {
    extent = MIN(buffer->size - offset, limit);
  } else {
    lovrCheck(offset + extent <= buffer->size, "Buffer bind range goes past the end of the Buffer");
    lovrCheck(extent <= limit, "Buffer bind range exceeds storageBufferRange/uniformBufferRange limit");
  }

  state.bindings[slot].buffer.object = buffer->mega.gpu;
  state.bindings[slot].buffer.offset = buffer->mega.offset + offset;
  state.bindings[slot].buffer.extent = extent;
  gpu_phase phase = GPU_PHASE_SHADER_VERTEX | GPU_PHASE_SHADER_FRAGMENT | GPU_PHASE_SHADER_COMPUTE; // TODO
  gpu_cache cache = storage ? GPU_CACHE_STORAGE_READ : GPU_CACHE_UNIFORM;
  BufferAccess access = { buffer, phase, cache };
  arr_bufferaccess_t* buffers = state.batch ? &state.batch->buffers : &state.pass->buffers;
  arr_push(buffers, access);
  lovrRetain(buffer);

  state.emptyBindingMask &= ~(1 << slot);
  state.bindingsDirty = true;
}

void lovrGraphicsSetTexture(const char* name, size_t length, uint32_t slot, Texture* texture) {
  Shader* shader = state.pipeline->shader;
  lovrCheck(shader, "A Shader must be active to bind resources");

  if (name) {
    slot = ~0u;
    uint32_t hash = hash32(name, length);
    for (uint32_t i = 0; i < shader->resourceCount; i++) {
      if (shader->resourceLookup[i] == hash) {
        slot = shader->resourceSlots[i];
        break;
      }
    }
    lovrCheck(slot != ~0u, "Shader has no resource named '%s'", name);
  }

  bool storage = shader->storageMask & (1 << slot);
  lovrCheck(shader->textureMask & (1 << slot), "Shader slot %d is not a Texture", slot + 1);

  if (storage) {
    lovrCheck(texture->info.usage & TEXTURE_STORAGE, "Textures must be created with the 'storage' flag to use them as storage textures");
  } else {
    lovrCheck(texture->info.usage & TEXTURE_SAMPLE, "Textures must be created with the 'sample' flag to sample them in shaders");
  }

  state.bindings[slot].texture = texture->gpu;
  gpu_phase phase = GPU_PHASE_SHADER_VERTEX | GPU_PHASE_SHADER_FRAGMENT | GPU_PHASE_SHADER_COMPUTE;
  gpu_cache cache = GPU_CACHE_TEXTURE;
  TextureAccess access = { texture, phase, cache };
  arr_textureaccess_t* textures = state.batch ? &state.batch->textures : &state.pass->textures;
  arr_push(textures, access);
  lovrRetain(texture);

  state.emptyBindingMask &= ~(1 << slot);
  state.bindingsDirty = true;
}

void lovrGraphicsSetSampler(const char* name, size_t length, uint32_t slot, Sampler* sampler) {
  Shader* shader = state.pipeline->shader;
  lovrCheck(shader, "A Shader must be active to bind resources");

  if (name) {
    slot = ~0u;
    uint32_t hash = hash32(name, length);
    for (uint32_t i = 0; i < shader->resourceCount; i++) {
      if (shader->resourceLookup[i] == hash) {
        slot = shader->resourceSlots[i];
        break;
      }
    }
    lovrCheck(slot != ~0u, "Shader has no resource named '%s'", name);
  }

  lovrCheck(shader->samplerMask & (1 << slot), "Shader slot %d is not a Sampler", slot + 1);

  state.bindings[slot].sampler = sampler->gpu;
  state.emptyBindingMask &= ~(1 << slot);
  state.bindingsDirty = true;
}

void lovrGraphicsSetConstant(const char* name, size_t length, void** data, FieldType* type) {
  Shader* shader = state.pipeline->shader;
  lovrCheck(shader, "A Shader must be active to set constants");
  uint32_t hash = hash32(name, length);
  uint32_t index = ~0u;
  for (uint32_t i = 0; i < shader->constantCount; i++) {
    if (shader->constantLookup[i] == hash) {
      index = i;
      break;
    }
  }
  lovrCheck(index != ~0u, "Shader has no constant named '%s'", name);
  *type = shader->constantTypes[index];
  *data = &state.constantData[shader->constantOffsets[index]];
  state.constantsDirty = true;
}

void lovrGraphicsSetColor(float color[4]) {
  state.pipeline->color[0] = lovrMathGammaToLinear(color[0]);
  state.pipeline->color[1] = lovrMathGammaToLinear(color[1]);
  state.pipeline->color[2] = lovrMathGammaToLinear(color[2]);
  state.pipeline->color[3] = color[3];
}

uint32_t lovrGraphicsMesh(DrawInfo* info, float* transform) {
  lovrCheck(state.pass && (state.pass->type == PASS_RENDER || state.pass->type == PASS_BATCH), "Drawing can only happen inside of a render pass or batch pass");
  Shader* shader = state.pipeline->shader ? state.pipeline->shader : lovrGraphicsGetDefaultShader(info->shader);
  Material* material = info->material ? info->material : &state.materials[shader->material].instances[0];
  Batch* batch = state.batch;

  // Pipeline

  gpu_pipeline* pipeline;
  uint32_t pipelineIndex;

  state.pipeline->dirty |= state.pipeline->info.drawMode != (gpu_draw_mode) info->mode;
  state.pipeline->info.drawMode = (gpu_draw_mode) info->mode;

  if (!state.pipeline->shader && shader->gpu != state.pipeline->info.shader) {
    state.pipeline->info.shader = shader->gpu;
    state.pipeline->info.flags = NULL;
    state.pipeline->info.flagCount = 0;
    state.pipeline->dirty = true;
  }

  gpu_vertex_format* format;
  uint64_t formatHash;
  uint32_t formatMask;

  if (info->vertex.buffer) {
    format = &info->vertex.buffer->format;
    formatHash = info->vertex.buffer->hash;
    formatMask = info->vertex.buffer->mask;
  } else {
    format = &state.formats[info->vertex.format];
    formatHash = state.formatHash[info->vertex.format];
    formatMask = state.formatMask[info->vertex.format];
  }

  if (state.pipeline->format != formatHash) {
    state.pipeline->format = formatHash;
    state.pipeline->info.vertex = *format;
    uint32_t missingLocations = shader->attributeMask & ~formatMask;

    if (missingLocations) {
      gpu_vertex_format* vertex = &state.pipeline->info.vertex;
      vertex->bufferCount++;
      vertex->bufferStrides[1] = 0;
      for (uint32_t i = 0; i < 32; i++) {
        if (missingLocations & (1u << i)) {
          vertex->attributes[vertex->attributeCount++] = (gpu_attribute) {
            .buffer = 1,
            .location = i,
            .offset = 0,
            .type = GPU_TYPE_F32x4
          };
        }
      }
    }

    state.pipeline->dirty = true;
  }

  if (state.pipeline->dirty) {
    uint32_t hash = hash32(&state.pipeline->info, sizeof(gpu_pipeline_info));
    uint32_t mask = COUNTOF(state.pipelines) - 1;
    uint32_t bucket = hash & mask;

    while (state.pipelineLookup[bucket] && state.pipelineLookup[bucket] >> 32 != hash) {
      bucket = (bucket + 1) & mask;
    }

    if (!state.pipelineLookup[bucket]) {
      uint32_t index = state.pipelineCount++;
      lovrCheck(index < COUNTOF(state.pipelines), "Too many pipelines, please report this encounter");
      lovrAssert(gpu_pipeline_init_graphics(state.pipelines[index], &state.pipeline->info, 1), "Failed to initialize pipeline");
      state.pipelineLookup[bucket] = ((uint64_t) hash << 32) | index;
      state.pipeline->index = pipelineIndex = index;
      pipeline = state.pipelines[pipelineIndex];
    } else {
      state.pipeline->index = pipelineIndex = state.pipelineLookup[bucket] & 0xffff;
      pipeline = state.pipelines[pipelineIndex];
    }

    state.pipeline->dirty = false;
  } else {
    pipelineIndex = state.pipeline->index;
    pipeline = state.pipelines[pipelineIndex];
  }

  // Bundle

  gpu_bundle* bundle;
  uint32_t bundleIndex;

  if (state.bindingsDirty && shader->resourceCount > 0) {
    gpu_binding* bindings = talloc(shader->resourceCount * sizeof(gpu_binding));

    for (uint32_t i = 0; i < shader->resourceCount; i++) {
      bindings[i] = state.bindings[shader->resourceSlots[i]];
    }

    gpu_bundle_info info = {
      .layout = state.layouts[shader->layout],
      .bindings = bindings
    };

    if (batch) {
      bundleIndex = batch->bundleCount++;
      batch->bundleInfo[bundleIndex] = info;
    } else {
      bundle = allocateBundle(shader->layout);
      gpu_bundle_write(&bundle, &info, 1);
    }

    state.bindingsDirty = false;
  } else {
    bundle = state.boundBundle;
    bundleIndex = batch ? batch->bundleCount - 1 : 0;
  }

  // Buffers

  Megaview vertexBuffer;
  Megaview indexBuffer;

  bool hasVertices = info->vertex.buffer || info->vertex.data || info->vertex.pointer;
  bool hasIndices = info->index.buffer || info->index.data || info->index.pointer;
  gpu_index_type indexType;

  uint32_t start;
  uint32_t count;
  uint32_t baseVertex;

  if (hasVertices) {
    if (info->vertex.buffer) {
      Buffer* buffer = info->vertex.buffer;
      lovrCheck(buffer->info.type == BUFFER_VERTEX, "Buffers must have the 'vertex' type to use them for mesh vertices");

      vertexBuffer = buffer->mega;
      start = info->start + vertexBuffer.offset / buffer->info.stride;
      count = info->count;

      if (!buffer->transient && buffer != state.geometry.vertices) {
        BufferAccess access = { buffer, GPU_PHASE_INPUT_VERTEX, GPU_CACHE_VERTEX };
        arr_bufferaccess_t* buffers = state.batch ? &state.batch->buffers : &state.pass->buffers;
        arr_push(buffers, access);
        lovrRetain(buffer);
      }
    } else {
      uint32_t stride = format->bufferStrides[0];
      uint32_t size = info->vertex.count * stride;

      vertexBuffer = allocateBuffer(GPU_MEMORY_CPU_WRITE, size, stride);
      start = vertexBuffer.offset / stride;
      count = info->vertex.count;

      if (info->vertex.pointer) {
        *info->vertex.pointer = vertexBuffer.data;
      } else {
        memcpy(vertexBuffer.data, info->vertex.data, size);
      }
    }
  } else if (!hasIndices) {
    start = info->start;
    count = info->count;
  }

  if (hasIndices) {
    baseVertex = info->base + (hasVertices ? vertexBuffer.offset / format->bufferStrides[0] : 0);
    if (info->index.buffer) {
      Buffer* buffer = info->index.buffer;
      lovrCheck(buffer->info.type == BUFFER_INDEX, "Buffers must have the 'index' type to use them for mesh indices");

      indexBuffer = buffer->mega;
      indexType = buffer->info.stride == 4 ? GPU_INDEX_U32 : GPU_INDEX_U16;
      start = info->start + indexBuffer.offset / buffer->info.stride;
      count = info->count;

      if (!buffer->transient) {
        BufferAccess access = { buffer, GPU_PHASE_INPUT_INDEX, GPU_CACHE_INDEX };
        arr_bufferaccess_t* buffers = state.batch ? &state.batch->buffers : &state.pass->buffers;
        arr_push(buffers, access);
        lovrRetain(buffer);
      }
    } else {
      uint32_t stride = info->index.stride ? info->index.stride : sizeof(uint16_t);
      uint32_t size = info->index.count * stride;

      indexBuffer = allocateBuffer(GPU_MEMORY_CPU_WRITE, size, stride);
      indexType = stride == 4 ? GPU_INDEX_U32 : GPU_INDEX_U16;
      start = indexBuffer.offset / stride;
      count = info->index.count;

      if (info->index.pointer) {
        *info->index.pointer = indexBuffer.data;
      } else {
        memcpy(indexBuffer.data, info->index.data, size);
      }
    }
  }

  // Persistent batches copy immediate vertex/index data their stash, a device-local buffer
  /*if (batch && !batch->transient) {
    if (!info->vertex.buffer) {
      uint32_t stride = format->bufferStrides[0];
      uint32_t size = info->vertex.count * stride;

      if (batch->stashCursor % stride != 0) {
        batch->stashCursor += stride - batch->stashCursor % stride;
      }

      gpu_buffer_copy(state.pass->stream, vertexBuffer.gpu, batch->stash.gpu, vertexBuffer.offset, batch->stash.offset, 256 * 64);
      vertexBuffer = batch->stash;
      batch->stashCursor += size;
    }

    if (!info->index.buffer) {
      gpu_buffer_copy(state.pass->stream, src->gpu, dst->gpu, src->offset, dst->offset, 256 * 16);
      indexBuffer = batch->stash;
    }
  }*/

  // Uniforms

  // Grab new scratch buffers if they're full/non-existent (does not apply to transient batches)
  if ((state.drawCursor & 0xff) == 0 && (!batch || !batch->info.transient)) {
    if (batch) {
      Megaview* src = &state.drawBuffer;
      Megaview* dst = &batch->drawBuffer;
      gpu_copy_buffers(state.pass->stream, src->gpu, dst->gpu, src->offset, dst->offset, 256 * sizeof(DrawData));
    }

    state.drawBuffer = allocateBuffer(GPU_MEMORY_CPU_WRITE, 256 * sizeof(DrawData), state.limits.uniformBufferAlign);
  }

  float m[16];
  if (transform) {
    transform = mat4_mul(mat4_init(m, state.matrix), transform);
  } else {
    transform = state.matrix;
  }

  float normalMatrix[16];
  mat4_init(normalMatrix, transform);
  mat4_cofactor(normalMatrix);

  DrawData* draw = (DrawData*) state.drawBuffer.data;
  memcpy(draw->transform, transform, 64);
  memcpy(draw->normalMatrix, normalMatrix, 64);
  memcpy(draw->color, &state.pipeline->color, 16);
  state.drawBuffer.data += sizeof(DrawData);

  // Draw

  uint32_t instances = MAX(info->instances, 1);
  uint32_t baseInstance = state.drawCursor;

  if (batch) {
    lovrCheck(batch->drawCount < batch->info.capacity, "Batch is out of draws, try creating it with a higher capacity");
    lovrCheck(!info->indirect, "Indirect draws can not be recorded to a Batch");

    uint32_t id = batch->drawCount++;
    batch->activeDraws[batch->activeDrawCount++] = id;

    BatchDraw* draw = &batch->draws[id];
    draw->pipeline = (uint16_t) pipelineIndex;
    draw->bundle = (uint16_t) bundleIndex;
    draw->vertexBuffer = hasVertices ? (uint8_t) vertexBuffer.index : 0xff;
    draw->indexBuffer = hasIndices ? (uint8_t) indexBuffer.index : 0xff;
    draw->flags = 0;
    draw->flags |= hasVertices ? FLAG_VERTEX : 0;
    draw->flags |= hasIndices ? FLAG_INDEX : 0;
    draw->flags |= indexType == GPU_INDEX_U32 ? FLAG_INDEX32 : 0;
    draw->start = start;
    draw->count = count;
    draw->instances = instances;
    draw->baseVertex = baseVertex;
    return id;
  } else {
    if (pipeline != state.boundPipeline) {
      gpu_bind_pipeline_graphics(state.pass->stream, pipeline);
      state.boundPipeline = pipeline;
      state.stats.pipelineBinds++;
    }

    // TODO cache this better and use dynamic offsets more and dedupe with replay and stuff
    if (state.cameraDirty || (state.drawCursor & 0xff) == 0) {
      if (state.cameraDirty) {
        for (uint32_t i = 0; i < state.viewCount; i++) {
          mat4_init(state.cameras[i].viewProjection, state.cameras[i].projection);
          mat4_mul(state.cameras[i].viewProjection, state.cameras[i].view);
          mat4_init(state.cameras[i].inverseViewProjection, state.cameras[i].viewProjection);
          mat4_invert(state.cameras[i].inverseViewProjection);
        }

        uint32_t size = state.viewCount * sizeof(Camera);
        state.cameraBuffer = allocateBuffer(GPU_MEMORY_CPU_WRITE, size, state.limits.uniformBufferAlign);
        memcpy(state.cameraBuffer.data, state.cameras, size);
        state.cameraDirty = false;
      }

      gpu_bundle_info uniforms = {
        .layout = state.layouts[0],
        .bindings = (gpu_binding[]) {
          [0].buffer = { state.cameraBuffer.gpu, state.cameraBuffer.offset, state.viewCount * sizeof(Camera) },
          [1].buffer = { state.drawBuffer.gpu, state.drawBuffer.offset, 256 * sizeof(DrawData) },
          [2].sampler = state.defaultSamplers[0]->gpu,
          [3].sampler = state.defaultSamplers[1]->gpu,
          [4].sampler = state.defaultSamplers[2]->gpu,
          [5].sampler = state.defaultSamplers[3]->gpu
        }
      };
      gpu_bundle* uniformBundle = allocateBundle(0);
      gpu_bundle_write(&uniformBundle, &uniforms, 1);
      uint32_t dynamicOffsets[3] = { 0 };
      gpu_bind_bundle(state.pass->stream, pipeline, false, 0, uniformBundle, dynamicOffsets, 3);
      state.stats.bundleBinds++;
    }

    if (material != state.boundMaterial) {
      lovrCheck(material->block == shader->material, "Material is not compatible with active Shader");
      gpu_bundle* bundle = (gpu_bundle*) ((char*) state.materials[material->block].bundles + material->index * gpu_sizeof_bundle());
      gpu_bind_bundle(state.pass->stream, pipeline, false, 1, bundle, NULL, 0);
      state.boundMaterial = material;
      state.stats.bundleBinds++;
    }

    if (bundle != state.boundBundle) {
      gpu_bind_bundle(state.pass->stream, pipeline, false, 2, bundle, NULL, 0);
      state.boundBundle = bundle;
      state.stats.bundleBinds++;
    }

    if (hasVertices && vertexBuffer.gpu != state.boundVertexBuffer) {
      gpu_buffer* buffers[2] = { vertexBuffer.gpu, state.zeros.gpu };
      gpu_bind_vertex_buffers(state.pass->stream, buffers, (uint32_t[2]) { 0, 0 }, 0, 2);
      state.boundVertexBuffer = vertexBuffer.gpu;
    }

    if (hasIndices && (indexBuffer.gpu != state.boundIndexBuffer || indexType != state.boundIndexType)) {
      gpu_bind_index_buffer(state.pass->stream, indexBuffer.gpu, 0, indexType);
      state.boundIndexBuffer = indexBuffer.gpu;
      state.boundIndexType = indexType;
    }

    if (state.constantsDirty && shader->constantSize > 0) {
      gpu_push_constants(state.pass->stream, pipeline, state.constantData, shader->constantSize);
      state.constantsDirty = false;
    }

    if (info->indirect) {
      lovrCheck(info->indirect->info.type == BUFFER_COMPUTE, "Buffer must be created with the 'compute' type to use it for indirect rendering");
      lovrCheck(info->offset % 4 == 0, "Indirect render offset must be a multiple of 4");
      if (hasIndices) {
        lovrCheck(info->offset + 20 <= info->indirect->size, "Indirect render offset overflows the Buffer");
        gpu_draw_indirect_indexed(state.pass->stream, info->indirect->mega.gpu, info->offset, info->count);
      } else {
        lovrCheck(info->offset + 16 <= info->indirect->size, "Indirect render offset overflows the Buffer");
        gpu_draw_indirect(state.pass->stream, info->indirect->mega.gpu, info->offset, info->count);
      }
    } else {
      if (hasIndices) {
        gpu_draw_indexed(state.pass->stream, count, instances, start, baseVertex, baseInstance);
      } else {
        gpu_draw(state.pass->stream, count, instances, start, baseInstance);
      }
    }

    return state.drawCursor++;
  }
}

uint32_t lovrGraphicsPoints(Material* material, uint32_t count, float** vertices) {
  return lovrGraphicsMesh(&(DrawInfo) {
    .mode = DRAW_POINTS,
    .material = material,
    .vertex.format = VERTEX_POINT,
    .vertex.pointer = (void**) vertices,
    .vertex.count = count
  }, NULL);
}

uint32_t lovrGraphicsLine(Material* material, uint32_t count, float** vertices) {
  uint16_t* indices;

  uint32_t id = lovrGraphicsMesh(&(DrawInfo) {
    .mode = DRAW_LINES,
    .material = material,
    .vertex.format = VERTEX_POINT,
    .vertex.pointer = (void**) vertices,
    .vertex.count = count,
    .index.pointer = (void**) &indices,
    .index.count = 2 * (count - 1)
  }, NULL);

  for (uint32_t i = 0; i < count; i++) {
    indices[2 * i + 0] = i;
    indices[2 * i + 1] = i + 1;
  }

  return id;
}

uint32_t lovrGraphicsPlane(Material* material, float* transform, uint32_t detail) {
  detail = MIN(detail, 7);
  return lovrGraphicsMesh(&(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .material = material,
    .vertex.buffer = state.geometry.vertices,
    .index.buffer = state.geometry.indices,
    .start = state.geometry.start[SHAPE_GRID][detail],
    .count = state.geometry.count[SHAPE_GRID][detail],
    .base = state.geometry.base[SHAPE_GRID]
  }, transform);
}

uint32_t lovrGraphicsBox(Material* material, float* transform) {
  return lovrGraphicsMesh(&(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .material = material,
    .vertex.buffer = state.geometry.vertices,
    .index.buffer = state.geometry.indices,
    .start = state.geometry.start[SHAPE_CUBE][0],
    .count = state.geometry.count[SHAPE_CUBE][0],
    .base = state.geometry.base[SHAPE_CUBE]
  }, transform);
}

uint32_t lovrGraphicsCircle(Material* material, float* transform, uint32_t detail) {
  detail = MIN(detail, 6);
  return lovrGraphicsMesh(&(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .material = material,
    .vertex.buffer = state.geometry.vertices,
    .index.buffer = state.geometry.indices,
    .start = state.geometry.start[SHAPE_CONE][detail],
    .count = 3 * ((4 << detail) - 2),
    .base = state.geometry.base[SHAPE_CONE]
  }, transform);
}

uint32_t lovrGraphicsCone(Material* material, float* transform, uint32_t detail) {
  detail = MIN(detail, 6);
  return lovrGraphicsMesh(&(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .material = material,
    .vertex.buffer = state.geometry.vertices,
    .index.buffer = state.geometry.indices,
    .start = state.geometry.start[SHAPE_CONE][detail],
    .count = state.geometry.count[SHAPE_CONE][detail],
    .base = state.geometry.base[SHAPE_CONE]
  }, transform);
}

uint32_t lovrGraphicsCylinder(Material* material, mat4 transform, uint32_t detail, bool capped) {
  detail = MIN(detail, 6);
  uint32_t capIndexCount = 3 * ((4 << detail) - 2);
  return lovrGraphicsMesh(&(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .material = material,
    .vertex.buffer = state.geometry.vertices,
    .index.buffer = state.geometry.indices,
    .start = state.geometry.start[SHAPE_TUBE][detail],
    .count = state.geometry.count[SHAPE_TUBE][detail] - (capped ? 0 : 2 * capIndexCount),
    .base = state.geometry.base[SHAPE_TUBE]
  }, transform);
}

uint32_t lovrGraphicsSphere(Material* material, mat4 transform, uint32_t detail) {
  detail = MIN(detail, 4);
  return lovrGraphicsMesh(&(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .material = material,
    .vertex.buffer = state.geometry.vertices,
    .index.buffer = state.geometry.indices,
    .start = state.geometry.start[SHAPE_BALL][detail],
    .count = state.geometry.count[SHAPE_BALL][detail],
    .base = state.geometry.base[SHAPE_BALL]
  }, transform);
}

uint32_t lovrGraphicsSkybox(Material* material) {
  return lovrGraphicsMesh(&(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .shader = SHADER_CUBE, // TODO? type == TEXTURE_CUBE ? SHADER_CUBE : SHADER_PANO,
    .material = material,
    .vertex.format = VERTEX_EMPTY,
    .count = 3
  }, NULL);
}

uint32_t lovrGraphicsFill(Material* material) {
  return lovrGraphicsMesh(&(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .shader = SHADER_FILL,
    .material = material,
    .vertex.format = VERTEX_EMPTY,
    .count = 3
  }, NULL);
}

static void renderModelNode(Model* model, uint32_t index, bool children, uint32_t instances) {
  ModelNode* node = &model->data->nodes[index];
  mat4 globalTransform = model->globalTransforms + 16 * index;

  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    DrawInfo draw = model->draws[node->primitiveIndex + i]; // Make a copy for thread safety
    draw.instances = instances;
    lovrGraphicsMesh(&draw, globalTransform);
  }

  if (children) {
    for (uint32_t i = 0; i < node->childCount; i++) {
      renderModelNode(model, node->children[i], true, instances);
    }
  }
}

void lovrGraphicsModel(Model* model, mat4 transform, uint32_t node, bool children, uint32_t instances) {
  updateModelTransforms(model, model->data->rootNode, (float[]) MAT4_IDENTITY);

  if (node == ~0u) {
    node = model->data->rootNode;
  }

  lovrGraphicsPush(STACK_TRANSFORM, NULL);
  lovrGraphicsTransform(transform);
  renderModelNode(model, node, children, instances);
  lovrGraphicsPop(STACK_TRANSFORM);
}

uint32_t lovrGraphicsPrint(Font* font, const char* text, uint32_t length, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign) {
  lovrThrow("TODO");
}

void lovrGraphicsReplay(Batch* batch) {
  lovrCheck(state.pass && state.pass->type == PASS_RENDER, "Replaying a Batch can only happen inside a render pass");
  if (batch->activeDrawCount == 0) return;

  // Uniforms

  if (state.cameraDirty) {
    for (uint32_t i = 0; i < state.viewCount; i++) {
      mat4_init(state.cameras[i].viewProjection, state.cameras[i].projection);
      mat4_mul(state.cameras[i].viewProjection, state.cameras[i].view);
      mat4_init(state.cameras[i].inverseViewProjection, state.cameras[i].viewProjection);
      mat4_invert(state.cameras[i].inverseViewProjection);
    }

    uint32_t size = state.viewCount * sizeof(Camera);
    state.cameraBuffer = allocateBuffer(GPU_MEMORY_CPU_WRITE, size, state.limits.uniformBufferAlign);
    memcpy(state.cameraBuffer.data, state.cameras, state.viewCount * size);
    state.cameraDirty = false;
  }

  gpu_bundle_info uniforms = {
    .layout = state.layouts[0],
    .bindings = (gpu_binding[]) {
      [0].buffer = { state.cameraBuffer.gpu, state.cameraBuffer.offset, state.viewCount * sizeof(Camera) },
      [1].buffer = { batch->drawBuffer.gpu, batch->drawBuffer.offset, 256 * sizeof(DrawData) },
      [2].sampler = state.defaultSamplers[0]->gpu,
      [3].sampler = state.defaultSamplers[1]->gpu,
      [4].sampler = state.defaultSamplers[2]->gpu,
      [5].sampler = state.defaultSamplers[3]->gpu
    }
  };
  gpu_bundle* uniformBundle = allocateBundle(0);
  gpu_bundle_write(&uniformBundle, &uniforms, 1);
  uint32_t dynamicOffsets[3] = { 0 };

  // Group draws

  if (batch->groupedCount == 0) {
    BatchGroup* group = &batch->groups[0];
    BatchDraw* draw = &batch->draws[batch->activeDraws[0]];
    group->count = 1;
    group->dirty = 0;
    group->dirty |= DIRTY_PIPELINE;
    group->dirty |= (draw->flags & FLAG_VERTEX) ? DIRTY_VERTEX : 0;
    group->dirty |= (draw->flags & FLAG_INDEX) ? DIRTY_INDEX : 0;
    group->dirty |= DIRTY_CHUNK;
    group->dirty |= batch->bundleCount > 0 ? DIRTY_BUNDLE : 0;
    batch->groupedCount = 1;
    batch->groupCount = 1;
  }

  for (uint32_t i = batch->groupedCount; i < batch->activeDrawCount; i++, batch->groupedCount++) {
    BatchDraw* a = &batch->draws[batch->activeDraws[i - 0]];
    BatchDraw* b = &batch->draws[batch->activeDraws[i - 1]];

    bool pipelineChanged = a->pipeline != b->pipeline;
    bool vertexBufferChanged = (a->flags & FLAG_VERTEX) > (b->flags & FLAG_VERTEX) || a->vertexBuffer != b->vertexBuffer;
    bool indexBufferChanged = (a->flags & FLAG_INDEX) > (b->flags & FLAG_INDEX) || a->indexBuffer != b->indexBuffer;
    bool indexTypeChanged = (a->flags & FLAG_INDEX32) != (b->flags & FLAG_INDEX32);
    bool chunkChanged = batch->activeDraws[i] >> 8 != batch->activeDraws[i - 1] >> 8;
    bool bundleChanged = a->bundle != b->bundle;

    uint16_t dirty = 0;
    dirty |= -pipelineChanged & DIRTY_PIPELINE;
    dirty |= -vertexBufferChanged & DIRTY_VERTEX;
    dirty |= -(indexBufferChanged || indexTypeChanged) & DIRTY_INDEX;
    dirty |= -chunkChanged & DIRTY_CHUNK;
    dirty |= -bundleChanged & DIRTY_BUNDLE;

    if (dirty) {
      batch->groups[batch->groupCount++] = (BatchGroup) { .dirty = dirty, .count = 1 };
    } else {
      batch->groups[batch->groupCount - 1].count++;
    }
  }

  // Draws

  uint32_t activeDrawIndex = 0;
  BatchGroup* group = batch->groups;
  for (uint32_t i = 0; i < batch->groupCount; i++, group++) {
    uint32_t index = batch->activeDraws[activeDrawIndex];
    BatchDraw* first = &batch->draws[index];
    gpu_pipeline* pipeline = state.pipelines[first->pipeline];

    if (group->dirty & DIRTY_PIPELINE) {
      gpu_bind_pipeline_graphics(state.pass->stream, pipeline);
      state.boundPipeline = pipeline;
      state.stats.pipelineBinds++;
    }

    if (group->dirty & DIRTY_VERTEX) {
      uint32_t offsets[2] = { 0, 0 };
      gpu_buffer* buffers[2] = { state.buffers.list[first->vertexBuffer].gpu, state.zeros.gpu };
      gpu_bind_vertex_buffers(state.pass->stream, buffers, offsets, 0, 2);
      state.boundVertexBuffer = buffers[0];
    }

    if (group->dirty & DIRTY_INDEX) {
      gpu_index_type type = (first->flags & FLAG_INDEX32) ? GPU_INDEX_U32 : GPU_INDEX_U16;
      gpu_bind_index_buffer(state.pass->stream, state.buffers.list[first->indexBuffer].gpu, 0, type);
      state.boundIndexBuffer = state.buffers.list[first->indexBuffer].gpu;
      state.boundIndexType = type;
    }

    if (group->dirty & DIRTY_CHUNK) {
      dynamicOffsets[1] = (index >> 8) * 256 * 64;
      dynamicOffsets[2] = (index >> 8) * 256 * 16;
      gpu_bind_bundle(state.pass->stream, pipeline, false, 0, uniformBundle, dynamicOffsets, 3);
      state.stats.bundleBinds++;
    }

    if (group->dirty & DIRTY_BUNDLE) {
      gpu_bind_bundle(state.pass->stream, pipeline, false, 1, batch->bundles[first->bundle], NULL, 0);
      state.boundBundle = batch->bundles[first->bundle];
      state.stats.bundleBinds++;
    }

    if (first->flags & FLAG_INDEX) {
      for (uint16_t j = 0; j < group->count; j++) {
        index = batch->activeDraws[activeDrawIndex++];
        BatchDraw* draw = &batch->draws[index];
        gpu_draw_indexed(state.pass->stream, draw->count, draw->instances, draw->start, draw->baseVertex, index);
      }
    } else {
      for (uint16_t j = 0; j < group->count; j++) {
        index = batch->activeDraws[activeDrawIndex++];
        BatchDraw* draw = &batch->draws[index];
        gpu_draw(state.pass->stream, draw->count, draw->instances, draw->start, index);
      }
    }
  }

  for (uint32_t i = 0; i < batch->buffers.length; i++) {
    lovrRetain(batch->buffers.data[i].buffer);
  }

  for (uint32_t i = 0; i < batch->textures.length; i++) {
    lovrRetain(batch->textures.data[i].texture);
  }

  arr_append(&state.pass->buffers, batch->buffers.data, batch->buffers.length);
  arr_append(&state.pass->textures, batch->textures.data, batch->textures.length);
  state.stats.drawCalls += batch->activeDrawCount;
}

void lovrGraphicsCompute(uint32_t x, uint32_t y, uint32_t z, Buffer* buffer, uint32_t offset) {
  lovrCheck(state.pass && state.pass->type == PASS_COMPUTE, "Compute shaders can only run inside of a compute pass");

  Shader* shader = state.pipeline->shader;
  lovrCheck(shader && shader->info.type == SHADER_COMPUTE, "A compute shader must be bound before dispatching compute work");

  lovrCheck(x <= state.limits.computeDispatchCount[0], "Compute %s count exceeds computeDispatchCount limit", "x");
  lovrCheck(y <= state.limits.computeDispatchCount[1], "Compute %s count exceeds computeDispatchCount limit", "y");
  lovrCheck(z <= state.limits.computeDispatchCount[2], "Compute %s count exceeds computeDispatchCount limit", "z");

  gpu_pipeline* pipeline = state.pipelines[shader->computePipelineIndex];

  if (pipeline != state.boundPipeline) {
    gpu_bind_pipeline_compute(state.pass->stream, pipeline);
    state.boundPipeline = pipeline;
  }

  if (state.constantsDirty && shader->constantSize > 0) {
    gpu_push_constants(state.pass->stream, pipeline, state.constantData, shader->constantSize);
    state.constantsDirty = false;
  }

  if (state.bindingsDirty && shader->resourceCount > 0) {
    gpu_binding bindings[32];

    for (uint32_t i = 0; i < shader->resourceCount; i++) {
      bindings[i] = state.bindings[shader->resourceSlots[i]];
    }

    gpu_bundle_info info = {
      .layout = state.layouts[shader->layout],
      .bindings = bindings
    };

    gpu_bundle* bundle = allocateBundle(shader->layout);
    gpu_bundle_write(&bundle, &info, 1);
    gpu_bind_bundle(state.pass->stream, pipeline, true, 1, bundle, NULL, 0);
    state.boundBundle = bundle;
    state.bindingsDirty = false;
    state.stats.bundleBinds++;
  }

  if (buffer) {
    lovrCheck(buffer->info.type == BUFFER_COMPUTE, "Buffer must be created with the 'compute' type to use it for indirect compute");
    lovrCheck(offset % 4 == 0, "Indirect compute offset must be a multiple of 4");
    lovrCheck(offset + 12 <= buffer->size, "Indirect compute offset overflows the Buffer");
    gpu_compute_indirect(state.pass->stream, buffer->mega.gpu, buffer->mega.offset + offset);
  } else {
    gpu_compute(state.pass->stream, x, y, z);
  }
}

// Buffer

Buffer* lovrBufferInit(BufferInfo* info, bool transient, void** mapping) {
  info->stride = info->stride ? info->stride : state.formats[info->format].bufferStrides[0];
  uint32_t size = info->length * info->stride;
  lovrCheck(size > 0, "Buffer size must be positive");
  lovrCheck(size <= 1 << 30, "Max Buffer size is 1GB");
  Buffer* buffer = transient ? talloc(sizeof(Buffer)) : calloc(1, sizeof(Buffer));
  lovrAssert(buffer, "Out of memory");
  buffer->ref = 1;
  buffer->size = size;
  uint32_t align = 1;
  switch (info->type) {
    case BUFFER_VERTEX: align = info->stride; break;
    case BUFFER_INDEX: align = 4; break;
    case BUFFER_UNIFORM: align = state.limits.uniformBufferAlign; break;
    case BUFFER_COMPUTE: align = state.limits.storageBufferAlign; break;
  }
  buffer->mega = allocateBuffer(transient ? GPU_MEMORY_CPU_WRITE : GPU_MEMORY_GPU, size, align);
  buffer->info = *info;
  buffer->transient = transient;
  if (!transient) {
    state.buffers.list[buffer->mega.index].refs++;
  }
  if (info->type == BUFFER_VERTEX) {
    if (info->fieldCount == 0) {
      buffer->format = state.formats[info->format];
      buffer->mask = state.formatMask[info->format];
      buffer->hash = state.formatHash[info->format];
    } else {
      lovrCheck(info->stride < state.limits.vertexBufferStride, "Buffer with 'vertex' type has a stride of %d bytes, which exceeds vertexBufferStride limit (%d)", info->stride, state.limits.vertexBufferStride);
      buffer->mask = 0;
      buffer->format.bufferCount = 1;
      buffer->format.attributeCount = info->fieldCount;
      buffer->format.bufferStrides[0] = info->stride;
      for (uint32_t i = 0; i < info->fieldCount; i++) {
        lovrCheck(info->locations[i] < 16, "Vertex buffer attribute location %d is too big (max is 15)", info->locations[i]);
        lovrCheck(info->offsets[i] < 256, "Vertex buffer attribute offset %d is too big (max is 255)", info->offsets[i]);
        buffer->format.attributes[i] = (gpu_attribute) {
          .buffer = 0,
          .location = info->locations[i],
          .offset = info->offsets[i],
          .type = (gpu_type) info->types[i]
        };
        buffer->mask |= 1 << info->locations[i];
      }
      buffer->hash = hash64(&buffer->format, sizeof(buffer->format));
    }
  }
  return buffer;
}

Buffer* lovrGraphicsGetBuffer(BufferInfo* info, void** data) {
  Buffer* buffer = lovrBufferInit(info, true, data);
  if (data) *data = buffer->mega.data;
  return buffer;
}

Buffer* lovrBufferCreate(BufferInfo* info, void** data) {
  state.stats.buffers++;
  Buffer* buffer = lovrBufferInit(info, false, data);
  if (data) {
    if (buffer->mega.data) {
      *data = buffer->mega.data;
    } else {
      // TODO maybe there is a way to use lovrBufferMap here (maybe transfer passes are optional)
      lovrGraphicsPrepare();
      Megaview scratch = allocateBuffer(GPU_MEMORY_CPU_WRITE, buffer->size, 4);
      gpu_copy_buffers(state.uploads->stream, scratch.gpu, buffer->mega.gpu, scratch.offset, buffer->mega.offset, buffer->size);
      state.stats.copies++;
      *data = scratch.data;
    }
  }
  return buffer;
}

void lovrBufferDestroy(void* ref) {
  Buffer* buffer = ref;
  if (!buffer->transient) {
    if (--state.buffers.list[buffer->mega.index].refs == 0) {
      recycleBuffer(buffer->mega.index, GPU_MEMORY_GPU);
    }
    state.stats.buffers--;
    free(buffer);
  }
}

const BufferInfo* lovrBufferGetInfo(Buffer* buffer) {
  return &buffer->info;
}

void* lovrBufferMap(Buffer* buffer, uint32_t offset, uint32_t size) {
  if (size == ~0u) size = buffer->size - offset;
  lovrCheck(offset + size <= buffer->size, "Tried to write past the end of the Buffer");
  if (buffer->transient) {
    return buffer->mega.data + offset;
  } else {
    lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Writing to persistent buffers can only happen in a transfer pass");
    Megaview scratch = allocateBuffer(GPU_MEMORY_CPU_WRITE, size, 4);
    gpu_copy_buffers(state.pass->stream, scratch.gpu, buffer->mega.gpu, scratch.offset, buffer->mega.offset + offset, size);
    BufferAccess access = { buffer, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_WRITE };
    arr_push(&state.pass->buffers, access);
    lovrRetain(buffer);
    state.stats.copies++;
    return scratch.data;
  }
}

void lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t size) {
  lovrCheck(offset % 4 == 0, "Buffer clear offset must be a multiple of 4");
  lovrCheck(size % 4 == 0, "Buffer clear size must be a multiple of 4");
  lovrCheck(offset + size <= buffer->size, "Tried to clear past the end of the Buffer");
  if (buffer->transient) {
    memset(buffer->mega.data + offset, 0, size);
  } else {
    lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Clearing persistent buffers can only happen in a transfer pass");
    gpu_clear_buffer(state.pass->stream, buffer->mega.gpu, buffer->mega.offset + offset, size);
    BufferAccess access = { buffer, GPU_PHASE_CLEAR, GPU_CACHE_TRANSFER_WRITE };
    arr_push(&state.pass->buffers, access);
    lovrRetain(buffer);
    state.stats.copies++;
  }
}

void lovrBufferCopy(Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size) {
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Buffer copies can only happen in a transfer pass");
  lovrCheck(!dst->transient, "Unable to copy to transient Buffers");
  lovrCheck(srcOffset + size <= src->size, "Tried to read past the end of the source Buffer");
  lovrCheck(dstOffset + size <= dst->size, "Tried to copy past the end of the destination Buffer");
  gpu_copy_buffers(state.pass->stream, src->mega.gpu, dst->mega.gpu, src->mega.offset + srcOffset, dst->mega.offset + dstOffset, size);
  BufferAccess accessSrc = { src, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_READ };
  BufferAccess accessDst = { dst, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_WRITE };
  arr_push(&state.pass->buffers, accessSrc);
  arr_push(&state.pass->buffers, accessDst);
  lovrRetain(src);
  lovrRetain(dst);
  state.stats.copies++;
}

void lovrBufferRead(Buffer* buffer, uint32_t offset, uint32_t size, void (*callback)(void*, uint32_t, void*), void* userdata) {
  ReaderPool* readers = &state.readers;
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Reading buffer data can only happen in a transfer pass");
  lovrCheck(!buffer->transient, "Can not read from transient Buffers");
  lovrCheck(offset + size <= buffer->size, "Tried to read past the end of the Buffer");
  lovrCheck(readers->head - readers->tail != COUNTOF(readers->list), "Too many readbacks"); // TODO emergency waitIdle instead
  Megaview scratch = allocateBuffer(GPU_MEMORY_CPU_READ, size, 4);
  gpu_copy_buffers(state.pass->stream, buffer->mega.gpu, scratch.gpu, buffer->mega.offset + offset, scratch.offset, size);
  readers->list[readers->head++ & 0xf] = (Reader) {
    .callback = callback,
    .userdata = userdata,
    .data = scratch.data,
    .size = size,
    .tick = state.tick
  };
  BufferAccess access = { buffer, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_READ };
  arr_push(&state.pass->buffers, access);
  lovrRetain(buffer);
  state.stats.copies++;
}

// Texture

Texture* lovrGraphicsGetWindowTexture() {
  if (!state.window) {
    state.window = calloc(1, sizeof(Texture));
    lovrAssert(state.window, "Out of memory");
    int width, height;
    os_window_get_fbsize(&width, &height); // Currently immutable
    state.window->ref = 1;
    state.window->info.type = TEXTURE_2D;
    state.window->info.usage = TEXTURE_RENDER;
    state.window->info.format = ~0u;
    state.window->info.width = width;
    state.window->info.height = height;
    state.window->info.depth = 1;
    state.window->info.mipmaps = 1;
    state.window->info.samples = 1;
    state.window->info.srgb = true;
  }

  if (!state.window->gpu) {
    state.window->gpu = gpu_surface_acquire();
    state.window->renderView = state.window->gpu;
  }

  return state.window;
}

Texture* lovrGraphicsGetDefaultTexture() {
  if (state.defaultTexture) return state.defaultTexture;
  lovrGraphicsPrepare();
  state.defaultTexture = lovrTextureCreate(&(TextureInfo) {
    .type = TEXTURE_2D,
    .usage = TEXTURE_SAMPLE | TEXTURE_COPY,
    .format = FORMAT_RGBA8,
    .width = 4,
    .height = 4,
    .depth = 1,
    .mipmaps = 1,
    .samples = 1,
    .srgb = false,
    .label = "white"
  });
  gpu_clear_texture(state.uploads->stream, state.defaultTexture->gpu, 0, 1, 0, 1, (float[4]) { 1.f, 1.f, 1.f, 1.f });
  arr_push(&state.uploads->textures, (TextureAccess) { 0 }); // TODO maybe better way to ensure upload stream gets submitted
  return state.defaultTexture;
}

Texture* lovrTextureCreate(TextureInfo* info) {
  lovrGraphicsPrepare();

  uint32_t limits[] = {
    [TEXTURE_2D] = state.limits.textureSize2D,
    [TEXTURE_CUBE] = state.limits.textureSizeCube,
    [TEXTURE_ARRAY] = state.limits.textureSize2D,
    [TEXTURE_VOLUME] = state.limits.textureSize3D
  };

  uint32_t limit = limits[info->type];
  uint32_t mips = log2(MAX(MAX(info->width, info->height), (info->type == TEXTURE_VOLUME ? info->depth : 1))) + 1;
  uint8_t supports = state.features.formats[info->format];

  lovrCheck(info->width > 0, "Texture width must be greater than zero");
  lovrCheck(info->height > 0, "Texture height must be greater than zero");
  lovrCheck(info->depth > 0, "Texture depth must be greater than zero");
  lovrCheck(info->width <= limit, "Texture %s exceeds the limit for this texture type (%d)", "width", limit);
  lovrCheck(info->height <= limit, "Texture %s exceeds the limit for this texture type (%d)", "height", limit);
  lovrCheck(info->depth <= limit || info->type != TEXTURE_VOLUME, "Texture %s exceeds the limit for this texture type (%d)", "depth", limit);
  lovrCheck(info->depth <= state.limits.textureLayers || info->type != TEXTURE_ARRAY, "Texture %s exceeds the limit for this texture type (%d)", "depth", limit);
  lovrCheck(info->depth == 1 || info->type != TEXTURE_2D, "2D textures must have a depth of 1");
  lovrCheck(info->depth == 6 || info->type != TEXTURE_CUBE, "Cubemaps must have a depth of 6");
  lovrCheck(info->width == info->height || info->type != TEXTURE_CUBE, "Cubemaps must be square");
  lovrCheck(measureTexture(info->format, info->width, info->height, info->depth) < 1 << 30, "Memory for a Texture can not exceed 1GB");
  lovrCheck(info->samples == 1 || info->samples == 4, "Currently, Texture multisample count must be 1 or 4");
  lovrCheck(info->samples == 1 || info->type != TEXTURE_CUBE, "Cubemaps can not be multisampled");
  lovrCheck(info->samples == 1 || info->type != TEXTURE_VOLUME, "Volume textures can not be multisampled");
  lovrCheck(info->samples == 1 || ~info->usage & TEXTURE_STORAGE, "Currently, Textures with the 'storage' flag can not be multisampled");
  lovrCheck(info->samples == 1 || info->mipmaps == 1, "Multisampled textures can only have 1 mipmap");
  lovrCheck(~info->usage & TEXTURE_SAMPLE || (supports & GPU_FEATURE_SAMPLE), "GPU does not support the 'sample' flag for this format");
  lovrCheck(~info->usage & TEXTURE_RENDER || (supports & GPU_FEATURE_RENDER), "GPU does not support the 'render' flag for this format");
  lovrCheck(~info->usage & TEXTURE_STORAGE || (supports & GPU_FEATURE_STORAGE), "GPU does not support the 'storage' flag for this format");
  lovrCheck(~info->usage & TEXTURE_RENDER || info->width <= state.limits.renderSize[0], "Texture has 'render' flag but its size exceeds renderSize limit");
  lovrCheck(~info->usage & TEXTURE_RENDER || info->height <= state.limits.renderSize[1], "Texture has 'render' flag but its size exceeds renderSize limit");
  lovrCheck(info->mipmaps == ~0u || info->mipmaps <= mips, "Texture has more than the max number of mipmap levels for its size (%d)", mips);
  lovrCheck((info->format != FORMAT_BC6 && info->format != FORMAT_BC7) || state.features.bptc, "BC6/BC7 textures are not supported on this GPU");
  lovrCheck(info->format < FORMAT_ASTC_4x4 || state.features.astc, "ASTC textures are not supported on this GPU");

  Texture* texture = calloc(1, sizeof(Texture) + gpu_sizeof_texture());
  lovrAssert(texture, "Out of memory");
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = *info;
  texture->ref = 1;

  if (texture->info.mipmaps == ~0u) {
    texture->info.mipmaps = mips;
  } else {
    texture->info.mipmaps = MAX(texture->info.mipmaps, 1);
  }

  uint32_t levelCount = 0;
  uint32_t levelOffsets[16];
  bool generateMipmaps = false;
  Megaview buffer = { 0 };

  if (info->images) {
    levelCount = info->images[0]->mipmapCount;
    generateMipmaps = levelCount < texture->info.mipmaps;
    uint32_t total = 0;
    for (uint32_t i = 0; i < levelCount; i++) {
      levelOffsets[i] = total;
      uint32_t w = MAX(info->width >> i, 1);
      uint32_t h = MAX(info->height >> i, 1);
      uint32_t size = measureTexture(info->format, w, h, 1);
      lovrAssert(size == info->images[i]->blob->size, "Image byte size does not match expected size (internal error)");
      total += size;
    }
    buffer = allocateBuffer(GPU_MEMORY_CPU_WRITE, total, 64);
    for (uint32_t i = 0; i < levelCount; i++) {
      memcpy(buffer.data + levelOffsets[i], info->images[i]->blob->data, info->images[i]->blob->size);
      levelOffsets[i] += buffer.offset;
    }
  }

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
      ((info->usage & TEXTURE_COPY) ? GPU_TEXTURE_COPY_SRC | GPU_TEXTURE_COPY_DST : 0),
    .srgb = info->srgb,
    .handle = info->handle,
    .upload = {
      .stream = state.uploads->stream,
      .buffer = buffer.gpu,
      .levelOffsets = levelOffsets,
      .levelCount = levelCount,
      .generateMipmaps = generateMipmaps
    },
    .label = info->label
  });
  arr_push(&state.uploads->textures, (TextureAccess) { 0 }); // TODO maybe better way to ensure upload stream gets submitted

  // Automatically create a renderable view for renderable non-volume textures
  if (info->usage & TEXTURE_RENDER && info->type != TEXTURE_VOLUME && info->depth <= 6) {
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

  if (!info->handle) {
    uint32_t size = measureTexture(info->format, info->width, info->height, info->depth);
    state.stats.memory += size;
    state.stats.textureMemory += size;
  }

  state.stats.textures++;
  return texture;
}

Texture* lovrTextureCreateView(TextureViewInfo* view) {
  const TextureInfo* info = &view->parent->info;
  uint32_t maxDepth = info->type == TEXTURE_VOLUME ? MAX(info->depth >> view->levelIndex, 1) : info->depth;
  lovrCheck(!info->parent, "Can't nest texture views");
  lovrCheck(view->type != TEXTURE_VOLUME, "Texture views may not be volume textures");
  lovrCheck(view->layerCount > 0, "Texture view must have at least one layer");
  lovrCheck(view->levelCount > 0, "Texture view must have at least one mipmap");
  lovrCheck(view->layerIndex + view->layerCount <= maxDepth, "Texture view layer range exceeds depth of parent texture");
  lovrCheck(view->levelIndex + view->levelCount <= info->mipmaps, "Texture view mipmap range exceeds mipmap count of parent texture");
  lovrCheck(view->layerCount == 1 || view->type != TEXTURE_2D, "2D texture can only have a single layer");
  lovrCheck(view->layerCount == 6 || view->type != TEXTURE_CUBE, "Cubemaps can only have a six layers");
  lovrCheck(view->levelCount == 1 || info->type != TEXTURE_VOLUME, "Views of volume textures may only have a single mipmap level");

  Texture* texture = calloc(1, sizeof(Texture) + gpu_sizeof_texture());
  lovrAssert(texture, "Out of memory");
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = *info;
  texture->ref = 1;

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

  if (view->levelCount == 1 && view->type != TEXTURE_VOLUME && view->layerCount <= 6) {
    texture->renderView = texture->gpu;
  }

  state.stats.textures++;
  lovrRetain(view->parent);
  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  const TextureInfo* info = &texture->info;
  if (texture != state.window) {
    lovrRelease(texture->info.parent, lovrTextureDestroy);
    if (texture->renderView && texture->renderView != texture->gpu) gpu_texture_destroy(texture->renderView);
    if (texture->gpu) gpu_texture_destroy(texture->gpu);
    if (!info->parent && !info->handle) {
      uint32_t size = measureTexture(info->format, info->width, info->height, info->depth);
      state.stats.memory -= size;
      state.stats.textureMemory -= size;
    }
    state.stats.textures--;
  }
  free(texture);
}

const TextureInfo* lovrTextureGetInfo(Texture* texture) {
  return &texture->info;
}

void lovrTextureWrite(Texture* texture, uint16_t offset[4], uint16_t extent[3], void* data, uint32_t step[2]) {
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Writing to a Texture can only happen in a transfer pass");
  lovrCheck(!texture->info.parent, "Texture views can not be written to");
  lovrCheck(texture->info.usage & TEXTURE_COPY, "Texture must have the 'copy' flag to write to it");
  lovrCheck(texture->info.samples == 1, "Multisampled Textures can not be written to");
  checkTextureBounds(&texture->info, offset, extent);

  size_t fullSize = measureTexture(texture->info.format, extent[0], extent[1], extent[2]);
  size_t rowSize = measureTexture(texture->info.format, extent[0], 1, 1);
  size_t imgSize = measureTexture(texture->info.format, extent[0], extent[1], 1);
  Megaview scratch = allocateBuffer(GPU_MEMORY_CPU_WRITE, fullSize, 64);
  size_t jump = step[0] ? step[0] : rowSize;
  size_t leap = step[1] ? step[1] : imgSize;
  char* src = data;
  char* dst = scratch.data;

  for (uint16_t z = 0; z < extent[2]; z++) {
    for (uint16_t y = 0; y < extent[1]; y++) {
      memcpy(dst, src, rowSize);
      dst += rowSize;
      src += jump;
    }
    dst += imgSize;
    src += leap;
  }

  gpu_copy_buffer_texture(state.pass->stream, scratch.gpu, texture->gpu, scratch.offset, offset, extent);
  TextureAccess access = { texture, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_WRITE };
  arr_push(&state.pass->textures, access);
  lovrRetain(texture);
  state.stats.copies++;
}

void lovrTexturePaste(Texture* texture, Image* image, uint16_t srcOffset[2], uint16_t dstOffset[4], uint16_t extent[2]) {
  lovrCheck(texture->info.format == image->format, "Texture and Image formats must match");
  lovrCheck(srcOffset[0] + extent[0] <= image->width, "Tried to read pixels past the width of the Image");
  lovrCheck(srcOffset[1] + extent[1] <= image->height, "Tried to read pixels past the height of the Image");
  uint16_t fullExtent[3] = { extent[0], extent[1], 1 };
  uint32_t step[2] = { measureTexture(image->format, image->width, 1, 1), 0 };
  size_t offsetx = measureTexture(image->format, srcOffset[0], 1, 1);
  size_t offsety = srcOffset[1] * step[0];
  char* data = (char*) image->blob->data + offsety + offsetx;
  lovrTextureWrite(texture, dstOffset, fullExtent, data, step);
}

void lovrTextureClear(Texture* texture, uint16_t layer, uint16_t layerCount, uint16_t level, uint16_t levelCount, float color[4]) {
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Clearing a Texture can only happen in a transfer pass");
  lovrCheck(!texture->info.parent, "Texture views can not be cleared");
  lovrCheck(!isDepthFormat(texture->info.format), "Currently only color textures can be cleared");
  lovrCheck(texture->info.type == TEXTURE_VOLUME || layer + layerCount <= texture->info.depth, "Texture clear range exceeds texture layer count");
  lovrCheck(level + levelCount <= texture->info.mipmaps, "Texture clear range exceeds texture mipmap count");
  gpu_clear_texture(state.pass->stream, texture->gpu, layer, layerCount, level, levelCount, color);
  TextureAccess access = { texture, GPU_PHASE_CLEAR, GPU_CACHE_TRANSFER_WRITE };
  arr_push(&state.pass->textures, access);
  lovrRetain(texture);
  state.stats.copies++;
}

void lovrTextureRead(Texture* texture, uint16_t offset[4], uint16_t extent[3], void (*callback)(void* data, uint32_t size, void* userdata), void* userdata) {
  ReaderPool* readers = &state.readers;
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Downloading a Texture can only happen in a transfer pass");
  lovrCheck(!texture->info.parent, "Texture views can not be read");
  lovrCheck(texture->info.usage & TEXTURE_COPY, "Texture must have the 'copy' flag to read from it");
  lovrCheck(texture->info.samples == 1, "Multisampled Textures can not be read");
  checkTextureBounds(&texture->info, offset, extent);
  lovrCheck(readers->head - readers->tail != COUNTOF(readers->list), "Too many readbacks"); // TODO emergency waitIdle instead
  size_t size = measureTexture(texture->info.format, extent[0], extent[1], extent[2]);
  Megaview scratch = allocateBuffer(GPU_MEMORY_CPU_READ, size, 64);
  gpu_copy_texture_buffer(state.pass->stream, texture->gpu, scratch.gpu, offset, scratch.offset, extent);
  readers->list[readers->head++ & 0xf] = (Reader) {
    .callback = callback,
    .userdata = userdata,
    .data = scratch.data,
    .size = size,
    .tick = state.tick
  };
  TextureAccess access = { texture, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_READ };
  arr_push(&state.pass->textures, access);
  lovrRetain(texture);
  state.stats.copies++;
}

void lovrTextureCopy(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t extent[3]) {
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Texture copies can only happen in a transfer pass");
  lovrCheck(!src->info.parent && !dst->info.parent, "Can not copy Texture views");
  lovrCheck(src->info.usage & TEXTURE_COPY, "Texture must have the 'copy' flag to copy from it");
  lovrCheck(dst->info.usage & TEXTURE_COPY, "Texture must have the 'copy' flag to copy to it");
  lovrCheck(src->info.format == dst->info.format, "Copying between Textures requires them to have the same format");
  lovrCheck(src->info.samples == dst->info.samples, "Textures must have the same sample counts to copy between them");
  checkTextureBounds(&src->info, srcOffset, extent);
  checkTextureBounds(&dst->info, dstOffset, extent);
  gpu_copy_textures(state.pass->stream, src->gpu, dst->gpu, srcOffset, dstOffset, extent);
  TextureAccess accessSrc = { src, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_READ };
  TextureAccess accessDst = { dst, GPU_PHASE_COPY, GPU_CACHE_TRANSFER_WRITE };
  arr_push(&state.pass->textures, accessSrc);
  arr_push(&state.pass->textures, accessDst);
  lovrRetain(src);
  lovrRetain(dst);
  state.stats.copies++;
}

void lovrTextureBlit(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t srcExtent[3], uint16_t dstExtent[3], bool nearest) {
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Texture blits can only happen in a transfer pass");
  lovrCheck(!src->info.parent && !dst->info.parent, "Can not blit Texture views");
  lovrCheck(src->info.samples == 1 && dst->info.samples == 1, "Multisampled textures can not be used for blits");
  lovrCheck(src->info.usage & TEXTURE_COPY, "Texture must have the 'copy' flag to blit from it");
  lovrCheck(dst->info.usage & TEXTURE_COPY, "Texture must have the 'copy' flag to blit to it");
  lovrCheck(state.features.formats[src->info.format] & GPU_FEATURE_BLIT, "This GPU does not support blits for the source texture's format");
  lovrCheck(state.features.formats[dst->info.format] & GPU_FEATURE_BLIT, "This GPU does not support blits for the destination texture's format");
  lovrCheck(src->info.format == dst->info.format, "Texture formats must match to blit between them");
  checkTextureBounds(&src->info, srcOffset, srcExtent);
  checkTextureBounds(&dst->info, dstOffset, dstExtent);
  gpu_blit(state.pass->stream, src->gpu, dst->gpu, srcOffset, dstOffset, srcExtent, dstExtent, nearest);
  TextureAccess accessSrc = { src, GPU_PHASE_BLIT, GPU_CACHE_TRANSFER_READ };
  TextureAccess accessDst = { dst, GPU_PHASE_BLIT, GPU_CACHE_TRANSFER_WRITE };
  arr_push(&state.pass->textures, accessSrc);
  arr_push(&state.pass->textures, accessDst);
  lovrRetain(src);
  lovrRetain(dst);
  state.stats.copies++;
}

void lovrTextureGenerateMipmaps(Texture* texture) {
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Texture mipmap generation can only happen in a transfer pass");
  lovrCheck(!texture->info.parent, "Can not generate mipmaps on texture views");
  lovrCheck(texture->info.usage & TEXTURE_COPY, "Texture must have the 'copy' flag to generate mipmaps");
  lovrCheck(state.features.formats[texture->info.format] & GPU_FEATURE_BLIT, "This GPU does not support blits for the texture's format, which is required for mipmap generation");
  bool volumetric = texture->info.type == TEXTURE_VOLUME;
  for (uint32_t i = 1; i < texture->info.mipmaps; i++) {
    uint16_t srcOffset[4] = { 0, 0, 0, i - 1 };
    uint16_t dstOffset[4] = { 0, 0, 0, i };
    uint16_t srcExtent[3] = {
      MAX(texture->info.width >> (i - 1), 1),
      MAX(texture->info.height >> (i - 1), 1),
      volumetric ? MAX(texture->info.depth >> (i - 1), 1) : 1
    };
    uint16_t dstExtent[3] = {
      MAX(texture->info.width >> i, 1),
      MAX(texture->info.height >> i, 1),
      volumetric ? MAX(texture->info.depth >> i, 1) : 1
    };
    gpu_blit(state.pass->stream, texture->gpu, texture->gpu, srcOffset, dstOffset, srcExtent, dstExtent, GPU_FILTER_LINEAR);
    gpu_barrier barrier = {
      .prev = GPU_PHASE_BLIT,
      .next = GPU_PHASE_BLIT,
      .flush = GPU_CACHE_TRANSFER_WRITE,
      .invalidate = GPU_CACHE_TRANSFER_READ
    };
    gpu_sync(state.pass->stream, &barrier, 1);
  }
  TextureAccess access = { texture, GPU_PHASE_BLIT, GPU_CACHE_TRANSFER_READ | GPU_CACHE_TRANSFER_WRITE };
  arr_push(&state.pass->textures, access);
  lovrRetain(texture);
  state.stats.copies++;
}

// Sampler

Sampler* lovrSamplerCreate(SamplerInfo* info) {
  lovrCheck(info->range[1] < 0.f || info->range[1] >= info->range[0], "Invalid Sampler mipmap range");
  lovrCheck(info->anisotropy <= state.limits.anisotropy, "Sampler anisotropy (%f) exceeds anisotropy limit (%f)", info->anisotropy, state.limits.anisotropy);

  Sampler* sampler = calloc(1, sizeof(Sampler) + gpu_sizeof_sampler());
  lovrAssert(sampler, "Out of memory");
  sampler->gpu = (gpu_sampler*) (sampler + 1);
  sampler->info = *info;
  sampler->ref = 1;

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

  lovrAssert(gpu_sampler_init(sampler->gpu, &gpu), "Failed to initialize sampler");
  state.stats.samplers++;
  return sampler;
}

void lovrSamplerDestroy(void* ref) {
  Sampler* sampler = ref;
  gpu_sampler_destroy(sampler->gpu);
  state.stats.samplers--;
  free(sampler);
}

const SamplerInfo* lovrSamplerGetInfo(Sampler* sampler) {
  return &sampler->info;
}

// Shader

static void lovrShaderInit(Shader* shader) {
  // Shader stores full list of flags, but reorders them to put active (overridden) ones first
  shader->activeFlagCount = 0;
  for (uint32_t i = 0; i < shader->info.flagCount; i++) {
    ShaderFlag* flag = &shader->info.flags[i];
    uint32_t hash = flag->name ? hash32(flag->name, strlen(flag->name)) : 0;
    for (uint32_t j = 0; j < shader->flagCount; j++) {
      if (hash ? (hash != shader->flagLookup[j]) : (flag->id != shader->flags[j].id)) continue;
      uint32_t index = shader->activeFlagCount++;
      if (index != j) {
        gpu_shader_flag temp = shader->flags[index];
        shader->flags[index] = shader->flags[j];
        shader->flags[j] = temp;

        uint64_t tempName = shader->flagLookup[index];
        shader->flagLookup[index] = shader->flagLookup[j];
        shader->flagLookup[j] = tempName;
      }
      shader->flags[index].value = flag->value;
    }
  }

  if (shader->info.type == SHADER_COMPUTE) {
    gpu_compute_pipeline_info compute = {
      .shader = shader->gpu,
      .flags = shader->flags,
      .flagCount = shader->activeFlagCount
    };
    uint32_t index = state.pipelineCount++;
    lovrCheck(index < COUNTOF(state.pipelines), "Too many pipelines, please report this encounter");
    lovrAssert(gpu_pipeline_init_compute(state.pipelines[index], &compute), "Failed to initialize compute pipeline");
    shader->computePipelineIndex = index;
  }
}

Shader* lovrShaderCreate(ShaderInfo* info) {
  Shader* shader = calloc(1, sizeof(Shader) + gpu_sizeof_shader() + gpu_sizeof_pipeline());
  lovrAssert(shader, "Out of memory");
  shader->gpu = (gpu_shader*) (shader + 1);
  shader->info = *info;
  shader->ref = 1;

  ReflectionInfo reflection;
  memset(&reflection, 0, sizeof(reflection));

  if (info->type == SHADER_COMPUTE) {
    lovrCheck(info->source[0] && !info->source[1], "Compute shaders require one stage");
    parseSpirv(info->source[0], info->length[0], GPU_STAGE_COMPUTE, &reflection);
  } else {
    lovrCheck(info->source[0] && info->source[1], "Currently, graphics shaders require two stages");
    parseSpirv(info->source[0], info->length[0], GPU_STAGE_VERTEX, &reflection);
    parseSpirv(info->source[1], info->length[1], GPU_STAGE_FRAGMENT, &reflection);
  }

  // Copy some of the reflection info (maybe it should just write directly to Shader*)?
  reflection.material.size = ALIGN(reflection.material.size, state.limits.uniformBufferAlign);
  shader->constantSize = reflection.constantSize;
  shader->constantCount = reflection.constantCount;
  memcpy(shader->constantOffsets, reflection.constantOffsets, sizeof(reflection.constantOffsets));
  memcpy(shader->constantTypes, reflection.constantTypes, sizeof(reflection.constantTypes));
  memcpy(shader->constantLookup, reflection.constantLookup, sizeof(reflection.constantLookup));
  memcpy(shader->flagLookup, reflection.flagNames, sizeof(reflection.flagNames));
  memcpy(shader->flags, reflection.flags, sizeof(reflection.flags));
  shader->flagCount = reflection.flagCount;
  shader->attributeMask = reflection.attributeMask;

  // Validate built in bindings (can be as little or as much as we want really)
  lovrCheck(reflection.slots[0][0].type == GPU_SLOT_UNIFORM_BUFFER, "Expected uniform buffer for camera (slot 0.0)");
  lovrCheck(reflection.slots[0][1].type == GPU_SLOT_UNIFORM_BUFFER, "Expected uniform buffer for draws (slot 0.1)");
  lovrCheck(reflection.slots[0][2].type == GPU_SLOT_SAMPLER, "Expected sampler at slot 0.2");
  lovrCheck(reflection.slots[0][3].type == GPU_SLOT_SAMPLER, "Expected sampler at slot 0.3");
  lovrCheck(reflection.slots[0][4].type == GPU_SLOT_SAMPLER, "Expected sampler at slot 0.4");
  lovrCheck(reflection.slots[0][5].type == GPU_SLOT_SAMPLER, "Expected sampler at slot 0.5");

  // Preprocess user bindings
  for (uint32_t i = 0; i < COUNTOF(reflection.slots[2]); i++) {
    gpu_slot slot = reflection.slots[2][i];
    if (!slot.stage) continue;
    uint32_t index = shader->resourceCount++;
    bool buffer = slot.type == GPU_SLOT_UNIFORM_BUFFER || slot.type == GPU_SLOT_STORAGE_BUFFER;
    bool texture = slot.type == GPU_SLOT_SAMPLED_TEXTURE || slot.type == GPU_SLOT_STORAGE_TEXTURE;
    bool sampler = slot.type == GPU_SLOT_SAMPLER;
    bool storage = slot.type == GPU_SLOT_STORAGE_BUFFER || slot.type == GPU_SLOT_STORAGE_TEXTURE;
    shader->bufferMask |= (buffer << i);
    shader->textureMask |= (texture << i);
    shader->samplerMask |= (sampler << i);
    shader->storageMask |= (storage << i);
    shader->slotStages[i] = slot.stage;
    shader->resourceSlots[index] = i;
    shader->resourceLookup[index] = reflection.slotNames[i];
  }

  // Filter empties from slots, so contiguous slot list can be used to hash/create layout
  for (uint32_t i = 0; i < shader->resourceCount; i++) {
    if (shader->resourceSlots[i] > i) {
      reflection.slots[2][i] = reflection.slots[2][shader->resourceSlots[i]];
    }
  }

  shader->material = lookupMaterialBlock(&reflection.material);

  if (shader->resourceCount > 0) {
    shader->layout = lookupLayout(reflection.slots[2], shader->resourceCount);
  }

  gpu_shader_info gpuInfo = {
    .stages[0] = { info->source[0], info->length[0] },
    .stages[1] = { info->source[1], info->length[1] },
    .layouts[0] = state.layouts[0],
    .layouts[1] = state.layouts[state.materials[shader->material].layout],
    .layouts[2] = shader->resourceCount > 0 ? state.layouts[shader->layout] : NULL,
    .pushConstantSize = reflection.constantSize,
    .label = info->label
  };

  lovrAssert(gpu_shader_init(shader->gpu, &gpuInfo), "Could not create Shader");
  lovrShaderInit(shader);
  state.stats.shaders++;
  return shader;
}

Shader* lovrShaderClone(Shader* parent, ShaderFlag* flags, uint32_t count) {
  Shader* shader = calloc(1, sizeof(Shader) + gpu_sizeof_shader() + gpu_sizeof_pipeline());
  lovrAssert(shader, "Out of memory");
  shader->ref = 1;
  shader->gpu = parent->gpu;
  shader->info = parent->info;
  shader->info.flags = flags;
  shader->info.flagCount = count;
  shader->layout = parent->layout;
  shader->resourceCount = parent->resourceCount;
  shader->bufferMask = parent->bufferMask;
  shader->textureMask = parent->textureMask;
  shader->samplerMask = parent->samplerMask;
  shader->storageMask = parent->storageMask;
  memcpy(shader->resourceSlots, parent->resourceSlots, sizeof(shader->resourceSlots));
  memcpy(shader->resourceLookup, parent->resourceLookup, sizeof(shader->resourceLookup));
  memcpy(shader->flagLookup, parent->flagLookup, sizeof(shader->flagLookup));
  memcpy(shader->flags, parent->flags, sizeof(shader->flags));
  shader->flagCount = parent->flagCount;
  shader->attributeMask = parent->attributeMask;
  lovrShaderInit(shader);
  state.stats.shaders++;
  return shader;
}

Shader* lovrShaderCreateDefault(DefaultShader type, ShaderFlag* flags, uint32_t count) {
  return lovrShaderClone(lovrGraphicsGetDefaultShader(type), flags, count);
}

Shader* lovrGraphicsGetDefaultShader(DefaultShader type) {
  if (state.defaultShaders[type]) return state.defaultShaders[type];
  switch (type) {
    case SHADER_UNLIT:
      return state.defaultShaders[type] = lovrShaderCreate(&(ShaderInfo) {
        .type = SHADER_GRAPHICS,
        .source = { lovr_shader_unlit_vert, lovr_shader_unlit_frag },
        .length = { sizeof(lovr_shader_unlit_vert), sizeof(lovr_shader_unlit_frag) },
        .label = "unlit"
      });
    case SHADER_FILL:
      return state.defaultShaders[type] = lovrShaderCreate(&(ShaderInfo) {
        .type = SHADER_GRAPHICS,
        .source = { lovr_shader_fill_vert, lovr_shader_fill_frag },
        .length = { sizeof(lovr_shader_fill_vert), sizeof(lovr_shader_fill_frag) },
        .label = "fill"
      });
    case SHADER_CUBE:
      return state.defaultShaders[type] = lovrShaderCreate(&(ShaderInfo) {
        .type = SHADER_GRAPHICS,
        .source = { lovr_shader_cube_vert, lovr_shader_cube_frag },
        .length = { sizeof(lovr_shader_cube_vert), sizeof(lovr_shader_cube_frag) },
        .label = "cube"
      });
    case SHADER_PANO:
      return state.defaultShaders[type] = lovrShaderCreate(&(ShaderInfo) {
        .type = SHADER_GRAPHICS,
        .source = { lovr_shader_pano_vert, lovr_shader_pano_frag },
        .length = { sizeof(lovr_shader_pano_vert), sizeof(lovr_shader_pano_frag) },
        .label = "pano"
      });
    default: lovrThrow("Unreachable");
  }
}

void lovrShaderDestroy(void* ref) {
  Shader* shader = ref;
  gpu_shader_destroy(shader->gpu);
  state.stats.shaders--;
  free(shader);
}

const ShaderInfo* lovrShaderGetInfo(Shader* shader) {
  return &shader->info;
}

// Material

Material* lovrMaterialCreate(MaterialInfo* info) {
  uint32_t blockIndex = info->shader ? info->shader->material : info->type;
  MaterialBlock* block = &state.materials[blockIndex];
  Material* material = &block->instances[block->next];
  MaterialFormat* format = &block->format;

  // Grab a free material from the block's linked list
  lovrAssert(block->next != ~0u && gpu_finished(material->tick), "Out of material memory");
  if (block->next == block->last) block->last = ~0u;
  block->next = material->next;
  material->next = ~0u;
  material->ref = 1;

  // Get pointer to the material's memory (either write to mapped memory directly or stage a copy)
  char* base;
  if (block->buffer.data) {
    base = block->buffer.data + material->index * format->size;
  } else {
    lovrGraphicsPrepare();
    Megaview dst = block->buffer;
    Megaview src = allocateBuffer(GPU_MEMORY_CPU_WRITE, format->size, 4);
    uint32_t dstOffset = dst.offset + material->index * format->size;
    gpu_copy_buffers(state.uploads->stream, src.gpu, dst.gpu, src.offset, dstOffset, format->size);
    base = src.data;
  }

  // Set up bindings (buffer points at (potentially offset) region of block UBO, + default texture)
  gpu_binding bindings[16];
  uint32_t extent = format->size;
  uint32_t offset = material->index * format->size;
  bindings[0].buffer = (gpu_buffer_binding) { block->buffer.gpu, block->buffer.offset + offset, extent };

  // Pre hash property names to avoid big oh no n^2 party
  uint32_t hashes[32];
  for (uint32_t i = 0; i < info->propertyCount; i++) {
    hashes[i] = hash32(info->properties[i].name, strlen(info->properties[i].name));
  }

  // Write data to GPU memory
  for (uint32_t i = 0; i < format->count; i++) {
    bool scalar = format->scalars & (1 << i);
    bool vector = format->vectors & (1 << i);
    bool color = format->colors & (1 << i);
    bool scale = format->scales & (1 << i);

    union {
      char* raw;
      int32_t* i32;
      uint32_t* u32;
      float* f32;
    } data = { base + format->offsets[i] };

    MaterialProperty* property = NULL;
    for (uint32_t j = 0; j < info->propertyCount; j++) {
      if (hashes[j] == format->names[i]) {
        property = &info->properties[j];
        break;
      }
    }

    if (property) {
      if (scalar) {
        lovrCheck(property->type == PROPERTY_SCALAR, "Material property '%s' is a scalar, but the value provided is not a scalar", property->name);
        switch (format->types[i]) {
          case FIELD_I32: *data.i32 = (int32_t) property->value.scalar; break;
          case FIELD_U32: *data.u32 = (uint32_t) property->value.scalar; break;
          case FIELD_F32: *data.f32 = (float) property->value.scalar; break;
          default: lovrThrow("Unreachable");
        }
      } else if (vector) {
        if (property->type == PROPERTY_SCALAR && format->types[i] != FIELD_F32x2) {
          uint32_t hex = (uint32_t) property->value.scalar;
          data.f32[0] = ((hex >> 16) & 0xff) / 255.f;
          data.f32[1] = ((hex >> 8) & 0xff) / 255.f;
          data.f32[2] = ((hex >> 0) & 0xff) / 255.f;
          if (format->types[i] == FIELD_F32x4) {
            data.f32[3] = 1.f;
          }
        } else {
          lovrCheck(property->type == PROPERTY_VECTOR, "Material property '%s' is a vector, but the value provided is not a vector (or a hexcode, for vec3/vec4)", property->name);
          switch (format->types[i]) {
            case FIELD_F32x2: memcpy(data.f32, property->value.vector, 2 * sizeof(float)); break;
            case FIELD_F32x3: memcpy(data.f32, property->value.vector, 3 * sizeof(float)); break;
            case FIELD_F32x4: memcpy(data.f32, property->value.vector, 4 * sizeof(float)); break;
            default: lovrThrow("Unreachable");
          }
        }
      } else {
        lovrThrow("Unreachable");
      }
    } else {
      if (scalar) {
        switch (format->types[i]) {
          case FIELD_I32: *data.i32 = scale ? 1 : 0; break;
          case FIELD_U32: *data.u32 = scale ? 1u : 0u; break;
          case FIELD_F32: *data.f32 = scale ? 1.f : 0.f; break;
          default: lovrThrow("Unreachable");
        }
      } else if (vector) {
        float zero[4] = { 0.f, 0.f, 0.f, 0.f };
        float ones[4] = { 1.f, 1.f, 1.f, 1.f };
        switch (format->types[i]) {
          case FIELD_F32x2: memcpy(data.f32, (scale || color) ? ones : zero, 2 * sizeof(float)); break;
          case FIELD_F32x3: memcpy(data.f32, (scale || color) ? ones : zero, 3 * sizeof(float)); break;
          case FIELD_F32x4: memcpy(data.f32, (scale || color) ? ones : zero, 4 * sizeof(float)); break;
          default: lovrThrow("Unreachable");
        }
      } else {
        lovrThrow("Unreachable");
      }
    }
  }

  // Textures

  if (!material->textures) {
    material->textures = malloc(format->textureCount * sizeof(Texture*));
    lovrAssert(material->textures, "Out of memory");
  }

  for (uint32_t i = 0; i < format->textureCount; i++) {
    MaterialProperty* property = NULL;
    for (uint32_t j = 0; j < info->propertyCount; j++) {
      if (hashes[j] == format->textureNames[i]) {
        property = &info->properties[j];
        break;
      }
    }

    if (property) {
      lovrCheck(property->type == PROPERTY_TEXTURE, "Material property '%s' is a texture, but the value provided is not a texture", property->name);
      bindings[i + 1].texture = property->value.texture->gpu;
      material->textures[i] = property->value.texture;
      lovrRetain(property->value.texture);
    } else {
      bindings[i + 1].texture = lovrGraphicsGetDefaultTexture()->gpu;
      material->textures[i] = NULL;
    }
  }

  // Update material's bundle contents, it is guaranteed to not be in use by the GPU
  gpu_bundle_info write = { .layout = state.layouts[block->layout], .bindings = bindings };
  gpu_bundle* bundle = (gpu_bundle*) ((char*) state.materials[material->block].bundles + material->index * gpu_sizeof_bundle());
  gpu_bundle_write(&bundle, &write, 1);
  return material;
}

void lovrMaterialDestroy(void* ref) {
  Material* material = ref;
  MaterialBlock* block = &state.materials[material->block];
  material->tick = state.tick;
  block->last = material->index;
  if (block->next == ~0u) block->next = block->last;
  for (uint32_t i = 0; i < block->format.textureCount; i++) {
    lovrRelease(material->textures[i], lovrTextureDestroy);
  }
}

// Batch

Batch* lovrBatchCreate(BatchInfo* info) {
  lovrCheck(info->capacity <= 0xffff, "Currently, the maximum batch capacity is %d", 0xffff);
  Batch* batch = calloc(1, sizeof(Batch));
  lovrAssert(batch, "Out of memory");
  batch->ref = 1;
  batch->info = *info;
  batch->pass = lookupPass(info->canvas);
  batch->draws = malloc(info->capacity * sizeof(BatchDraw));
  batch->groups = malloc(info->capacity * sizeof(BatchGroup));
  batch->activeDraws = malloc(info->capacity * sizeof(uint32_t));
  batch->origins = malloc(info->capacity * 4 * sizeof(float));
  batch->bundles = malloc(info->capacity * sizeof(gpu_bundle*));
  batch->bundleInfo = malloc(info->capacity * sizeof(gpu_bundle_info));
  lovrAssert(batch->draws, "Out of memory");
  lovrAssert(batch->groups, "Out of memory");
  lovrAssert(batch->activeDraws, "Out of memory");
  lovrAssert(batch->origins, "Out of memory");
  lovrAssert(batch->bundles, "Out of memory");
  lovrAssert(batch->bundleInfo, "Out of memory");

  if (!info->transient) {
    batch->bunch = malloc(gpu_sizeof_bunch());
    batch->bundles[0] = malloc(info->capacity * gpu_sizeof_bundle());
    lovrAssert(batch->bunch, "Out of memory");
    lovrAssert(batch->bundles[0], "Out of memory");

    for (uint32_t i = 1; i < info->capacity; i++) {
      batch->bundles[i] = (gpu_bundle*) ((char*) batch->bundles[0] + i * gpu_sizeof_bundle());
    }

    uint32_t alignment = state.limits.uniformBufferAlign;
    batch->drawBuffer = allocateBuffer(GPU_MEMORY_GPU, info->capacity * sizeof(DrawData), alignment);
    batch->stash = allocateBuffer(GPU_MEMORY_GPU, info->bufferSize, 4);
  }

  arr_init(&batch->buffers, info->transient ? tgrow : realloc);
  arr_init(&batch->textures, info->transient ? tgrow : realloc);
  return batch;
}

void lovrBatchDestroy(void* ref) {
  Batch* batch = ref;
  for (size_t i = 0; i < batch->buffers.length; i++) {
    lovrRelease(batch->buffers.data[i].buffer, lovrBufferDestroy);
  }
  for (size_t i = 0; i < batch->textures.length; i++) {
    lovrRelease(batch->textures.data[i].texture, lovrTextureDestroy);
  }
  arr_free(&batch->buffers);
  arr_free(&batch->textures);
  if (!batch->info.transient) {
    if (--state.buffers.list[batch->drawBuffer.index].refs == 0) {
      recycleBuffer(batch->drawBuffer.index, GPU_MEMORY_GPU);
    }
    if (--state.buffers.list[batch->stash.index].refs == 0) {
      recycleBuffer(batch->stash.index, GPU_MEMORY_GPU);
    }
    if (batch->bundleCount > 0) {
      gpu_bunch_destroy(batch->bunch);
    }
    free(batch->bundles[0]);
    free(batch->bunch);
  }
  free(batch->bundleInfo);
  free(batch->bundles);
  free(batch->origins);
  free(batch->activeDraws);
  free(batch->groups);
  free(batch->draws);
  free(batch);
}

const BatchInfo* lovrBatchGetInfo(Batch* batch) {
  return &batch->info;
}

uint32_t lovrBatchGetCount(Batch* batch) {
  return batch->drawCount;
}

void lovrBatchReset(Batch* batch) {
  batch->drawCount = 0;
  batch->groupCount = 0;
  batch->activeDrawCount = 0;
  batch->groupedCount = 0;
  batch->stashCursor = 0;

  if (!batch->info.transient && batch->lastBundleCount > 0) {
    gpu_bunch_destroy(batch->bunch);
  }

  batch->bundleCount = 0;
  batch->lastBundleCount = 0;

  for (size_t i = 0; i < batch->buffers.length; i++) {
    lovrRelease(batch->buffers.data[i].buffer, lovrBufferDestroy);
  }

  for (size_t i = 0; i < batch->textures.length; i++) {
    lovrRelease(batch->textures.data[i].texture, lovrTextureDestroy);
  }

  arr_clear(&batch->buffers);
  arr_clear(&batch->textures);
}

static LOVR_THREAD_LOCAL Batch* sortBatch;

// Opaque sort groups by state change, and uses front-to-back depth as a tie breaker
static int cmpOpaque(const void* a, const void* b) {
  uint32_t i = *(uint32_t*) a;
  uint32_t j = *(uint32_t*) b;
  uint64_t key1, key2;
  memcpy(&key1, &sortBatch->draws[i].pipeline, sizeof(uint64_t));
  memcpy(&key2, &sortBatch->draws[j].pipeline, sizeof(uint64_t));
  if (key1 == key2) {
    return sortBatch->draws[i].depth - sortBatch->draws[j].depth;
  }
  return (key1 > key2) - (key1 < key2);
}

// Transparent sort orders by depth and the 32 bits after depth (e.g. pipeline, doesn't matter much)
static int cmpTransparent(const void* a, const void* b) {
  uint32_t i = *(uint32_t*) a;
  uint32_t j = *(uint32_t*) b;
  uint64_t key1, key2;
  memcpy(&key1, &sortBatch->draws[i].depth, sizeof(uint64_t));
  memcpy(&key2, &sortBatch->draws[j].depth, sizeof(uint64_t));
  return (key1 > key2) - (key1 < key2);
}

void lovrBatchSort(Batch* batch, SortMode mode) {
  for (uint32_t i = 0; i < batch->activeDrawCount; i++) {
    float v[4];
    vec3_init(v, batch->origins + 4 * batch->activeDraws[i]);
    mat4_mulVec4(state.cameras[0].view, v);
    batch->draws[batch->activeDraws[i]].depth = -v[2];
  }

  sortBatch = batch;
  qsort(batch->activeDraws, batch->activeDrawCount, sizeof(uint32_t), mode == SORT_OPAQUE ? cmpOpaque : cmpTransparent);
  batch->groupedCount = 0;
}

void lovrBatchFilter(Batch* batch, bool (*predicate)(void* context, uint32_t i), void* context) {
  batch->activeDrawCount = 0;
  for (uint32_t i = 0; i < batch->drawCount; i++) {
    if (predicate(context, i)) {
      batch->activeDraws[batch->activeDrawCount++] = i;
    }
  }
  batch->groupedCount = 0;
}

// Model

Model* lovrModelCreate(ModelInfo* info) {
  ModelData* data = info->data;
  Model* model = calloc(1, sizeof(Model));
  lovrAssert(model, "Out of memory");
  model->ref = 1;
  model->data = data;
  lovrRetain(data);

  model->draws = calloc(data->primitiveCount, sizeof(DrawInfo));
  lovrAssert(model->draws, "Out of memory");

  model->textures = malloc(data->imageCount * sizeof(Texture*));
  lovrAssert(model->textures, "Out of memory");

  model->materials = malloc(data->materialCount * sizeof(Material*));
  lovrAssert(model->materials, "Out of memory");

  for (uint32_t i = 0; i < data->imageCount; i++) {
    Image* image = data->images[i];
    model->textures[i] = lovrTextureCreate(&(TextureInfo) {
      .type = TEXTURE_2D,
      .usage = TEXTURE_SAMPLE,
      .format = image->format,
      .width = image->width,
      .height = image->height,
      .depth = 1,
      .mipmaps = ~0u,
      .samples = 1,
      .srgb = true, // TODO modeldata can u pls tell me if this is used for diffuse/emissive or not
      .images = &image
    });
  }

  for (uint32_t i = 0; i < data->materialCount; i++) {
    ModelMaterial* material = &data->materials[i];
    MaterialProperty properties[] = {
      [0] = { "metalness", PROPERTY_SCALAR, .value.scalar = material->metalness },
      [1] = { "roughness", PROPERTY_SCALAR, .value.scalar = material->roughness },
      [2] = { "color", PROPERTY_VECTOR, .value.vector = { 0.f } },
      [3] = { "emissive", PROPERTY_VECTOR, .value.vector = { 0.f } },
      [4] = { "colorTexture", PROPERTY_TEXTURE, .value.texture = model->textures[material->colorTexture] },
      [5] = { "emissiveTexture", PROPERTY_TEXTURE, .value.texture = NULL },
      [6] = { "metalnessRoughnessTexture", PROPERTY_TEXTURE, .value.texture = NULL },
      [7] = { "occlusionTexture", PROPERTY_TEXTURE, .value.texture = NULL },
      [8] = { "normalTexture", PROPERTY_TEXTURE, .value.texture = NULL }
    };

    // ew
    memcpy(properties[2].value.vector, material->color, sizeof(material->color));
    memcpy(properties[3].value.vector, material->emissive, sizeof(material->emissive));
    if (material->colorTexture != ~0u) properties[4].value.texture = model->textures[material->colorTexture];
    if (material->emissiveTexture != ~0u) properties[5].value.texture = model->textures[material->emissiveTexture];
    if (material->metalnessRoughnessTexture != ~0u) properties[6].value.texture = model->textures[material->metalnessRoughnessTexture];
    if (material->occlusionTexture != ~0u) properties[7].value.texture = model->textures[material->occlusionTexture];
    if (material->normalTexture != ~0u) properties[8].value.texture = model->textures[material->normalTexture];

    model->materials[i] = lovrMaterialCreate(&(MaterialInfo) {
      .shader = info->shader,
      .type = info->material,
      .properties = properties,
      .propertyCount = COUNTOF(properties)
    });
  }

  // First pass: determine vertex format, count vertices and indices, validate meshes
  // TODO modeldata can u pls tell me how many vertices/indices total
  uint32_t totalIndexCount = 0;
  uint32_t totalVertexCount = 0;
  gpu_index_type indexType = GPU_INDEX_U16;
  for (uint32_t i = 0; i < data->primitiveCount; i++) {
    ModelPrimitive* primitive = &data->primitives[i];
    lovrCheck(primitive->attributes[ATTR_POSITION], "Sorry, currently I can not load models without position attributes!");
    lovrCheck(primitive->topology != TOPOLOGY_LINE_LOOP, "Sorry, currently I can not load models with a 'line loop' draw mode (please report this!)");
    lovrCheck(primitive->topology != TOPOLOGY_LINE_STRIP, "Sorry, currently I can not load models with a 'line strip' draw mode (please report this!)");
    lovrCheck(primitive->topology != TOPOLOGY_TRIANGLE_STRIP, "Sorry, currently I can not load models with a 'triangle strip' draw mode (please report this!)");
    lovrCheck(primitive->topology != TOPOLOGY_TRIANGLE_FAN, "Sorry, currently I can not load models with a 'triangle fan' draw mode (please report this!)");
    totalVertexCount += primitive->attributes[ATTR_POSITION]->count;
    if (primitive->indices) {
      totalIndexCount += primitive->indices->count;
      if (primitive->indices->type == U32) {
        indexType = GPU_INDEX_U32;
      }
    }
  }

  // Create buffers

  void* vertices;
  void* indices;

  model->vertexBuffer = lovrBufferCreate(&(BufferInfo) {
    .type = BUFFER_VERTEX,
    .length = totalVertexCount,
    .format = VERTEX_MODEL
  }, &vertices);

  uint32_t indexStride = 2 << (indexType == GPU_INDEX_U32);
  if (totalIndexCount > 0) {
    model->indexBuffer = lovrBufferCreate(&(BufferInfo) {
      .type = BUFFER_INDEX,
      .length = totalIndexCount,
      .stride = indexStride,
      .fieldCount = 1,
      .types[0] = indexType == GPU_INDEX_U32 ? FIELD_U32 : FIELD_U16
    }, (void**) &indices);
  }

  // Second pass: write draws, now that vertex format and buffers are known
  uint32_t indexCursor = 0;
  uint32_t vertexCursor = 0;
  for (uint32_t i = 0; i < data->primitiveCount; i++) {
    DrawInfo* draw = &model->draws[i];
    ModelPrimitive* primitive = &data->primitives[i];
    uint32_t vertexCount = primitive->attributes[ATTR_POSITION]->count;
    uint32_t indexCount = primitive->indices ? primitive->indices->count : 0;

    switch (primitive->topology) {
      case TOPOLOGY_POINTS: draw->mode = DRAW_POINTS;
      case TOPOLOGY_LINES: draw->mode = DRAW_LINES;
      case TOPOLOGY_TRIANGLES: draw->mode = DRAW_TRIANGLES;
      default: break;
    }

    draw->material = primitive->material == ~0u ? NULL : model->materials[primitive->material];

    draw->vertex.buffer = model->vertexBuffer;
    draw->index.buffer = model->indexBuffer;
    draw->index.stride = indexStride;

    if (primitive->indices) {
      draw->start = indexCursor;
      draw->count = indexCount;
      draw->base = vertexCursor;
    } else {
      draw->start = vertexCursor;
      draw->count = vertexCount;
    }

    vertexCursor += vertexCount;
    indexCursor += indexCount;
  }

  // Third pass: Write data to buffers, converting to the vertex format's types
  // TODO modeldata can u pls tell me the pointer/stride of each attr idc if it duplicates info
  indexCursor = 0;
  vertexCursor = 0;
  for (uint32_t i = 0; i < data->primitiveCount; i++) {
    ModelPrimitive* primitive = &data->primitives[i];
    uint32_t count = primitive->attributes[ATTR_POSITION]->count;
    ModelAttribute* attribute;

    // Position
    ModelVertex* vertex = vertices;
    if ((attribute = primitive->attributes[ATTR_POSITION]) != NULL) {
      lovrCheck(attribute->type == F32 && attribute->components == 3, "Model position attribute must be 3 floats");
      char* src = data->buffers[attribute->buffer].data + attribute->offset;
      uint32_t stride = data->buffers[attribute->buffer].stride ? data->buffers[attribute->buffer].stride : 12;
      for (uint32_t i = 0; i < count; i++, src += stride, vertex++) {
        memcpy(&vertex->position, src, 3 * sizeof(float));
      }
    } else {
      for (uint32_t i = 0; i < count; i++, vertex++) {
        memset(&vertex->position, 0, 3 * sizeof(float));
      }
    }

    // Normal
    vertex = vertices;
    if ((attribute = primitive->attributes[ATTR_NORMAL]) != NULL) {
      lovrCheck(attribute->type == F32 && attribute->components == 3, "Model normal attribute must be 3 floats");
      char* src = data->buffers[attribute->buffer].data + attribute->offset;
      uint32_t stride = data->buffers[attribute->buffer].stride ? data->buffers[attribute->buffer].stride : 12;
      for (uint32_t i = 0; i < count; i++, src += stride, vertex++) {
        float* normal = (float*) src;
        vertex->normal.nx = (unsigned) ((normal[0] + 1.f) * .5f * 0x3ff);
        vertex->normal.ny = (unsigned) ((normal[1] + 1.f) * .5f * 0x3ff);
        vertex->normal.nz = (unsigned) ((normal[2] + 1.f) * .5f * 0x3ff);
      }
    } else {
      for (uint32_t i = 0; i < count; i++, vertex++) {
        vertex->normal.nx = 0x200;
        vertex->normal.ny = 0x200;
        vertex->normal.nz = 0x200;
      }
    }

    // UV
    vertex = vertices;
    if ((attribute = primitive->attributes[ATTR_TEXCOORD]) != NULL) {
      char* src = data->buffers[attribute->buffer].data + attribute->offset;
      uint32_t stride = data->buffers[attribute->buffer].stride;
      if (attribute->type == U8 && attribute->normalized) {
        stride = stride ? stride : 2;
        for (uint32_t i = 0; i < count; i++, src += stride, vertex++) {
          uint8_t* uv = (uint8_t*) src;
          vertex->uv.u = uv[0] / 255.f;
          vertex->uv.v = uv[1] / 255.f;
        }
      } else if (attribute->type == U16 && attribute->normalized) {
        stride = stride ? stride : 4;
        for (uint32_t i = 0; i < count; i++, src += stride, vertex++) {
          uint16_t* uv = (uint16_t*) src;
          vertex->uv.u = uv[0] / 65535.f;
          vertex->uv.v = uv[1] / 65535.f;
        }
      } else if (attribute->type == F32) {
        stride = stride ? stride : 8;
        for (uint32_t i = 0; i < count; i++, src += stride, vertex++) {
          memcpy(&vertex->uv, src, 2 * sizeof(float));
        }
      } else {
        lovrThrow("Model uses unsupported data type for texcoord attribute");
      }
    } else {
      for (uint32_t i = 0; i < count; i++, vertex++) {
        vertex->uv.u = 0.f;
        vertex->uv.v = 0.f;
      }
    }

    vertex = vertices;
    if ((attribute = primitive->attributes[ATTR_COLOR]) != NULL) {
      char* src = data->buffers[attribute->buffer].data + attribute->offset;
      uint32_t stride = data->buffers[attribute->buffer].stride;
      if (attribute->type == U8 && attribute->normalized) {
        if (attribute->components == 4) {
          stride = stride ? stride : 4;
          for (uint32_t i = 0; i < count; i++, src += stride, vertex++) {
            memcpy(&vertex->color, src, 4);
          }
        } else {
          stride = stride ? stride : 3;
          for (uint32_t i = 0; i < count; i++, src += stride, vertex++) {
            memcpy(&vertex->color, src, 3);
            vertex->color.a = 255;
          }
        }
      } else if (attribute->type == U16 && attribute->normalized) {
        stride = stride ? stride : (2 * attribute->components);
        for (uint32_t i = 0; i < count; i++, src += stride, vertex++) {
          uint16_t* color = (uint16_t*) src;
          vertex->color.r = color[0] >> 8;
          vertex->color.g = color[1] >> 8;
          vertex->color.b = color[2] >> 8;
          vertex->color.a = attribute->components == 3 ? 255 : (color[3] >> 8);
        }
      } else if (attribute->type == F32) {
        stride = stride ? stride : (4 * attribute->components);
        for (uint32_t i = 0; i < count; i++, src += stride, vertex++) {
          float* color = (float*) src;
          vertex->color.r = color[0] * 255.f + .5f;
          vertex->color.g = color[1] * 255.f + .5f;
          vertex->color.b = color[2] * 255.f + .5f;
          vertex->color.a = attribute->components == 3 ? 255 : (color[3] * 255.f + .5f);
        }
      } else {
        lovrThrow("Model uses unsupported data type for color attribute");
      }
    } else {
      for (uint32_t i = 0; i < count; i++, vertex++) {
        memset(&vertex->color, 0xff, 4 * sizeof(uint8_t));
      }
    }

    vertex = vertices;
    if ((attribute = primitive->attributes[ATTR_TANGENT]) != NULL) {
      lovrCheck(attribute->type == F32 && attribute->components == 3, "Model tangent attribute must be 3 floats");
      char* src = data->buffers[attribute->buffer].data + attribute->offset;
      uint32_t stride = data->buffers[attribute->buffer].stride ? data->buffers[attribute->buffer].stride : 12;
      for (uint32_t i = 0; i < count; i++, src += stride, vertex++) {
        float* tangent = (float*) src;
        vertex->tangent.x = (unsigned) ((tangent[0] + 1.f) * .5f * 0x3ff);
        vertex->tangent.y = (unsigned) ((tangent[1] + 1.f) * .5f * 0x3ff);
        vertex->tangent.z = (unsigned) ((tangent[2] + 1.f) * .5f * 0x3ff);
        vertex->tangent.handedness = (unsigned) (tangent[3] == -1.f ? 0x0 : 0x3);
      }
    } else {
      for (uint32_t i = 0; i < count; i++, vertex++) {
        vertex->tangent.x = 0x200;
        vertex->tangent.y = 0x200;
        vertex->tangent.z = 0x200;
        vertex->tangent.handedness = 0;
      }
    }

    if (primitive->indices) {
      char* src = data->buffers[primitive->indices->buffer].data + primitive->indices->offset;
      memcpy(indices, src, primitive->indices->count * indexStride);
    }
  }

  for (uint32_t i = 0; i < data->skinCount; i++) {
    uint32_t jointCount = data->skins[i].jointCount;
    lovrAssert(jointCount <= 0xff, "ModelData skin #%d has too many joints (%d, max is %d)", i + 1, jointCount, 0xff);
  }

  // Allocate transform arrays
  model->localTransforms = malloc(sizeof(NodeTransform) * data->nodeCount);
  model->globalTransforms = malloc(16 * sizeof(float) * data->nodeCount);
  lovrAssert(model->localTransforms && model->globalTransforms, "Out of memory");
  lovrModelResetPose(model);
  return model;
}

void lovrModelDestroy(void* ref) {
  Model* model = ref;
  for (uint32_t i = 0; i < model->data->imageCount; i++) {
    lovrRelease(model->textures[i], lovrTextureDestroy);
  }
  for (uint32_t i = 0; i < model->data->materialCount; i++) {
    lovrRelease(model->materials[i], lovrMaterialDestroy);
  }
  lovrRelease(model->data, lovrModelDataDestroy);
  lovrRelease(model->vertexBuffer, lovrBufferDestroy);
  lovrRelease(model->indexBuffer, lovrBufferDestroy);
  free(model->draws);
  free(model->materials);
  free(model->textures);
  free(model->vertices);
  free(model->indices);
  free(model);
}

ModelData* lovrModelGetModelData(Model* model) {
  return model->data;
}

void lovrModelResetPose(Model* model) {
  for (uint32_t i = 0; i < model->data->nodeCount; i++) {
    vec3 position = model->localTransforms[i].properties[PROP_TRANSLATION];
    quat orientation = model->localTransforms[i].properties[PROP_ROTATION];
    vec3 scale = model->localTransforms[i].properties[PROP_SCALE];
    if (model->data->nodes[i].matrix) {
      mat4_getPosition(model->data->nodes[i].transform.matrix, position);
      mat4_getOrientation(model->data->nodes[i].transform.matrix, orientation);
      mat4_getScale(model->data->nodes[i].transform.matrix, scale);
    } else {
      vec3_init(position, model->data->nodes[i].transform.properties.translation);
      quat_init(orientation, model->data->nodes[i].transform.properties.rotation);
      vec3_init(scale, model->data->nodes[i].transform.properties.scale);
    }
  }
  model->transformsDirty = true;
}

void lovrModelAnimate(Model* model, uint32_t animationIndex, float time, float alpha) {
  if (alpha <= 0.f) return;

  lovrAssert(animationIndex < model->data->animationCount, "Invalid animation index '%d' (Model has %d animation%s)", animationIndex + 1, model->data->animationCount, model->data->animationCount == 1 ? "" : "s");
  ModelAnimation* animation = &model->data->animations[animationIndex];
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

void lovrModelPose(Model* model, uint32_t node, float position[4], float rotation[4], float alpha) {
  if (alpha <= 0.f) return;

  lovrAssert(node < model->data->nodeCount, "Invalid node index '%d' (Model has %d node%s)", node, model->data->nodeCount, model->data->nodeCount == 1 ? "" : "s");
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

void lovrModelGetNodePose(Model* model, uint32_t node, float position[4], float rotation[4], CoordinateSpace space) {
  lovrAssert(node < model->data->nodeCount, "Invalid node index '%d' (Model has %d node%s)", node, model->data->nodeCount, model->data->nodeCount == 1 ? "" : "s");
  if (space == SPACE_LOCAL) {
    vec3_init(position, model->localTransforms[node].properties[PROP_TRANSLATION]);
    quat_init(rotation, model->localTransforms[node].properties[PROP_ROTATION]);
  } else {
    updateModelTransforms(model, model->data->rootNode, (float[]) MAT4_IDENTITY);
    mat4_getPosition(model->globalTransforms + 16 * node, position);
    mat4_getOrientation(model->globalTransforms + 16 * node, rotation);
  }
}

Texture* lovrModelGetTexture(Model* model, uint32_t index) {
  lovrAssert(index < model->data->imageCount, "Invalid texture index '%d' (Model has %d texture%s)", index, model->data->imageCount, model->data->imageCount == 1 ? "" : "s");
  return model->textures[index];
}

Material* lovrModelGetMaterial(Model* model, uint32_t index) {
  lovrAssert(index < model->data->materialCount, "Invalid material index '%d' (Model has %d material%s)", index, model->data->materialCount, model->data->materialCount == 1 ? "" : "s");
  return model->materials[index];
}

Buffer* lovrModelGetVertexBuffer(Model* model) {
  return model->vertexBuffer;
}

Buffer* lovrModelGetIndexBuffer(Model* model) {
  return model->indexBuffer;
}

static void countVertices(Model* model, uint32_t nodeIndex, uint32_t* vertexCount, uint32_t* indexCount) {
  if (model->vertices) return;

  ModelNode* node = &model->data->nodes[nodeIndex];
  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    ModelPrimitive* primitive = &model->data->primitives[node->primitiveIndex + i];
    ModelAttribute* positions = primitive->attributes[ATTR_POSITION];
    ModelAttribute* indices = primitive->indices;
    uint32_t count = positions ? positions->count : 0;
    *vertexCount += count;
    *indexCount += indices ? indices->count : count;
  }

  for (uint32_t i = 0; i < node->childCount; i++) {
    countVertices(model, node->children[i], vertexCount, indexCount);
  }

  if (nodeIndex == model->data->rootNode && !model->vertices) {
    model->vertices = malloc(model->vertexCount * 3 * sizeof(float));
    model->indices = malloc(model->indexCount * sizeof(uint32_t));
    lovrAssert(model->vertices && model->indices, "Out of memory");
  }
}

static void collectVertices(Model* model, uint32_t nodeIndex, float** vertices, uint32_t** indices, uint32_t* baseIndex) {
  ModelNode* node = &model->data->nodes[nodeIndex];
  mat4 transform = model->globalTransforms + 16 * nodeIndex;

  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    ModelPrimitive* primitive = &model->data->primitives[node->primitiveIndex + i];

    ModelAttribute* positions = primitive->attributes[ATTR_POSITION];
    if (!positions) continue;

    ModelBuffer* buffer = &model->data->buffers[positions->buffer];
    char* data = buffer->data + positions->offset;
    size_t stride = buffer->stride == 0 ? 3 * sizeof(float) : buffer->stride;

    for (uint32_t j = 0; j < positions->count; j++) {
      float v[4];
      memcpy(v, data, 3 * sizeof(float));
      mat4_transform(transform, v);
      memcpy(*vertices, v, 3 * sizeof(float));
      *vertices += 3;
      data += stride;
    }

    ModelAttribute* index = primitive->indices;
    if (index) {
      buffer = &model->data->buffers[index->buffer];
      data = buffer->data + index->offset;

      if (index->type == U16) {
        uint16_t* u16 = (uint16_t*) data;
        for (uint32_t j = 0; j < index->count; j++) {
          **indices = (uint32_t) *u16 + *baseIndex;
          *indices += 1;
          u16++;
        }
      } else {
        uint32_t* u32 = (uint32_t*) data;
        for (uint32_t j = 0; j < index->count; j++) {
          **indices = *u32 + *baseIndex;
          *indices += 1;
          u32++;
        }
      }
    } else {
      for (uint32_t j = 0; j < positions->count; j++) {
        **indices = j + *baseIndex;
        *indices += 1;
      }
    }
  }
}

void lovrModelGetTriangles(Model* model, float** vertices, uint32_t* vertexCount, uint32_t** indices, uint32_t* indexCount) {
  updateModelTransforms(model, model->data->rootNode, (float[]) MAT4_IDENTITY);
  countVertices(model, model->data->rootNode, &model->vertexCount, &model->indexCount);
  *vertices = model->vertices;
  *indices = model->indices;
  uint32_t baseIndex = 0;
  collectVertices(model, model->data->rootNode, vertices, indices, &baseIndex);
  *vertexCount = model->vertexCount;
  *indexCount = model->indexCount;
  *vertices = model->vertices;
  *indices = model->indices;
}

uint32_t lovrModelGetTriangleCount(Model* model) {
  countVertices(model, model->data->rootNode, &model->vertexCount, &model->indexCount);
  return model->indexCount / 3;
}

uint32_t lovrModelGetVertexCount(Model* model) {
  countVertices(model, model->data->rootNode, &model->vertexCount, &model->indexCount);
  return model->vertexCount;
}

static void applyAABB(Model* model, uint32_t nodeIndex, float bounds[6]) {
  ModelNode* node = &model->data->nodes[nodeIndex];
  mat4 m = model->globalTransforms + 16 * nodeIndex;

  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    ModelAttribute* position = model->data->primitives[node->primitiveIndex + i].attributes[ATTR_POSITION];

    if (!position || !position->hasMin || !position->hasMax) {
      continue;
    }

    float xa[3] = { position->min[0] * m[0], position->min[0] * m[1], position->min[0] * m[2] };
    float xb[3] = { position->max[0] * m[0], position->max[0] * m[1], position->max[0] * m[2] };

    float ya[3] = { position->min[1] * m[4], position->min[1] * m[5], position->min[1] * m[6] };
    float yb[3] = { position->max[1] * m[4], position->max[1] * m[5], position->max[1] * m[6] };

    float za[3] = { position->min[2] * m[8], position->min[2] * m[9], position->min[2] * m[10] };
    float zb[3] = { position->max[2] * m[8], position->max[2] * m[9], position->max[2] * m[10] };

    float min[3] = {
      MIN(xa[0], xb[0]) + MIN(ya[0], yb[0]) + MIN(za[0], zb[0]) + m[12],
      MIN(xa[1], xb[1]) + MIN(ya[1], yb[1]) + MIN(za[1], zb[1]) + m[13],
      MIN(xa[2], xb[2]) + MIN(ya[2], yb[2]) + MIN(za[2], zb[2]) + m[14]
    };

    float max[3] = {
      MAX(xa[0], xb[0]) + MAX(ya[0], yb[0]) + MAX(za[0], zb[0]) + m[12],
      MAX(xa[1], xb[1]) + MAX(ya[1], yb[1]) + MAX(za[1], zb[1]) + m[13],
      MAX(xa[2], xb[2]) + MAX(ya[2], yb[2]) + MAX(za[2], zb[2]) + m[14]
    };

    bounds[0] = MIN(bounds[0], min[0]);
    bounds[1] = MAX(bounds[1], max[0]);
    bounds[2] = MIN(bounds[2], min[1]);
    bounds[3] = MAX(bounds[3], max[1]);
    bounds[4] = MIN(bounds[4], min[2]);
    bounds[5] = MAX(bounds[5], max[2]);
  }

  for (uint32_t i = 0; i < node->childCount; i++) {
    applyAABB(model, node->children[i], bounds);
  }
}

void lovrModelGetBoundingBox(Model* model, float bounds[6]) {
  updateModelTransforms(model, model->data->rootNode, (float[]) MAT4_IDENTITY);
  bounds[0] = bounds[2] = bounds[4] = FLT_MAX;
  bounds[1] = bounds[3] = bounds[5] = FLT_MIN;
  applyAABB(model, model->data->rootNode, bounds);
}

void lovrModelGetBoundingSphere(Model* model, float sphere[4]) {
  lovrThrow("TODO");
}

// Font

Font* lovrFontCreate(Rasterizer* rasterizer) {
  Font* font = calloc(1, sizeof(Font));
  lovrAssert(font, "Out of memory");
  font->ref = 1;
  font->rasterizer = rasterizer;
  lovrRetain(rasterizer);
  return font;
}

void lovrFontDestroy(void* ref) {
  Font* font = ref;
  lovrRelease(font->rasterizer, lovrRasterizerDestroy);
  free(font);
}

// Helpers

// Allocates temporary memory, reclaimed when the next frame starts
static void* talloc(size_t size) {
  while (state.allocator.cursor + size > state.allocator.length) {
    lovrAssert(state.allocator.length << 1 <= state.allocator.limit, "Out of memory");
    os_vm_commit(state.allocator.memory + state.allocator.length, state.allocator.length);
    state.allocator.length <<= 1;
  }

  uint32_t cursor = ALIGN(state.allocator.cursor, 8);
  state.allocator.cursor = cursor + size;
  return state.allocator.memory + cursor;
}

// Like realloc for temporary memory, only supports growing, can be used as arr_t allocator
static void* tgrow(void* p, size_t n) {
  if (n == 0) return NULL;
  void* new = talloc(n);
  if (!p) return new;
  return memcpy(new, p, n >> 1);
}

// Suballocates from a Megabuffer
static Megaview allocateBuffer(gpu_memory_type type, uint32_t size, uint32_t align) {
  uint32_t active = state.buffers.active[type];
  uint32_t oldest = state.buffers.oldest[type];
  uint32_t cursor = state.buffers.cursor[type];

  if ((align & (align - 1)) == 0) {
    cursor = ALIGN(cursor, align);
  } else if (cursor % align != 0) {
    cursor += align - cursor % align;
  }

  if (type == GPU_MEMORY_CPU_WRITE) {
    state.stats.scratchMemory += (cursor - state.buffers.cursor[type]) + size;
  }

  // If there's an active Megabuffer and it has room, use it
  if (active != ~0u && cursor + size <= state.buffers.list[active].size) {
    state.buffers.cursor[type] = cursor + size;
    Megabuffer* buffer = &state.buffers.list[active];
    char* data = buffer->pointer ? (buffer->pointer + cursor) : NULL;
    return (Megaview) { .gpu = buffer->gpu, .data = data, .index = active, .offset = cursor };
  }

  // If the active Megabuffer is full and has no users, it can be reused when GPU is done with it
  if (active != ~0u && state.buffers.list[active].refs == 0) {
    recycleBuffer(active, type);
  }

  // If the GPU is finished with the oldest Megabuffer, use it
  // TODO can only use if big enough, search through all available ones
  if (oldest != ~0u && gpu_finished(state.buffers.list[oldest].tick)) {

    // Linked list madness (basically moving oldest -> active)
    state.buffers.oldest[type] = state.buffers.list[oldest].next;
    state.buffers.list[oldest].next = ~0u;
    state.buffers.active[type] = oldest;
    state.buffers.cursor[type] = size;

    Megabuffer* buffer = &state.buffers.list[oldest];
    return (Megaview) { .gpu = buffer->gpu, .data = buffer->pointer, .index = oldest, .offset = 0 };
  }

  // No Megabuffers were available, time for a new one
  lovrAssert(state.buffers.count < COUNTOF(state.buffers.list), "Out of Buffer memory");
  state.buffers.active[type] = active = state.buffers.count++;
  state.buffers.cursor[type] = size;

  Megabuffer* buffer = &state.buffers.list[active];
  buffer->size = type == GPU_MEMORY_GPU ? MAX(state.blockSize, size) : (1 << 24);
  buffer->next = ~0u;

  uint32_t usage[] = {
    [GPU_MEMORY_GPU] = ~0u,
    [GPU_MEMORY_CPU_WRITE] = ~(GPU_BUFFER_STORAGE | GPU_BUFFER_COPY_DST),
    [GPU_MEMORY_CPU_READ] = GPU_BUFFER_COPY_DST
  };

  gpu_buffer_info info = {
    .size = buffer->size,
    .usage = usage[type],
    .memory = type,
    .mapping = (void**) &buffer->pointer
  };

  lovrAssert(gpu_buffer_init(buffer->gpu, &info), "Failed to initialize Buffer");
  state.stats.bufferMemory += buffer->size;
  state.stats.memory += buffer->size;

  return (Megaview) { .gpu = buffer->gpu, .data = buffer->pointer, .index = active, .offset = 0 };
}

// Returns a Megabuffer to the pool
static void recycleBuffer(uint8_t index, gpu_memory_type type) {
  Megabuffer* buffer = &state.buffers.list[index];
  lovrCheck(buffer->refs == 0, "Trying to release a Buffer while people are still using it");

  // If there's a "newest" Megabuffer, make it the second newest
  if (state.buffers.newest[type] != ~0u) {
    state.buffers.list[state.buffers.newest[type]].next = index;
  }

  // If the waitlist is completely empty, this Megabuffer should become both the oldest and newest
  if (state.buffers.oldest[type] == ~0u) {
    state.buffers.oldest[type] = index;
  }

  // This Megabuffer is the newest
  state.buffers.newest[type] = index;
  buffer->next = ~0u;
  buffer->tick = state.tick;
}

static gpu_bundle* allocateBundle(uint32_t layout) {
  Bunch* bunch = state.bunches.head[layout];

  // If there's a bunch, try to use it
  if (bunch) {
    if (bunch->cursor < BUNDLES_PER_BUNCH) {
      return (gpu_bundle*) ((char*) bunch->bundles + gpu_sizeof_bundle() * bunch->cursor++);
    }

    // If the bunch had no room, move it to the end of the list, try using next one
    state.bunches.tail[layout]->next = bunch; // tail will never be NULL if head exists
    state.bunches.tail[layout] = bunch;
    state.bunches.head[layout] = bunch->next; // next will never be NULL, may be self-referential
    bunch->next = NULL;
    bunch->tick = state.tick;
    bunch = state.bunches.head[layout];
    if (gpu_finished(bunch->tick)) {
      bunch->cursor = 0;
      return (gpu_bundle*) ((char*) bunch->bundles + gpu_sizeof_bundle() * bunch->cursor++);
    }
  }

  // Otherwise, make a new one
  uint32_t index = state.bunches.count++;
  lovrCheck(index < MAX_BUNCHES, "Too many bunches, please report this encounter");
  bunch = &state.bunches.list[index];

  bunch->bundles = malloc(BUNDLES_PER_BUNCH * gpu_sizeof_bundle());
  lovrAssert(bunch->bundles, "Out of memory");

  gpu_bunch_info info = {
    .bundles = bunch->bundles,
    .layout = state.layouts[layout],
    .count = BUNDLES_PER_BUNCH
  };

  lovrAssert(gpu_bunch_init(bunch->gpu, &info), "Failed to initialize bunch");
  bunch->next = state.bunches.head[layout];
  state.bunches.head[layout] = bunch;
  if (!state.bunches.tail[layout]) state.bunches.tail[layout] = bunch;
  bunch->cursor = 1;
  return bunch->bundles;
}

static gpu_pass* lookupPass(Canvas* canvas) {
  Texture* texture = canvas->textures[0] ? canvas->textures[0] : canvas->depth.texture;
  bool resolve = texture->info.samples == 1 && canvas->samples > 1;

  gpu_pass_info info = {
    .views = texture->info.depth,
    .samples = canvas->samples,
    .resolve = resolve
  };

  for (uint32_t i = 0; i < COUNTOF(canvas->textures) && canvas->textures[i]; i++, info.count++) {
    info.color[i] = (gpu_pass_color_info) {
      .format = (gpu_texture_format) canvas->textures[i]->info.format,
      .load = (gpu_load_op) canvas->loads[i],
      .save = (gpu_save_op) GPU_SAVE_OP_SAVE,
      .usage = canvas->textures[i]->info.usage,
      .srgb = canvas->textures[i]->info.srgb
    };
  }

  if (canvas->depth.texture || canvas->depth.format) {
    info.depth = (gpu_pass_depth_info) {
      .format = (gpu_texture_format) (canvas->depth.texture ? canvas->depth.texture->info.format : canvas->depth.format),
      .load = (gpu_load_op) canvas->depth.load,
      .stencilLoad = (gpu_load_op) canvas->depth.load,
      .save = canvas->depth.texture ? GPU_SAVE_OP_SAVE : GPU_SAVE_OP_DISCARD,
      .stencilSave = canvas->depth.texture ? GPU_SAVE_OP_SAVE : GPU_SAVE_OP_DISCARD,
      .usage = canvas->depth.texture ? canvas->depth.texture->info.usage : 0
    };
  }

  uint64_t hash = hash64(&info, sizeof(info));
  for (uint32_t i = 0; i < state.gpuPassCount; i++) {
    if (state.passKeys[i] == hash) {
      return state.gpuPasses[i];
    }
  }

  lovrCheck(state.gpuPassCount < COUNTOF(state.gpuPasses), "Too many passes, please report this encounter");

  // Create new pass
  lovrAssert(gpu_pass_init(state.gpuPasses[state.gpuPassCount], &info), "Failed to initialize pass");
  state.passKeys[state.gpuPassCount] = hash;
  return state.gpuPasses[state.gpuPassCount++];
}

static uint32_t lookupLayout(gpu_slot* slots, uint32_t count) {
  uint64_t hash = hash64(slots, count * sizeof(gpu_slot));

  uint32_t index;
  for (index = 0; index < COUNTOF(state.layouts) && state.layoutLookup[index]; index++) {
    if (state.layoutLookup[index] == hash) {
      return index;
    }
  }

  lovrCheck(index < COUNTOF(state.layouts), "Too many shader layouts, please report this encounter");

  // Create new layout
  gpu_layout_info info = {
    .slots = slots,
    .count = count
  };

  lovrAssert(gpu_layout_init(state.layouts[index], &info), "Failed to initialize shader layout");
  state.layoutLookup[index] = hash;
  return index;
}

static uint32_t lookupMaterialBlock(MaterialFormat* format) {
  uint64_t hash = hash64(format, sizeof(*format));

  uint32_t index;
  for (index = 0; index < COUNTOF(state.materialLookup) && state.materialLookup[index]; index++) {
    if (state.materialLookup[index] == hash) {
      return index;
    }
  }

  lovrCheck(index < COUNTOF(state.materials), "Too many material types, try combining types, please report this encounter");
  state.materialLookup[index] = hash;

  MaterialBlock* block = &state.materials[index];
  block->format = *format;
  block->instances = malloc(MATERIALS_PER_BLOCK * sizeof(Material));
  lovrAssert(block->instances, "Out of memory");
  block->buffer = allocateBuffer(GPU_MEMORY_GPU, format->size * MATERIALS_PER_BLOCK, state.limits.uniformBufferAlign);
  for (uint32_t i = 0; i < MATERIALS_PER_BLOCK; i++) {
    Material* material = &block->instances[i];
    material->ref = 0;
    material->next = i + 1;
    material->block = index;
    material->index = i;
    material->tick = state.tick >= 4 ? state.tick - 4 : 0; // Some tick "far" in the past, so it can be used immediately
    material->textures = NULL;
  }
  block->instances[MATERIALS_PER_BLOCK - 1].next = ~0u;
  block->next = 0;

  block->bunch = malloc(gpu_sizeof_bunch());
  block->bundles = malloc(gpu_sizeof_bundle() * MATERIALS_PER_BLOCK);
  lovrAssert(block->bunch && block->bundles, "Out of memory");

  gpu_slot slots[16];
  slots[0] = (gpu_slot) { 0, GPU_SLOT_UNIFORM_BUFFER, GPU_STAGE_GRAPHICS, 1 };
  for (uint32_t i = 0; i < format->textureCount; i++) {
    slots[i + 1] = (gpu_slot) { format->textureSlots[i], GPU_SLOT_SAMPLED_TEXTURE, GPU_STAGE_GRAPHICS, 1 };
  }

  block->layout = lookupLayout(slots, format->textureCount + 1);

  gpu_bunch_info info = {
    .bundles = block->bundles,
    .layout = state.layouts[block->layout],
    .count = MATERIALS_PER_BLOCK
  };

  lovrAssert(gpu_bunch_init(block->bunch, &info), "Failed to initialize bunch for material block");
  lovrMaterialCreate(&(MaterialInfo) { .type = index }); // Default material is always first in block
  return index;
}

static void generateGeometry() {
  uint32_t total;

  // Vertices

  total = 0;
  uint32_t vertexCount[SHAPE_MAX];
  state.geometry.base[SHAPE_GRID] = total, total += vertexCount[SHAPE_GRID] = 129 * 129;
  state.geometry.base[SHAPE_CUBE] = total, total += vertexCount[SHAPE_CUBE] = 24;
  state.geometry.base[SHAPE_CONE] = total, total += vertexCount[SHAPE_CONE] = 768;
  state.geometry.base[SHAPE_TUBE] = total, total += vertexCount[SHAPE_TUBE] = 1024;
  state.geometry.base[SHAPE_BALL] = total, total += vertexCount[SHAPE_BALL] = (32 + 1) * (64 + 1);

  ShapeVertex* vertices;
  state.geometry.vertices = lovrBufferCreate(&(BufferInfo) {
    .type = BUFFER_VERTEX,
    .length = total,
    .format = VERTEX_SHAPE
  }, (void**) &vertices);

  // Grid
  uint32_t n = 0;
  for (uint32_t i = 0; i <= 128; i++) {
    for (uint32_t j = 0; j <= 128; j++) {
      float x = j / 128.f - .5f;
      float y = .5f - i / 128.f;
      float z = 0.f;
      uint16_t u = (x + .5f) * 0xffff;
      uint16_t v = (.5f - y) * 0xffff;
      *vertices++ = (ShapeVertex) { { x, y, z }, { 0x200, 0x200, 0x3ff, 0x0 }, { u, v } };
      n++;
    }
  }

  // Cube
  ShapeVertex cube[] = {
    { { -.5f, -.5f, -.5f }, { 0x200, 0x200, 0x000, 0x0 }, { 0x0000, 0x0000 } }, // Front
    { { -.5f,  .5f, -.5f }, { 0x200, 0x200, 0x000, 0x0 }, { 0x0000, 0xffff } },
    { {  .5f, -.5f, -.5f }, { 0x200, 0x200, 0x000, 0x0 }, { 0xffff, 0x0000 } },
    { {  .5f,  .5f, -.5f }, { 0x200, 0x200, 0x000, 0x0 }, { 0xffff, 0xffff } },
    { {  .5f,  .5f, -.5f }, { 0x3ff, 0x200, 0x200, 0x0 }, { 0x0000, 0xffff } }, // Right
    { {  .5f,  .5f,  .5f }, { 0x3ff, 0x200, 0x200, 0x0 }, { 0xffff, 0xffff } },
    { {  .5f, -.5f, -.5f }, { 0x3ff, 0x200, 0x200, 0x0 }, { 0x0000, 0x0000 } },
    { {  .5f, -.5f,  .5f }, { 0x3ff, 0x200, 0x200, 0x0 }, { 0xffff, 0x0000 } },
    { {  .5f, -.5f,  .5f }, { 0x200, 0x200, 0x3ff, 0x0 }, { 0x0000, 0x0000 } }, // Back
    { {  .5f,  .5f,  .5f }, { 0x200, 0x200, 0x3ff, 0x0 }, { 0x0000, 0xffff } },
    { { -.5f, -.5f,  .5f }, { 0x200, 0x200, 0x3ff, 0x0 }, { 0xffff, 0x0000 } },
    { { -.5f,  .5f,  .5f }, { 0x200, 0x200, 0x3ff, 0x0 }, { 0xffff, 0xffff } },
    { { -.5f,  .5f,  .5f }, { 0x000, 0x200, 0x200, 0x0 }, { 0x0000, 0xffff } }, // Left
    { { -.5f,  .5f, -.5f }, { 0x000, 0x200, 0x200, 0x0 }, { 0xffff, 0xffff } },
    { { -.5f, -.5f,  .5f }, { 0x000, 0x200, 0x200, 0x0 }, { 0x0000, 0x0000 } },
    { { -.5f, -.5f, -.5f }, { 0x000, 0x200, 0x200, 0x0 }, { 0xffff, 0x0000 } },
    { { -.5f, -.5f, -.5f }, { 0x200, 0x000, 0x200, 0x0 }, { 0x0000, 0x0000 } }, // Bottom
    { {  .5f, -.5f, -.5f }, { 0x200, 0x000, 0x200, 0x0 }, { 0xffff, 0x0000 } },
    { { -.5f, -.5f,  .5f }, { 0x200, 0x000, 0x200, 0x0 }, { 0x0000, 0xffff } },
    { {  .5f, -.5f,  .5f }, { 0x200, 0x000, 0x200, 0x0 }, { 0xffff, 0xffff } },
    { { -.5f,  .5f, -.5f }, { 0x200, 0x3ff, 0x200, 0x0 }, { 0x0000, 0xffff } }, // Top
    { { -.5f,  .5f,  .5f }, { 0x200, 0x3ff, 0x200, 0x0 }, { 0x0000, 0x0000 } },
    { {  .5f,  .5f, -.5f }, { 0x200, 0x3ff, 0x200, 0x0 }, { 0xffff, 0xffff } },
    { {  .5f,  .5f,  .5f }, { 0x200, 0x3ff, 0x200, 0x0 }, { 0xffff, 0x0000 } }
  };
  memcpy(vertices, cube, sizeof(cube));
  vertices += COUNTOF(cube);

  // Cone and tube
  ShapeVertex cone[768];
  ShapeVertex tube[1024];
  for (uint32_t i = 0; i < 256; i++) {
    float t = i / 256.f;
    float theta = t * 2.f * (float) M_PI;
    float x = cosf(theta) * .5f;
    float y = sinf(theta) * .5f;
    uint16_t nx = (x + .5f) * 0x3ff;
    uint16_t ny = (y + .5f) * 0x3ff;
    uint16_t u = (x + .5f) * 0xffff;
    uint16_t v = (.5f - y) * 0xffff;
    uint16_t oneOverRoot2 = 0x369;
    uint16_t conenx = (x + .5f) * oneOverRoot2;
    uint16_t coneny = (y + .5f) * oneOverRoot2;
    uint16_t conenz = oneOverRoot2;
    cone[i + 0x0] = (ShapeVertex) { { x, y, 0.f }, { 0x200, 0x200, 0x3ff, 0x0 }, { u, v } };
    cone[i + 256] = (ShapeVertex) { { x, y, 0.f }, { conenx, coneny, conenz, 0x0 }, { u, v } };
    cone[i + 512] = (ShapeVertex) { { 0.f, 0.f, -1.f }, { 0x200, 0x200, 0x200, 0x0 }, { u, v } };
    tube[i + 0x0] = (ShapeVertex) { { x, y, -.5f }, { nx, ny, 0x200, 0x0 }, { (1.f - t) * 0xffff, 0xffff } };
    tube[i + 256] = (ShapeVertex) { { x, y,  .5f }, { nx, ny, 0x200, 0x0 }, { (1.f - t) * 0xffff, 0x0000 } };
    tube[i + 512] = (ShapeVertex) { { x, y, -.5f }, { 0x200, 0x200, 0x000, 0x0 }, { 0xffff - u, v } };
    tube[i + 768] = (ShapeVertex) { { x, y,  .5f }, { 0x200, 0x200, 0x3ff, 0x0 }, { u, v } };
  }
  memcpy(vertices, cone, sizeof(cone)), vertices += COUNTOF(cone);
  memcpy(vertices, tube, sizeof(tube)), vertices += COUNTOF(tube);

  // Ball
  uint32_t lats = 32;
  uint32_t lons = 64;
  for (uint32_t lat = 0; lat <= lats; lat++) {
    float v = lat / (float) lats;
    float phi = v * (float) M_PI;
    float sinphi = sinf(phi);
    float cosphi = cosf(phi);
    for (uint32_t lon = 0; lon <= lons; lon++) {
      float u = lon / (float) lons;
      float theta = u * 2.f * (float) M_PI;
      float sintheta = sinf(theta);
      float costheta = cosf(theta);
      float x = sintheta * sinphi;
      float y = cosphi;
      float z = -costheta * sinphi;
      uint16_t nx = (x * .5f + 1.f) * 0x3ff + .5f;
      uint16_t ny = (y * .5f + 1.f) * 0x3ff + .5f;
      uint16_t nz = (z * .5f + 1.f) * 0x3ff + .5f;
      ShapeVertex vertex = { { x, y, z }, { nx, ny, nz, 0x0 }, { u * 0xffff, v * 0xffff } };
      memcpy(vertices, &vertex, sizeof(vertex));
      vertices++;
    }
  }

  // Indices

  total = 0;

  // The grid at detail n has (2^n)^2 cells, each cell is 2 triangles, each triangle is 3 indices
  for (uint32_t detail = 0; detail < 8; detail++) {
    uint32_t count = 6 * (1 << detail) * (1 << detail);
    state.geometry.start[SHAPE_GRID][detail] = total;
    state.geometry.count[SHAPE_GRID][detail] = count;
    total += count;
  }

  // The cube is a cube
  state.geometry.start[SHAPE_CUBE][0] = total;
  state.geometry.count[SHAPE_CUBE][0] = 36;
  total += 36;

  // The cone base has 4*2^n vertices arranged as a triangle fan (subtract 2 due to vertex sharing)
  // The cone tip has a triangle for each pair of the vertices, so 4*2^n extra triangles
  for (uint32_t detail = 0; detail < 7; detail++) {
    uint32_t vertexCount = 4 << detail;
    uint32_t baseCount = (vertexCount - 2) * 3;
    uint32_t tipCount = vertexCount * 3;
    uint32_t count = baseCount + tipCount;
    state.geometry.start[SHAPE_CONE][detail] = total;
    state.geometry.count[SHAPE_CONE][detail] = count;
    total += count;
  }

  // The tube is like a disk -- 4*2^n vertices for the rings, duplicated so that the tube and cap
  // can have different normals.  The tube vertices on opposite ends are stitched together with
  // quads, and the caps use the same triangle fan topology of the disk.
  for (uint32_t detail = 0; detail < 7; detail++) {
    uint32_t vertexCount = 4 << detail;
    uint32_t tubeIndexCount = 6 * vertexCount;
    uint32_t capIndexCount = 3 * (vertexCount - 2);
    uint32_t count = tubeIndexCount + 2 * capIndexCount;
    state.geometry.start[SHAPE_TUBE][detail] = total;
    state.geometry.count[SHAPE_TUBE][detail] = count;
    total += count;
  }

  // The ball has 2*2^n latitutdes (rows of quads) and 4*2^n longitudes, all are connected via quads
  // It's like a 2n x n grid of cells that gets wrapped around a ball
  for (uint32_t detail = 0; detail < 5; detail++) {
    uint32_t lats = 2 << detail;
    uint32_t lons = 4 << detail;
    uint32_t count = lats * lons * 6;
    state.geometry.start[SHAPE_BALL][detail] = total;
    state.geometry.count[SHAPE_BALL][detail] = count;
    total += count;
  }

  uint16_t* indices;
  state.geometry.indices = lovrBufferCreate(&(BufferInfo) {
    .type = BUFFER_INDEX,
    .length = total,
    .stride = sizeof(uint16_t),
    .fieldCount = 1,
    .types[0] = FIELD_U16
  }, (void**) &indices);

  // Grid
  for (uint32_t detail = 0; detail <= 7; detail++) {
    uint32_t n = 1 << detail;
    uint16_t skip = 1 << (7 - detail);
    uint16_t jump = 129 << (7 - detail);
    for (uint16_t row = 0, base = 0; row < n; row++, base += jump) {
      for (uint16_t col = 0, index = base; col < n; col++, index += skip) {
        /* a---b
         * | / |
         * c---d */
        uint16_t a = index;
        uint16_t b = index + skip;
        uint16_t c = index + jump;
        uint16_t d = index + jump + skip;
        uint16_t cell[6] = { a, b, c, b, d, c };
        memcpy(indices, cell, sizeof(cell));
        indices += COUNTOF(cell);
      }
    }
  }

  // Cube
  const uint16_t cubeIndex[] = {
     0,  1,  2,  2,  1,  3,
     4,  5,  6,  6,  5,  7,
     8,  9, 10, 10,  9, 11,
    12, 13, 14, 14, 13, 15,
    16, 17, 18, 18, 17, 19,
    20, 21, 22, 22, 21, 23
  };
  memcpy(indices, cubeIndex, sizeof(cubeIndex));
  indices += COUNTOF(cubeIndex);

  // Cone
  for (uint32_t detail = 0; detail <= 6; detail++) {
    uint16_t skip = 64 >> detail;
    uint16_t vertexCount = 4 << detail;
    uint16_t baseIndexCount = 3 * (vertexCount - 2);
    uint16_t tipIndexCount = 3 * vertexCount;
    for (uint16_t i = 0, j = skip; i < baseIndexCount; i += 3, j += skip) {
      *indices++ = 0;
      *indices++ = j;
      *indices++ = j + skip;
    }
    for (uint16_t i = 0, j = 0; i < tipIndexCount; i += 3, j += skip) {
      *indices++ = 256 + (j + skip) & 0xff;
      *indices++ = 256 + j;
      *indices++ = 512 + j;
    }
  }

  // Tube
  for (uint32_t detail = 0; detail <= 6; detail++) {
    uint16_t skip = 64 >> detail;
    uint16_t vertexCount = 4 << detail;
    uint16_t tubeIndexCount = 6 * vertexCount;
    uint16_t capIndexCount = 3 * (vertexCount - 2);
    for (uint16_t i = 0, j = 0; i < tubeIndexCount; i += 6, j = (j + skip) & 0xff) { // Tube
      uint16_t k = (j + skip) & 0xff;
      uint16_t quad[6] = { j, k, j + 256, j + 256, k, k + 256 };
      memcpy(indices, quad, sizeof(quad));
      indices += COUNTOF(quad);
    }
    for (uint16_t i = 0, j = skip; i < capIndexCount; i += 3, j += skip) { // -z cap
      *indices++ = 512;
      *indices++ = 768 - j;
      *indices++ = 768 - j - skip;
    }
    for (uint16_t i = 0, j = skip; i < capIndexCount; i += 3, j += skip) { // +z cap
      *indices++ = 768;
      *indices++ = 768 + j;
      *indices++ = 768 + j + skip;
    }
  }

  // Ball
  for (uint32_t detail = 0; detail <= 4; detail++) {
    uint16_t lats = 2 << detail;
    uint16_t lons = 4 << detail;
    uint16_t skip = 16 >> detail;
    uint16_t jump = 65 << (4 - detail);
    for (uint16_t i = 0, base = 0; i < lats; i++, base += jump) {
      for (uint16_t j = 0, index = base; j < lons; j++, index += skip) {
        uint16_t a = index;
        uint16_t b = index + skip;
        uint16_t c = index + jump;
        uint16_t d = index + jump + skip;
        uint16_t quad[6] = { a, b, c, b, d, c };
        memcpy(indices, quad, sizeof(quad));
        indices += COUNTOF(quad);
      }
    }
  }
}

static void clearState(gpu_pass* pass) {
  state.matrixIndex = 0;
  state.matrix = state.matrixStack[0];
  mat4_identity(state.matrix);

  state.pipelineIndex = 0;
  state.pipeline = &state.pipelineStack[0];
  memset(&state.pipeline->info, 0, sizeof(state.pipeline->info));
  state.pipeline->info.pass = pass;
  state.pipeline->info.depth.test = GPU_COMPARE_LEQUAL;
  state.pipeline->info.depth.write = true;
  state.pipeline->info.colorMask = 0xf;
  state.pipeline->format = 0;
  state.pipeline->color[0] = 1.f;
  state.pipeline->color[1] = 1.f;
  state.pipeline->color[2] = 1.f;
  state.pipeline->color[3] = 1.f;
  state.pipeline->shader = NULL;
  state.pipeline->dirty = true;

  state.emptyBindingMask = ~0u;
  state.bindingsDirty = true;

  memset(state.constantData, 0, state.limits.pushConstantSize);
  state.constantsDirty = true;

  state.drawCursor = 0;

  state.boundPipeline = NULL;
  state.boundBundle = NULL;
  state.boundMaterial = NULL;
  state.boundVertexBuffer = NULL;
  state.boundIndexBuffer = NULL;
}

static void onMessage(void* context, const char* message, int severe) {
  if (severe) {
    lovrThrow(message);
  } else {
    lovrLog(LOG_DEBUG, "GPU", message);
  }
}

static bool getInstanceExtensions(char* buffer, uint32_t size) {
  uint32_t count;
  const char** extensions = os_vk_get_instance_extensions(&count);
  for (uint32_t i = 0; i < count; i++) {
    size_t length = strlen(extensions[i]);
    if (length >= size) return false;
    memcpy(buffer, extensions[i], length);
    buffer[length] = ' ' ;
    buffer += length + 1;
    size -= length + 1;
  }

#ifndef LOVR_DISABLE_HEADSET
  if (lovrHeadsetDisplayDriver && lovrHeadsetDisplayDriver->getVulkanInstanceExtensions) {
    lovrHeadsetDisplayDriver->getVulkanInstanceExtensions(buffer, size);
  } else {
    buffer[count ? -1 : 0] = '\0';
  }
#else
  buffer[count ? -1 : 0] = '\0';
#endif

  return true;
}

static bool isDepthFormat(TextureFormat format) {
  return format == FORMAT_D16 || format == FORMAT_D24S8 || format == FORMAT_D32F;
}

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
    case FORMAT_BC6:
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
    default: lovrThrow("Unreachable");
  }
}

static void checkTextureBounds(const TextureInfo* info, uint16_t offset[4], uint16_t extent[3]) {
  uint16_t maxWidth = MAX(info->width >> offset[3], 1);
  uint16_t maxHeight = MAX(info->height >> offset[3], 1);
  uint16_t maxDepth = info->type == TEXTURE_VOLUME ? MAX(info->depth >> offset[3], 1) : info->depth;
  lovrCheck(offset[0] + extent[0] <= maxWidth, "Texture x range [%d,%d] exceeds width (%d)", offset[0], offset[0] + extent[0], maxWidth);
  lovrCheck(offset[1] + extent[1] <= maxHeight, "Texture y range [%d,%d] exceeds height (%d)", offset[1], offset[1] + extent[1], maxHeight);
  lovrCheck(offset[2] + extent[2] <= maxDepth, "Texture z range [%d,%d] exceeds depth (%d)", offset[2], offset[2] + extent[2], maxDepth);
  lovrCheck(offset[3] < info->mipmaps, "Texture mipmap %d exceeds its mipmap count (%d)", offset[3] + 1, info->mipmaps);
}

#define MIN_SPIRV_WORDS 8

typedef union {
  struct {
    uint16_t location;
    uint16_t name;
  } attribute;
  struct {
    uint8_t group;
    uint8_t binding;
    uint16_t name;
  } resource;
  struct {
    uint16_t number;
    uint16_t name;
  } flag;
  struct {
    uint32_t word;
  } constant;
  struct {
    uint16_t word;
    uint16_t name;
  } type;
} CacheData;

// Only an explicit set of spir-v capabilities are allowed
// Some capabilities require a GPU feature to be supported
// Some common unsupported capabilities are checked directly, to provide better error messages
static bool checkShaderCapability(uint32_t capability) {
  switch (capability) {
    case 0: break; // Matrix
    case 1: break; // Shader
    case 2: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "geometry shading");
    case 3: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "tessellation shading");
    case 5: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "linkage");
    case 9: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "half floats");
    case 10: lovrCheck(state.features.float64, "GPU does not support shader feature #%d: %s", capability, "64 bit floats"); break;
    case 11: lovrCheck(state.features.int64, "GPU does not support shader feature #%d: %s", capability, "64 bit integers"); break;
    case 12: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "64 bit atomics");
    case 22: lovrCheck(state.features.int16, "GPU does not support shader feature #%d: %s", capability, "16 bit integers"); break;
    case 23: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "tessellation shading");
    case 24: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "geometry shading");
    case 25: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "extended image gather");
    case 27: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "multisample storage textures");
    case 28: lovrCheck(state.features.dynamicIndexing, "GPU does not support shader feature #%d: %s", capability, "dynamic indexing"); break;
    case 29: lovrCheck(state.features.dynamicIndexing, "GPU does not support shader feature #%d: %s", capability, "dynamic indexing"); break;
    case 30: lovrCheck(state.features.dynamicIndexing, "GPU does not support shader feature #%d: %s", capability, "dynamic indexing"); break;
    case 31: lovrCheck(state.features.dynamicIndexing, "GPU does not support shader feature #%d: %s", capability, "dynamic indexing"); break;
    case 32: lovrCheck(state.features.clipDistance, "GPU does not support shader feature #%d: %s", capability, "clip distance"); break;
    case 33: lovrCheck(state.features.cullDistance, "GPU does not support shader feature #%d: %s", capability, "cull distance"); break;
    case 34: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "cubemap array textures");
    case 35: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "sample rate shading");
    case 36: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "rectangle textures");
    case 37: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "rectangle textures");
    case 39: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "8 bit integers");
    case 40: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "input attachments");
    case 41: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "sparse residency");
    case 42: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "min LOD");
    case 43: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "1D textures");
    case 44: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "1D textures");
    case 45: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "cubemap array textures");
    case 46: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "texel buffers");
    case 47: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "texel buffers");
    case 48: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "multisampled storage textures");
    case 49: break; // StorageImageExtendedFormats (?)
    case 50: break; // ImageQuery
    case 51: break; // DerivativeControl
    case 52: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "sample rate shading");
    case 53: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "transform feedback");
    case 54: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "geometry shading");
    case 55: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "autoformat storage textures");
    case 56: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "autoformat storage textures");
    case 57: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "multiviewport");
    case 69: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "layered rendering");
    case 70: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "multiviewport");
    case 4427: break; // ShaderDrawParameters
    case 4437: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "multigpu");
    case 4439: lovrCheck(state.limits.renderSize[2] > 1, "GPU does not support shader feature #%d: %s", capability, "multiview"); break;
    case 5301: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "non-uniform indexing");
    case 5306: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "non-uniform indexing");
    case 5307: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "non-uniform indexing");
    case 5308: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "non-uniform indexing");
    case 5309: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "non-uniform indexing");
    default: lovrThrow("Shader uses unknown feature #%d", capability);
  }
  return false;
}

// Parse a slot type and array size from a variable instruction, throwing on failure
static void parseResourceType(const uint32_t* words, uint32_t wordCount, CacheData* cache, uint32_t bound, const uint32_t* instruction, gpu_slot_type* slotType, uint32_t* count) {
  const uint32_t* edge = words + wordCount - MIN_SPIRV_WORDS;
  uint32_t type = instruction[1];
  uint32_t id = instruction[2];
  uint32_t storageClass = instruction[3];

  // Follow the variable's type id to the OpTypePointer instruction that declares its pointer type
  // Then unwrap the pointer to get to the inner type of the variable
  instruction = words + cache[type].type.word;
  lovrCheck(instruction < edge && instruction[3] < bound, "Invalid Shader code: id overflow");
  instruction = words + cache[instruction[3]].type.word;
  lovrCheck(instruction < edge, "Invalid Shader code: id overflow");

  if ((instruction[0] & 0xffff) == 28) { // OpTypeArray
    // Read array size
    lovrCheck(instruction[3] < bound && words + cache[instruction[3]].constant.word < edge, "Invalid Shader code: id overflow");
    const uint32_t* size = words + cache[instruction[3]].type.word;
    if ((size[0] & 0xffff) == 43 || (size[0] & 0xffff) == 50) { // OpConstant || OpSpecConstant
      *count = size[3]; // Both constant instructions store their value in the 4th word
    } else {
      lovrThrow("Invalid Shader code: resource %d is an array, but the array size is not a constant", id);
    }

    // Unwrap array to get to inner array type and keep going
    lovrCheck(instruction[2] < bound && words + cache[instruction[2]].type.word < edge, "Invalid Shader code: id overflow");
    instruction = words + cache[instruction[2]].type.word;
  } else {
    *count = 1;
  }

  // Use StorageClass to detect uniform/storage buffers
  switch (storageClass) {
    case 12: *slotType = GPU_SLOT_STORAGE_BUFFER; return;
    case 2: *slotType = GPU_SLOT_UNIFORM_BUFFER; return;
    default: break; // It's not a Buffer, keep going to see if it's a valid Texture
  }

  // If it's a sampler type, we're done
  if ((instruction[0] & 0xffff) == 26) { // OpTypeSampler
    *slotType = GPU_SLOT_SAMPLER;
    return;
  }

  // Combined image samplers are currently not supported (thank you webgpu)
  if ((instruction[0] & 0xffff) == 27) { // OpTypeSampledImage
    lovrThrow("Invalid Shader code: combined image samplers (e.g. sampler2D) are not currently supported");
  } else if ((instruction[0] & 0xffff) != 25) { // OpTypeImage
    lovrThrow("Invalid Shader code: variable %d is not recognized as a valid buffer or texture resource", id);
  }

  // Reject texel buffers (DimBuffer) and input attachments (DimSubpassData)
  if (instruction[3] == 5 || instruction[3] == 6) {
    lovrThrow("Unsupported Shader code: texel buffers and input attachments are not supported");
  }

  // Read the Sampled key to determine if it's a sampled image (1) or a storage image (2)
  switch (instruction[7]) {
    case 1: *slotType = GPU_SLOT_SAMPLED_TEXTURE; return;
    case 2: *slotType = GPU_SLOT_STORAGE_TEXTURE; return;
    default: lovrThrow("Unsupported Shader code: texture variable %d is not marked as a sampled image or storage image", id);
  }
}

// Beware, this function is long and ugly.  It parses spirv bytecode.  Maybe it could be moved to
// its own library or swapped out for spirv-reflect.  However, spirv-reflect would cost 6k+ lines
// and require a bunch of work to parse *its* output and transform it into something useful.
static bool parseSpirv(const void* source, uint32_t size, uint8_t stage, ReflectionInfo* reflection) {
  const uint32_t* words = source;
  uint32_t wordCount = size / sizeof(uint32_t);
  const uint32_t* edge = words + wordCount - MIN_SPIRV_WORDS;

  if (wordCount < MIN_SPIRV_WORDS || words[0] != 0x07230203) {
    return false;
  }

  uint32_t bound = words[3];
  lovrCheck(bound < 0xffff, "Unsupported Shader code: id bound is too big (max is 65534)");

  // The cache stores information for spirv ids
  // - For buffer/texture resources, stores the slot's group and binding number
  // - For vertex attributes, stores the attribute location decoration
  // - For specialization constants, stores the constant's name and its constant id
  // - For types, stores the id of its declaration instruction and its name (for block resources)
  size_t cacheSize = bound * sizeof(CacheData);
  void* cacheData = talloc(cacheSize);
  memset(cacheData, 0xff, cacheSize);
  CacheData* cache = cacheData;

  uint32_t pushConstantStructId = ~0u;
  uint32_t materialStructId = ~0u;

  const uint32_t* instruction = words + 5;

  // Performs a single pass over the spirv words, doing the following:
  // - Validate declared capabilities
  // - Gather metadata about slots (binding number, type, stage, count) in first 2 groups
  // - Gather metadata about specialization constants
  // - Parse struct layout of push constant block
  // - Determine which attribute locations are defined (not the locations that are used, yet)
  while (instruction < words + wordCount) {
    uint16_t opcode = instruction[0] & 0xffff;
    uint16_t length = instruction[0] >> 16;

    uint32_t id;
    uint32_t index;
    uint32_t decoration;
    uint32_t value;
    const char* name;

    lovrCheck(length > 0, "Invalid Shader code: zero-length instruction");
    lovrCheck(instruction + length <= words + wordCount, "Invalid Shader code: instruction overflow");

    switch (opcode) {
      case 17: // OpCapability
        if (length != 2) break;
        checkShaderCapability(instruction[1]);
        break;
      case 5: // OpName
        if (length < 3 || instruction[1] >= bound) break;
        id = instruction[1];
        cache[id].type.name = instruction - words + 2;
        name = (char*) (instruction + 2);
        // This is not technically correct, because it relies on the OpName for the block to come
        // before the OpMemberName's for its members.  It looks like most compilers output things in
        // this order, and doing it the right way would require 2 passees over the words.  Can be
        // improved if/when there's evidence of a compiler that uses the opposite order.
        if (!strncmp(name, "Constants", sizeof("Constants"))) {
          pushConstantStructId = id;
        }
        if (!strncmp(name, "Material", sizeof("Material"))) {
          materialStructId = id;
        }
        break;
      case 6: // OpMemberName
        if (length < 4 || instruction[1] >= bound) break;
        id = instruction[1];
        index = instruction[2];
        name = (char*) (instruction + 3);
        size_t nameLength = strnlen(name, (length - 3) * 4);
        if (id == pushConstantStructId) {
          lovrCheck(index < COUNTOF(reflection->constantLookup), "Too many constant fields");
          reflection->constantLookup[index] = hash32(name, nameLength);
        } else if (id == materialStructId) {
          lovrCheck(index < COUNTOF(reflection->material.names), "Too many material fields");
          reflection->material.names[index] = hash32(name, nameLength);
          // There is a somewhat magical convention to make material properties more convenient
          // - Vector fields ending with "color"/"scale" have a default value of 1s
          // - Properties ending with "color" are gamma corrected
          // The string comparisons are case insensitive
          if (nameLength >= 5 && !memcmp(name + (nameLength - 5), "color", strlen("color"))) {
            reflection->material.colors |= (1 << index);
          } else if (nameLength >= 5 && !memcmp(name + (nameLength - 5), "scale", strlen("scale"))) {
            reflection->material.scales |= (1 << index);
          } else if (nameLength >= 5 && !memcmp(name + (nameLength - 5), "Color", strlen("Color"))) {
            reflection->material.colors |= (1 << index);
          } else if (nameLength >= 5 && !memcmp(name + (nameLength - 5), "Scale", strlen("Scale"))) {
            reflection->material.scales |= (1 << index);
          }
        }
        break;
      case 71: // OpDecorate
        if (length < 4 || instruction[1] >= bound) break;

        id = instruction[1];
        decoration = instruction[2];
        value = instruction[3];

        if (decoration == 33) { // Binding
          lovrCheck(value < 32, "Unsupported Shader code: variable %d uses binding %d, but the binding must be less than 32", id, value);
          cache[id].resource.binding = value;
        } else if (decoration == 34) { // Group
          lovrCheck(value < 2, "Unsupported Shader code: variable %d is in group %d, but group must be less than 2", id, value);
          cache[id].resource.group = value;
        } else if (decoration == 30) { // Location
          lovrCheck(value < 32, "Unsupported Shader code: vertex shader uses attribute location %d, but locations must be less than 16", value);
          cache[id].attribute.location = value;
        } else if (decoration == 1) { // SpecId
          lovrCheck(value <= 2000, "Unsupported Shader code: specialization constant id is too big (max is 2000)");
          cache[id].flag.number = value;
        }
        break;
      case 72: // OpMemberDecorate
        if (length < 5 || instruction[1] >= bound) break;
        id = instruction[1];
        index = instruction[2];
        decoration = instruction[3];
        value = instruction[4];
        if (decoration != 35) { // Offset
          break;
        }
        if (id == pushConstantStructId) {
          lovrCheck(index < COUNTOF(reflection->constantOffsets), "Too many constants");
          reflection->constantOffsets[index] = value;
        } else if (id == materialStructId) {
          lovrCheck(index < COUNTOF(reflection->material.offsets), "Too many material fields");
          reflection->material.offsets[index] = value;
        }
        break;
      case 19: // OpTypeVoid
      case 20: // OpTypeBool
      case 21: // OpTypeInt
      case 22: // OpTypeFloat
      case 23: // OpTypeVector
      case 24: // OpTypeMatrix
      case 25: // OpTypeImage
      case 26: // OpTypeSampler
      case 27: // OpTypeSampledImage
      case 28: // OpTypeArray
      case 29: // OpTypeRuntimeArray
      case 30: // OpTypeStruct
      case 31: // OpTypeOpaque
      case 32: // OpTypePointer
        if (length < 2 || instruction[1] >= bound) break;
        cache[instruction[1]].type.word = instruction - words;
        break;
      case 48: // OpSpecConstantTrue
      case 49: // OpSpecConstantFalse
      case 50: // OpSpecConstant
        if (length < 2 || instruction[2] >= bound) break;
        id = instruction[2];

        lovrCheck(reflection->flagCount < COUNTOF(reflection->flags), "Shader has too many flags");
        index = reflection->flagCount++;

        lovrCheck(cache[id].flag.number != 0xffff, "Invalid Shader code: Specialization constant has no ID");
        reflection->flags[index].id = cache[id].flag.number;

        // If it's a regular SpecConstant, parse its type for i32/u32/f32, otherwise use b32
        if (opcode == 50) {
          const uint32_t* type = words + cache[instruction[1]].type.word;
          lovrCheck(type < edge, "Invalid Shader code: Specialization constant has invalid type");
          if ((type[0] & 0xffff) == 21 && type[2] == 32) { // OpTypeInt
            if (type[3] == 0) {
              reflection->flags[index].type = GPU_FLAG_U32;
            } else {
              reflection->flags[index].type = GPU_FLAG_I32;
            }
          } else if ((type[0] & 0xffff) == 22 && type[2] == 32) { // OpTypeFloat
            reflection->flags[index].type = GPU_FLAG_F32;
          } else {
            lovrThrow("Invalid Shader code: Specialization constant has unsupported type (use bool, int, uint, or float)");
          }
        } else {
          reflection->flags[index].type = GPU_FLAG_B32;
        }

        if (cache[id].flag.name != 0xffff) {
          uint32_t nameWord = cache[id].flag.name;
          name = (char*) (words + nameWord);
          size_t nameLength = strnlen(name, (wordCount - nameWord) * sizeof(uint32_t));
          reflection->flagNames[index] = hash32(name, nameLength);
        }

        // Currently, the cache contains {number,name}.  It gets replaced with the index of this
        // word so that other people using this constant (OpTypeArray) can find it later.
        cache[id].constant.word = instruction - words;
        break;
      case 43: // OpConstant
        if (length < 3 || instruction[2] >= bound) break;
        cache[instruction[2]].constant.word = instruction - words;
        break;
      case 59: // OpVariable
        if (length < 4 || instruction[2] >= bound) break;

        id = instruction[2];
        uint32_t type = instruction[1];
        uint32_t storageClass = instruction[3];

        // Vertex shaders track attribute locations (Input StorageClass (1) decorated with Location)
        if (stage == GPU_STAGE_VERTEX && storageClass == 1 && cache[id].attribute.location < 32) {
          reflection->attributeMask |= (1 << cache[id].attribute.location);
          break;
        }

        // Parse push constant types (currently only scalars, vectors, and matrices are supported)
        // Member count of OpTypeStruct is the instruction length - 2, type ids start at word #2
        // TODO this needs to be hardened to check for valid ids / overflows
        if (storageClass == 9 && reflection->constantCount == 0) {
          uint32_t structId = (words + cache[type].type.word)[3];
          const uint32_t* structType = words + cache[structId].type.word;
          reflection->constantCount = (structType[0] >> 16) - 2;
          for (uint32_t i = 0; i < reflection->constantCount; i++) {
            uint32_t fieldId = structType[2 + i];
            const uint32_t* fieldType = words + cache[fieldId].type.word;
            uint32_t fieldOpcode = fieldType[0] & 0xffff;

            bool matrix = false;
            bool vector = false;
            FieldType scalar;

            uint32_t columnCount = 1;
            uint32_t componentCount = 1;

            if (fieldOpcode == 24) { // OpTypeMatrix
              matrix = true;
              columnCount = fieldType[3];
              fieldId = fieldType[2];
              fieldType = words + cache[fieldId].type.word;
              fieldOpcode = fieldType[0] & 0xffff;
            }

            if (fieldOpcode == 23) { // OpTypeVector
              vector = true;
              componentCount = fieldType[3];
              fieldId = fieldType[2];
              fieldType = words + cache[fieldId].type.word;
              fieldOpcode = fieldType[0] & 0xffff;
            }

            if (fieldOpcode == 22) { // OpTypeFloat
              lovrCheck(fieldType[2] == 32, "Currently, push constant floats must be 32 bits");
              scalar = FIELD_F32;
            } else if (fieldOpcode == 21) { // OpTypeInt
              lovrCheck(fieldType[2] == 32, "Currently, push constant integers must be 32 bits");
              if (fieldType[3] > 0) {
                scalar = FIELD_I32;
              } else {
                scalar = FIELD_U32;
              }
            } else { // OpTypeBool
              lovrCheck(fieldOpcode == 20, "Unsupported push constant type");
              scalar = FIELD_U32;
            }

            if (matrix) {
              lovrCheck(vector, "Invalid shader code: Matrices must contain vectors");
              lovrCheck(scalar == FIELD_F32, "Invalid shader code: Matrices must be floating point");
              lovrCheck(columnCount == componentCount, "Currently, only square matrices are supported");
              switch (columnCount) {
                case 2: reflection->constantTypes[i] = FIELD_MAT2; break;
                case 3: reflection->constantTypes[i] = FIELD_MAT3; break;
                case 4: reflection->constantTypes[i] = FIELD_MAT4; break;
                default: lovrThrow("Invalid shader code: Matrices must have 2, 3, or 4 columns");
              }
            } else if (vector) {
              if (scalar == FIELD_I32) {
                if (componentCount == 2) reflection->constantTypes[i] = FIELD_I32x2;
                if (componentCount == 3) reflection->constantTypes[i] = FIELD_I32x3;
                if (componentCount == 4) reflection->constantTypes[i] = FIELD_I32x4;
              } else if (scalar == FIELD_U32) {
                if (componentCount == 2) reflection->constantTypes[i] = FIELD_U32x2;
                if (componentCount == 3) reflection->constantTypes[i] = FIELD_U32x3;
                if (componentCount == 4) reflection->constantTypes[i] = FIELD_U32x4;
              } else if (scalar == FIELD_F32) {
                if (componentCount == 2) reflection->constantTypes[i] = FIELD_F32x2;
                if (componentCount == 3) reflection->constantTypes[i] = FIELD_F32x3;
                if (componentCount == 4) reflection->constantTypes[i] = FIELD_F32x4;
              }
            } else {
              reflection->constantTypes[i] = scalar;
            }

            uint32_t totalSize = columnCount * componentCount * 4;
            uint32_t limit = state.limits.pushConstantSize;
            uint32_t offset = reflection->constantOffsets[i];
            lovrCheck(offset + totalSize <= limit, "Size of push constant block exceeds 'shaderConstantSize' limit");
            reflection->constantSize = MAX(reflection->constantSize, offset + totalSize);
          }
          break;
        }

        uint32_t group = cache[id].resource.group;
        uint32_t number = cache[id].resource.binding;

        // Parse Material struct format
        if (group == 1 && number == 0 && storageClass == 2) {
          uint32_t structId = (words + cache[type].type.word)[3];
          const uint32_t* structType = words + cache[structId].type.word;
          reflection->material.count = (structType[0] >> 16) - 2;

          for (uint32_t i = 0; i < reflection->material.count; i++) {
            uint32_t fieldId = structType[2 + i];
            const uint32_t* fieldType = words + cache[fieldId].type.word;
            uint32_t fieldOpcode = fieldType[0] & 0xffff;
            uint32_t totalSize = 4;

            if (fieldOpcode == 23) { // OpTypeVector
              uint32_t componentCount = fieldType[3];
              totalSize *= componentCount;
              switch (componentCount) {
                case 2: reflection->material.types[i] = FIELD_F32x2; break;
                case 3: reflection->material.types[i] = FIELD_F32x3; break;
                case 4: reflection->material.types[i] = FIELD_F32x4; break;
                default: lovrThrow("Invalid vector component count");
              }
              reflection->material.vectors |= (1 << i);
              fieldId = fieldType[2];
              fieldType = words + cache[fieldId].type.word;
              fieldOpcode = fieldType[0] & 0xffff;
              lovrCheck(fieldOpcode == 22, "Currently, material vectors must contain 32 bit floats");
            } else if (fieldOpcode == 22) { // OpTypeFloat
              lovrCheck(fieldType[2] == 32, "Currently, material floats must be 32 bits");
              reflection->material.types[i] = FIELD_F32;
              reflection->material.scalars |= (1 << i);
            } else if (fieldOpcode == 21) { // OpTypeInt
              lovrCheck(fieldType[2] == 32, "Currently, material integers must be 32 bits");
              if (fieldType[3] > 0) {
                reflection->material.types[i] = FIELD_I32;
              } else {
                reflection->material.types[i] = FIELD_U32;
              }
              reflection->material.scalars |= (1 << i);
            } else if (fieldOpcode == 20) { // OpTypeBool
              reflection->material.types[i] = FIELD_U32;
              reflection->material.scalars |= (1 << i);
            } else {
              lovrThrow("Invalid material field type");
            }

            uint32_t offset = reflection->material.offsets[i];
            lovrCheck(offset + totalSize <= 1024, "Currently, material data must be less than or equal to 1024 bytes");
            reflection->material.size = MAX(reflection->material.size, offset + totalSize);
          }
        }

        // Material textures
        if (group == 1 && number > 0 && storageClass == 0) {
          uint32_t imageId = (words + cache[type].type.word)[3];
          const uint32_t* imageType = words + cache[imageId].type.word;
          uint32_t imageOpcode = imageType[0] & 0xffff;
          lovrCheck(imageOpcode == 25, "Materials can only contain textures (group 1, slot > 0)");

          if (cache[id].resource.name != 0xffff) {
            uint32_t nameWord = cache[id].resource.name;
            char* name = (char*) (words + nameWord);
            size_t nameLength = strnlen(name, (wordCount - nameWord) * sizeof(uint32_t));
            uint32_t hash = hash32(name, nameLength);
            uint32_t textureIndex = reflection->material.textureCount++;
            reflection->material.textureNames[textureIndex] = hash;
            reflection->material.textureSlots[textureIndex] = number;
          }
        }

        // Ignore inputs/outputs and variables that aren't decorated with a group and binding (e.g. globals)
        if (storageClass == 1 || storageClass == 3 || group > 2 || number == 0xff) {
          break;
        }

        uint32_t count;
        gpu_slot_type slotType;
        parseResourceType(words, wordCount, cache, bound, instruction, &slotType, &count);

        // Name (buffers need to be unwrapped to get the struct name, images/samplers are named directly)
        bool buffer = slotType == GPU_SLOT_UNIFORM_BUFFER || slotType == GPU_SLOT_STORAGE_BUFFER;
        uint32_t namedType = buffer ? (words + cache[type].type.word)[3] : type;
        uint32_t nameWord = buffer ? cache[namedType].type.name : cache[namedType].resource.name;
        if (group == 2 && !reflection->slotNames[number] && nameWord != 0xffff) {
          char* name = (char*) (words + nameWord);
          size_t nameLength = strnlen(name, (wordCount - nameWord) * sizeof(uint32_t));
          reflection->slotNames[number] = hash32(name, nameLength);
        }

        // Either merge our info into an existing slot, or add the slot
        gpu_slot* slot = &reflection->slots[group][number];
        if (slot->stage != 0) {
          lovrCheck(slot->type == slotType, "Variable (%d,%d) is in multiple shader stages with different types");
          lovrCheck(slot->count == count, "Variable (%d,%d) is in multiple shader stages with different array lengths");
          slot->stage |= stage;
        } else {
          lovrCheck(count > 0, "Variable (%d,%d) has array length of zero");
          lovrCheck(count < 256, "Variable (%d,%d) has array length of %d, but the max is 255", count);
          slot->number = number;
          slot->type = slotType;
          slot->stage = stage;
          slot->count = count;
        }
        break;
      case 54: // OpFunction
        instruction = words + wordCount; // Exit early upon encountering actual shader code
        break;
      default:
        break;
    }

    instruction += length;
  }

  return true;
}

static gpu_texture* getScratchTexture(uint32_t size[2], uint32_t layers, TextureFormat format, bool srgb, uint32_t samples) {
  uint16_t key[] = { size[0], size[1], layers, format, srgb, samples };
  uint32_t hash = hash32(key, sizeof(key));

  // Search for matching texture in cache table
  uint32_t rows = COUNTOF(state.attachmentCache);
  uint32_t cols = COUNTOF(state.attachmentCache[0]);
  ScratchTexture* row = state.attachmentCache[hash & (rows - 1)];
  ScratchTexture* entry = NULL;
  for (uint32_t i = 0; i < cols; i++) {
    if (row[i].hash == hash) {
      entry = &row[i];
      break;
    }
  }

  // If there's a match, use it
  if (entry) {
    entry->tick = state.tick;
    return entry->handle;
  }

  // Otherwise, create new texture, add to an empty slot, evicting oldest if needed
  gpu_texture_info info = {
    .type = GPU_TEXTURE_ARRAY,
    .format = (gpu_texture_format) format,
    .size[0] = size[0],
    .size[1] = size[1],
    .size[2] = layers,
    .mipmaps = 1,
    .samples = samples,
    .usage = GPU_TEXTURE_RENDER | GPU_TEXTURE_TRANSIENT,
    .upload.stream = state.uploads->stream,
    .srgb = srgb
  };
  arr_push(&state.uploads->textures, (TextureAccess) { 0 }); // TODO maybe better way to ensure upload stream gets submitted

  entry = &row[0];
  for (uint32_t i = 1; i < cols; i++) {
    if (!row[i].handle || row[i].tick < entry->tick) {
      entry = &row[i];
      break;
    }
  }

  if (!entry->handle) {
    entry->handle = calloc(1, gpu_sizeof_texture());
    lovrAssert(entry->handle, "Out of memory");
  } else {
    gpu_texture_destroy(entry->handle);
  }

  lovrAssert(gpu_texture_init(entry->handle, &info), "Failed to create scratch texture");
  entry->hash = hash;
  entry->tick = state.tick;
  return entry->handle;
}

static void updateModelTransforms(Model* model, uint32_t nodeIndex, float* parent) {
  if (!model->transformsDirty) return;

  mat4 global = model->globalTransforms + 16 * nodeIndex;
  NodeTransform* local = &model->localTransforms[nodeIndex];
  vec3 T = local->properties[PROP_TRANSLATION];
  quat R = local->properties[PROP_ROTATION];
  vec3 S = local->properties[PROP_SCALE];

  mat4_init(global, parent);
  mat4_translate(global, T[0], T[1], T[2]);
  mat4_rotateQuat(global, R);
  mat4_scale(global, S[0], S[1], S[2]);

  ModelNode* node = &model->data->nodes[nodeIndex];
  for (uint32_t i = 0; i < node->childCount; i++) {
    updateModelTransforms(model, node->children[i], global);
  }

  model->transformsDirty = false;
}
