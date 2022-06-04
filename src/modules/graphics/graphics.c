#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "math/math.h"
#include "core/gpu.h"
#include "core/maf.h"
#include "core/spv.h"
#include "core/os.h"
#include "util.h"
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
#define MAX_RESOURCES_PER_SHADER 32

typedef struct {
  gpu_vertex_format gpu;
  uint64_t hash;
  uint32_t mask;
} VertexFormat;

struct Buffer {
  uint32_t ref;
  uint32_t size;
  char* pointer;
  gpu_buffer* gpu;
  BufferInfo info;
  VertexFormat format;
};

struct Texture {
  uint32_t ref;
  gpu_texture* gpu;
  gpu_texture* renderView;
  TextureInfo info;
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

struct Shader {
  uint32_t ref;
  Shader* parent;
  gpu_shader* gpu;
  ShaderInfo info;
  uint32_t layout;
  uint32_t computePipeline;
  uint32_t attributeMask;
  uint32_t bufferMask;
  uint32_t textureMask;
  uint32_t samplerMask;
  uint32_t storageMask;
  uint32_t constantSize;
  uint32_t constantCount;
  uint32_t resourceCount;
  ShaderConstant* constants;
  ShaderResource* resources;
  uint32_t flagCount;
  uint32_t overrideCount;
  gpu_shader_flag* flags;
  uint32_t* flagLookup;
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
  uint64_t formatHash;
  gpu_pipeline_info info;
  bool dirty;
} Pipeline;

typedef struct {
  float transform[16];
  float cofactor[16];
  float color[4];
} DrawData;

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
};

typedef enum {
  VERTEX_SHAPE,
  VERTEX_POINT,
  VERTEX_MODEL,
  VERTEX_GLYPH,
  VERTEX_EMPTY,
  DEFAULT_FORMAT_COUNT
} DefaultFormat;

typedef struct {
  struct { float x, y, z; } position;
  struct { float x, y, z; } normal;
  struct { float u, v; } uv;
} ShapeVertex;

typedef struct {
  gpu_draw_mode mode;
  DefaultShader shader;
  float* transform;
  struct {
    Buffer* buffer;
    DefaultFormat format;
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
} Draw;

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
} Attachment;

typedef struct {
  char* memory;
  uint32_t cursor;
  uint32_t length;
} Allocator;

static struct {
  bool initialized;
  bool active;
  uint32_t tick;
  Pass* transfers;
  gpu_device_info device;
  gpu_features features;
  gpu_limits limits;
  float background[4];
  Texture* window;
  Attachment attachments[16];
  Buffer* defaultBuffer;
  Texture* defaultTexture;
  Sampler* defaultSampler;
  Shader* defaultShaders[DEFAULT_SHADER_COUNT];
  VertexFormat vertexFormats[DEFAULT_FORMAT_COUNT];
  map_t pipelineLookup;
  arr_t(gpu_pipeline*) pipelines;
  arr_t(Layout) layouts;
  uint32_t builtinLayout;
  Allocator allocator;
} state;

// Helpers

