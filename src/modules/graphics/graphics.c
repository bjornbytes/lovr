#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "event/event.h"
#include "core/maf.h"
#include "core/map.h"
#include "core/gpu.h"
#include "core/os.h"
#include "core/util.h"
#include "lib/tinycthread/tinycthread.h"
#include <string.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <math.h>

const char** os_vk_get_instance_extensions(uint32_t* count);
uint32_t os_vk_create_surface(void* instance, void** surface);

struct Buffer {
  uint32_t ref;
  gpu_buffer* gpu;
  BufferInfo info;
  uint32_t size;
};

struct Texture {
  uint32_t ref;
  gpu_texture* gpu;
  TextureInfo info;
  uint32_t baseLayer;
  uint32_t baseLevel;
};

struct Canvas {
  uint32_t ref;
  gpu_pass* gpu;
  gpu_batch* commands;
  gpu_render_target target;
  CanvasInfo info;
  struct {
    gpu_pipeline_info info;
    gpu_pipeline* instance;
    uint64_t hash;
    bool dirty;
  } pipeline;
  Shader* shader;
  uint32_t transform;
  float transforms[64][16];
  float viewMatrix[6][16];
  float projection[6][16];
};

typedef struct {
  uint64_t hash;
  uint16_t slotMask;
  uint16_t bufferMask;
  uint16_t textureMask;
  uint16_t dynamicMask;
  gpu_slot slots[16];
  uint8_t slotCount;
  uint8_t slotIndex[16];
  uint16_t bindingCount;
  uint16_t bindingIndex[16];
  uint16_t dynamicOffsetCount;
  uint16_t dynamicOffsetIndex[16];
} ShaderGroup;

struct Shader {
  uint32_t ref;
  gpu_shader* gpu;
  gpu_pipeline* computePipeline;
  ShaderInfo info;
  ShaderGroup groups[4];
  map_t lookup;
};

struct Bundle {
  uint32_t ref;
  gpu_bundle* gpu;
  Shader* shader;
  ShaderGroup* group;
  gpu_binding* bindings;
  uint32_t* dynamicOffsets;
  uint32_t version;
  bool dirty;
};

static struct {
  bool initialized;
  bool recording;
  bool debug;
  int width;
  int height;
  gpu_features features;
  gpu_limits limits;
  gpu_batch* computer;
  map_t pipelines;
  map_t pipelineLobby;
  mtx_t pipelineLock;
  uint32_t activeCanvasCount;
} state;

static void onDebugMessage(void* context, const char* message, int severe) {
  lovrLog(severe ? LOG_ERROR : LOG_DEBUG, "GPU", message);
}

static void onQuitRequest() {
  lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit = { .exitCode = 0 } });
}

static void onResizeWindow(int width, int height) {
  state.width = width;
  state.height = height;
  lovrEventPush((Event) { .type = EVENT_RESIZE, .data.resize = { width, height } });
}

bool lovrGraphicsInit(bool debug) {
  state.debug = debug;
  return false;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  map_free(&state.pipelines);
  map_free(&state.pipelineLobby);
  mtx_destroy(&state.pipelineLock);
  gpu_thread_detach();
  gpu_destroy();
  memset(&state, 0, sizeof(state));
}

const char** os_vk_get_instance_extensions(uint32_t* count);
uint32_t os_vk_create_surface(void* instance, void** surface);

void lovrGraphicsCreateWindow(os_window_config* window) {
  window->debug = state.debug;
  lovrAssert(!state.initialized, "Window is already created");
  lovrAssert(os_window_open(window), "Could not create window");
  os_window_set_vsync(window->vsync); // Force vsync in case lovr.headset changed it in a previous restart
  os_on_quit(onQuitRequest);
  os_on_resize(onResizeWindow);
  os_window_get_fbsize(&state.width, &state.height);

  gpu_config config = {
    .debug = state.debug,
    .features = &state.features,
    .limits = &state.limits,
    .callback = onDebugMessage,
#ifdef LOVR_VK
    .vk.surface = true,
    .vk.vsync = window->vsync,
    .vk.getExtraInstanceExtensions = os_vk_get_instance_extensions,
    .vk.createSurface = os_vk_create_surface
#endif
  };

  lovrAssert(gpu_init(&config), "Could not initialize GPU");
  gpu_thread_attach();
  map_init(&state.pipelines, 0);
  map_init(&state.pipelineLobby, 0);
  mtx_init(&state.pipelineLock, mtx_plain);

  state.initialized = true;
}

bool lovrGraphicsHasWindow() {
  return os_window_is_open();
}

uint32_t lovrGraphicsGetWidth() {
  return state.width;
}

uint32_t lovrGraphicsGetHeight() {
  return state.height;
}

float lovrGraphicsGetPixelDensity() {
  int width, height, framebufferWidth, framebufferHeight;
  os_window_get_size(&width, &height);
  os_window_get_fbsize(&framebufferWidth, &framebufferHeight);
  if (width == 0 || framebufferWidth == 0) {
    return 0.f;
  } else {
    return (float) framebufferWidth / (float) width;
  }
}

void lovrGraphicsGetFeatures(GraphicsFeatures* features) {
  features->bptc = state.features.bptc;
  features->astc = state.features.astc;
  features->pointSize = state.features.pointSize;
  features->wireframe = state.features.wireframe;
  features->multiview = state.features.multiview;
  features->multiblend = state.features.multiblend;
  features->anisotropy = state.features.anisotropy;
  features->depthClamp = state.features.depthClamp;
  features->depthNudgeClamp = state.features.depthOffsetClamp;
  features->clipDistance = state.features.clipDistance;
  features->cullDistance = state.features.cullDistance;
  features->fullIndexBufferRange = state.features.fullIndexBufferRange;
  features->indirectDrawCount = state.features.indirectDrawCount;
  features->indirectDrawFirstInstance = state.features.indirectDrawFirstInstance;
  features->extraShaderInputs = state.features.extraShaderInputs;
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
  limits->renderWidth = state.limits.renderSize[0];
  limits->renderHeight = state.limits.renderSize[1];
  limits->renderViews = state.limits.renderViews;
  limits->bundleCount = state.limits.bundleCount;
  limits->bundleSlots = state.limits.bundleSlots;
  limits->uniformBufferRange = state.limits.uniformBufferRange;
  limits->storageBufferRange = state.limits.storageBufferRange;
  limits->uniformBufferAlign = state.limits.uniformBufferAlign;
  limits->storageBufferAlign = state.limits.storageBufferAlign;
  limits->vertexAttributes = state.limits.vertexAttributes;
  limits->vertexAttributeOffset = state.limits.vertexAttributeOffset;
  limits->vertexBuffers = state.limits.vertexBuffers;
  limits->vertexBufferStride = state.limits.vertexBufferStride;
  limits->vertexShaderOutputs = state.limits.vertexShaderOutputs;
  memcpy(limits->computeCount, state.limits.computeCount, 3 * sizeof(uint32_t));
  memcpy(limits->computeGroupSize, state.limits.computeGroupSize, 3 * sizeof(uint32_t));
  limits->computeGroupVolume = state.limits.computeGroupVolume;
  limits->computeSharedMemory = state.limits.computeSharedMemory;
  limits->indirectDrawCount = state.limits.indirectDrawCount;
  limits->allocationSize = state.limits.allocationSize;
  limits->pointSize[0] = state.limits.pointSize[0];
  limits->pointSize[1] = state.limits.pointSize[1];
  limits->anisotropy = state.limits.anisotropy;
}

void lovrGraphicsBegin() {
  if (!state.recording) {
    state.recording = true;
    gpu_begin();
  }
}

void lovrGraphicsFlush() {
  lovrAssert(state.activeCanvasCount == 0, "Tried to submit graphics commands while a Canvas is still active");

  if (state.computer) {
    gpu_batch_end(state.computer);
    state.computer = NULL;
  }

  if (state.recording) {
    state.recording = false;
    gpu_flush();
  }

  // TODO flush pipeline lobby
}

// Buffer

