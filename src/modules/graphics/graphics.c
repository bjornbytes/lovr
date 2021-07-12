#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "event/event.h"
#include "headset/headset.h"
#include "core/maf.h"
#include "core/map.h"
#include "core/gpu.h"
#include "core/os.h"
#include "core/util.h"
#include "resources/shaders.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define INTERNAL_VERTEX_BUFFERS 1
#define MAX_STREAMS 32

struct Buffer {
  uint32_t ref;
  gpu_buffer* gpu;
  BufferInfo info;
  void* pointer;
  uint32_t base;
  uint32_t frame;
  uint32_t size;
};

struct Texture {
  uint32_t ref;
  gpu_texture* gpu;
  gpu_texture* renderView;
  TextureInfo info;
  uint32_t baseLayer;
  uint32_t baseLevel;
};

typedef struct {
  float projection[6][16];
  float viewMatrix[6][16];
} Camera;

struct Canvas {
  uint32_t ref;
  bool temporary;
  gpu_canvas gpu;
  CanvasInfo info;
  Texture* colorTextures[4];
  Texture* resolveTextures[4];
  Texture* depthTexture;
  Camera camera;
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
  uint32_t locationMask;
  map_t lookup;
};

struct Batch {
  uint32_t ref;
  BatchInfo info;
  Shader* shader;
  uint32_t attributeCount;
  VertexAttribute attributes[16];
  float transforms[64][16];
  uint32_t transform;
  gpu_pipeline_info* pipeline;
};

static struct {
  bool initialized;
  bool active;
  uint32_t blockSize;
  gpu_hardware hardware;
  gpu_features features;
  gpu_limits limits;
  map_t pipelines;
  map_t canvases;
  map_t passes;
  Canvas* window;
  Shader* defaultShader;
  gpu_stream* transfers;
  gpu_stream* streams[MAX_STREAMS];
  int8_t streamOrder[MAX_STREAMS];
  uint32_t streamCount;
  uint32_t frame;
} state;

static void callback(void* context, const char* message, int severe) {
  if (severe) {
    lovrThrow(message);
  } else {
    lovrLog(LOG_DEBUG, "GPU", message);
  }
}

const char** os_vk_get_instance_extensions(uint32_t* count);
uint32_t os_vk_create_surface(void* instance, void** surface);

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
  } else
#endif
  buffer[-1] = '\0';

  return true;
}

bool lovrGraphicsInit(bool debug, uint32_t blockSize) {
  lovrAssert(blockSize <= 1 << 30, "Block size can not exceed 1GB");
  state.blockSize = blockSize;

  gpu_config config = {
    .debug = debug,
    .hardware = &state.hardware,
    .features = &state.features,
    .limits = &state.limits,
    .callback = callback,
    .vk.getInstanceExtensions = getInstanceExtensions,
#if defined(LOVR_VK) && !defined(LOVR_DISABLE_HEADSET)
    .vk.getDeviceExtensions = lovrHeadsetDisplayDriver ? lovrHeadsetDisplayDriver->getVulkanDeviceExtensions : NULL
#endif
  };

#if defined(LOVR_VK) && !defined(__ANDROID__)
  if (os_window_is_open()) {
    config.vk.surface = true;
    config.vk.vsync = false; // TODO
    config.vk.createSurface = os_vk_create_surface;
  }
#endif

  if (!gpu_init(&config)) {
    lovrThrow("Failed to initialize GPU");
  }

  map_init(&state.pipelines, 8);
  map_init(&state.canvases, 4);
  map_init(&state.passes, 4);

  state.limits.vertexBuffers -= INTERNAL_VERTEX_BUFFERS;

  state.defaultShader = lovrShaderCreate(&(ShaderInfo) {
    .type = SHADER_GRAPHICS,
    .source[0] = lovr_shader_unlit_vert,
    .source[1] = lovr_shader_unlit_frag,
    .length[0] = sizeof(lovr_shader_unlit_vert),
    .length[1] = sizeof(lovr_shader_unlit_frag),
    .label = "unlit"
  });

  return state.initialized = true;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  for (size_t i = 0; i < state.pipelines.size; i++) {
    if (state.pipelines.values[i] == MAP_NIL) continue;
    gpu_pipeline* pipeline = (gpu_pipeline*) (uintptr_t) state.pipelines.values[i];
    gpu_pipeline_destroy(pipeline);
    free(pipeline);
  }
  for (size_t i = 0; i < state.canvases.size; i++) {
    if (state.canvases.values[i] == MAP_NIL) continue;
    Canvas* canvas = (Canvas*) (uintptr_t) state.canvases.values[i];
    lovrRelease(canvas, lovrCanvasDestroy);
    free(canvas);
  }
  for (uint32_t i = 0; i < state.passes.size; i++) {
    if (state.passes.values[i] == MAP_NIL) continue;
    gpu_pass* pass = (gpu_pass*) (uintptr_t) state.passes.values[i];
    gpu_pass_destroy(pass);
    free(pass);
  }
  map_free(&state.pipelines);
  map_free(&state.canvases);
  map_free(&state.passes);
  lovrRelease(state.defaultShader, lovrShaderDestroy);
  lovrRelease(state.window, lovrCanvasDestroy);
  gpu_destroy();
  memset(&state, 0, sizeof(state));
}

void lovrGraphicsGetHardware(GraphicsHardware* hardware) {
  hardware->vendorId = state.hardware.vendorId;
  hardware->deviceId = state.hardware.deviceId;
  hardware->deviceName = state.hardware.deviceName;
#ifdef LOVR_VK
  hardware->renderer = "vulkan";
#endif
  hardware->driverMajor = state.hardware.driverMajor;
  hardware->driverMinor = state.hardware.driverMinor;
  hardware->driverPatch = state.hardware.driverPatch;
  hardware->discrete = state.hardware.discrete;
}

void lovrGraphicsGetFeatures(GraphicsFeatures* features) {
  features->bptc = state.features.bptc;
  features->astc = state.features.astc;
  features->pointSize = state.features.pointSize;
  features->wireframe = state.features.wireframe;
  features->multiblend = state.features.multiblend;
  features->anisotropy = state.features.anisotropy;
  features->depthClamp = state.features.depthClamp;
  features->depthNudgeClamp = state.features.depthOffsetClamp;
  features->clipDistance = state.features.clipDistance;
  features->cullDistance = state.features.cullDistance;
  features->fullIndexBufferRange = state.features.fullIndexBufferRange;
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
  limits->renderViews = MIN(state.limits.renderViews, 6);
  limits->bundleCount = state.limits.bundleCount;
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
  limits->pointSize[0] = state.limits.pointSize[0];
  limits->pointSize[1] = state.limits.pointSize[1];
  limits->anisotropy = state.limits.anisotropy;
}