static void* tempAlloc(size_t size);
static void beginFrame(void);
static gpu_stream* getTransfers(void);
static uint32_t getLayout(gpu_slot* slots, uint32_t count);
static gpu_bundle* getBundle(uint32_t layout);
static gpu_texture* getAttachment(uint32_t size[2], uint32_t layers, TextureFormat format, bool srgb, uint32_t samples);
static size_t measureTexture(TextureFormat format, uint16_t w, uint16_t h, uint16_t d);
static void checkTextureBounds(const TextureInfo* info, uint32_t offset[4], uint32_t extent[3]);
static uint32_t findShaderSlot(Shader* shader, const char* name, size_t length);
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

  if (!gpu_init(&config)) {
    lovrThrow("Failed to initialize GPU");
  }

  // Temporary frame memory uses a large 1GB virtual memory allocation, committing pages as needed
  state.allocator.length = 1 << 14;
  state.allocator.memory = os_vm_init(MAX_FRAME_MEMORY);
  os_vm_commit(state.allocator.memory, state.allocator.length);

  map_init(&state.pipelineLookup, 64);
  arr_init(&state.pipelines, realloc);
  arr_init(&state.layouts, realloc);

  gpu_slot builtins[] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER, GPU_STAGE_ALL }, // Cameras
    { 1, GPU_SLOT_UNIFORM_BUFFER, GPU_STAGE_ALL }, // Draw data
    { 2, GPU_SLOT_SAMPLER, GPU_STAGE_ALL } // Default sampler
  };

  state.builtinLayout = getLayout(builtins, COUNTOF(builtins));

  state.defaultBuffer = lovrBufferCreate(&(BufferInfo) {
    .length = 4096,
    .stride = 1,
    .label = "Default Buffer"
  }, NULL);

  state.defaultTexture = lovrTextureCreate(&(TextureInfo) {
    .type = TEXTURE_2D,
    .usage = TEXTURE_SAMPLE | TEXTURE_TRANSFER,
    .format = FORMAT_RGBA8,
    .width = 4,
    .height = 4,
    .depth = 1,
    .mipmaps = 1,
    .samples = 1,
    .srgb = true,
    .label = "Default Texture"
  });

  state.defaultSampler = lovrSamplerCreate(&(SamplerInfo) {
    .min = FILTER_LINEAR,
    .mag = FILTER_LINEAR,
    .mip = FILTER_LINEAR,
    .wrap = { WRAP_REPEAT, WRAP_REPEAT, WRAP_REPEAT }
  });

  gpu_stream* transfers = getTransfers();
  gpu_clear_buffer(transfers, state.defaultBuffer->gpu, 0, 4096);
  gpu_clear_texture(transfers, state.defaultTexture->gpu, (float[4]) { 1.f, 1.f, 1.f, 1.f }, 0, ~0u, 0, ~0u);

  state.vertexFormats[VERTEX_SHAPE].gpu = (gpu_vertex_format) {
    .bufferCount = 2,
    .attributeCount = 3,
    .bufferStrides[1] = sizeof(ShapeVertex),
    .attributes[0] = { 1, 0, offsetof(ShapeVertex, position), GPU_TYPE_F32x3 },
    .attributes[1] = { 1, 1, offsetof(ShapeVertex, normal), GPU_TYPE_F32x3 },
    .attributes[2] = { 1, 2, offsetof(ShapeVertex, uv), GPU_TYPE_F32x2 }
  };

  state.vertexFormats[VERTEX_POINT].gpu = (gpu_vertex_format) {
    .bufferCount = 2,
    .attributeCount = 1,
    .bufferStrides[1] = 12,
    .attributes[0] = { 1, 0, 0, GPU_TYPE_F32x3 }
  };

  for (uint32_t i = 0; i < COUNTOF(state.vertexFormats); i++) {
    for (uint32_t j = 0; j < state.vertexFormats[i].gpu.attributeCount; j++) {
      state.vertexFormats[i].mask |= (1 << state.vertexFormats[i].gpu.attributes[j].location);
    }
    state.vertexFormats[i].hash = hash64(&state.vertexFormats[i].gpu, sizeof(gpu_vertex_format));
  }

  float16Init();
  glslang_initialize_process();
  state.initialized = true;
  return true;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  lovrRelease(state.window, lovrTextureDestroy);
  for (uint32_t i = 0; i < COUNTOF(state.attachments); i++) {
    if (state.attachments[i].texture) {
      gpu_texture_destroy(state.attachments[i].texture);
      free(state.attachments[i].texture);
    }
  }
  lovrRelease(state.defaultBuffer, lovrBufferDestroy);
  lovrRelease(state.defaultTexture, lovrTextureDestroy);
  lovrRelease(state.defaultSampler, lovrSamplerDestroy);
  for (uint32_t i = 0; i < COUNTOF(state.defaultShaders); i++) {
    lovrRelease(state.defaultShaders[i], lovrShaderDestroy);
  }
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
  limits->uniformBuffersPerStage = MIN(state.limits.uniformBuffersPerStage - 2, MAX_RESOURCES_PER_SHADER);
  limits->storageBuffersPerStage = MIN(state.limits.storageBuffersPerStage, MAX_RESOURCES_PER_SHADER);
  limits->sampledTexturesPerStage = MIN(state.limits.sampledTexturesPerStage, MAX_RESOURCES_PER_SHADER);
  limits->storageTexturesPerStage = MIN(state.limits.storageTexturesPerStage, MAX_RESOURCES_PER_SHADER);
  limits->samplersPerStage = MIN(state.limits.samplersPerStage - 1, MAX_RESOURCES_PER_SHADER);
  limits->resourcesPerShader = MAX_RESOURCES_PER_SHADER;
  limits->uniformBufferRange = state.limits.uniformBufferRange;
  limits->storageBufferRange = state.limits.storageBufferRange;
  limits->uniformBufferAlign = state.limits.uniformBufferAlign;
  limits->storageBufferAlign = state.limits.storageBufferAlign;
  limits->vertexAttributes = state.limits.vertexAttributes;
  limits->vertexBufferStride = state.limits.vertexBufferStride;
  limits->vertexShaderOutputs = state.limits.vertexShaderOutputs;
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

