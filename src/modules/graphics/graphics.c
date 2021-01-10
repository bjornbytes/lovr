#include "graphics/graphics.h"
#include "data/image.h"
#include "event/event.h"
#include "core/maf.h"
#include "core/map.h"
#include "core/gpu.h"
#include "core/os.h"
#include "core/util.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

struct Buffer {
  gpu_buffer* gpu;
  BufferInfo info;
  uint32_t access;
};

struct Texture {
  gpu_texture* gpu;
  TextureInfo info;
};

static LOVR_THREAD_LOCAL struct {
  uint32_t transform;
  float transforms[64][16];
  struct {
    gpu_pipeline_info info;
    bool dirty;
  } pipeline;
} thread;

static struct {
  bool initialized;
  bool debug;
  gpu_features features;
  gpu_limits limits;
  int width;
  int height;
  gpu_batch* batch;
  map_t passes;
} state;

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
  [FORMAT_RGB565] = GPU_TEXTURE_FORMAT_RGB565,
  [FORMAT_RGB5A1] = GPU_TEXTURE_FORMAT_RGB5A1,
  [FORMAT_RGB10A2] = GPU_TEXTURE_FORMAT_RGB10A2,
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
  map_init(&state.passes, 0);
  return false;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  for (uint32_t i = 0; i < state.passes.size; i++) {
    if (state.passes.values[i] != MAP_NIL) {
      gpu_pass* pass = (gpu_pass*) (uintptr_t) state.passes.values[i];
      gpu_pass_destroy(pass);
      free(pass);
    }
  }
  map_free(&state.passes);
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
  limits->renderSize[0] = state.limits.renderSize[0];
  limits->renderSize[1] = state.limits.renderSize[1];
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
  gpu_begin();
}

void lovrGraphicsFlush() {
  gpu_flush();
}

void lovrGraphicsRender(Canvas* canvas) {
  gpu_pass_info passInfo;
  gpu_render_info renderInfo;
  memset(&passInfo, 0, sizeof(passInfo));

  gpu_load_op loads[] = {
    [LOAD_KEEP] = GPU_LOAD_OP_LOAD,
    [LOAD_CLEAR] = GPU_LOAD_OP_CLEAR,
    [LOAD_DISCARD] = GPU_LOAD_OP_DISCARD
  };

  gpu_save_op saves[] = {
    [SAVE_KEEP] = GPU_SAVE_OP_SAVE,
    [SAVE_DISCARD] = GPU_SAVE_OP_DISCARD
  };

  for (uint32_t i = 0; i < 4; i++) {
    if (!canvas->color[i].texture && !canvas->color[i].resolve) {
      renderInfo.color[i].texture = NULL;
      break;
    }

    lovrAssert(canvas->color[i].texture, "TODO: Anonymous MSAA targets");

    passInfo.color[i].format = gpuTextureFormats[canvas->color[i].texture->info.format];
    passInfo.color[i].load = loads[canvas->color[i].load];
    passInfo.color[i].save = saves[canvas->color[i].save];
    passInfo.color[i].srgb = canvas->color[i].texture->info.srgb;

    renderInfo.color[i].texture = canvas->color[i].texture->gpu;
    renderInfo.color[i].resolve = canvas->color[i].resolve ? canvas->color[i].resolve->gpu : NULL;
    memcpy(renderInfo.color[i].clear, canvas->color[i].clear, 4 * sizeof(float));

    // TODO view must have a single mipmap
  }

  if (canvas->depth.enabled) {
    lovrAssert(canvas->depth.texture, "TODO: Anonymous depth targets");

    passInfo.depth.format = gpuTextureFormats[canvas->depth.texture->info.format];
    passInfo.depth.load = loads[canvas->depth.load];
    passInfo.depth.save = saves[canvas->depth.save];
    passInfo.depth.stencilLoad = loads[canvas->depth.stencil.load];
    passInfo.depth.stencilSave = saves[canvas->depth.stencil.save];

    renderInfo.depth.texture = canvas->depth.texture->gpu;
    renderInfo.depth.clear = canvas->depth.clear;
    renderInfo.depth.stencilClear = canvas->depth.stencil.clear;
  }

  TextureInfo* textureInfo = canvas->color[0].texture ? &canvas->color[0].texture->info : &canvas->depth.texture->info;
  passInfo.views = textureInfo->type == TEXTURE_ARRAY ? textureInfo->size[2] : 0;
  passInfo.samples = canvas->samples;

  uint64_t hash = hash64(&passInfo, sizeof(passInfo));
  uint64_t value = map_get(&state.passes, hash);

  if (value == MAP_NIL) {
    gpu_pass* pass = calloc(1, gpu_sizeof_pass());
    gpu_pass_init(pass, &passInfo);
    value = (uintptr_t) pass;
    map_set(&state.passes, hash, value);
  }

  renderInfo.pass = (gpu_pass*) (uintptr_t) value;
  renderInfo.size[0] = canvas->color[0].texture->info.size[0];
  renderInfo.size[1] = canvas->color[0].texture->info.size[1];
  state.batch = gpu_render(&renderInfo, NULL, 0);
}