void lovrGraphicsBegin() {
  if (state.active) return;
  state.active = true;
  state.frame++;
  gpu_begin();
  state.streams[0] = state.transfers = gpu_stream_begin();
  state.streamOrder[0] = 0;
  state.streamCount = 1;
}

static int streamSort(const void* a, const void* b) {
  return state.streamOrder[*(uint32_t*) a] - state.streamOrder[*(uint32_t*) b];
}

void lovrGraphicsSubmit() {
  if (!state.active) return;
  state.active = false;

  if (state.window) {
    state.window->gpu.color[0].texture = NULL;
  }

  uint32_t streamMap[MAX_STREAMS];
  for (uint32_t i = 0; i < state.streamCount; i++) {
    streamMap[i] = i;
  }

  qsort(streamMap, state.streamCount, sizeof(uint32_t), streamSort);

  gpu_stream* streams[MAX_STREAMS]; // Sorted
  for (uint32_t i = 0; i < state.streamCount; i++) {
    streams[i] = state.streams[streamMap[i]];
    gpu_stream_end(streams[i]);
  }

  gpu_submit(streams, state.streamCount);
}

void lovrGraphicsRender(Canvas* canvas, Batch* batch) {
  lovrAssert(state.active, "Graphics is not active");

  // TODO pre-render hook
  if (canvas == state.window) {
    if (!canvas->gpu.color[0].texture) {
      canvas->gpu.color[0].texture = gpu_surface_acquire();
    }
    int width, height;
    os_window_get_fbsize(&width, &height); // TODO resize swapchain?
    canvas->gpu.size[0] = width;
    canvas->gpu.size[1] = height;
    canvas->gpu.color[0].clear[0] = 0.f;
    canvas->gpu.color[0].clear[1] = 0.f;
    canvas->gpu.color[0].clear[2] = 0.f;
    canvas->gpu.color[0].clear[3] = 1.f;
    // TODO set clear to background
  }

  gpu_stream* stream = gpu_stream_begin();
  gpu_render_begin(stream, &canvas->gpu);
  gpu_render_end(stream);

  state.streamOrder[state.streamCount] = 1;
  state.streams[state.streamCount++] = stream;
}

// Buffer

Buffer* lovrBufferCreate(BufferInfo* info) {
  size_t size = info->length * MAX(info->stride, 1);
  lovrAssert(size > 0, "Buffer size must be positive");
  lovrAssert(size <= 1 << 30, "Max Buffer size is 1GB");

  Buffer* buffer = calloc(1, sizeof(Buffer) + gpu_sizeof_buffer());
  lovrAssert(buffer, "Out of memory");
  buffer->gpu = (gpu_buffer*) (buffer + 1);
  buffer->info = *info;
  buffer->size = size;
  buffer->ref = 1;

  gpu_buffer_init(buffer->gpu, &(gpu_buffer_info) {
    .size = ALIGN(size, 4),
    .usage = info->usage,
    .handle = info->handle,
    .label = info->label
  });

  return buffer;
}

void lovrBufferDestroy(void* ref) {
  Buffer* buffer = ref;
  free(buffer);
}

const BufferInfo* lovrBufferGetInfo(Buffer* buffer) {
  return &buffer->info;
}

void* lovrBufferMap(Buffer* buffer, uint32_t offset, uint32_t size) {
  if (size == ~0u) size = buffer->size - offset;
  lovrAssert(state.active, "Graphics is not active");
  lovrAssert(offset + size <= buffer->size, "Tried to write past the end of the Buffer");
  lovrAssert(buffer->info.usage & BUFFER_COPYTO, "Buffers must have the 'copyto' usage to write to them");
  gpu_scratchpad scratch = gpu_scratch(size, 4);
  gpu_copy_buffer(state.transfers, scratch.buffer, buffer->gpu, scratch.offset, offset, size);
  return scratch.pointer;
}

void lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t size) {
  lovrAssert(state.active, "Graphics is not active");
  lovrAssert(offset % 4 == 0, "Buffer clear offset must be a multiple of 4");
  lovrAssert(size % 4 == 0, "Buffer clear size must be a multiple of 4");
  lovrAssert(offset + size <= buffer->size, "Tried to clear past the end of the Buffer");
  lovrAssert(buffer->info.usage & BUFFER_COPYTO, "Buffers must have the 'copyto' usage to clear them");
  gpu_clear_buffer(state.transfers, buffer->gpu, offset, size, 0);
}

void lovrBufferCopy(Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size) {
  lovrAssert(state.active, "Graphics is not active");
  lovrAssert(src->info.usage & BUFFER_COPYFROM, "Buffer must have the 'copyfrom' usage to copy from it");
  lovrAssert(dst->info.usage & BUFFER_COPYTO, "Buffer must have the 'copyto' usage to copy to it");
  lovrAssert(srcOffset + size <= src->size, "Tried to read past the end of the source Buffer");
  lovrAssert(dstOffset + size <= dst->size, "Tried to copy past the end of the destination Buffer");
  gpu_copy_buffer(state.transfers, src->gpu, dst->gpu, srcOffset, dstOffset, size);
}

void lovrBufferRead(Buffer* buffer, uint32_t offset, uint32_t size, void (*callback)(void* data, uint32_t size, void* userdata), void* userdata) {
  lovrAssert(state.active, "Graphics is not active");
  lovrAssert(buffer->info.usage & BUFFER_COPYFROM, "Buffer must have the 'copyfrom' usage to read from it");
  lovrAssert(offset + size <= buffer->size, "Tried to read past the end of the Buffer");
  gpu_read_buffer(state.transfers, buffer->gpu, offset, size, callback, userdata);
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
  uint16_t maxWidth = MAX(info->width >> offset[3], 1);
  uint16_t maxHeight = MAX(info->height >> offset[3], 1);
  uint16_t maxDepth = info->type == TEXTURE_VOLUME ? MAX(info->depth >> offset[3], 1) : info->depth;
  lovrAssert(offset[0] + extent[0] <= maxWidth, "Texture x range [%d,%d] exceeds width (%d)", offset[0], offset[0] + extent[0], maxWidth);
  lovrAssert(offset[1] + extent[1] <= maxHeight, "Texture y range [%d,%d] exceeds height (%d)", offset[1], offset[1] + extent[1], maxHeight);
  lovrAssert(offset[2] + extent[2] <= maxDepth, "Texture z range [%d,%d] exceeds depth (%d)", offset[2], offset[2] + extent[2], maxDepth);
  lovrAssert(offset[3] < info->mipmaps, "Texture mipmap %d exceeds its mipmap count (%d)", offset[3] + 1, info->mipmaps);
}

