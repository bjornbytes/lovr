#include "graphics/graphics.h"
#include "data/image.h"
#include "core/gpu.h"
#include "core/maf.h"
#include "core/os.h"
#include "util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

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

struct Pass {
  uint32_t ref;
  PassInfo info;
  gpu_stream* stream;
  float* transform;
  uint32_t transformIndex;
  float transforms[16][16];
};

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
  Allocator allocator;
} state;

// Helpers

static void* tempAlloc(size_t size);
static void beginFrame(void);
static gpu_stream* getTransfers(void);
static size_t measureTexture(TextureFormat format, uint16_t w, uint16_t h, uint16_t d);
static void onMessage(void* context, const char* message, bool severe);

// Entry

bool lovrGraphicsInit(bool debug) {
  if (state.initialized) return false;

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
  gpu_destroy();
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

void lovrGraphicsSubmit(Pass** passes, uint32_t count) {
  if (!state.active) {
    return;
  }

  // Allocate a few extra stream handles for any internal passes we sneak in
  gpu_stream** streams = tempAlloc((count + 3) * sizeof(gpu_stream*));

  uint32_t extraPassCount = 0;

  if (state.transfers) {
    streams[extraPassCount++] = state.transfers->stream;
  }

  for (uint32_t i = 0; i < count; i++) {
    streams[extraPassCount + i] = passes[i]->stream;
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
      ((info->usage & TEXTURE_COPY) ? GPU_TEXTURE_COPY_SRC | GPU_TEXTURE_COPY_DST : 0),
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
  lovrRelease(texture->info.parent, lovrTextureDestroy);
  if (texture->renderView && texture->renderView != texture->gpu) gpu_texture_destroy(texture->renderView);
  if (texture->gpu) gpu_texture_destroy(texture->gpu);
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

// Pass

Pass* lovrGraphicsGetPass(PassInfo* info) {
  beginFrame();
  Pass* pass = tempAlloc(sizeof(Pass));
  pass->ref = 1;
  pass->info = *info;
  pass->stream = gpu_stream_begin(info->label);
  return pass;
}

void lovrPassDestroy(void* ref) {
  //
}

const PassInfo* lovrPassGetInfo(Pass* pass) {
  return &pass->info;
}

void lovrPassPush(Pass* pass, StackType stack) {
  lovrCheck(pass->info.type == PASS_RENDER, "This function can only be called on a render pass");
  if (stack == STACK_TRANSFORM) {
    pass->transform = pass->transforms[++pass->transformIndex];
    lovrCheck(pass->transformIndex < COUNTOF(pass->transforms), "Transform stack overflow (more pushes than pops?)");
    mat4_init(pass->transforms[pass->transformIndex], pass->transforms[pass->transformIndex - 1]);
  }
}

void lovrPassPop(Pass* pass, StackType stack) {
  lovrCheck(pass->info.type == PASS_RENDER, "This function can only be called on a render pass");
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

static void onMessage(void* context, const char* message, bool severe) {
  if (severe) {
    lovrLog(LOG_ERROR, "GPU", message);
  } else {
    lovrLog(LOG_DEBUG, "GPU", message);
  }
}