void lovrGraphicsSubmit(Pass** passes, uint32_t count) {
  if (!state.active) {
    return;
  }

  if (state.window) {
    state.window->gpu = NULL;
    state.window->renderView = NULL;
  }

  // Allocate a few extra stream handles for any internal passes we sneak in
  gpu_stream** streams = tempAlloc((count + 3) * sizeof(gpu_stream*));

  uint32_t extraPassCount = 0;

  if (state.transfers) {
    streams[extraPassCount++] = state.transfers->stream;
  }

  for (uint32_t i = 0; i < count; i++) {
    for (uint32_t j = 0; j <= passes[i]->pipelineIndex; j++) {
      lovrRelease(passes[i]->pipelines[j].shader, lovrShaderDestroy);
      passes[i]->pipelines[j].shader = NULL;
    }
    streams[extraPassCount + i] = passes[i]->stream;
    if (passes[i]->info.type == PASS_RENDER) {
      gpu_render_end(passes[i]->stream);
    }
  }

  for (uint32_t i = 0; i < extraPassCount + count; i++) {
    gpu_stream_end(streams[i]);
  }

  gpu_submit(streams, extraPassCount + count);

  state.transfers = NULL;
  state.active = false;
}

void lovrGraphicsWait() {
  gpu_wait();
}

// Buffer

static void lovrBufferInitFormat(VertexFormat* format, BufferInfo* info) {
  format->mask = 0;
  format->gpu.bufferCount = 2;
  format->gpu.bufferStrides[1] = info->stride;
  format->gpu.attributeCount = info->fieldCount;
  for (uint32_t i = 0; i < info->fieldCount; i++) {
    format->gpu.attributes[i] = (gpu_attribute) {
      .buffer = 1,
      .location = info->fields[i].location,
      .offset = info->fields[i].offset,
      .type = (gpu_attribute_type) info->fields[i].type
    };
    format->mask |= (1 << i);
  }
  format->hash = hash64(&format->gpu, sizeof(format->gpu));
}

Buffer* lovrGraphicsGetBuffer(BufferInfo* info, void** data) {
  uint32_t size = info->length * info->stride;
  lovrCheck(size > 0, "Buffer size can not be zero");
  lovrCheck(size <= 1 << 30, "Max buffer size is 1GB");

  Buffer* buffer = tempAlloc(sizeof(Buffer) + gpu_sizeof_buffer());
  buffer->ref = 1;
  buffer->size = size;
  buffer->gpu = (gpu_buffer*) (buffer + 1);
  buffer->info = *info;

  buffer->pointer = gpu_map(buffer->gpu, size, state.limits.uniformBufferAlign, GPU_MAP_WRITE);

  lovrBufferInitFormat(&buffer->format, info);

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

  gpu_buffer_init(buffer->gpu, &(gpu_buffer_info) {
    .size = buffer->size,
    .label = info->label,
    .pointer = data
  });

  lovrBufferInitFormat(&buffer->format, info);

  if (data && *data == NULL) {
    gpu_stream* transfers = getTransfers();
    gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
    *data = gpu_map(scratchpad, size, 4, GPU_MAP_WRITE);
    gpu_copy_buffers(transfers, scratchpad, buffer->gpu, 0, 0, size);
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
  if (size == ~0u) {
    size = buffer->size - offset;
  }

  lovrCheck(offset + size <= buffer->size, "Buffer write range [%d,%d] exceeds buffer size", offset, offset + size);

  if (lovrBufferIsTemporary(buffer)) {
    return buffer->pointer + offset;
  }

  gpu_stream* transfers = getTransfers();
  gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
  void* data = gpu_map(scratchpad, size, 4, GPU_MAP_WRITE);
  gpu_copy_buffers(transfers, scratchpad, buffer->gpu, 0, offset, size);
  return data;
}

void lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t size) {
  lovrCheck(size % 4 == 0, "Buffer clear size must be a multiple of 4");
  lovrCheck(offset % 4 == 0, "Buffer clear offset must be a multiple of 4");
  lovrCheck(offset + size <= buffer->size, "Tried to clear past the end of the Buffer");
  if (lovrBufferIsTemporary(buffer)) {
    memset(buffer->pointer + offset, 0, size);
  } else {
    gpu_stream* transfers = getTransfers();
    gpu_clear_buffer(transfers, buffer->gpu, offset, size);
  }
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
        lovrCheck(size == levelSizes[level], "Texture/Image size mismatch!");
        void* pixels = lovrImageGetLayerData(image, level, slice);
        memcpy(data, pixels, size);
        data += size;
      }
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
      ((info->usage & TEXTURE_TRANSFER) ? GPU_TEXTURE_COPY_SRC | GPU_TEXTURE_COPY_DST : 0),
    .srgb = info->srgb,
    .handle = info->handle,
    .label = info->label,
    .upload = {
      .stream = getTransfers(),
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
    lovrRelease(texture->info.parent, lovrTextureDestroy);
    if (texture->renderView && texture->renderView != texture->gpu) gpu_texture_destroy(texture->renderView);
    if (texture->gpu) gpu_texture_destroy(texture->gpu);
  }
  free(texture);
}