Texture* lovrTextureCreate(TextureInfo* info) {
  uint32_t mips = log2(MAX(MAX(info->width, info->height), (info->type == TEXTURE_VOLUME ? info->depth : 1))) + 1;
  uint8_t supports = state.features.formats[info->format];

  uint32_t limits[] = {
    [TEXTURE_2D] = state.limits.textureSize2D,
    [TEXTURE_CUBE] = state.limits.textureSizeCube,
    [TEXTURE_ARRAY] = state.limits.textureSize2D,
    [TEXTURE_VOLUME] = state.limits.textureSize3D
  };

  lovrAssert(info->width > 0, "Texture width must be greater than zero");
  lovrAssert(info->height > 0, "Texture height must be greater than zero");
  lovrAssert(info->depth > 0, "Texture depth must be greater than zero");
  lovrAssert(info->width <= limits[info->type], "Texture %s exceeds the limit for this texture type (%d)", "width", limits[info->type]);
  lovrAssert(info->height <= limits[info->type], "Texture %s exceeds the limit for this texture type (%d)", "height", limits[info->type]);
  lovrAssert(info->depth <= limits[info->type] || info->type != TEXTURE_VOLUME, "Texture %s exceeds the limit for this texture type (%d)", "depth", limits[info->type]);
  lovrAssert(info->depth <= state.limits.textureLayers || info->type != TEXTURE_ARRAY, "Texture %s exceeds the limit for this texture type (%d)", "depth", limits[info->type]);
  lovrAssert(info->depth == 1 || info->type != TEXTURE_2D, "2D textures must have a depth of 1");
  lovrAssert(info->depth == 6 || info->type != TEXTURE_CUBE, "Cubemaps must have a depth of 6");
  lovrAssert(info->width == info->height || info->type != TEXTURE_CUBE, "Cubemaps must be square");
  lovrAssert(getTextureRegionSize(info->format, info->width, info->height, info->depth) < 1 << 30, "Memory for a Texture can not exceed 1GB");
  lovrAssert((info->samples & (info->samples - 1)) == 0, "Texture multisample count must be a power of 2");
  lovrAssert(info->samples == 1 || info->type != TEXTURE_CUBE, "Cubemaps can not be multisampled");
  lovrAssert(info->samples == 1 || info->type != TEXTURE_VOLUME, "Volume textures can not be multisampled");
  lovrAssert(info->samples == 1 || ~info->usage & TEXTURE_STORAGE, "Currently, Textures with the 'compute' flag can not be multisampled");
  lovrAssert(info->samples == 1 || info->mipmaps == 1, "Multisampled textures can only have 1 mipmap");
  lovrAssert(~info->usage & TEXTURE_SAMPLE || (supports & GPU_FEATURE_SAMPLE), "GPU does not support the 'sample' flag for this format");
  lovrAssert(~info->usage & TEXTURE_CANVAS || (supports & GPU_FEATURE_RENDER), "GPU does not support the 'render' flag for this format");
  lovrAssert(~info->usage & TEXTURE_STORAGE || (supports & GPU_FEATURE_STORAGE), "GPU does not support the 'compute' flag for this format");
  lovrAssert(~info->usage & TEXTURE_CANVAS || info->width <= state.limits.renderSize[0], "Texture has 'render' flag but its size exceeds limits.renderSize");
  lovrAssert(~info->usage & TEXTURE_CANVAS || info->height <= state.limits.renderSize[1], "Texture has 'render' flag but its size exceeds limits.renderSize");
  lovrAssert(info->mipmaps == ~0u || info->mipmaps <= mips, "Texture has more than the max number of mipmap levels for its size (%d)", mips);

  Texture* texture = calloc(1, sizeof(Texture) + gpu_sizeof_texture());
  lovrAssert(texture, "Out of memory");
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = *info;
  texture->ref = 1;

  gpu_texture_init(texture->gpu, &(gpu_texture_info) {
    .type = (gpu_texture_type) info->type,
    .format = (gpu_texture_format) info->format,
    .size = { info->width, info->height, info->depth },
    .mipmaps = info->mipmaps == ~0u ? mips : info->mipmaps + !info->mipmaps,
    .samples = info->samples + !info->samples,
    .usage = info->usage,
    .srgb = info->srgb,
    .handle = info->handle,
    .label = info->label
  });

  if (info->mipmaps == 1 && (info->type == TEXTURE_2D || info->type == TEXTURE_ARRAY)) {
    texture->renderView = texture->gpu;
  }

  return texture;
}

Texture* lovrTextureCreateView(TextureViewInfo* view) {
  const TextureInfo* info = &view->parent->info;
  uint32_t maxDepth = info->type == TEXTURE_VOLUME ? MAX(info->depth >> view->levelIndex, 1) : info->depth;
  lovrAssert(!info->parent, "Can't nest texture views");
  lovrAssert(view->type != TEXTURE_VOLUME, "Texture views may not be volume textures");
  lovrAssert(view->layerCount > 0, "Texture view must have at least one layer");
  lovrAssert(view->levelCount > 0, "Texture view must have at least one mipmap");
  lovrAssert(view->layerIndex + view->layerCount <= maxDepth, "Texture view layer range exceeds depth of parent texture");
  lovrAssert(view->levelIndex + view->levelCount <= info->mipmaps, "Texture view mipmap range exceeds mipmap count of parent texture");
  lovrAssert(view->layerCount == 1 || view->type != TEXTURE_2D, "2D texture can only have a single layer");
  lovrAssert(view->layerCount == 6 || view->type != TEXTURE_CUBE, "Cubemaps can only have a six layers");
  lovrAssert(view->levelCount == 1 || info->type != TEXTURE_VOLUME, "Views of volume textures may only have a single mipmap level");

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
  texture->baseLayer = view->layerIndex;
  texture->baseLevel = view->levelIndex;

  gpu_texture_init_view(texture->gpu, &(gpu_texture_view_info) {
    .source = view->parent->gpu,
    .type = (gpu_texture_type) view->type,
    .layerIndex = view->layerIndex,
    .layerCount = view->layerCount,
    .levelIndex = view->levelIndex,
    .levelCount = view->levelCount
  });

  if (view->levelCount == 1 && (view->type == TEXTURE_2D || view->type == TEXTURE_ARRAY)) {
    texture->renderView = texture->gpu;
  }

  lovrRetain(view->parent);
  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  lovrRelease(texture->info.parent, lovrTextureDestroy);
  if (texture->renderView != texture->gpu) gpu_texture_destroy(texture->renderView);
  gpu_texture_destroy(texture->gpu);
  free(texture);
}

const TextureInfo* lovrTextureGetInfo(Texture* texture) {
  return &texture->info;
}

