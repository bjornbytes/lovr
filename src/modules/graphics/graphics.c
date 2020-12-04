#include "graphics/graphics.h"
#include "data/image.h"
#include "event/event.h"
#include "core/gpu.h"
#include "core/os.h"
#include "core/util.h"
#include <string.h>
#include <stdlib.h>

struct Buffer {
  gpu_buffer* gpu;
  BufferInfo info;
};

struct Texture {
  gpu_texture* gpu;
  TextureInfo info;
};

static struct {
  bool initialized;
  bool debug;
  gpu_features features;
  gpu_limits limits;
  int width;
  int height;
} state;

void onDebugMessage(void* context, const char* message, int severe) {
  lovrLog(severe ? LOG_ERROR : LOG_DEBUG, "GPU", message);
}

void onQuitRequest() {
  lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit = { .exitCode = 0 } });
}

void onResizeWindow(int width, int height) {
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
  gpu_thread_detach();
  gpu_destroy();
  memset(&state, 0, sizeof(state));
}

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
    .vk.surface = true,
    .vk.vsync = window->vsync,
#ifdef LOVR_VK
    .vk.getExtraInstanceExtensions = os_vk_get_instance_extensions,
    .vk.createSurface = os_vk_create_surface
#endif
  };

  lovrAssert(gpu_init(&config), "Could not initialize GPU");
  gpu_thread_attach();
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
  features->anisotropy = state.features.anisotropy;
  features->clipDistance = state.features.clipDistance;
  features->cullDistance = state.features.cullDistance;
  features->fullIndexBufferRange = state.features.fullIndexBufferRange;
  features->indirectDrawCount = state.features.indirectDrawCount;
  features->indirectDrawFirstInstance = state.features.indirectDrawFirstInstance;
  features->extraShaderInputs = state.features.extraShaderInputs;
  features->multiview = state.features.multiview;
}

void lovrGraphicsGetLimits(GraphicsLimits* limits) {
  limits->textureSize2D = state.limits.textureSize2D;
  limits->textureSize3D = state.limits.textureSize3D;
  limits->textureSizeCube = state.limits.textureSizeCube;
  limits->textureLayers = state.limits.textureLayers;
  limits->canvasSize[0] = state.limits.canvasSize[0];
  limits->canvasSize[1] = state.limits.canvasSize[1];
  limits->canvasViews = state.limits.canvasViews;
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
  gpu_begin();
}

void lovrGraphicsFlush() {
  gpu_flush();
}

// Buffer

Buffer* lovrBufferCreate(BufferInfo* info) {
  Buffer* buffer = calloc(1, sizeof(Buffer) + gpu_sizeof_buffer());
  buffer->gpu = (gpu_buffer*) (buffer + 1);
  buffer->info = *info;

  gpu_buffer_usage gpuBufferUsage[] = {
    [BUFFER_VERTEX] = GPU_BUFFER_USAGE_VERTEX,
    [BUFFER_INDEX] = GPU_BUFFER_USAGE_INDEX,
    [BUFFER_UNIFORM] = GPU_BUFFER_USAGE_UNIFORM,
    [BUFFER_COMPUTE] = GPU_BUFFER_USAGE_STORAGE,
    [BUFFER_ARGUMENT] = GPU_BUFFER_USAGE_INDIRECT,
    [BUFFER_UPLOAD] = GPU_BUFFER_USAGE_UPLOAD,
    [BUFFER_DOWNLOAD] = GPU_BUFFER_USAGE_DOWNLOAD
  };

  uint32_t usage = 0;
  for (uint32_t i = 0; i < sizeof(gpuBufferUsage) / sizeof(gpuBufferUsage[0]); i++) {
    if (info->usage & (1 << i)) {
      usage |= gpuBufferUsage[i];
    }
  }

  gpu_buffer_info gpuInfo = {
    .size = info->size,
    .usage = usage,
    .label = info->label
  };

  if (!gpu_buffer_init(buffer->gpu, &gpuInfo)) {
    lovrRelease(buffer, lovrBufferDestroy);
    return NULL;
  }

  return buffer;
}

void lovrBufferDestroy(void* ref) {
  Buffer* buffer = ref;
  gpu_buffer_destroy(buffer->gpu);
}

const BufferInfo* lovrBufferGetInfo(Buffer* buffer) {
  return &buffer->info;
}

// Texture