Buffer* lovrBufferCreate(BufferInfo* info) {
  lovrAssert(info->length * info->stride > 0, "Buffer size must be greater than zero");

  if (info->flags & BUFFER_WRITE) {
    lovrAssert(~info->flags & BUFFER_COMPUTE, "Buffers can not have both the 'write' and 'compute' flags");
  }

  Buffer* buffer = calloc(1, sizeof(Buffer) + gpu_sizeof_buffer());
  buffer->gpu = (gpu_buffer*) (buffer + 1);
  buffer->info = *info;
  buffer->ref = 1;

  gpu_buffer_info gpuInfo = {
    .size = ALIGN(info->length * info->stride, 4),
    .flags =
      ((info->flags & BUFFER_VERTEX) ? GPU_BUFFER_FLAG_VERTEX : 0) |
      ((info->flags & BUFFER_INDEX) ? GPU_BUFFER_FLAG_INDEX : 0) |
      ((info->flags & BUFFER_UNIFORM) ? GPU_BUFFER_FLAG_UNIFORM : 0) |
      ((info->flags & BUFFER_COMPUTE) ? GPU_BUFFER_FLAG_STORAGE : 0) |
      ((info->flags & BUFFER_PARAMETER) ? GPU_BUFFER_FLAG_INDIRECT : 0) |
      ((info->flags & BUFFER_COPY) ? GPU_BUFFER_FLAG_COPY_SRC : 0) |
      ((info->flags & BUFFER_WRITE) ? GPU_BUFFER_FLAG_MAPPABLE : 0) |
      ((info->flags & BUFFER_RETAIN) ? GPU_BUFFER_FLAG_PRESERVE : 0) |
      ((~info->flags & BUFFER_WRITE) ? GPU_BUFFER_FLAG_COPY_DST : 0),
    .pointer = info->initialContents,
    .label = info->label
  };

  if ((~info->flags & BUFFER_WRITE) && info->initialContents) lovrGraphicsBegin();
  lovrAssert(gpu_buffer_init(buffer->gpu, &gpuInfo), "Could not create Buffer");
  buffer->size = info->length * info->stride;
  return buffer;
}

void lovrBufferDestroy(void* ref) {
  Buffer* buffer = ref;
  gpu_buffer_destroy(buffer->gpu);
}

const BufferInfo* lovrBufferGetInfo(Buffer* buffer) {
  return &buffer->info;
}

void* lovrBufferMap(Buffer* buffer) {
  lovrAssert(buffer->info.flags & BUFFER_WRITE, "A Buffer must have the 'write' flag to write to it");
  return gpu_buffer_map(buffer->gpu);
}

void lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t size) {
  lovrAssert(offset % 4 == 0, "Buffer clear offset must be a multiple of 4");
  lovrAssert(size % 4 == 0, "Buffer clear size must be a multiple of 4");
  lovrAssert(offset + size <= buffer->size, "Tried to clear past the end of the Buffer");
  if (~buffer->info.flags & BUFFER_WRITE) lovrGraphicsBegin();
  gpu_buffer_clear(buffer->gpu, offset, size);
}

void lovrBufferCopy(Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size) {
  lovrAssert(src->info.flags & BUFFER_COPY, "A Buffer can only be copied if it has the 'copy' flag");
  lovrAssert(~dst->info.flags & BUFFER_WRITE, "Buffers with the 'write' flag can not be copied to");
  lovrAssert(srcOffset + size <= src->size, "Tried to read past the end of the source Buffer");
  lovrAssert(dstOffset + size <= dst->size, "Tried to copy past the end of the destination Buffer");
  lovrGraphicsBegin();
  gpu_buffer_copy(src->gpu, dst->gpu, srcOffset, dstOffset, size);
}

void lovrBufferRead(Buffer* buffer, uint32_t offset, uint32_t size, void (*callback)(void* data, uint64_t size, void* userdata), void* userdata) {
  lovrAssert(buffer->info.flags & BUFFER_COPY, "A Buffer can only be read if it has the 'copy' flag");
  lovrAssert(offset + size <= buffer->size, "Tried to read past the end of the Buffer");
  lovrGraphicsBegin();
  gpu_buffer_read(buffer->gpu, offset, size, callback, userdata);
}

// Texture

static bool isDepthFormat(TextureFormat format) {
  return format == FORMAT_D16 || format == FORMAT_D24S8 || format == FORMAT_D32F;
}

static size_t getTextureRegionSize(TextureFormat format, uint16_t w, uint16_t h, uint16_t d) {
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
  uint16_t bounds[3] = {
    MAX(info->size[0] >> offset[3], 1),
    MAX(info->size[1] >> offset[3], 1),
    info->type == TEXTURE_VOLUME ? MAX(info->size[2] >> offset[3], 1) : info->size[2]
  };

  lovrAssert(offset[0] + extent[0] <= bounds[0], "Texture x range [%d,%d] exceeds width (%d)", offset[0], offset[0] + extent[0], bounds[0]);
  lovrAssert(offset[1] + extent[1] <= bounds[1], "Texture y range [%d,%d] exceeds height (%d)", offset[1], offset[1] + extent[1], bounds[1]);
  lovrAssert(offset[2] + extent[2] <= bounds[2], "Texture z range [%d,%d] exceeds depth (%d)", offset[2], offset[2] + extent[2], bounds[2]);
  lovrAssert(offset[3] < info->mipmaps, "Texture mipmap %d exceeds its mipmap count (%d)", offset[3] + 1, info->mipmaps);
}

Texture* lovrTextureCreate(TextureInfo* info) {
  lovrAssert(!info->parent, "Textures can only have parents when created as views");
  lovrAssert(info->size[0] > 0, "Texture width must be greater than zero");
  lovrAssert(info->size[1] > 0, "Texture height must be greater than zero");
  lovrAssert(info->size[2] > 0, "Texture depth must be greater than zero");
  lovrAssert(info->mipmaps > 0, "Texture mipmap count must be greater than zero");
  lovrAssert(info->samples > 0, "Texture sample count must be greater than zero");
  lovrAssert((info->samples & (info->samples - 1)) == 0, "Texture multisample count must be a power of 2");
  lovrAssert(info->samples == 1 || info->mipmaps == 1, "Multisampled textures can only have 1 mipmap");

  if (info->type == TEXTURE_2D) {
    uint32_t maxSize = state.limits.textureSize2D;
    lovrAssert(info->size[2] == 1, "2D textures must have a depth of 1");
    lovrAssert(info->size[0] <= maxSize, "2D texture %s exceeds the textureSize2D limit of this GPU (%d)", "width", maxSize);
    lovrAssert(info->size[1] <= maxSize, "2D texture %s exceeds the textureSize2D limit of this GPU (%d)", "height", maxSize);
  }

  if (info->type == TEXTURE_CUBE) {
    uint32_t maxSize = state.limits.textureSizeCube;
    lovrAssert(info->size[0] == info->size[1], "Cubemaps must have square dimensions");
    lovrAssert(info->size[2] == 6, "Cubemaps must have a depth of 6");
    lovrAssert(info->samples == 1, "Cubemaps can not be multisampled");
    lovrAssert(info->size[0] <= maxSize, "Cubemap size exceeds the textureSizeCube limit of this GPU (%d)", maxSize);
  }

  if (info->type == TEXTURE_VOLUME) {
    uint32_t maxSize = state.limits.textureSize3D;
    lovrAssert(info->samples == 1, "Volume textures can not be multisampled");
    lovrAssert(info->size[0] <= maxSize, "Volume texture %s exceeds the textureSize3D limit of this GPU (%d)", "width", maxSize);
    lovrAssert(info->size[1] <= maxSize, "Volume texture %s exceeds the textureSize3D limit of this GPU (%d)", "height", maxSize);
    lovrAssert(info->size[2] <= maxSize, "Volume texture %s exceeds the textureSize3D limit of this GPU (%d)", "depth", maxSize);
  }

  if (info->type == TEXTURE_ARRAY) {
    uint32_t maxLayers = state.limits.textureLayers;
    lovrAssert(info->size[2] <= maxLayers, "Array texture layer count exceeds the textureLayers limit of this GPU (%d)", maxLayers);
  }

  if (info->flags & TEXTURE_SAMPLE) {
    bool canSample = state.features.formats[info->format] & GPU_FORMAT_FEATURE_SAMPLE;
    lovrAssert(canSample, "Texture has 'sample' flag but this GPU does not support sampling this format");
  }

  if (info->flags & TEXTURE_RENDER) {
    bool canRender = state.features.formats[info->format] & (GPU_FORMAT_FEATURE_RENDER_COLOR | GPU_FORMAT_FEATURE_RENDER_DEPTH);
    lovrAssert(canRender, "Texture has 'render' flag but this GPU does not support rendering to this format");
    uint32_t maxWidth = state.limits.renderSize[0];
    uint32_t maxHeight = state.limits.renderSize[1];
    bool excessive = info->size[0] > maxWidth || info->size[1] > maxHeight;
    lovrAssert(!excessive, "Texture has 'render' flag but it exceeds the renderSize limit of this GPU (%d,%d)", maxWidth, maxHeight);
  }

  if (info->flags & TEXTURE_COMPUTE) {
    bool allowed = state.features.formats[info->format] & GPU_FORMAT_FEATURE_STORAGE;
    lovrAssert(allowed, "Texture has 'compute' flag but this GPU does not support this for this format");
    lovrAssert(info->samples == 1, "Textures with the 'compute' flag can not currently be multisampled");
  }

  if (info->flags & TEXTURE_TRANSIENT) {
    lovrAssert((info->flags & ~TEXTURE_TRANSIENT) == TEXTURE_RENDER, "Textures with the 'transient' flag must have the 'render' flag set (and no other flags)");
  }

  uint32_t mipDepth = info->type == TEXTURE_VOLUME ? info->size[2] : 1;
  uint32_t mipMax = log2(MAX(MAX(info->size[0], info->size[1]), mipDepth)) + 1;

  if (info->mipmaps == ~0u) {
    info->mipmaps = mipMax;
  } else {
    lovrAssert(info->mipmaps <= mipMax, "Texture has more than the max number of mipmap levels for its size (%d)", mipMax);
  }

  Texture* texture = calloc(1, sizeof(Texture) + gpu_sizeof_texture());
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = *info;
  texture->ref = 1;

  gpu_texture_info gpuInfo = {
    .type = (gpu_texture_type) info->type,
    .format = (gpu_texture_format) info->format,
    .size[0] = info->size[0],
    .size[1] = info->size[1],
    .size[2] = info->size[2],
    .mipmaps = info->mipmaps,
    .samples = info->samples,
    .flags =
      ((info->flags & TEXTURE_SAMPLE) ? GPU_TEXTURE_FLAG_SAMPLE : 0) |
      ((info->flags & TEXTURE_RENDER) ? GPU_TEXTURE_FLAG_RENDER : 0) |
      ((info->flags & TEXTURE_COMPUTE) ? GPU_TEXTURE_FLAG_STORAGE : 0) |
      ((info->flags & TEXTURE_COPY) ? GPU_TEXTURE_FLAG_COPY_SRC : 0) |
      ((info->flags & TEXTURE_TRANSIENT) ? GPU_TEXTURE_FLAG_TRANSIENT : 0) |
      ((info->flags & TEXTURE_TRANSIENT) ? 0 : GPU_TEXTURE_FLAG_COPY_DST),
    .srgb = info->srgb,
    .label = info->label
  };

  lovrAssert(gpu_texture_init(texture->gpu, &gpuInfo), "Could not create Texture");
  return texture;
}

