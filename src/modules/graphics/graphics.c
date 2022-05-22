#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "math/math.h"
#include "core/gpu.h"
#include "core/maf.h"
#include "core/spv.h"
#include "core/os.h"
#include "util.h"
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

struct Buffer {
  uint32_t ref;
  uint32_t size;
  gpu_buffer* gpu;
  BufferInfo info;
  char* pointer;
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
  gpu_shader* gpu;
  ShaderInfo info;
  uint32_t attributeMask;
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
  float color[4];
  Shader* shader;
  gpu_pipeline_info info;
  bool dirty;
} Pipeline;

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
};

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
  Allocator allocator;
} state;

// Helpers

static void* tempAlloc(size_t size);
static void beginFrame(void);
static gpu_stream* getTransfers(void);
static gpu_texture* getAttachment(uint32_t size[2], uint32_t layers, TextureFormat format, bool srgb, uint32_t samples);
static size_t measureTexture(TextureFormat format, uint16_t w, uint16_t h, uint16_t d);
static void checkShaderFeatures(uint32_t* features, uint32_t count);
static void onMessage(void* context, const char* message, bool severe);

// Entry

bool lovrGraphicsInit(bool debug, bool vsync) {
  if (state.initialized) return false;

  glslang_initialize_process();
  float16Init();

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

  state.initialized = true;
  return true;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  for (uint32_t i = 0; i < COUNTOF(state.attachments); i++) {
    if (state.attachments[i].texture) {
      gpu_texture_destroy(state.attachments[i].texture);
      free(state.attachments[i].texture);
    }
  }
  lovrRelease(state.window, lovrTextureDestroy);
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
    state.window->gpu = state.window->renderView = NULL;
  }

  // Allocate a few extra stream handles for any internal passes we sneak in
  gpu_stream** streams = tempAlloc((count + 3) * sizeof(gpu_stream*));

  uint32_t extraPassCount = 0;

  if (state.transfers) {
    streams[extraPassCount++] = state.transfers->stream;
  }

  for (uint32_t i = 0; i < count; i++) {
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

  if (data && *data == NULL) {
    gpu_buffer* scratchpad = tempAlloc(gpu_sizeof_buffer());
    *data = gpu_map(scratchpad, size, 4, GPU_MAP_WRITE);
    // TODO copy scratchpad to buffer
  }

  return buffer;
}

void lovrBufferDestroy(void* ref) {
  Buffer* buffer = ref;
  if (buffer->pointer) return;
  gpu_buffer_destroy(buffer->gpu);
  free(buffer);
}

const BufferInfo* lovrBufferGetInfo(Buffer* buffer) {
  return &buffer->info;
}

bool lovrBufferIsTemporary(Buffer* buffer) {
  return !!buffer->pointer;
}

void* lovrBufferMap(Buffer* buffer, uint32_t offset, uint32_t size) {
  if (size == ~0u) {
    size = buffer->size - offset;
  }

  lovrCheck(offset + size <= buffer->size, "Buffer write range [%d,%d] exceeds buffer size", offset, offset + size);

  if (buffer->pointer) {
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
  if (buffer->pointer) {
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
  lovrCheck(info->samples == 1 || info->samples == 4, "Currently, Texture multisample count must be 1 or 4");
  lovrCheck(info->samples == 1 || info->type != TEXTURE_CUBE, "Cubemaps can not be multisampled");
  lovrCheck(info->samples == 1 || info->type != TEXTURE_3D, "Volume textures can not be multisampled");
  lovrCheck(info->samples == 1 || ~info->usage & TEXTURE_STORAGE, "Currently, Textures with the 'storage' flag can not be multisampled");
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

      slots[index] = (gpu_slot) {
        .number = resource->binding,
        .type = resourceTypes[resource->type],
        .stage = stage,
        .count = resource->arraySize
      };

      shader->resources[index] = (ShaderResource) {
        .hash = hash,
        .binding = resource->binding,
        .stageMask = stage,
        .type = resourceTypes[resource->type]
      };
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

  gpu_shader_info gpu = {
    .stages[0] = { info->stages[0]->data, info->stages[0]->size },
    .stages[1] = { info->stages[1]->data, info->stages[1]->size },
    .pushConstantSize = shader->constantSize,
    .label = info->label
  };

  gpu_shader_init(shader->gpu, &gpu);
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

  if (info->type == PASS_RENDER) {
    Canvas* canvas = &info->canvas;
    const TextureInfo* main = canvas->textures[0] ? &canvas->textures[0]->info : &canvas->depth.texture->info;
    lovrCheck(canvas->textures[0] || canvas->depth.texture, "Render pass must have at least one color or depth texture");
    lovrCheck(main->width <= state.limits.renderSize[0], "Render pass width (%d) exceeds the renderSize limit of this GPU (%d)", main->width, state.limits.renderSize[0]);
    lovrCheck(main->height <= state.limits.renderSize[1], "Render pass height (%d) exceeds the renderSize limit of this GPU (%d)", main->height, state.limits.renderSize[1]);
    lovrCheck(main->depth <= state.limits.renderSize[2], "Render pass view count (%d) exceeds the renderSize limit of this GPU (%d)", main->depth, state.limits.renderSize[2]);
    lovrCheck(canvas->samples == 1 || canvas->samples == 4, "Currently, render pass sample count must be 1 or 4");

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
        lovrCheck(texture->samples == main->samples, "Currently, depth buffer sample count must match the main render pass sample count");
      }
    }

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
    } else {
      target.depth.texture = getAttachment(target.size, main->depth, canvas->depth.format, false, canvas->samples);
    }

    target.depth.load = target.depth.stencilLoad = (gpu_load_op) canvas->depth.load;
    target.depth.save = canvas->depth.texture ? GPU_SAVE_OP_SAVE : GPU_SAVE_OP_DISCARD;
    target.depth.clear.depth = canvas->depth.clear;

    gpu_render_begin(pass->stream, &target);
  }

  return pass;
}

void lovrPassDestroy(void* ref) {
  //
}

const PassInfo* lovrPassGetInfo(Pass* pass) {
  return &pass->info;
}

void lovrPassPush(Pass* pass, StackType stack) {
  if (stack == STACK_TRANSFORM) {
    pass->transform = pass->transforms[++pass->transformIndex];
    lovrCheck(pass->transformIndex < COUNTOF(pass->transforms), "Transform stack overflow (more pushes than pops?)");
    mat4_init(pass->transforms[pass->transformIndex], pass->transforms[pass->transformIndex - 1]);
  }
}

void lovrPassPop(Pass* pass, StackType stack) {
  if (stack == STACK_TRANSFORM) {
    pass->transform = pass->transforms[--pass->transformIndex];
    lovrCheck(pass->transformIndex < COUNTOF(pass->transforms), "Transform stack underflow (more pops than pushes?)");
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
  lovrRetain(shader);
  lovrRelease(pass->pipeline->shader, lovrShaderDestroy);

  pass->pipeline->shader = shader;
  pass->pipeline->info.shader = shader ? shader->gpu : NULL;
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