Texture* lovrTextureCreate(TextureInfo* info) {
  Texture* texture = calloc(1, sizeof(Texture) + gpu_sizeof_texture());
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = *info;

  static const gpu_texture_type gpuTextureTypes[] = {
    [TEXTURE_2D] = GPU_TEXTURE_TYPE_2D,
    [TEXTURE_CUBE] = GPU_TEXTURE_TYPE_CUBE,
    [TEXTURE_VOLUME] = GPU_TEXTURE_TYPE_3D,
    [TEXTURE_ARRAY] = GPU_TEXTURE_TYPE_ARRAY
  };

  static const gpu_texture_format gpuTextureFormats[] = {
    [FORMAT_R8] = GPU_TEXTURE_FORMAT_R8,
    [FORMAT_RG8] = GPU_TEXTURE_FORMAT_RG8,
    [FORMAT_RGBA8] = GPU_TEXTURE_FORMAT_RGBA8,
    [FORMAT_R16] = GPU_TEXTURE_FORMAT_R16,
    [FORMAT_RG16] = GPU_TEXTURE_FORMAT_RG16,
    [FORMAT_RGBA16] = GPU_TEXTURE_FORMAT_RGBA16,
    [FORMAT_R16F] = GPU_TEXTURE_FORMAT_R16F,
    [FORMAT_RG16F] = GPU_TEXTURE_FORMAT_RG16F,
    [FORMAT_RGBA16F] = GPU_TEXTURE_FORMAT_RGBA16F,
    [FORMAT_R32F] = GPU_TEXTURE_FORMAT_R32F,
    [FORMAT_RG32F] = GPU_TEXTURE_FORMAT_RG32F,
    [FORMAT_RGBA32F] = GPU_TEXTURE_FORMAT_RGBA32F,
    [FORMAT_RG11B10F] = GPU_TEXTURE_FORMAT_RG11B10F,
    [FORMAT_D16] = GPU_TEXTURE_FORMAT_D16,
    [FORMAT_D24S8] = GPU_TEXTURE_FORMAT_D24S8,
    [FORMAT_D32F] = GPU_TEXTURE_FORMAT_D32F,
    [FORMAT_BC6] = GPU_TEXTURE_FORMAT_BC6,
    [FORMAT_BC7] = GPU_TEXTURE_FORMAT_BC7,
    [FORMAT_ASTC_4x4] = GPU_TEXTURE_FORMAT_ASTC_4x4,
    [FORMAT_ASTC_5x4] = GPU_TEXTURE_FORMAT_ASTC_5x4,
    [FORMAT_ASTC_5x5] = GPU_TEXTURE_FORMAT_ASTC_5x5,
    [FORMAT_ASTC_6x5] = GPU_TEXTURE_FORMAT_ASTC_6x5,
    [FORMAT_ASTC_6x6] = GPU_TEXTURE_FORMAT_ASTC_6x6,
    [FORMAT_ASTC_8x5] = GPU_TEXTURE_FORMAT_ASTC_8x5,
    [FORMAT_ASTC_8x6] = GPU_TEXTURE_FORMAT_ASTC_8x6,
    [FORMAT_ASTC_8x8] = GPU_TEXTURE_FORMAT_ASTC_8x8,
    [FORMAT_ASTC_10x5] = GPU_TEXTURE_FORMAT_ASTC_10x5,
    [FORMAT_ASTC_10x6] = GPU_TEXTURE_FORMAT_ASTC_10x6,
    [FORMAT_ASTC_10x8] = GPU_TEXTURE_FORMAT_ASTC_10x8,
    [FORMAT_ASTC_10x10] = GPU_TEXTURE_FORMAT_ASTC_10x10,
    [FORMAT_ASTC_12x10] = GPU_TEXTURE_FORMAT_ASTC_12x10,
    [FORMAT_ASTC_12x12] = GPU_TEXTURE_FORMAT_ASTC_12x12
  };

  static const gpu_texture_usage gpuTextureUsages[] = {
    [TEXTURE_SAMPLE] = GPU_TEXTURE_USAGE_SAMPLE,
    [TEXTURE_RENDER] = GPU_TEXTURE_USAGE_RENDER,
    [TEXTURE_COMPUTE] = GPU_TEXTURE_USAGE_STORAGE,
    [TEXTURE_UPLOAD] = GPU_TEXTURE_USAGE_PASTE,
    [TEXTURE_DOWNLOAD] = GPU_TEXTURE_USAGE_COPY
  };

  uint32_t usage = 0;
  for (uint32_t i = 0; i < sizeof(gpuTextureUsages) / sizeof(gpuTextureUsages[0]); i++) {
    if (info->usage & (1 << i)) {
      usage |= gpuTextureUsages[i];
    }
  }

  gpu_texture_info gpuInfo = {
    .type = gpuTextureTypes[info->type],
    .format = gpuTextureFormats[info->format],
    .size[0] = info->size[0],
    .size[1] = info->size[1],
    .size[2] = info->size[2],
    .mipmaps = info->mipmaps,
    .samples = info->samples,
    .usage = usage,
    .srgb = info->srgb,
    .label = info->label
  };

  if (!gpu_texture_init(texture->gpu, &gpuInfo)) {
    lovrRelease(texture, lovrTextureDestroy);
    return NULL;
  }

  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  gpu_texture_destroy(texture->gpu);
}