Texture* lovrTextureCreateView(TextureViewInfo* view) {
  lovrAssert(view->parent, "Texture view must have a parent texture");
  lovrAssert(view->type != TEXTURE_VOLUME, "Texture views may not be volume textures");

  const TextureInfo* info = &view->parent->info;
  lovrAssert(!info->parent, "Can't create a Texture view from another Texture view");
  lovrAssert(view->layerCount > 0, "Texture view must have at least one layer");
  lovrAssert(view->levelCount > 0, "Texture view must have at least one mipmap");
  uint32_t maxDepth = info->type == TEXTURE_VOLUME ? MAX(info->size[2] >> view->levelIndex, 1) : info->size[2];
  lovrAssert(view->layerIndex + view->layerCount <= maxDepth, "Texture view layer range exceeds depth of parent texture");
  lovrAssert(view->levelIndex + view->levelCount <= info->mipmaps, "Texture view mipmap range exceeds mipmap count of parent texture");

  if (view->type == TEXTURE_2D) {
    lovrAssert(view->layerCount == 1, "2D textures can only have a single layer");
  }

  if (view->type == TEXTURE_CUBE) {
    lovrAssert(view->layerCount == 6, "Cubemaps must have 6 layers");
  }

  if (info->type == TEXTURE_VOLUME) {
    lovrAssert(view->levelCount == 1, "Views created from volume textures may only have a single mipmap level");
  }

  Texture* texture = calloc(1, sizeof(Texture) + gpu_sizeof_texture());
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = *info;
  texture->ref = 1;

  texture->info.parent = view->parent;
  texture->info.mipmaps = view->levelCount;
  texture->info.size[0] = MAX(info->size[0] >> view->levelIndex, 1);
  texture->info.size[1] = MAX(info->size[1] >> view->levelIndex, 1);
  texture->info.size[2] = view->layerCount;
  texture->baseLayer = view->layerIndex;
  texture->baseLevel = view->levelIndex;

  gpu_texture_view_info gpuInfo = {
    .source = view->parent->gpu,
    .type = (gpu_texture_type) view->type,
    .layerIndex = view->layerIndex,
    .layerCount = view->layerCount,
    .levelIndex = view->levelIndex,
    .levelCount = view->levelCount
  };

  lovrAssert(gpu_texture_init_view(texture->gpu, &gpuInfo), "Could not create Texture view");
  lovrRetain(view->parent);
  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  lovrRelease(texture->info.parent, lovrTextureDestroy);
  gpu_texture_destroy(texture->gpu);
}

const TextureInfo* lovrTextureGetInfo(Texture* texture) {
  return &texture->info;
}

void lovrTextureWrite(Texture* texture, uint16_t offset[4], uint16_t extent[3], void* data, uint32_t step[2]) {
  lovrAssert(~texture->info.flags & TEXTURE_TRANSIENT, "Transient Textures can not be written to");
  lovrAssert(texture->info.samples == 1, "Multisampled Textures can not be written to");
  checkTextureBounds(&texture->info, offset, extent);
  lovrGraphicsBegin();

  char* src = data;
  uint16_t realOffset[4] = { offset[0], offset[1], offset[2] + texture->baseLayer, offset[3] + texture->baseLevel };
  char* dst = gpu_texture_map(texture->gpu, realOffset, extent);
  size_t rowSize = getTextureRegionSize(texture->info.format, extent[0], 1, 1);
  size_t imgSize = getTextureRegionSize(texture->info.format, extent[0], extent[1], 1);
  size_t jump = step[0] ? step[0] : rowSize;
  size_t leap = step[1] ? step[1] : imgSize;
  for (uint16_t z = 0; z < extent[2]; z++) {
    for (uint16_t y = 0; y < extent[1]; y++) {
      memcpy(dst, src, rowSize);
      dst += rowSize;
      src += jump;
    }
    dst += imgSize;
    src += leap;
  }
}

void lovrTexturePaste(Texture* texture, Image* image, uint16_t srcOffset[2], uint16_t dstOffset[4], uint16_t extent[2]) {
  lovrAssert(texture->info.format == image->format, "Texture and Image formats must match");
  lovrAssert(srcOffset[0] + extent[0] <= image->width, "Tried to read pixels past the width of the Image");
  lovrAssert(srcOffset[1] + extent[1] <= image->height, "Tried to read pixels past the height of the Image");

  uint16_t fullExtent[3] = { extent[0], extent[1], 1 };
  uint32_t step[2] = { getTextureRegionSize(image->format, image->width, 1, 1), 0 };
  size_t offsetx = getTextureRegionSize(image->format, srcOffset[0], 1, 1);
  size_t offsety = srcOffset[1] * step[0];
  char* data = (char*) image->blob->data + offsety + offsetx;

  lovrTextureWrite(texture, dstOffset, fullExtent, data, step);
}

void lovrTextureClear(Texture* texture, uint16_t layer, uint16_t layerCount, uint16_t level, uint16_t levelCount, float color[4]) {
  lovrAssert(~texture->info.flags & TEXTURE_TRANSIENT, "Transient Textures can not be cleared");
  lovrAssert(!isDepthFormat(texture->info.format), "Currently only color textures can be cleared");
  lovrAssert(texture->info.type == TEXTURE_VOLUME || layer + layerCount <= texture->info.size[2], "Texture clear range exceeds texture layer count");
  lovrAssert(level + levelCount <= texture->info.mipmaps, "Texture clear range exceeds texture mipmap count");
  lovrGraphicsBegin();
  gpu_texture_clear(texture->gpu, layer + texture->baseLayer, layerCount, level + texture->baseLevel, levelCount, color);
}