void lovrTextureWrite(Texture* texture, uint16_t offset[4], uint16_t extent[3], void* data, uint32_t step[2]) {
  lovrAssert(state.active, "Graphics is not active");
  lovrAssert(texture->info.usage & TEXTURE_COPYTO, "Texture must have the 'copyto' flag to write to it");
  lovrAssert(texture->info.samples == 1, "Multisampled Textures can not be written to");
  checkTextureBounds(&texture->info, offset, extent);

  char* src = data;
  uint16_t realOffset[4] = { offset[0], offset[1], offset[2] + texture->baseLayer, offset[3] + texture->baseLevel };
  char* dst = gpu_map_texture(state.transfers, texture->gpu, realOffset, extent);
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
  lovrAssert(state.active, "Graphics is not active");
  lovrAssert(!isDepthFormat(texture->info.format), "Currently only color textures can be cleared");
  lovrAssert(texture->info.type == TEXTURE_VOLUME || layer + layerCount <= texture->info.depth, "Texture clear range exceeds texture layer count");
  lovrAssert(level + levelCount <= texture->info.mipmaps, "Texture clear range exceeds texture mipmap count");
  gpu_clear_texture(state.transfers, texture->gpu, layer + texture->baseLayer, layerCount, level + texture->baseLevel, levelCount, color);
}

void lovrTextureRead(Texture* texture, uint16_t offset[4], uint16_t extent[3], void (*callback)(void* data, uint32_t size, void* userdata), void* userdata) {
  lovrAssert(state.active, "Graphics is not active");
  lovrAssert(texture->info.usage & TEXTURE_COPYFROM, "Texture must have the 'copyfrom' flag to read from it");
  lovrAssert(texture->info.samples == 1, "Multisampled Textures can not be read");
  checkTextureBounds(&texture->info, offset, extent);

  uint16_t realOffset[4] = { offset[0], offset[1], offset[2] + texture->baseLayer, offset[3] + texture->baseLevel };
  gpu_read_texture(state.transfers, texture->gpu, realOffset, extent, callback, userdata);
}

void lovrTextureCopy(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t extent[3]) {
  lovrAssert(state.active, "Graphics is not active");
  lovrAssert(src->info.usage & TEXTURE_COPYFROM, "Texture must have the 'copyfrom' flag to copy from it");
  lovrAssert(dst->info.usage & TEXTURE_COPYTO, "Texture must have the 'copyto' flag to copy to it");
  lovrAssert(src->info.format == dst->info.format, "Copying between Textures requires them to have the same format");
  lovrAssert(src->info.samples == dst->info.samples, "Textures must have the same sample counts to copy between them");
  checkTextureBounds(&src->info, srcOffset, extent);
  checkTextureBounds(&dst->info, dstOffset, extent);
  uint16_t realSrcOffset[4] = { srcOffset[0], srcOffset[1], srcOffset[2] + src->baseLayer, srcOffset[3] + src->baseLevel };
  uint16_t realDstOffset[4] = { dstOffset[0], dstOffset[1], dstOffset[2] + dst->baseLayer, dstOffset[3] + dst->baseLevel };
  gpu_copy_texture(state.transfers, src->gpu, dst->gpu, realSrcOffset, realDstOffset, extent);
}

void lovrTextureBlit(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t srcExtent[3], uint16_t dstExtent[3], bool nearest) {
  lovrAssert(state.active, "Graphics is not active");
  lovrAssert(src->info.samples == 1 && dst->info.samples == 1, "Multisampled textures can not be used for blits");
  lovrAssert(src->info.usage & TEXTURE_COPYFROM, "Texture must have the 'copyfrom' flag to blit from it");
  lovrAssert(dst->info.usage & TEXTURE_COPYTO, "Texture must have the 'copyto' flag to blit to it");
  lovrAssert(state.features.formats[src->info.format] & GPU_FEATURE_BLIT, "This GPU does not support blits for the source texture's format");
  lovrAssert(state.features.formats[dst->info.format] & GPU_FEATURE_BLIT, "This GPU does not support blits for the destination texture's format");

  if (isDepthFormat(src->info.format) || isDepthFormat(dst->info.format)) {
    lovrAssert(src->info.format == dst->info.format, "Blitting between depth textures requires them to have the same format");
  }

  checkTextureBounds(&src->info, srcOffset, srcExtent);
  checkTextureBounds(&dst->info, dstOffset, dstExtent);
  gpu_blit(state.transfers, src->gpu, dst->gpu, srcOffset, dstOffset, srcExtent, dstExtent, nearest);
}

// Canvas