void lovrGraphicsCompute() {
  state.batch = gpu_compute();
}

void lovrGraphicsEndPass() {
  gpu_batch_end(state.batch);
}

void lovrGraphicsGetDepthTest(CompareMode* test, bool* write) {
  *write = thread.pipeline.info.depthWrite;
  switch (thread.pipeline.info.depthTest) {
    case GPU_COMPARE_EQUAL: *test = COMPARE_EQUAL; break;
    case GPU_COMPARE_NEQUAL: *test = COMPARE_NEQUAL; break;
    case GPU_COMPARE_LESS: *test = COMPARE_LESS; break;
    case GPU_COMPARE_LEQUAL: *test = COMPARE_LEQUAL; break;
    case GPU_COMPARE_GREATER: *test = COMPARE_GREATER; break;
    case GPU_COMPARE_GEQUAL: *test = COMPARE_GEQUAL; break;
    case GPU_COMPARE_NONE: default: *test = COMPARE_NONE; break;
  }
}

void lovrGraphicsSetDepthTest(CompareMode test, bool write) {
  switch (test) {
    case COMPARE_EQUAL: thread.pipeline.info.depthTest = GPU_COMPARE_EQUAL; break;
    case COMPARE_NEQUAL: thread.pipeline.info.depthTest = GPU_COMPARE_NEQUAL; break;
    case COMPARE_LESS: thread.pipeline.info.depthTest = GPU_COMPARE_LESS; break;
    case COMPARE_LEQUAL: thread.pipeline.info.depthTest = GPU_COMPARE_LEQUAL; break;
    case COMPARE_GREATER: thread.pipeline.info.depthTest = GPU_COMPARE_GREATER; break;
    case COMPARE_GEQUAL: thread.pipeline.info.depthTest = GPU_COMPARE_GEQUAL; break;
    case COMPARE_NONE: default: thread.pipeline.info.depthTest = GPU_COMPARE_NONE; break;
  }
  thread.pipeline.info.depthWrite = write;
  thread.pipeline.dirty = true;
}

void lovrGraphicsGetDepthNudge(float* nudge, float* sloped, float* clamp) {
  *nudge = thread.pipeline.info.depthOffset;
  *sloped = thread.pipeline.info.depthOffsetSloped;
  *clamp = thread.pipeline.info.depthOffsetClamp;
}

void lovrGraphicsSetDepthNudge(float nudge, float sloped, float clamp) {
  thread.pipeline.info.depthOffset = nudge;
  thread.pipeline.info.depthOffsetSloped = sloped;
  thread.pipeline.info.depthOffsetClamp = clamp;
  thread.pipeline.dirty = true;
}

bool lovrGraphicsGetDepthClamp() {
  return thread.pipeline.info.depthClamp;
}

void lovrGraphicsSetDepthClamp(bool clamp) {
  thread.pipeline.info.depthClamp = clamp;
  thread.pipeline.dirty = true;
}

void lovrGraphicsPush() {
  lovrAssert(++thread.transform < COUNTOF(thread.transforms), "Unbalanced matrix stack (more pushes than pops?)");
  mat4_init(thread.transforms[thread.transform], thread.transforms[thread.transform - 1]);
}

void lovrGraphicsPop() {
  lovrAssert(--thread.transform < COUNTOF(thread.transforms), "Unbalanced matrix stack (more pops than pushes?)");
}

void lovrGraphicsOrigin() {
  mat4_identity(thread.transforms[thread.transform]);
}

void lovrGraphicsTranslate(vec3 translation) {
  mat4_translate(thread.transforms[thread.transform], translation[0], translation[1], translation[2]);
}