void lovrTextureRead(Texture* texture, uint16_t offset[4], uint16_t extent[3], void (*callback)(void* data, uint64_t size, void* userdata), void* userdata) {
  lovrAssert(texture->info.flags & TEXTURE_COPY, "Texture must have the 'copy' flag to read from it");
  lovrAssert(~texture->info.flags & TEXTURE_TRANSIENT, "Transient Textures can not be read");
  lovrAssert(texture->info.samples == 1, "Multisampled Textures can not be read");
  checkTextureBounds(&texture->info, offset, extent);

  lovrGraphicsBegin();
  uint16_t realOffset[4] = { offset[0], offset[1], offset[2] + texture->baseLayer, offset[3] + texture->baseLevel };
  gpu_texture_read(texture->gpu, realOffset, extent, callback, userdata);
}

void lovrTextureCopy(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t extent[3]) {
  lovrAssert(~dst->info.flags & TEXTURE_TRANSIENT, "Transient Textures can not be copied to");
  lovrAssert(src->info.flags & TEXTURE_COPY, "Texture must have the 'copy' flag to copy from it");
  lovrAssert(src->info.format == dst->info.format, "Copying between Textures requires them to have the same format");
  lovrAssert(src->info.samples == dst->info.samples, "Textures must have the same sample counts to copy between them");
  checkTextureBounds(&src->info, srcOffset, extent);
  checkTextureBounds(&dst->info, dstOffset, extent);
  uint16_t realSrcOffset[4] = { srcOffset[0], srcOffset[1], srcOffset[2] + src->baseLayer, srcOffset[3] + src->baseLevel };
  uint16_t realDstOffset[4] = { dstOffset[0], dstOffset[1], dstOffset[2] + dst->baseLayer, dstOffset[3] + dst->baseLevel };
  lovrGraphicsBegin();
  gpu_texture_copy(src->gpu, dst->gpu, realSrcOffset, realDstOffset, extent);
}

void lovrTextureBlit(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t srcExtent[3], uint16_t dstExtent[3], bool nearest) {
  lovrAssert(~dst->info.flags & TEXTURE_TRANSIENT, "Transient Textures can not be copied to");
  lovrAssert(src->info.samples == 1 && dst->info.samples == 1, "Multisampled textures can not be used for blits");
  lovrAssert(src->info.flags & TEXTURE_COPY, "Texture must have the 'copy' flag to blit from it");
  lovrAssert(state.features.formats[src->info.format] & GPU_FORMAT_FEATURE_BLIT, "This GPU does not support blits for the source texture's format");
  lovrAssert(state.features.formats[dst->info.format] & GPU_FORMAT_FEATURE_BLIT, "This GPU does not support blits for the destination texture's format");

  if (isDepthFormat(src->info.format) || isDepthFormat(dst->info.format)) {
    lovrAssert(src->info.format == dst->info.format, "Blitting between depth textures requires them to have the same format");
  }

  checkTextureBounds(&src->info, srcOffset, srcExtent);
  checkTextureBounds(&dst->info, dstOffset, dstExtent);
  lovrGraphicsBegin();
  gpu_texture_blit(src->gpu, dst->gpu, srcOffset, dstOffset, srcExtent, dstExtent, nearest);
}

// Canvas

Canvas* lovrCanvasCreate(CanvasInfo* info) {
  Texture* firstTexture = info->color[0].texture ? info->color[0].texture : info->depth.texture;
  lovrAssert(firstTexture, "Canvas must have at least one color or depth texture");
  uint32_t width = firstTexture->info.size[0], height = firstTexture->info.size[1], views = firstTexture->info.size[2];
  lovrAssert(width <= state.limits.renderSize[0], "Canvas width exceeds the renderWidth limit of this GPU (%d)", state.limits.renderSize[0]);
  lovrAssert(height <= state.limits.renderSize[1], "Canvas width exceeds the renderHeight limit of this GPU (%d)", state.limits.renderSize[1]);
  lovrAssert(views == 1 || state.features.multiview, "Canvas has multiple views but multiview is not supported by this GPU");
  lovrAssert(views <= state.limits.renderViews, "Canvas view count (%d) exceeds the renderViews limit of this GPU (%d)", views, state.limits.renderViews);
  uint32_t samples = firstTexture->info.samples > 1 ? firstTexture->info.samples : info->samples;
  lovrAssert((samples & (samples - 1)) == 0, "Canvas multisample count must be a power of 2");

  for (uint32_t i = 0; i < info->colorCount && i < MAX_COLOR_ATTACHMENTS; i++) {
    Texture* texture = info->color[i].texture;
    bool renderable = state.features.formats[texture->info.format] & GPU_FORMAT_FEATURE_RENDER_COLOR;
    lovrAssert(renderable, "This GPU does not support rendering to the texture format used by Canvas color attachment #%d", i + 1);
    lovrAssert(texture->info.flags & TEXTURE_RENDER, "Textures must be created with the 'render' flag to attach them to Canvases");
    lovrAssert(!memcmp(texture->info.size, firstTexture->info.size, 3 * sizeof(uint32_t)), "Canvas texture sizes must match");
    lovrAssert(texture->info.samples == firstTexture->info.samples, "Canvas texture sample counts must match");
  }

  if (info->depth.enabled) {
    Texture* texture = info->depth.texture;
    TextureFormat format = texture ? texture->info.format : info->depth.format;
    bool renderable = state.features.formats[format] & GPU_FORMAT_FEATURE_RENDER_DEPTH;
    lovrAssert(renderable, "This GPU does not support rendering to the Canvas depth buffer's format");
    if (texture) {
      lovrAssert(texture->info.flags & TEXTURE_RENDER, "Textures must be created with the 'render' flag to attach them to a Canvas");
      lovrAssert(!memcmp(texture->info.size, firstTexture->info.size, 3 * sizeof(uint32_t)), "Canvas texture sizes must match");
      lovrAssert(texture->info.samples == samples, "Currently, Canvas depth buffer sample count must match its main multisample count");
    }
  }

  Canvas* canvas = calloc(1, sizeof(Canvas) + gpu_sizeof_pass());
  canvas->gpu = (gpu_pass*) (canvas + 1);
  canvas->info = *info;
  canvas->ref = 1;

  gpu_pass_info gpuInfo = {
    .colorCount = info->colorCount,
    .samples = samples,
    .views = views,
    .resolve = info->samples > 1 && firstTexture->info.samples == 1,
    .label = info->label
  };

  for (uint32_t i = 0; i < info->colorCount; i++) {
    gpuInfo.color[i] = (gpu_pass_color_info) {
      .format = (gpu_texture_format) info->color[i].texture->info.format,
      .load = (gpu_load_op) info->color[i].load,
      .save = (gpu_save_op) info->color[i].save
    };
  }

  if (info->depth.enabled) {
    gpuInfo.depth = (gpu_pass_depth_info) {
      .enabled = true,
      .format = (gpu_texture_format) (info->depth.texture ? info->depth.texture->info.format : info->depth.format),
      .load = (gpu_load_op) info->depth.load,
      .save = (gpu_save_op) info->depth.save,
      .stencilLoad = (gpu_load_op) info->depth.stencilLoad,
      .stencilSave = (gpu_save_op) info->depth.stencilSave
    };
  }

  lovrAssert(gpu_pass_init(canvas->gpu, &gpuInfo), "Could not create Canvas");

  canvas->target.size[0] = width;
  canvas->target.size[1] = height;

  // Temporary targets are created for multisample/depth textures as needed.  (TODO pool/share them)
  TextureInfo targetInfo = {
    .type = views > 1 ? TEXTURE_ARRAY : TEXTURE_2D,
    .size = { width, height, views },
    .mipmaps = 1,
    .samples = info->samples,
    .flags = GPU_TEXTURE_FLAG_RENDER | GPU_TEXTURE_FLAG_TRANSIENT
  };

  // Attachments can only have a single mip level and must be 2D/array textures.
  // For convenience, texture views are created for any incompatible textures.
  TextureViewInfo viewInfo = {
    .type = views > 1 ? TEXTURE_ARRAY : TEXTURE_2D,
    .layerCount = views,
    .levelCount = 1
  };

  for (uint32_t i = 0; i < info->colorCount; i++) {
    Texture* texture = info->color[i].texture;

    if (texture->info.mipmaps > 1 || (texture->info.type != TEXTURE_ARRAY && texture->info.type != TEXTURE_2D)) {
      viewInfo.parent = texture->info.parent ? texture->info.parent : texture;
      viewInfo.layerIndex = texture->baseLayer;
      viewInfo.levelIndex = texture->baseLevel;
      texture = lovrTextureCreateView(&viewInfo);
    } else {
      lovrRetain(texture);
    }

    canvas->target.color[i].texture = texture->gpu;

    if (texture->info.samples == 1 && info->samples > 1) {
      targetInfo.format = texture->info.format;
      canvas->target.color[i].texture = lovrTextureCreate(&targetInfo)->gpu;
      canvas->target.color[i].resolve = texture->gpu;
    }
  }

  if (info->depth.enabled) {
    if (info->depth.texture) {
      Texture* texture = info->depth.texture;
      if (texture->info.mipmaps > 1 || (texture->info.type != TEXTURE_ARRAY && texture->info.type != TEXTURE_2D)) {
        viewInfo.parent = texture->info.parent ? texture->info.parent : texture;
        viewInfo.layerIndex = texture->baseLayer;
        viewInfo.levelIndex = texture->baseLevel;
        texture = lovrTextureCreateView(&viewInfo);
      } else {
        lovrRetain(texture);
      }
      canvas->target.depth.texture = texture->gpu;
    } else {
      targetInfo.format = info->depth.format;
      canvas->target.depth.texture = lovrTextureCreate(&targetInfo)->gpu;
    }
  }

  return canvas;
}