static void lovrCanvasInit(Canvas* canvas, CanvasInfo* info) {
  lovrAssert(info->color[0].texture || info->depth.texture, "Canvas must have at least one color or depth texture");
  const TextureInfo* first = info->color[0].texture ? &info->color[0].texture->info : &info->depth.texture->info;

  uint32_t width = first->width;
  uint32_t height = first->height;
  uint32_t samples = first->samples > 1 ? first->samples : info->samples;
  bool resolve = first->samples == 1 && info->samples > 1;

  // Validate size/samples
  lovrAssert(width <= state.limits.renderSize[0], "Canvas width exceeds the renderWidth limit of this GPU (%d)", state.limits.renderSize[0]);
  lovrAssert(height <= state.limits.renderSize[1], "Canvas width exceeds the renderHeight limit of this GPU (%d)", state.limits.renderSize[1]);
  lovrAssert(info->views <= state.limits.renderViews, "Canvas view count (%d) exceeds the renderViews limit of this GPU (%d)", info->views, state.limits.renderViews);
  lovrAssert((samples & (samples - 1)) == 0, "Canvas multisample count must be a power of 2");

  // Validate color attachments
  for (uint32_t i = 0; i < info->count && i < 4; i++) {
    Texture* texture = info->color[i].texture;
    bool renderable = texture->info.format == ~0u || (state.features.formats[texture->info.format] & GPU_FEATURE_RENDER_COLOR);
    lovrAssert(renderable, "This GPU does not support rendering to the texture format used by Canvas color attachment #%d", i + 1);
    lovrAssert(texture->info.usage & TEXTURE_CANVAS, "Texture must be created with the 'render' flag to attach it to a Canvas");
    lovrAssert(texture->info.width == first->width, "Canvas texture sizes must match");
    lovrAssert(texture->info.height == first->height, "Canvas texture sizes must match");
    lovrAssert(texture->info.depth == first->depth, "Canvas texture sizes must match");
    lovrAssert(texture->info.samples == first->samples, "Canvas texture sample counts must match");
  }

  // Validate depth attachment
  if (info->depth.enabled) {
    Texture* texture = info->depth.texture;
    TextureFormat format = texture ? texture->info.format : info->depth.format;
    bool renderable = state.features.formats[format] & GPU_FEATURE_RENDER_DEPTH;
    lovrAssert(renderable, "This GPU does not support rendering to the Canvas depth buffer's format");
    if (texture) {
      lovrAssert(texture->info.usage & TEXTURE_CANVAS, "Textures must be created with the 'render' flag to attach them to a Canvas");
      lovrAssert(texture->info.width == first->width, "Canvas texture sizes must match");
      lovrAssert(texture->info.height == first->height, "Canvas texture sizes must match");
      lovrAssert(texture->info.depth == first->depth, "Canvas texture sizes must match");
      lovrAssert(texture->info.samples == samples, "Currently, Canvas depth buffer sample count must match its main multisample count");
    }
  }

  // Pass info
  gpu_pass_info pass = {
    .count = info->count,
    .samples = info->samples,
    .views = info->views,
    .resolve = resolve
  };

  for (uint32_t i = 0; i < info->count; i++) {
    pass.color[i] = (gpu_pass_color_info) {
      .format = (gpu_texture_format) info->color[i].texture->info.format,
      .load = (gpu_load_op) info->color[i].load,
      .save = (gpu_save_op) info->color[i].save,
      .srgb = info->color[i].texture->info.srgb
    };
  }

  if (info->depth.enabled) {
    pass.depth = (gpu_pass_depth_info) {
      .format = (gpu_texture_format) info->depth.format,
      .load = (gpu_load_op) info->depth.load,
      .save = (gpu_save_op) info->depth.save,
      .stencilLoad = (gpu_load_op) info->depth.stencilLoad,
      .stencilSave = (gpu_save_op) info->depth.stencilSave
    };
  }

  // Get/create cached pass
  uint64_t hash = hash64(&pass, sizeof(pass));
  uint64_t value = map_get(&state.passes, hash);

  if (value == MAP_NIL) {
    gpu_pass* instance = calloc(1, gpu_sizeof_pass());
    lovrAssert(instance, "Out of memory");
    lovrAssert(gpu_pass_init(instance, &pass), "Failed to initialize pass");
    value = (uintptr_t) instance;
    map_set(&state.passes, hash, value);
  }

  canvas->gpu.pass = (gpu_pass*) (uintptr_t) value;
  canvas->gpu.size[0] = width;
  canvas->gpu.size[1] = height;

  // Create any missing targets

  TextureInfo textureInfo = {
    .type = TEXTURE_ARRAY,
    .width = width,
    .height = height,
    .depth = info->views,
    .mipmaps = 1,
    .samples = info->samples,
    .usage = GPU_TEXTURE_RENDER | GPU_TEXTURE_TRANSIENT
  };

  gpu_texture_view_info viewInfo = {
    .type = GPU_TEXTURE_ARRAY,
    .layerCount = info->views,
    .levelCount = 1
  };

  // Color
  for (uint32_t i = 0; i < info->count; i++) {
    Texture* texture = info->color[i].texture;

    if (!texture->renderView) {
      viewInfo.source = texture->info.parent ? texture->info.parent->gpu : texture->gpu;
      viewInfo.layerIndex = texture->baseLayer;
      viewInfo.levelIndex = texture->baseLevel;
      texture->renderView = malloc(gpu_sizeof_texture());
      lovrAssert(texture->renderView, "Out of memory");
      lovrAssert(gpu_texture_init_view(texture->renderView, &viewInfo), "Failed to create texture view for canvas");
    }

    lovrRetain(texture);
    canvas->colorTextures[i] = texture;
    canvas->gpu.color[i].texture = texture->renderView;
  }

  // Resolve (swap color -> resolve and create temp msaa targets as new color targets)
  if (resolve) {
    for (uint32_t i = 0; i < info->count; i++) {
      canvas->resolveTextures[i] = canvas->colorTextures[i];
      canvas->gpu.color[i].resolve = canvas->gpu.color[i].texture;

      textureInfo.format = canvas->colorTextures[i]->info.format;
      canvas->colorTextures[i] = lovrTextureCreate(&textureInfo);
      canvas->gpu.color[i].texture = canvas->colorTextures[i]->renderView;
    }
  }

  // Depth
  if (info->depth.enabled) {
    if (info->depth.texture) {
      if (!info->depth.texture->renderView) {
        Texture* texture = canvas->depthTexture;
        viewInfo.source = texture->info.parent ? texture->info.parent->gpu : texture->gpu;
        viewInfo.layerIndex = texture->baseLayer;
        viewInfo.levelIndex = texture->baseLevel;
        texture->renderView = malloc(gpu_sizeof_texture());
        lovrAssert(texture->renderView, "Out of memory");
        lovrAssert(gpu_texture_init_view(texture->renderView, &viewInfo), "Failed to create texture view for canvas");
      }
      canvas->depthTexture = info->depth.texture;
      canvas->gpu.depth.texture = canvas->depthTexture->renderView;
      lovrRetain(canvas->depthTexture);
    } else {
      textureInfo.format = info->depth.format;
      canvas->depthTexture = lovrTextureCreate(&textureInfo);
      canvas->gpu.depth.texture = canvas->depthTexture->renderView;
    }
  }

  // Default camera, for funsies
  for (uint32_t i = 0; i < info->views; i++) {
    mat4_perspective(canvas->camera.projection[i], .01f, 100.f, 67.f * (float) M_PI / 180.f, (float) width / height);
    mat4_identity(canvas->camera.viewMatrix[i]);
  }
}

Canvas* lovrCanvasCreate(CanvasInfo* info) {
  Canvas* canvas = calloc(1, sizeof(Canvas));
  lovrAssert(canvas, "Out of memory");
  canvas->info = *info;
  canvas->ref = 1;
  lovrCanvasInit(canvas, info);
  return canvas;
}

Canvas* lovrCanvasGetTemporary(CanvasInfo* info) {
  uint64_t hash = hash64(info, sizeof(*info));
  uint64_t value = map_get(&state.canvases, hash);
  if (value == MAP_NIL) {
    Canvas* canvas = calloc(1, sizeof(Canvas));
    lovrAssert(canvas, "Out of memory");
    canvas->ref = 1;
    canvas->info = *info;
    canvas->temporary = true;
    lovrCanvasInit(canvas, info);
    value = (uintptr_t) canvas;
    map_set(&state.canvases, hash, value);
  }
  return (Canvas*) (uintptr_t) value;
}