const TextureInfo* lovrTextureGetInfo(Texture* texture) {
  return &texture->info;
}

// Sampler

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

  const glslang_resource_t* resource = glslang_default_resource();

  glslang_input_t input = {
    .language = GLSLANG_SOURCE_GLSL,
    .stage = stages[stage],
    .client = GLSLANG_CLIENT_VULKAN,
    .client_version = GLSLANG_TARGET_VULKAN_1_1,
    .target_language = GLSLANG_TARGET_SPV,
    .target_language_version = GLSLANG_TARGET_SPV_1_3,
    .code = source->data,
    .length = source->size,
    .default_version = 460,
    .default_profile = GLSLANG_NO_PROFILE,
    .resource = resource
  };

  glslang_shader_t* shader = glslang_shader_create(&input);

  if (!glslang_shader_preprocess(shader, &input)) {
    lovrLog(LOG_INFO, "Could not preprocess shader: %s", glslang_shader_get_info_log(shader));
    return NULL;
  }

  if (!glslang_shader_parse(shader, &input)) {
    lovrLog(LOG_INFO, "Could not parse shader: %s", glslang_shader_get_info_log(shader));
    return NULL;
  }

  glslang_program_t* program = glslang_program_create();
  glslang_program_add_shader(program, shader);

  if (!glslang_program_link(program, 0)) {
    lovrLog(LOG_INFO, "Could not link shader: %s", glslang_program_get_info_log(program));
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
    spv[i].resources = tempAlloc(spv[i].resourceCount * sizeof(spv_resource));

    result = spv_parse(info->stages[i]->data, info->stages[i]->size, &spv[i]);
    lovrCheck(result == SPV_OK, "Failed to load Shader: %s\n", spv_result_to_string(result));

    checkShaderFeatures(spv[i].features, spv[i].featureCount);
  }

  uint32_t constantStage = spv[0].pushConstantSize > spv[1].pushConstantSize ? 0 : 1;
  uint32_t maxFlags = spv[0].specConstantCount + spv[1].specConstantCount;

  shader->attributeMask = spv[0].inputLocationMask;
  shader->constantSize = MAX(spv[0].pushConstantSize, spv[1].pushConstantSize);
  shader->constants = malloc(spv[constantStage].pushConstantCount * sizeof(ShaderConstant));
  shader->resources = malloc((spv[0].resourceCount + spv[1].resourceCount) * sizeof(ShaderResource));
  gpu_slot* slots = tempAlloc((spv[0].resourceCount + spv[1].resourceCount) * sizeof(gpu_slot));
  shader->flags = malloc(maxFlags * sizeof(gpu_shader_flag));
  shader->flagLookup = malloc(maxFlags * sizeof(uint32_t));
  lovrAssert(shader->constants && shader->resources && shader->flags && shader->flagLookup, "Out of memory");

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

      if (shader->resourceCount > MAX_RESOURCES_PER_SHADER) {
        lovrThrow("Shader resource count exceeds resourcesPerShader limit (%d)", MAX_RESOURCES_PER_SHADER);
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
      shader->flagLookup[index] = (uint32_t) hash64(constant->name, strlen(constant->name));
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
    .stages[1] = { info->stages[1]->data, info->stages[1]->size },
    .pushConstantSize = shader->constantSize,
    .label = info->label
  };

  if (info->type == SHADER_GRAPHICS) {
    gpu.layouts[0] = state.layouts.data[state.builtinLayout].gpu;
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
  shader->attributeMask = parent->attributeMask;
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
  free(shader);
}