void lovrCanvasDestroy(void* ref) {
  Canvas* canvas = ref;
  gpu_pass_destroy(canvas->gpu);
  for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++) {
    char* texture = canvas->target.color[i].texture ? (char*) canvas->target.color[i].texture - sizeof(Texture) : NULL;
    char* resolve = canvas->target.color[i].resolve ? (char*) canvas->target.color[i].resolve - sizeof(Texture) : NULL;
    lovrRelease(texture, lovrTextureDestroy);
    lovrRelease(resolve, lovrTextureDestroy);
  }
  char* depth = canvas->target.depth.texture ? (char*) canvas->target.depth.texture - sizeof(Texture) : NULL;
  lovrRelease(depth, lovrTextureDestroy);
}

const CanvasInfo* lovrCanvasGetInfo(Canvas* canvas) {
  return &canvas->info;
}

void lovrCanvasBegin(Canvas* canvas) {
  lovrGraphicsBegin();
  state.activeCanvasCount++;
  canvas->transform = 0;
  lovrCanvasOrigin(canvas);
  memset(&canvas->pipeline, 0, sizeof(canvas->pipeline));
  canvas->pipeline.dirty = true;
  canvas->commands = gpu_batch_init_render(canvas->gpu, &canvas->target, NULL, 0);
}

void lovrCanvasFinish(Canvas* canvas) {
  if (state.computer) {
    gpu_batch_end(state.computer);
    state.computer = NULL;
  }

  state.activeCanvasCount--;
  canvas->commands = NULL;
}

bool lovrCanvasIsActive(Canvas* canvas) {
  return canvas->commands;
}

bool lovrCanvasGetAlphaToCoverage(Canvas* canvas) {
  return canvas->pipeline.info.alphaToCoverage;
}

void lovrCanvasSetAlphaToCoverage(Canvas* canvas, bool enabled) {
  canvas->pipeline.info.alphaToCoverage = enabled;
  canvas->pipeline.dirty = true;
}