Canvas* lovrCanvasGetWindow() {
  if (state.window) return state.window;

  Canvas* canvas = calloc(1, sizeof(Canvas));
  lovrAssert(canvas, "Out of memory");
  canvas->ref = 1;

  gpu_pass_info info = {
    .label = "Window",
    .color[0].format = ~0u,
    .color[0].load = GPU_LOAD_OP_CLEAR,
    .color[0].save = GPU_SAVE_OP_SAVE,
    .color[0].srgb = true,
    .depth.format = 0, // TODO customizable
    .depth.load = GPU_LOAD_OP_CLEAR,
    .depth.save = GPU_SAVE_OP_DISCARD,
    .depth.stencilLoad = GPU_LOAD_OP_CLEAR,
    .depth.stencilSave = GPU_SAVE_OP_DISCARD,
    .count = 1,
    .samples = 1,
    .views = 1
  };

  int width, height;
  os_window_get_fbsize(&width, &height);

  // Default camera, for funsies (TODO dedupe)
  for (uint32_t i = 0; i < info.views; i++) {
    mat4_perspective(canvas->camera.projection[i], .01f, 100.f, 67.f * (float) M_PI / 180.f, (float) width / height);
    mat4_identity(canvas->camera.viewMatrix[i]);
  }

  canvas->gpu.pass = calloc(1, gpu_sizeof_pass());
  lovrAssert(canvas->gpu.pass, "Out of memory");
  lovrAssert(gpu_pass_init(canvas->gpu.pass, &info), "Failed to create window pass");
  return state.window = canvas;
}

void lovrCanvasDestroy(void* ref) {
  Canvas* canvas = ref;
  for (uint32_t i = 0; i < canvas->info.count; i++) {
    lovrRelease(canvas->colorTextures[i], lovrTextureDestroy);
    lovrRelease(canvas->resolveTextures[i], lovrTextureDestroy);
  }
  lovrRelease(canvas->depthTexture, lovrTextureDestroy);
  if (canvas == state.window) {
    gpu_pass_destroy(canvas->gpu.pass);
    free(canvas->gpu.pass);
  }
  if (!canvas->temporary) free(canvas);
}

const CanvasInfo* lovrCanvasGetInfo(Canvas* canvas) {
  return &canvas->info;
}

uint32_t lovrCanvasGetWidth(Canvas* canvas) {
  return canvas->gpu.size[0];
}

uint32_t lovrCanvasGetHeight(Canvas* canvas) {
  return canvas->gpu.size[1];
}

void lovrCanvasGetViewMatrix(Canvas* canvas, uint32_t index, float* viewMatrix) {
  lovrAssert(index < COUNTOF(canvas->camera.viewMatrix), "Invalid view index %d", index);
  mat4_init(viewMatrix, canvas->camera.viewMatrix[index]);
}

void lovrCanvasSetViewMatrix(Canvas* canvas, uint32_t index, float* viewMatrix) {
  lovrAssert(index < COUNTOF(canvas->camera.viewMatrix), "Invalid view index %d", index);
  mat4_init(canvas->camera.viewMatrix[index], viewMatrix);
}

void lovrCanvasGetProjection(Canvas* canvas, uint32_t index, float* projection) {
  lovrAssert(index < COUNTOF(canvas->camera.projection), "Invalid view index %d", index);
  mat4_init(projection, canvas->camera.projection[index]);
}

void lovrCanvasSetProjection(Canvas* canvas, uint32_t index, float* projection) {
  lovrAssert(index < COUNTOF(canvas->camera.projection), "Invalid view index %d", index);
  mat4_init(canvas->camera.projection[index], projection);
}

void lovrCanvasGetClear(Canvas* canvas, float color[4][4], float* depth, uint8_t* stencil) {
  for (uint32_t i = 0; i < canvas->info.count; i++) {
    memcpy(color[i], canvas->gpu.color[i].clear, 4 * sizeof(float));
  }
  *depth = canvas->gpu.depth.clear.depth;
  *stencil = canvas->gpu.depth.clear.stencil;
}

void lovrCanvasSetClear(Canvas* canvas, float color[4][4], float depth, uint8_t stencil) {
  for (uint32_t i = 0; i < canvas->info.count; i++) {
    memcpy(canvas->gpu.color[i].clear, color[i], 4 * sizeof(float));
  }
  canvas->gpu.depth.clear.depth = depth;
  canvas->gpu.depth.clear.stencil = stencil;
}

void lovrCanvasGetTextures(Canvas* canvas, Texture* textures[4], Texture** depth) {
  for (uint32_t i = 0; i < canvas->info.count; i++) {
    textures[i] = canvas->info.samples > 1 ? canvas->resolveTextures[i] : canvas->colorTextures[i];
  }
  *depth = canvas->info.depth.enabled ? canvas->depthTexture : NULL;
}

void lovrCanvasSetTextures(Canvas* canvas, Texture* textures[4], Texture* depth) {
  for (uint32_t i = 0; i < canvas->info.count; i++) {
    if (!textures[i]) continue;
    lovrRetain(textures[i]);
    if (canvas->info.samples > 1) {
      lovrRelease(canvas->resolveTextures[i], lovrTextureDestroy);
      canvas->gpu.color[i].resolve = textures[i]->renderView;
      canvas->colorTextures[i] = textures[i];
    }
  }

  if (canvas->info.depth.enabled && depth) {
    lovrRetain(depth);
    lovrRelease(canvas->depthTexture, lovrTextureDestroy);
    canvas->gpu.depth.texture = depth->renderView;
    canvas->depthTexture = depth;
  }
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
    case 10: lovrAssert(state.features.float64, "GPU does not support shader feature #%d: %s", capability, "64 bit floats"); break;
    case 11: lovrAssert(state.features.int64, "GPU does not support shader feature #%d: %s", capability, "64 bit integers"); break;
    case 12: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "64 bit atomics");
    case 22: lovrAssert(state.features.int16, "GPU does not support shader feature #%d: %s", capability, "16 bit integers"); break;
    case 23: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "tessellation shading");
    case 24: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "geometry shading");
    case 25: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "extended image gather");
    case 27: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "multisample storage textures");
    case 28: lovrAssert(state.features.dynamicIndexing, "GPU does not support shader feature #%d: %s", capability, "dynamic indexing"); break;
    case 29: lovrAssert(state.features.dynamicIndexing, "GPU does not support shader feature #%d: %s", capability, "dynamic indexing"); break;
    case 30: lovrAssert(state.features.dynamicIndexing, "GPU does not support shader feature #%d: %s", capability, "dynamic indexing"); break;
    case 31: lovrAssert(state.features.dynamicIndexing, "GPU does not support shader feature #%d: %s", capability, "dynamic indexing"); break;
    case 32: lovrAssert(state.features.clipDistance, "GPU does not support shader feature #%d: %s", capability, "clip distance"); break;
    case 33: lovrAssert(state.features.cullDistance, "GPU does not support shader feature #%d: %s", capability, "cull distance"); break;
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
    case 4427: lovrAssert(state.features.extraShaderInputs, "GPU does not support shader feature #%d: %s", capability, "extra shader inputs"); break;
    case 4437: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "multigpu");
    case 4439: lovrAssert(state.limits.renderViews > 1, "GPU does not support shader feature #%d: %s", capability, "multiview"); break;
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