const ShaderInfo* lovrShaderGetInfo(Shader* shader) {
  return &shader->info;
}

// Pass

Pass* lovrGraphicsGetPass(PassInfo* info) {
  beginFrame();
  Pass* pass = tempAlloc(sizeof(Pass));
  pass->ref = 1;
  pass->info = *info;
  pass->stream = gpu_stream_begin(info->label);

  if (info->type != PASS_RENDER) {
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
  }

  if (canvas->depth.texture) {
    target.depth.texture = canvas->depth.texture->renderView;
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

  float viewport[4] = { 0.f, 0.f, (float) main->width, (float) main->height };
  float depthRange[2] = { 0.f, 1.f };
  gpu_set_viewport(pass->stream, viewport, depthRange);

  uint32_t scissor[4] = { 0, 0, main->width, main->height };
  gpu_set_scissor(pass->stream, scissor);

  // The default Buffer (filled with zero) is always at slot #0, used for missing vertex attributes
  uint32_t offset = 0;
  gpu_bind_vertex_buffers(pass->stream, &state.defaultBuffer->gpu, &offset, 0, 1);

  // Reset state

  pass->transform = pass->transforms[0];
  mat4_identity(pass->transform);

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
  pass->pipeline->dirty = true;

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
  pass->builtins[2] = (gpu_binding) { 2, GPU_SLOT_SAMPLER, .sampler = state.defaultSampler->gpu };

  pass->vertexBuffer = NULL;
  pass->indexBuffer = NULL;

  return pass;
}

void lovrPassDestroy(void* ref) {
  Pass* pass = ref;
  for (uint32_t i = 0; i <= pass->pipelineIndex; i++) {
    lovrRelease(pass->pipelines[i].shader, lovrShaderDestroy);
    pass->pipelines[i].shader = NULL;
  }
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
      lovrRetain(pass->pipeline->shader);
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
      lovrRelease(pass->pipeline->shader, lovrShaderDestroy);
      pass->pipeline = &pass->pipelines[--pass->pipelineIndex];
      lovrCheck(pass->pipelineIndex < COUNTOF(pass->pipelines), "%s stack underflow (more pops than pushes?)", "Pipeline");
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

void lovrPassSetShader(Pass* pass, Shader* shader) {
  Shader* previous = pass->pipeline->shader;
  if (shader == previous) return;

  // Clear any bindings for resources that share the same slot but have different types
  if (previous) {
    for (uint32_t i = 0, j = 0; i < previous->resourceCount && j < shader->resourceCount;) {
      if (previous->resources[i].binding < shader->resources[j].binding) {
        i++;
      } else if (previous->resources[i].binding > shader->resources[j].binding) {
        j++;
      } else {
        if (previous->resources[i].type != shader->resources[j].type) {
          pass->bindingMask &= ~(1 << shader->resources[j].binding);
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
        pass->bindings[i].buffer = (gpu_buffer_binding) { state.defaultBuffer->gpu, 0, 4096 };
      } else if (shader->textureMask & bit) {
        pass->bindings[i].texture = state.defaultTexture->gpu;
      } else if (shader->samplerMask & bit) {
        pass->bindings[i].sampler = state.defaultSampler->gpu;
      }

      pass->bindingMask |= bit;
    }

    pass->bindingsDirty = true;
  }

  lovrRetain(shader);
  lovrRelease(previous, lovrShaderDestroy);

  pass->pipeline->shader = shader;
  pass->pipeline->info.shader = shader->gpu;
  pass->pipeline->info.flags = shader->flags;
  pass->pipeline->info.flagCount = shader->overrideCount;
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
  slot = name ? findShaderSlot(shader, name, length) : slot;

  lovrCheck(shader->bufferMask & (1 << slot), "Trying to send a Buffer to slot %d, but the active Shader doesn't have a Buffer in that slot");
  lovrCheck(offset < buffer->size, "Buffer offset is past the end of the Buffer");

  uint32_t limit;

  if (shader->storageMask & (1 << slot)) {
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
  pass->bindingMask |= (1 << slot);
  pass->bindingsDirty = true;
}

void lovrPassSendTexture(Pass* pass, const char* name, size_t length, uint32_t slot, Texture* texture) {
  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader, "A Shader must be active to send resources");
  slot = name ? findShaderSlot(shader, name, length) : slot;

  lovrCheck(shader->textureMask & (1 << slot), "Trying to send a Texture to slot %d, but the active Shader doesn't have a Texture in that slot");

  if (shader->storageMask & (1 << slot)) {
    lovrCheck(texture->info.usage & TEXTURE_STORAGE, "Textures must be created with the 'storage' usage to send them to image variables in shaders");
  } else {
    lovrCheck(texture->info.usage & TEXTURE_SAMPLE, "Textures must be created with the 'sample' usage to send them to sampler variables in shaders");
  }

  pass->bindings[slot].texture = texture->gpu;
  pass->bindingMask |= (1 << slot);
  pass->bindingsDirty = true;
}

void lovrPassSendSampler(Pass* pass, const char* name, size_t length, uint32_t slot, Sampler* sampler) {
  Shader* shader = pass->pipeline->shader;
  lovrCheck(shader, "A Shader must be active to send resources");
  slot = name ? findShaderSlot(shader, name, length) : slot;

  lovrCheck(shader->samplerMask & (1 << slot), "Trying to send a Sampler to slot %d, but the active Shader doesn't have a Sampler in that slot");

  pass->bindings[slot].sampler = sampler->gpu;
  pass->bindingMask |= (1 << slot);
  pass->bindingsDirty = true;
}

static void flushPipeline(Pass* pass, Draw* draw, Shader* shader) {
  Pipeline* pipeline = pass->pipeline;

  if (pipeline->info.drawMode != draw->mode) {
    pipeline->info.drawMode = draw->mode;
    pipeline->dirty = true;
  }

  if (!pipeline->info.shader && pipeline->info.shader != shader->gpu) {
    pipeline->info.shader = shader->gpu;
    pipeline->info.flags = NULL;
    pipeline->info.flagCount = 0;
    pipeline->dirty = true;
  }

  VertexFormat* format = draw->vertex.buffer ? &draw->vertex.buffer->format : &state.vertexFormats[draw->vertex.format];

  if (format->hash != pipeline->formatHash) {
    pipeline->info.vertex = format->gpu;
    gpu_vertex_format* vertex = &pipeline->info.vertex;
    uint32_t missingAttributes = shader->attributeMask & ~format->mask;

    if (missingAttributes) {
      vertex->bufferStrides[0] = 0;
      for (uint32_t i = 0; i < 32 && missingAttributes; i++) {
        if (missingAttributes & (1 << i)) { // TODO clz
          missingAttributes &= ~(1 << i);
          vertex->attributes[vertex->attributeCount++] = (gpu_attribute) {
            .buffer = 0,
            .location = i,
            .offset = 0,
            .type = GPU_TYPE_F32x4
          };
        }
      }
    }

    pipeline->formatHash = format->hash;
    pipeline->dirty = true;
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
  gpu_bind_bundle(pass->stream, shader->gpu, 2, bundle, NULL, 0);
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

  if (rebind) {
    gpu_bundle_info bundleInfo = {
      .layout = state.layouts.data[state.builtinLayout].gpu,
      .bindings = pass->builtins,
      .count = COUNTOF(pass->builtins)
    };

    gpu_bundle* bundle = getBundle(state.builtinLayout);
    gpu_bundle_write(&bundle, &bundleInfo, 1);
    gpu_bind_bundle(pass->stream, shader->gpu, 0, bundle, NULL, 0);
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

static void flushBuffers(Pass* pass, Draw* draw) {
  uint32_t vertexOffset = 0;

  if (!draw->vertex.buffer && draw->vertex.count > 0) {
    lovrCheck(draw->vertex.count < UINT16_MAX, "This draw has too many vertices (max is 65534), try splitting it up into multiple draws or using a Buffer");
    uint32_t stride = state.vertexFormats[draw->vertex.format].gpu.bufferStrides[1];
    uint32_t size = draw->vertex.count * stride;

    gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
    void* pointer = gpu_map(scratchpad, size, stride, GPU_MAP_WRITE);

    if (draw->vertex.pointer) {
      *draw->vertex.pointer = pointer;
    } else {
      memcpy(pointer, draw->vertex.data, size);
    }

    gpu_bind_vertex_buffers(pass->stream, &scratchpad, &vertexOffset, 1, 1);
    pass->vertexBuffer = NULL;
  } else if (draw->vertex.buffer && draw->vertex.buffer->gpu != pass->vertexBuffer) {
    lovrCheck(draw->vertex.buffer->info.stride <= state.limits.vertexBufferStride, "Vertex buffer stride exceeds vertexBufferStride limit");
    gpu_bind_vertex_buffers(pass->stream, &draw->vertex.buffer->gpu, &vertexOffset, 1, 1);
    pass->vertexBuffer = draw->vertex.buffer->gpu;
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
  }
}

static void lovrPassDraw(Pass* pass, Draw* draw) {
  lovrCheck(pass->info.type == PASS_RENDER, "This function can only be called on a render pass");
  Shader* shader = pass->pipeline->shader ? pass->pipeline->shader : lovrGraphicsGetDefaultShader(draw->shader);

  flushPipeline(pass, draw, shader);
  flushConstants(pass, shader);
  flushBindings(pass, shader);
  flushBuiltins(pass, draw, shader);
  flushBuffers(pass, draw);

  bool indexed = draw->index.buffer || draw->index.count > 0;
  uint32_t defaultCount = draw->index.count > 0 ? draw->index.count : draw->vertex.count;
  uint32_t count = draw->count > 0 ? draw->count : defaultCount;
  uint32_t instances = MAX(draw->instances, 1);
  uint32_t id = pass->drawCount & 0xff;

  if (draw->indirect) {
    if (indexed) {
      gpu_draw_indirect_indexed(pass->stream, draw->indirect->gpu, draw->offset, count);
    } else {
      gpu_draw_indirect(pass->stream, draw->indirect->gpu, draw->offset, count);
    }
  } else {
    if (indexed) {
      gpu_draw_indexed(pass->stream, count, instances, draw->start, draw->base, id);
    } else {
      gpu_draw(pass->stream, count, instances, draw->start, id);
    }
  }

  pass->drawCount++;
}

void lovrPassPoints(Pass* pass, uint32_t count, float** points) {
  lovrPassDraw(pass, &(Draw) {
    .mode = GPU_DRAW_POINTS,
    .vertex.format = VERTEX_POINT,
    .vertex.pointer = (void**) points,
    .vertex.count = count
  });
}

void lovrPassLine(Pass* pass, uint32_t count, float** points) {
  uint16_t* indices;

  lovrPassDraw(pass, &(Draw) {
    .mode = GPU_DRAW_LINES,
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

void lovrPassPlane(Pass* pass, float* transform, uint32_t cols, uint32_t rows) {
  ShapeVertex* vertices;
  uint16_t* indices;

  uint32_t vertexCount = (cols + 1) * (rows + 1);
  uint32_t indexCount = (cols * rows) * 6;

  lovrPassDraw(pass, &(Draw) {
    .mode = GPU_DRAW_TRIANGLES,
    .transform = transform,
    .vertex.format = VERTEX_SHAPE,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = vertexCount,
    .index.pointer = (void**) &indices,
    .index.count = indexCount
  });

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

  for (uint32_t y = 0; y < rows; y++) {
    for (uint32_t x = 0; x < cols; x++) {
      uint16_t a = (y * (cols + 1)) + x;
      uint16_t b = a + 1;
      uint16_t c = a + cols + 1;
      uint16_t d = a + cols + 2;
      uint16_t cell[] = { a, b, c, c, b, d };
      memcpy(indices, cell, sizeof(cell));
      indices += COUNTOF(cell);
    }
  }
}

void lovrPassBox(Pass* pass, float* transform) {
  ShapeVertex* vertices;
  uint16_t* indices;

  ShapeVertex vertexData[] = {
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

  uint16_t indexData[] = {
    0,  1,   2,  2,  1,  3,
    4,  5,   6,  6,  5,  7,
    8,  9,  10, 10,  9, 11,
    12, 13, 14, 14, 13, 15,
    16, 17, 18, 18, 17, 19,
    20, 21, 22, 22, 21, 23
  };

  lovrPassDraw(pass, &(Draw) {
    .mode = GPU_DRAW_TRIANGLES,
    .transform = transform,
    .vertex.format = VERTEX_SHAPE,
    .vertex.pointer = (void**) &vertices,
    .vertex.count = COUNTOF(vertexData),
    .index.pointer = (void**) &indices,
    .index.count = COUNTOF(indexData)
  });

  memcpy(vertices, vertexData, sizeof(vertexData));
  memcpy(indices, indexData, sizeof(indexData));
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
}

void lovrPassClearTexture(Pass* pass, Texture* texture, float value[4], uint32_t layer, uint32_t layerCount, uint32_t level, uint32_t levelCount) {
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!texture->info.parent, "Texture views can not be cleared");
  lovrCheck(texture->info.usage & TEXTURE_TRANSFER, "Texture must be created with 'transfer' usage to clear it");
  lovrCheck(texture->info.type == TEXTURE_3D || layer + layerCount <= texture->info.depth, "Texture clear range exceeds texture layer count");
  lovrCheck(level + levelCount <= texture->info.mipmaps, "Texture clear range exceeds texture mipmap count");
  gpu_clear_texture(pass->stream, texture->gpu, value, layer, layerCount, level, levelCount);
}

void lovrPassCopyDataToBuffer(Pass* pass, void* data, Buffer* buffer, uint32_t offset, uint32_t extent) {
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!lovrBufferIsTemporary(buffer), "Temporary buffers can not be copied to, use Buffer:setData");
  lovrCheck(offset + extent <= buffer->size, "Buffer copy range goes past the end of the Buffer");
  gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
  void* pointer = gpu_map(scratchpad, extent, 4, GPU_MAP_WRITE);
  gpu_copy_buffers(pass->stream, scratchpad, buffer->gpu, 0, offset, extent);
  memcpy(pointer, data, extent);
}

void lovrPassCopyBufferToBuffer(Pass* pass, Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t extent) {
  lovrCheck(pass->info.type == PASS_TRANSFER, "This function can only be called on a transfer pass");
  lovrCheck(!lovrBufferIsTemporary(dst), "Temporary buffers can not be copied to");
  lovrCheck(srcOffset + extent <= src->size, "Buffer copy range goes past the end of the source Buffer");
  lovrCheck(dstOffset + extent <= dst->size, "Buffer copy range goes past the end of the destination Buffer");
  gpu_copy_buffers(pass->stream, src->gpu, dst->gpu, srcOffset, dstOffset, extent);
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
    gpu_blit(pass->stream, texture->gpu, texture->gpu, srcOffset, dstOffset, srcExtent, dstExtent, GPU_FILTER_LINEAR);
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

static void beginFrame(void) {
  if (state.active) {
    return;
  }

  state.active = true;
  state.tick = gpu_begin();
}

static gpu_stream* getTransfers(void) {
  if (!state.transfers) {
    state.transfers = lovrGraphicsGetPass(&(PassInfo) {
      .type = PASS_TRANSFER,
      .label = "Internal Transfers"
    });
  }

  return state.transfers->stream;
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

static gpu_texture* getAttachment(uint32_t size[2], uint32_t layers, TextureFormat format, bool srgb, uint32_t samples) {
  uint16_t key[] = { size[0], size[1], layers, format, srgb, samples };
  uint32_t hash = hash64(key, sizeof(key));

  Attachment* attachment = state.attachments;
  for (uint32_t i = 0; i < COUNTOF(state.attachments) && attachment->texture; i++, attachment++) {
    if (attachment->hash == hash && attachment->tick != state.tick) {
      attachment->tick = state.tick;
      return attachment->texture;
    }
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
    .upload.stream = getTransfers(),
    .srgb = srgb
  };

  uint32_t oldest = ~0u;
  for (uint32_t i = 0; i < COUNTOF(state.attachments); i++) {
    if (!state.attachments[i].texture) {
      attachment = &state.attachments[i];
      break;
    } else if (state.attachments[i].tick < oldest) {
      attachment = &state.attachments[i];
      oldest = attachment->tick;
    }
  }

  if (!attachment->texture) {
    attachment->texture = calloc(1, gpu_sizeof_texture());
    lovrAssert(attachment->texture, "Out of memory");
  } else {
    gpu_texture_destroy(attachment->texture);
  }

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

static uint32_t findShaderSlot(Shader* shader, const char* name, size_t length) {
  uint32_t hash = (uint32_t) hash64(name, length);
  for (uint32_t i = 0; i < shader->resourceCount; i++) {
    if (shader->resources[i].hash == hash) {
      return shader->resources[i].binding;
    }
  }
  lovrThrow("Shader has no variable named '%s'", name);
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
    lovrLog(LOG_ERROR, "GPU", message);
  } else {
    lovrLog(LOG_DEBUG, "GPU", message);
  }
}