static const gpu_blend_state blendModes[] = {
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

void lovrCanvasGetBlendMode(Canvas* canvas, uint32_t target, BlendMode* mode, BlendAlphaMode* alphaMode) {
  gpu_blend_state* blend = &canvas->pipeline.info.blend[target];

  if (!blend->enabled) {
    *mode = BLEND_NONE;
    *alphaMode = BLEND_ALPHA_MULTIPLY;
    return;
  }

  for (uint32_t i = 0; i < COUNTOF(blendModes); i++) {
    const gpu_blend_state* a = blend;
    const gpu_blend_state* b = &blendModes[i];
    if (a->color.dst == b->color.dst && a->alpha.src == b->alpha.src && a->alpha.dst == b->alpha.dst && a->color.op == b->color.op && a->alpha.op == b->alpha.op) {
      *mode = i;
      *alphaMode = a->color.src == GPU_BLEND_ONE ? BLEND_PREMULTIPLIED : BLEND_ALPHA_MULTIPLY;
    }
  }

  lovrThrow("Unreachable");
}

void lovrCanvasSetBlendMode(Canvas* canvas, uint32_t target, BlendMode mode, BlendAlphaMode alphaMode) {
  if (mode == BLEND_NONE) {
    memset(&canvas->pipeline.info.blend[target], 0, sizeof(gpu_blend_state));
    return;
  }

  canvas->pipeline.info.blend[target] = blendModes[mode];
  if (alphaMode == BLEND_PREMULTIPLIED && mode != BLEND_MULTIPLY) {
    canvas->pipeline.info.blend[target].color.src = GPU_BLEND_ONE;
  }
  canvas->pipeline.info.blend[target].enabled = true;
  canvas->pipeline.dirty = true;
}

void lovrCanvasGetClear(Canvas* canvas, float color[MAX_COLOR_ATTACHMENTS][4], float* depth, uint8_t* stencil) {
  for (uint32_t i = 0; i < canvas->info.colorCount; i++) {
    memcpy(color[i], canvas->target.color[i].clear, 4 * sizeof(float));
  }
  *depth = canvas->target.depth.clear;
  *stencil = canvas->target.depth.stencilClear;
}

void lovrCanvasSetClear(Canvas* canvas, float color[MAX_COLOR_ATTACHMENTS][4], float depth, uint8_t stencil) {
  for (uint32_t i = 0; i < canvas->info.colorCount; i++) {
    memcpy(canvas->target.color[i].clear, color[i], 4 * sizeof(float));
  }
  canvas->target.depth.clear = depth;
  canvas->target.depth.stencilClear = stencil;
}

void lovrCanvasGetColorMask(Canvas* canvas, uint32_t target, bool* r, bool* g, bool* b, bool* a) {
  uint8_t mask = canvas->pipeline.info.colorMask[target];
  *r = mask & 0x1;
  *g = mask & 0x2;
  *b = mask & 0x4;
  *a = mask & 0x8;
}

void lovrCanvasSetColorMask(Canvas* canvas, uint32_t target, bool r, bool g, bool b, bool a) {
  canvas->pipeline.info.colorMask[target] = (r << 0) | (g << 1) | (b << 2) | (a << 3);
  canvas->pipeline.dirty = true;
}

CullMode lovrCanvasGetCullMode(Canvas* canvas) {
  return (CullMode) canvas->pipeline.info.rasterizer.cullMode;
}

void lovrCanvasSetCullMode(Canvas* canvas, CullMode mode) {
  canvas->pipeline.info.rasterizer.cullMode = (gpu_cull_mode) mode;
  canvas->pipeline.dirty = true;
}

void lovrCanvasGetDepthTest(Canvas* canvas, CompareMode* test, bool* write) {
  *test = (CompareMode) canvas->pipeline.info.depth.test;
  *write = canvas->pipeline.info.depth.write;
}

void lovrCanvasSetDepthTest(Canvas* canvas, CompareMode test, bool write) {
  canvas->pipeline.info.depth.test = (gpu_compare_mode) test;
  canvas->pipeline.info.depth.write = write;
  canvas->pipeline.dirty = true;
}

void lovrCanvasGetDepthNudge(Canvas* canvas, float* nudge, float* sloped, float* clamp) {
  *nudge = canvas->pipeline.info.rasterizer.depthOffset;
  *sloped = canvas->pipeline.info.rasterizer.depthOffsetSloped;
  *clamp = canvas->pipeline.info.rasterizer.depthOffsetClamp;
}

void lovrCanvasSetDepthNudge(Canvas* canvas, float nudge, float sloped, float clamp) {
  canvas->pipeline.info.rasterizer.depthOffset = nudge;
  canvas->pipeline.info.rasterizer.depthOffsetSloped = sloped;
  canvas->pipeline.info.rasterizer.depthOffsetClamp = clamp;
  canvas->pipeline.dirty = true;
}

bool lovrCanvasGetDepthClamp(Canvas* canvas) {
  return canvas->pipeline.info.rasterizer.depthClamp;
}

void lovrCanvasSetDepthClamp(Canvas* canvas, bool clamp) {
  canvas->pipeline.info.rasterizer.depthClamp = clamp;
  canvas->pipeline.dirty = true;
}

void lovrCanvasGetStencilTest(Canvas* canvas, CompareMode* test, uint8_t* value) {
  switch (canvas->pipeline.info.stencil.test) {
    case GPU_COMPARE_NONE: default: *test = COMPARE_NONE; break;
    case GPU_COMPARE_EQUAL: *test = COMPARE_EQUAL; break;
    case GPU_COMPARE_NEQUAL: *test = COMPARE_NEQUAL; break;
    case GPU_COMPARE_LESS: *test = COMPARE_GREATER; break;
    case GPU_COMPARE_LEQUAL: *test = COMPARE_GEQUAL; break;
    case GPU_COMPARE_GREATER: *test = COMPARE_LESS; break;
    case GPU_COMPARE_GEQUAL: *test = COMPARE_LEQUAL; break;
  }
  *value = canvas->pipeline.info.stencil.value;
}

void lovrCanvasSetStencilTest(Canvas* canvas, CompareMode test, uint8_t value) {
  switch (test) {
    case COMPARE_NONE: default: canvas->pipeline.info.stencil.test = GPU_COMPARE_NONE; break;
    case COMPARE_EQUAL: canvas->pipeline.info.stencil.test = GPU_COMPARE_EQUAL; break;
    case COMPARE_NEQUAL: canvas->pipeline.info.stencil.test = GPU_COMPARE_NEQUAL; break;
    case COMPARE_LESS: canvas->pipeline.info.stencil.test = GPU_COMPARE_GREATER; break;
    case COMPARE_LEQUAL: canvas->pipeline.info.stencil.test = GPU_COMPARE_GEQUAL; break;
    case COMPARE_GREATER: canvas->pipeline.info.stencil.test = GPU_COMPARE_LESS; break;
    case COMPARE_GEQUAL: canvas->pipeline.info.stencil.test = GPU_COMPARE_LEQUAL; break;
  }
  canvas->pipeline.info.stencil.value = value;
  canvas->pipeline.dirty = true;
}

Shader* lovrCanvasGetShader(Canvas* canvas) {
  return canvas->shader;
}

void lovrCanvasSetShader(Canvas* canvas, Shader* shader) {
  lovrAssert(!shader || shader->info.type == SHADER_GRAPHICS, "Compute shaders can not be used with setShader");
  if (shader == canvas->shader) return;
  lovrRetain(shader);
  lovrRelease(canvas->shader, lovrShaderDestroy);
  canvas->shader = shader;
  canvas->pipeline.info.shader = shader->gpu;
  canvas->pipeline.dirty = true;
}

Winding lovrCanvasGetWinding(Canvas* canvas) {
  return (Winding) canvas->pipeline.info.rasterizer.winding;
}

void lovrCanvasSetWinding(Canvas* canvas, Winding winding) {
  canvas->pipeline.info.rasterizer.winding = (gpu_winding) winding;
  canvas->pipeline.dirty = true;
}

bool lovrCanvasIsWireframe(Canvas* canvas) {
  return canvas->pipeline.info.rasterizer.wireframe;
}

void lovrCanvasSetWireframe(Canvas* canvas, bool wireframe) {
  canvas->pipeline.info.rasterizer.wireframe = wireframe;
  canvas->pipeline.dirty = true;
}

void lovrCanvasPush(Canvas* canvas) {
  lovrAssert(++canvas->transform < COUNTOF(canvas->transforms), "Unbalanced matrix stack (more pushes than pops?)");
  mat4_init(canvas->transforms[canvas->transform], canvas->transforms[canvas->transform - 1]);
}

void lovrCanvasPop(Canvas* canvas) {
  lovrAssert(--canvas->transform < COUNTOF(canvas->transforms), "Unbalanced matrix stack (more pops than pushes?)");
}

void lovrCanvasOrigin(Canvas* canvas) {
  mat4_identity(canvas->transforms[canvas->transform]);
}

void lovrCanvasTranslate(Canvas* canvas, vec3 translation) {
  mat4_translate(canvas->transforms[canvas->transform], translation[0], translation[1], translation[2]);
}

void lovrCanvasRotate(Canvas* canvas, quat rotation) {
  mat4_rotateQuat(canvas->transforms[canvas->transform], rotation);
}

void lovrCanvasScale(Canvas* canvas, vec3 scale) {
  mat4_scale(canvas->transforms[canvas->transform], scale[0], scale[1], scale[2]);
}

void lovrCanvasTransform(Canvas* canvas, mat4 transform) {
  mat4_mul(canvas->transforms[canvas->transform], transform);
}

void lovrCanvasGetViewMatrix(Canvas* canvas, uint32_t index, float* viewMatrix) {
  lovrAssert(index < COUNTOF(canvas->viewMatrix), "Invalid view index %d", index);
  mat4_init(viewMatrix, canvas->viewMatrix[index]);
}

void lovrCanvasSetViewMatrix(Canvas* canvas, uint32_t index, float* viewMatrix) {
  lovrAssert(index < COUNTOF(canvas->viewMatrix), "Invalid view index %d", index);
  mat4_init(canvas->viewMatrix[index], viewMatrix);
}

void lovrCanvasGetProjection(Canvas* canvas, uint32_t index, float* projection) {
  lovrAssert(index < COUNTOF(canvas->projection), "Invalid view index %d", index);
  mat4_init(projection, canvas->projection[index]);
}

void lovrCanvasSetProjection(Canvas* canvas, uint32_t index, float* projection) {
  lovrAssert(index < COUNTOF(canvas->projection), "Invalid view index %d", index);
  mat4_init(canvas->projection[index], projection);
}

void lovrCanvasStencil(Canvas* canvas, StencilAction action, StencilAction depthFailAction, uint8_t value, StencilCallback* callback, void* userdata) {
  uint8_t oldValue = canvas->pipeline.info.stencil.value;
  gpu_compare_mode oldTest = canvas->pipeline.info.stencil.test;
  // TODO must be in render pass
  // TODO must have stencil buffer?
  canvas->pipeline.info.stencil.test = GPU_COMPARE_NONE;
  canvas->pipeline.info.stencil.passOp = (gpu_stencil_op) action;
  canvas->pipeline.info.stencil.depthFailOp = (gpu_stencil_op) depthFailAction;
  canvas->pipeline.info.stencil.value = value;
  canvas->pipeline.dirty = true;
  callback(userdata);
  canvas->pipeline.info.stencil.test = oldTest;
  canvas->pipeline.info.stencil.passOp = GPU_STENCIL_KEEP;
  canvas->pipeline.info.stencil.depthFailOp = GPU_STENCIL_KEEP;
  canvas->pipeline.info.stencil.value = oldValue;
  canvas->pipeline.dirty = true;
}

// Shader

#define MIN_SPIRV_WORDS 8

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
    case 10: lovrAssert(state.features.float64, "GPU does not support shader feature #%d: %s", capability, "64 bit floats");
    case 11: lovrAssert(state.features.int64, "GPU does not support shader feature #%d: %s", capability, "64 bit integers");
    case 12: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "64 bit atomics");
    case 22: lovrAssert(state.features.int16, "GPU does not support shader feature #%d: %s", capability, "16 bit integers");
    case 23: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "tessellation shading");
    case 24: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "geometry shading");
    case 25: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "extended image gather");
    case 27: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "multisample storage textures");
    case 28: lovrAssert(state.features.dynamicIndexing, "GPU does not support shader feature #%d: %s", capability, "dynamic indexing");
    case 29: lovrAssert(state.features.dynamicIndexing, "GPU does not support shader feature #%d: %s", capability, "dynamic indexing");
    case 30: lovrAssert(state.features.dynamicIndexing, "GPU does not support shader feature #%d: %s", capability, "dynamic indexing");
    case 31: lovrAssert(state.features.dynamicIndexing, "GPU does not support shader feature #%d: %s", capability, "dynamic indexing");
    case 32: lovrAssert(state.features.clipDistance, "GPU does not support shader feature #%d: %s", capability, "clip distance");
    case 33: lovrAssert(state.features.cullDistance, "GPU does not support shader feature #%d: %s", capability, "cull distance");
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
    case 4427: lovrAssert(state.features.extraShaderInputs, "GPU does not support shader feature #%d: %s", capability, "extra shader inputs");
    case 4437: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "multigpu");
    case 4439: lovrAssert(state.features.multiview, "GPU does not support shader feature #%d: %s", capability, "multiview");
    case 5301: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "non uniform indexing");
    case 5306: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "non-uniform indexing");
    case 5307: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "non-uniform indexing");
    case 5308: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "non-uniform indexing");
    case 5309: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "non-uniform indexing");
    default: lovrThrow("Shader uses unknown feature #%d", capability);
  }
  return false;
}