void lovrGraphicsRotate(quat rotation) {
  mat4_rotateQuat(thread.transforms[thread.transform], rotation);
}

void lovrGraphicsScale(vec3 scale) {
  mat4_scale(thread.transforms[thread.transform], scale[0], scale[1], scale[2]);
}

void lovrGraphicsMatrixTransform(mat4 transform) {
  mat4_mul(thread.transforms[thread.transform], transform);
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

  lovrAssert(gpu_buffer_init(buffer->gpu, &gpuInfo), "Could not create Buffer");
  return buffer;
}

void lovrBufferDestroy(void* ref) {
  Buffer* buffer = ref;
  gpu_buffer_destroy(buffer->gpu);
}

const BufferInfo* lovrBufferGetInfo(Buffer* buffer) {
  return &buffer->info;
}

#define WRITE_MASK (1 << GPU_WRITE_COLOR_TARGET) | (1 << GPU_WRITE_DEPTH_TARGET) | (1 << GPU_WRITE_COMPUTE_SHADER_STORAGE) | (1 << GPU_WRITE_UPLOAD)

void* lovrBufferMap(Buffer* buffer, uint32_t offset, uint32_t size) {
  return gpu_buffer_map(buffer->gpu, offset, size);
}

// Texture

static const gpu_texture_type gpuTextureTypes[] = {
  [TEXTURE_2D] = GPU_TEXTURE_TYPE_2D,
  [TEXTURE_CUBE] = GPU_TEXTURE_TYPE_CUBE,
  [TEXTURE_VOLUME] = GPU_TEXTURE_TYPE_3D,
  [TEXTURE_ARRAY] = GPU_TEXTURE_TYPE_ARRAY
};

Texture* lovrTextureCreate(TextureInfo* info) {
  Texture* texture = calloc(1, sizeof(Texture) + gpu_sizeof_texture());
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = *info;

  static const gpu_texture_usage gpuTextureUsages[] = {
    [TEXTURE_SAMPLE] = GPU_TEXTURE_USAGE_SAMPLE,
    [TEXTURE_RENDER] = GPU_TEXTURE_USAGE_RENDER,
    [TEXTURE_COMPUTE] = GPU_TEXTURE_USAGE_STORAGE,
    [TEXTURE_UPLOAD] = GPU_TEXTURE_USAGE_UPLOAD,
    [TEXTURE_DOWNLOAD] = GPU_TEXTURE_USAGE_DOWNLOAD
  };

  uint32_t usage = 0;
  for (uint32_t i = 0; i < sizeof(gpuTextureUsages) / sizeof(gpuTextureUsages[0]); i++) {
    if (info->usage & (1 << i)) {
      usage |= gpuTextureUsages[i];
    }
  }

  if (info->mipmaps == ~0u) {
    info->mipmaps = log2(MAX(MAX(info->size[0], info->size[1]), info->size[2])) + 1;
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

  lovrAssert(gpu_texture_init(texture->gpu, &gpuInfo), "Could not create Texture");
  return texture;
}

Texture* lovrTextureCreateView(TextureView* view) {
  Texture* texture = calloc(1, sizeof(Texture) + gpu_sizeof_texture());
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = view->source->info;
  texture->info.view = *view;

  gpu_texture_view_info gpuInfo = {
    .source = view->source->gpu,
    .type = gpuTextureTypes[view->type],
    .layerIndex = view->layerIndex,
    .layerCount = view->layerCount,
    .mipmapIndex = view->mipmapIndex,
    .mipmapCount = view->mipmapCount
  };

  lovrAssert(gpu_texture_init_view(texture->gpu, &gpuInfo), "Could not create Texture view");
  lovrRetain(view->source);
  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  gpu_texture_destroy(texture->gpu);
  if (texture->info.view.source) {
    lovrRelease(texture->info.view.source, lovrTextureDestroy);
  }
}

const TextureInfo* lovrTextureGetInfo(Texture* texture) {
  return &texture->info;
}

void lovrTextureGetPixels(Texture* texture, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t layer, uint32_t level, void (*callback)(void* data, uint64_t size, void* context), void* context) {
  uint16_t offset[4] = { x, y, layer, level };
  uint16_t extent[3] = { w, h, 1 };
  gpu_texture_read(texture->gpu, offset, extent, callback, context);
}