static bool parseSpirv(Shader* shader, const void* source, uint32_t size, uint8_t stage) {
  const uint32_t* words = source;
  uint32_t wordCount = size / sizeof(uint32_t);

  if (wordCount < MIN_SPIRV_WORDS || words[0] != 0x07230203) {
    return false;
  }

  uint32_t bound = words[3];
  lovrAssert(bound <= 0xffff, "Unsupported Shader code: id bound is too big (max is 65535)");

  // The cache stores information for spirv ids, allowing everything to be parsed in a single pass
  // - For bundle variables, stores the slot's group, index, and name offset
  // - For vertex attributes, stores the attribute location decoration
  // - For types, stores the index of its declaration instruction
  // Need to study whether aliasing cache as u32 and struct var violates strict aliasing
  struct var { uint8_t group; uint8_t binding; uint16_t name; };
  size_t cacheSize = bound * sizeof(uint32_t);
  void* cache = malloc(cacheSize);
  lovrAssert(cache, "Out of memory");
  memset(cache, 0xff, cacheSize);
  struct var* vars = cache;
  uint32_t* locations = cache;
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
          vars[id].binding = value;
        } else if (decoration == 34) { // Group
          lovrAssert(value < 4, "Unsupported Shader code: variable %d is in group %d, but group must be less than 4", id, value);
          vars[id].group = value;
        } else if (decoration == 30) { // Location
          lovrAssert(value < 16, "Unsupported Shader code: vertex shader uses attribute location %d, but locations must be less than 16", value);
          locations[id] = value;
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

        // For vertex shaders, track which attribute locations are in use.
        // Vertex attributes are OpDecorated with Location and have an Input (1) StorageClass.
        uint32_t storageClass = instruction[3];
        if (stage == GPU_STAGE_VERTEX && storageClass == 1 && locations[id] < 16) {
          shader->locationMask |= (1 << locations[id]);
          break;
        }

        // Ignore inputs/outputs and variables that aren't decorated with a group and binding (e.g. global variables)
        if (storageClass == 1 || storageClass == 3 || vars[id].group == 0xff || vars[id].binding == 0xff) {
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
          if (len > 0) {
            uint64_t hash = hash64(name, len);
            uint64_t old = map_get(&shader->lookup, hash);
            uint64_t new = ((uint64_t) vars[id].binding << 32) | vars[id].group;
            if (old == MAP_NIL) {
              map_set(&shader->lookup, hash, new);
            } else {
              lovrAssert(old == new, "Unsupported Shader code: variable named '%s' is bound to 2 different locations", name);
            }
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
          group->slots[group->slotCount] = (gpu_slot) { slotId, type, stage, count };
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
  lovrAssert(shader, "Out of memory");
  shader->gpu = (gpu_shader*) (shader + 1);
  shader->info = *info;
  shader->ref = 1;
  map_init(&shader->lookup, 64);

  gpu_shader_info gpuInfo = {
    .stages[0] = { info->source[0], info->length[0], NULL },
    .stages[1] = { info->source[1], info->length[1], NULL },
    .label = info->label
  };

  // Perform reflection on the shader code to get slot info
  // This temporarily populates the slotIndex so variables can be merged between stages
  // After reflection and handling dynamic buffers, slots are sorted and lookup tables are generated
  if (info->type == SHADER_COMPUTE) {
    lovrAssert(info->source[0] && !info->source[1], "Compute shaders require one stage");
    parseSpirv(shader, info->source[0], info->length[0], GPU_STAGE_COMPUTE);
  } else {
    lovrAssert(info->source[0] && info->source[1], "Currently, graphics shaders require two stages");
    parseSpirv(shader, info->source[0], info->length[0], GPU_STAGE_VERTEX);
    parseSpirv(shader, info->source[1], info->length[1], GPU_STAGE_FRAGMENT);
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
  free(shader);
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

// Batch

Batch* lovrBatchCreate(BatchInfo* info) {
  Batch* batch = calloc(1, sizeof(Batch));
  lovrAssert(batch, "Out of memory");
  batch->info = *info;
  batch->ref = 1;
  return batch;
}

void lovrBatchDestroy(void* ref) {
  Batch* batch = ref;
  free(batch);
}

void lovrBatchReset(Batch* batch) {
  batch->transform = 0;
  lovrBatchOrigin(batch);
}

void lovrBatchPush(Batch* batch) {
  lovrAssert(++batch->transform < COUNTOF(batch->transforms), "Unbalanced matrix stack (more pushes than pops?)");
  mat4_init(batch->transforms[batch->transform], batch->transforms[batch->transform - 1]);
}

void lovrBatchPop(Batch* batch) {
  lovrAssert(--batch->transform < COUNTOF(batch->transforms), "Unbalanced matrix stack (more pops than pushes?)");
}

void lovrBatchOrigin(Batch* batch) {
  mat4_identity(batch->transforms[batch->transform]);
}

void lovrBatchTranslate(Batch* batch, vec3 translation) {
  mat4_translate(batch->transforms[batch->transform], translation[0], translation[1], translation[2]);
}

void lovrBatchRotate(Batch* batch, quat rotation) {
  mat4_rotateQuat(batch->transforms[batch->transform], rotation);
}

void lovrBatchScale(Batch* batch, vec3 scale) {
  mat4_scale(batch->transforms[batch->transform], scale[0], scale[1], scale[2]);
}

void lovrBatchTransform(Batch* batch, mat4 transform) {
  mat4_mul(batch->transforms[batch->transform], transform);
}

bool lovrBatchGetAlphaToCoverage(Batch* batch) {
  return batch->pipeline->alphaToCoverage;
}

void lovrBatchSetAlphaToCoverage(Batch* batch, bool enabled) {
  batch->pipeline->alphaToCoverage = enabled;
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

void lovrBatchGetBlendMode(Batch* batch, uint32_t target, BlendMode* mode, BlendAlphaMode* alphaMode) {
  gpu_blend_state* blend = &batch->pipeline->blend[target];

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

void lovrBatchSetBlendMode(Batch* batch, uint32_t target, BlendMode mode, BlendAlphaMode alphaMode) {
  if (mode == BLEND_NONE) {
    memset(&batch->pipeline->blend[target], 0, sizeof(gpu_blend_state));
    return;
  }

  batch->pipeline->blend[target] = blendModes[mode];
  if (alphaMode == BLEND_PREMULTIPLIED && mode != BLEND_MULTIPLY) {
    batch->pipeline->blend[target].color.src = GPU_BLEND_ONE;
  }
  batch->pipeline->blend[target].enabled = true;
}

void lovrBatchGetColorMask(Batch* batch, uint32_t target, bool* r, bool* g, bool* b, bool* a) {
  uint8_t mask = batch->pipeline->colorMask[target];
  *r = mask & 0x1;
  *g = mask & 0x2;
  *b = mask & 0x4;
  *a = mask & 0x8;
}

void lovrBatchSetColorMask(Batch* batch, uint32_t target, bool r, bool g, bool b, bool a) {
  batch->pipeline->colorMask[target] = (r << 0) | (g << 1) | (b << 2) | (a << 3);
}

CullMode lovrBatchGetCullMode(Batch* batch) {
  return (CullMode) batch->pipeline->rasterizer.cullMode;
}

void lovrBatchSetCullMode(Batch* batch, CullMode mode) {
  batch->pipeline->rasterizer.cullMode = (gpu_cull_mode) mode;
}

void lovrBatchGetDepthTest(Batch* batch, CompareMode* test, bool* write) {
  *test = (CompareMode) batch->pipeline->depth.test;
  *write = batch->pipeline->depth.write;
}

void lovrBatchSetDepthTest(Batch* batch, CompareMode test, bool write) {
  batch->pipeline->depth.test = (gpu_compare_mode) test;
  batch->pipeline->depth.write = write;
}

void lovrBatchGetDepthNudge(Batch* batch, float* nudge, float* sloped, float* clamp) {
  *nudge = batch->pipeline->rasterizer.depthOffset;
  *sloped = batch->pipeline->rasterizer.depthOffsetSloped;
  *clamp = batch->pipeline->rasterizer.depthOffsetClamp;
}

void lovrBatchSetDepthNudge(Batch* batch, float nudge, float sloped, float clamp) {
  batch->pipeline->rasterizer.depthOffset = nudge;
  batch->pipeline->rasterizer.depthOffsetSloped = sloped;
  batch->pipeline->rasterizer.depthOffsetClamp = clamp;
}

bool lovrBatchGetDepthClamp(Batch* batch) {
  return batch->pipeline->rasterizer.depthClamp;
}

void lovrBatchSetDepthClamp(Batch* batch, bool clamp) {
  batch->pipeline->rasterizer.depthClamp = clamp;
}

void lovrBatchGetStencilTest(Batch* batch, CompareMode* test, uint8_t* value) {
  switch (batch->pipeline->stencil.test) {
    case GPU_COMPARE_NONE: default: *test = COMPARE_NONE; break;
    case GPU_COMPARE_EQUAL: *test = COMPARE_EQUAL; break;
    case GPU_COMPARE_NEQUAL: *test = COMPARE_NEQUAL; break;
    case GPU_COMPARE_LESS: *test = COMPARE_GREATER; break;
    case GPU_COMPARE_LEQUAL: *test = COMPARE_GEQUAL; break;
    case GPU_COMPARE_GREATER: *test = COMPARE_LESS; break;
    case GPU_COMPARE_GEQUAL: *test = COMPARE_LEQUAL; break;
  }
  *value = batch->pipeline->stencil.value;
}

void lovrBatchSetStencilTest(Batch* batch, CompareMode test, uint8_t value) {
  switch (test) {
    case COMPARE_NONE: default: batch->pipeline->stencil.test = GPU_COMPARE_NONE; break;
    case COMPARE_EQUAL: batch->pipeline->stencil.test = GPU_COMPARE_EQUAL; break;
    case COMPARE_NEQUAL: batch->pipeline->stencil.test = GPU_COMPARE_NEQUAL; break;
    case COMPARE_LESS: batch->pipeline->stencil.test = GPU_COMPARE_GREATER; break;
    case COMPARE_LEQUAL: batch->pipeline->stencil.test = GPU_COMPARE_GEQUAL; break;
    case COMPARE_GREATER: batch->pipeline->stencil.test = GPU_COMPARE_LESS; break;
    case COMPARE_GEQUAL: batch->pipeline->stencil.test = GPU_COMPARE_LEQUAL; break;
  }
  batch->pipeline->stencil.value = value;
}

Shader* lovrBatchGetShader(Batch* batch) {
  return batch->shader;
}

void lovrBatchSetShader(Batch* batch, Shader* shader) {
  lovrAssert(!shader || shader->info.type == SHADER_GRAPHICS, "Compute shaders can not be used with setShader");
  if (shader == batch->shader) return;
  lovrRetain(shader);
  lovrRelease(batch->shader, lovrShaderDestroy);
  batch->shader = shader;
}

void lovrBatchGetVertexFormat(Batch* batch, VertexAttribute attributes[16], uint32_t* count) {
  memcpy(attributes, batch->attributes, batch->attributeCount * sizeof(VertexAttribute));
  *count = batch->attributeCount;
}

void lovrBatchSetVertexFormat(Batch* batch, VertexAttribute attributes[16], uint32_t count) {
  lovrAssert(count < state.limits.vertexAttributes, "Vertex attribute count must be less than limits.vertexAttributes (%d)", state.limits.vertexAttributes);

  for (uint32_t i = 0; i < count; i++) {
    lovrAssert(attributes[i].location < 16, "Vertex attribute location must be less than 16");
    lovrAssert(attributes[i].buffer < state.limits.vertexBuffers, "Vertex attribute buffer index must be less than limits.vertexBuffers (%d)", state.limits.vertexBuffers);
    lovrAssert(attributes[i].fieldType < FIELD_MAT2, "Matrix types can not be used as vertex attribute formats");
    lovrAssert(attributes[i].offset < state.limits.vertexAttributeOffset, "Vertex attribute byte offset must be less than limits.vertexAttributeOffset (%d)", state.limits.vertexAttributeOffset);
  }

  memcpy(batch->attributes, attributes, count * sizeof(VertexAttribute));
  batch->attributeCount = count;
}

Winding lovrBatchGetWinding(Batch* batch) {
  return (Winding) batch->pipeline->rasterizer.winding;
}

void lovrBatchSetWinding(Batch* batch, Winding winding) {
  batch->pipeline->rasterizer.winding = (gpu_winding) winding;
}

bool lovrBatchIsWireframe(Batch* batch) {
  return batch->pipeline->rasterizer.wireframe;
}

void lovrBatchSetWireframe(Batch* batch, bool wireframe) {
  batch->pipeline->rasterizer.wireframe = wireframe;
}