// Parse a slot type and array size from a variable instruction, throwing on failure
static void parseTypeInfo(const uint32_t* words, uint32_t wordCount, uint32_t* cache, uint32_t bound, const uint32_t* instruction, gpu_slot_type* slotType, uint32_t* count) {
  const uint32_t* edge = words + wordCount - MIN_SPIRV_WORDS;
  uint32_t type = instruction[1];
  uint32_t id = instruction[2];
  uint32_t storageClass = instruction[3];

  // Follow the variable's type id to the OpTypePointer instruction that declares its pointer type
  // Then unwrap the pointer to get to the inner type of the variable
  instruction = words + cache[type];
  lovrAssert(instruction < edge && instruction[3] < bound, "Invalid Shader code: id overflow");
  instruction = words + cache[instruction[3]];
  lovrAssert(instruction < edge, "Invalid Shader code: id overflow");

  if ((instruction[0] & 0xffff) == 28) { // OpTypeArray
    // Read array size
    lovrAssert(instruction[3] < bound && words + cache[instruction[3]] < edge, "Invalid Shader code: id overflow");
    const uint32_t* size = words + cache[instruction[3]];
    if ((size[0] & 0xffff) == 43 || (size[0] & 0xffff) == 50) { // OpConstant || OpSpecConstant
      lovrAssert(size[3] <= 0xffff, "Unsupported Shader code: variable %d has array size of %d, but the max array size is 65535", id, size[3]);
      *count = size[3];
    } else {
      lovrThrow("Invalid Shader code: variable %d is an array, but the array size is not a constant", id);
    }

    // Unwrap array to get to inner array type and keep going
    lovrAssert(instruction[2] < bound && words + cache[instruction[2]] < edge, "Invalid Shader code: id overflow");
    instruction = words + cache[instruction[2]];
  } else {
    *count = 1;
  }

  // Use StorageClass to detect uniform/storage buffers
  switch (storageClass) {
    case 12: *slotType = GPU_SLOT_STORAGE_BUFFER; return;
    case 2: *slotType = GPU_SLOT_UNIFORM_BUFFER; return;
    default: break; // It's not a Buffer, keep going to see if it's a valid Texture
  }

  // If it's a sampled image, unwrap to get to the image type.  If it's not an image, fail
  if ((instruction[0] & 0xffff) == 27) { // OpTypeSampledImage
    instruction = words + cache[instruction[2]];
    lovrAssert(instruction < edge, "Invalid Shader code: id overflow");
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
    default: lovrThrow("Unsupported Shader code: texture variable %d isn't a sampled texture or a storage texture", id);
  }
}

static bool parseSpirv(Shader* shader, Blob* source, uint8_t stage) {
  const uint32_t* words = source->data;
  uint32_t wordCount = source->size / sizeof(uint32_t);

  if (wordCount < MIN_SPIRV_WORDS || words[0] != 0x07230203) {
    return false;
  }

  uint32_t bound = words[3];
  lovrAssert(bound <= 0xffff, "Unsupported Shader code: id bound is too big (max is 65535)");

  // The cache stores information for spirv ids, allowing everything to be parsed in a single pass
  // - For variables, stores the slot's group, index, and name offset
  // - For types, stores the index of its declaration instruction
  struct var { uint8_t group; uint8_t binding; uint16_t name; };
  size_t cacheSize = bound * sizeof(uint32_t);
  void* cache = malloc(cacheSize);
  lovrAssert(cache, "Out of memory");
  memset(cache, 0xff, cacheSize);
  struct var* vars = cache;
  uint32_t* types = cache;

  const uint32_t* instruction = words + 5;

  while (instruction < words + wordCount) {
    uint16_t opcode = instruction[0] & 0xffff;
    uint16_t length = instruction[0] >> 16;
    uint32_t id;

    lovrAssert(length > 0, "Invalid Shader code: zero-length instruction");
    lovrAssert(instruction + length <= words + wordCount, "Invalid Shader code: instruction overflow");

    switch (opcode) {
      case 17: // OpCapability
        if (length != 2) break;
        checkShaderCapability(instruction[1]);
        break;
      case 5: // OpName
        if (length < 3 || instruction[1] >= bound) break;
        id = instruction[1];
        vars[id].name = instruction - words + 2;
        break;
      case 71: // OpDecorate
        if (length < 4 || instruction[1] >= bound) break;

        id = instruction[1];
        uint32_t decoration = instruction[2];
        uint32_t value = instruction[3];

        if (decoration == 33) { // Binding
          lovrAssert(value < 16, "Unsupported Shader code: variable %d uses binding %d, but the binding must be less than 16", id, value);
          vars[id].binding = value << 16;
        } else if (decoration == 34) { // Group
          lovrAssert(value < 4, "Unsupported Shader code: variable %d is in group %d, but group must be less than 4", id, value);
          vars[id].group = value << 24;
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
        types[instruction[1]] = instruction - words;
        break;
      case 59: // OpVariable
        if (length < 4 || instruction[2] >= bound) break;
        id = instruction[2];

        // Ignore variables that aren't decorated with a group and binding (e.g. global variables)
        if (vars[id].group == 0xff || vars[id].binding == 0xff) {
          break;
        }

        uint32_t count;
        gpu_slot_type type;
        parseTypeInfo(words, wordCount, types, bound, instruction, &type, &count);

        // If the variable has a name, add it to the shader's lookup
        // If the lookup already has one with the same name, it must be at the same binding location
        if (vars[id].name != 0xffff) {
          char* name = (char*) (words + vars[id].name);
          size_t len = strnlen(name, 4 * (wordCount - vars[id].name));
          uint64_t hash = hash64(name, len);
          uint64_t old = map_get(&shader->lookup, hash);
          uint64_t new = ((uint64_t) vars[id].binding << 32) | vars[id].group;
          if (old == MAP_NIL) {
            map_set(&shader->lookup, hash, new);
          } else {
            lovrAssert(old == new, "Unsupported Shader code: variable named '%s' is bound to 2 different locations", name);
          }
        }

        uint32_t groupId = vars[id].group;
        uint32_t slotId = vars[id].binding;
        ShaderGroup* group = &shader->groups[groupId];

        // Add a new slot or merge our info into a slot added by a different stage
        if (group->slotMask & (1 << slotId)) {
          gpu_slot* other = &group->slots[group->slotIndex[slotId]];
          lovrAssert(other->type == type, "Unsupported Shader code: variable (%d,%d) is in multiple shader stages with different types", groupId, slotId);
          lovrAssert(other->count == count, "Unsupported Shader code: variable (%d,%d) is in multiple shader stages with different array sizes", groupId, slotId);
          other->stage |= stage;
        } else {
          lovrAssert(count < 256, "Unsupported Shader code: variable (%d,%d) has array size of %d (max is 255)", groupId, slotId, count);
          bool buffer = type == GPU_SLOT_UNIFORM_BUFFER || type == GPU_SLOT_STORAGE_BUFFER;
          group->slotMask |= 1 << slotId;
          group->bufferMask |= buffer ? (1 << slotId) : 0;
          group->textureMask |= !buffer ? (1 << slotId) : 0;
          group->slots[group->slotCount++] = (gpu_slot) { slotId, type, stage, count };
          group->slotIndex[slotId] = group->slotCount;
          group->bindingIndex[slotId] = group->bindingCount;
          group->bindingCount += count;
          group->slotCount++;
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

  free(cache);
  return true;
}

Shader* lovrShaderCreate(ShaderInfo* info) {
  Shader* shader = calloc(1, sizeof(Shader) + gpu_sizeof_shader() + gpu_sizeof_pipeline());
  shader->gpu = (gpu_shader*) (shader + 1);
  shader->info = *info;
  shader->ref = 1;
  map_init(&shader->lookup, 64);
  lovrRetain(info->vertex);
  lovrRetain(info->fragment);
  lovrRetain(info->compute);

  gpu_shader_info gpuInfo = {
    .compute = { info->compute->data, info->compute->size, NULL },
    .vertex = { info->vertex->data, info->vertex->size, NULL },
    .fragment = { info->fragment->data, info->fragment->size, NULL },
    .label = info->label
  };

  // Perform reflection on the shader code to get slot info
  // This temporarily populates the slotIndex so variables can be merged between stages
  // After reflection and handling dynamic buffers, slots are sorted and lookup tables are generated
  if (info->type == SHADER_COMPUTE) {
    parseSpirv(shader, info->compute, GPU_STAGE_COMPUTE);
  } else {
    parseSpirv(shader, info->vertex, GPU_STAGE_VERTEX);
    parseSpirv(shader, info->fragment, GPU_STAGE_FRAGMENT);
  }

  for (uint32_t i = 0; i < info->dynamicBufferCount; i++) {
    uint32_t group, index;
    const char* name = info->dynamicBuffers[i];
    bool exists = lovrShaderResolveName(shader, hash64(name, strlen(name)), &group, &index);
    lovrAssert(exists, "Dynamic buffer '%s' does not exist in the shader", name);
    gpu_slot* slot = &shader->groups[group].slots[shader->groups[group].slotIndex[index]];
    switch (slot->type) {
      case GPU_SLOT_UNIFORM_BUFFER: slot->type = GPU_SLOT_UNIFORM_BUFFER_DYNAMIC; break;
      case GPU_SLOT_STORAGE_BUFFER: slot->type = GPU_SLOT_STORAGE_BUFFER_DYNAMIC; break;
      default: lovrThrow("Dynamic buffer '%s' is not a buffer", name);
    }
  }

  for (uint32_t i = 0; i < COUNTOF(shader->groups); i++) {
    ShaderGroup* group = &shader->groups[i];

    if (group->slotCount == 0) continue;

    for (uint32_t j = 0; j < group->slotCount; j++) {
      gpu_slot* slot = &group->slots[j];
      if (slot->type == GPU_SLOT_UNIFORM_BUFFER_DYNAMIC || slot->type == GPU_SLOT_STORAGE_BUFFER_DYNAMIC) {
        group->dynamicMask |= 1 << j;
        group->dynamicOffsetIndex[slot->id] = group->dynamicOffsetCount;
        group->dynamicOffsetCount += slot->count;
      }
    }

    group->hash = hash64(group->slots, group->slotCount * sizeof(gpu_slot));
    gpuInfo.slotCount[i] = group->slotCount;
    gpuInfo.slots[i] = group->slots;
  }

  lovrAssert(gpu_shader_init(shader->gpu, &gpuInfo), "Could not create Shader");

  if (info->type == SHADER_COMPUTE) {
    shader->computePipeline = (gpu_pipeline*) ((char*) shader + gpu_sizeof_shader());
    lovrAssert(gpu_pipeline_init_compute(shader->computePipeline, shader->gpu, NULL), "Could not create Shader compute pipeline");
  }

  return shader;
}

void lovrShaderDestroy(void* ref) {
  Shader* shader = ref;
  gpu_shader_destroy(shader->gpu);
  if (shader->computePipeline) gpu_pipeline_destroy(shader->computePipeline);
  lovrRelease(shader->info.vertex, lovrBlobDestroy);
  lovrRelease(shader->info.fragment, lovrBlobDestroy);
  lovrRelease(shader->info.compute, lovrBlobDestroy);
}

const ShaderInfo* lovrShaderGetInfo(Shader* shader) {
  return &shader->info;
}

bool lovrShaderResolveName(Shader* shader, uint64_t hash, uint32_t* group, uint32_t* id) {
  uint64_t value = map_get(&shader->lookup, hash);
  if (value == MAP_NIL) return false;
  *group = value & ~0u;
  *id = value >> 32;
  return true;
}

void lovrShaderCompute(Shader* shader, uint32_t x, uint32_t y, uint32_t z) {
  lovrAssert(shader->info.type == SHADER_COMPUTE, "Shader must be a compute shader to compute with it");

  if (!state.computer) {
    state.computer = gpu_batch_init_compute();
  }

  gpu_batch_bind_pipeline(state.computer, shader->computePipeline);
  gpu_batch_compute(state.computer, shader->gpu, x, y, z);
}

void lovrShaderComputeIndirect(Shader* shader, Buffer* buffer, uint32_t offset) {
  lovrAssert(shader->info.type == SHADER_COMPUTE, "Shader must be a compute shader to compute with it");
  lovrAssert(buffer->info.flags & BUFFER_PARAMETER, "Buffer must be created with the 'parameter' flag to be used for compute parameters");

  if (!state.computer) {
    state.computer = gpu_batch_init_compute();
  }

  gpu_batch_bind_pipeline(state.computer, shader->computePipeline);
  gpu_batch_compute_indirect(state.computer, shader->gpu, buffer->gpu, offset);
}

// Bundle

Bundle* lovrBundleCreate(Shader* shader, uint32_t group) {
  Bundle* bundle = calloc(1, sizeof(Bundle) + gpu_sizeof_bundle());
  bundle->gpu = (gpu_bundle*) (bundle + 1);
  bundle->ref = 1;
  lovrRetain(shader);
  bundle->shader = shader;
  bundle->group = &shader->groups[group];
  bundle->bindings = calloc(bundle->group->bindingCount, sizeof(gpu_binding));
  bundle->dynamicOffsets = calloc(bundle->group->dynamicOffsetCount, sizeof(uint32_t));
  return bundle;
}

void lovrBundleDestroy(void* ref) {
  Bundle* bundle = ref;
  gpu_bundle_destroy(bundle->gpu);
  lovrRelease(bundle->shader, lovrShaderDestroy);
  free(bundle->bindings);
  free(bundle->dynamicOffsets);
}

Shader* lovrBundleGetShader(Bundle* bundle) {
  return bundle->shader;
}

uint32_t lovrBundleGetGroup(Bundle* bundle) {
  return bundle->group - bundle->shader->groups;
}

bool lovrBundleBindBuffer(Bundle* bundle, uint32_t id, uint32_t element, Buffer* buffer, uint32_t offset, uint32_t extent) {
  ShaderGroup* group = bundle->group;
  bool isSlot = group->slotMask & (1 << id);
  bool isBuffer = group->bufferMask & (1 << id);
  bool isDynamic = group->dynamicMask & (1 << id);
  gpu_slot* slot = &group->slots[group->slotIndex[id]];
  gpu_buffer_binding* binding = &bundle->bindings[group->bindingIndex[id]].buffer + element;

  if (!isSlot || !isBuffer || element >= slot->count) return false;

  if (isDynamic) {
    bundle->dynamicOffsets[group->dynamicOffsetIndex[id]] = offset;
    if (binding->object == buffer->gpu && extent == binding->extent) {
      bundle->version++;
      return true;
    } else {
      offset = 0;
    }
  }

  binding->object = buffer->gpu;
  binding->offset = offset;
  binding->extent = extent;
  bundle->dirty = true;
  lovrRetain(buffer);
  return true;
}

bool lovrBundleBindTexture(Bundle* bundle, uint32_t id, uint32_t element, Texture* texture) {
  ShaderGroup* group = bundle->group;
  bool isSlot = group->slotMask & (1 << id);
  bool isTexture = group->textureMask & (1 << id);
  gpu_slot* slot = &group->slots[bundle->group->slotIndex[id]];
  gpu_texture_binding* binding = &bundle->bindings[group->bindingIndex[id]].texture + element;

  if (!isSlot || !isTexture || element >= slot->count) return false;

  binding->object = texture->gpu;
  binding->sampler = NULL; // TODO
  bundle->dirty = true;
  lovrRetain(texture);
  return true;
}
