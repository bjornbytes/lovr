#include "graphics/graphics.h"
#include "data/blob.h"
#include "event/event.h"
#include "core/maf.h"
#include "core/map.h"
#include "core/gpu.h"
#include "core/os.h"
#include "core/util.h"
#include "lib/tinycthread/tinycthread.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

struct Buffer {
  uint32_t ref;
  gpu_buffer* gpu;
  BufferInfo info;
};

struct Texture {
  uint32_t ref;
  gpu_texture* gpu;
  TextureInfo info;
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

static LOVR_THREAD_LOCAL struct {
  struct {
    gpu_pipeline_info info;
    gpu_pipeline* instance;
    uint64_t hash;
    bool dirty;
  } pipeline;
  Shader* shader;
  Bundle* bundles[4];
  uint32_t transform;
  float transforms[64][16];
  float viewMatrix[6][16];
  float projection[6][16];
} thread;

static struct {
  bool initialized;
  bool debug;
  int width;
  int height;
  gpu_features features;
  gpu_limits limits;
  gpu_batch* batch;
  map_t passes;
  map_t pipelines;
  map_t pipelineLobby;
  mtx_t pipelineLock;
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
  for (uint32_t i = 0; i < state.passes.size; i++) {
    if (state.passes.values[i] != MAP_NIL) {
      gpu_pass* pass = (gpu_pass*) (uintptr_t) state.passes.values[i];
      gpu_pass_destroy(pass);
      free(pass);
    }
  }
  map_free(&state.passes);
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
  map_init(&state.passes, 0);
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

  // TODO flush pipeline lobby
}

void lovrGraphicsRender(Canvas* canvas) {
  gpu_pass_info info;
  gpu_render_target target;
  memset(&info, 0, sizeof(info));

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
      target.color[i].texture = NULL;
      break;
    }

    lovrAssert(canvas->color[i].texture, "TODO: Anonymous MSAA targets");

    info.color[i].format = (gpu_texture_format) canvas->color[i].texture->info.format;
    info.color[i].load = loads[canvas->color[i].load];
    info.color[i].save = saves[canvas->color[i].save];
    info.color[i].srgb = canvas->color[i].texture->info.srgb;

    target.color[i].texture = canvas->color[i].texture->gpu;
    target.color[i].resolve = canvas->color[i].resolve ? canvas->color[i].resolve->gpu : NULL;
    memcpy(target.color[i].clear, canvas->color[i].clear, 4 * sizeof(float));

    // TODO view must have a single mipmap
  }

  if (canvas->depth.enabled) {
    lovrAssert(canvas->depth.texture, "TODO: Anonymous depth targets");

    info.depth.format = (gpu_texture_format) canvas->depth.texture->info.format;
    info.depth.load = loads[canvas->depth.load];
    info.depth.save = saves[canvas->depth.save];
    info.depth.stencilLoad = loads[canvas->depth.stencil.load];
    info.depth.stencilSave = saves[canvas->depth.stencil.save];

    target.depth.texture = canvas->depth.texture->gpu;
    target.depth.clear = canvas->depth.clear;
    target.depth.stencilClear = canvas->depth.stencil.clear;
  }

  TextureInfo* textureInfo = canvas->color[0].texture ? &canvas->color[0].texture->info : &canvas->depth.texture->info;
  info.views = textureInfo->type == TEXTURE_ARRAY ? textureInfo->size[2] : 0;
  info.samples = canvas->samples;

  uint64_t hash = hash64(&info, sizeof(info));
  uint64_t value = map_get(&state.passes, hash);

  // TODO better allocator
  // TODO eviction
  if (value == MAP_NIL) {
    gpu_pass* pass = calloc(1, gpu_sizeof_pass());
    lovrAssert(pass, "Out of memory");
    gpu_pass_init(pass, &info);
    value = (uintptr_t) pass;
    map_set(&state.passes, hash, value);
  }

  gpu_pass* pass = (gpu_pass*) (uintptr_t) value;
  target.size[0] = canvas->color[0].texture->info.size[0];
  target.size[1] = canvas->color[0].texture->info.size[1];
  state.batch = gpu_batch_init_render(pass, &target, NULL, 0);

  // TODO reset pipeline state
  memset(thread.bundles, 0, sizeof(thread.bundles));
}

void lovrGraphicsEndPass() {
  gpu_batch_end(state.batch);
}

void lovrGraphicsBind(uint32_t group, Bundle* bundle) {
  lovrAssert(group < 4, "Attempt to bind a bundle to group %d, but the max group is 3", group);
  thread.bundles[group] = bundle;
}

/*
static gpu_pipeline* resolvePipeline() {

  // If the pipeline info hasn't changed, and a pipeline is already bound, just use that
  if (thread.pipeline.instance && !thread.pipeline.dirty) {
    return thread.pipeline.instance;
  }

  uint64_t hash = hash64(&thread.pipeline.info, sizeof(gpu_pipeline_info));

  // If the pipeline info is the same as the one already bound, just use the existing pipeline
  if (thread.pipeline.instance && thread.pipeline.hash == hash) {
    return thread.pipeline.instance;
  }

  // If there is already an existing pipeline with the matching info, use that
  uint64_t value = map_get(&state.pipelines, hash);
  if (value != MAP_NIL) {
    return (gpu_pipeline*) (uintptr_t) value;
  }

  // Otherwise, check the lobby
  mtx_lock(&state.pipelineLock);
  value = map_get(&state.pipelineLobby, hash);
  if (value != MAP_NIL) {
    mtx_unlock(&state.pipelineLock);
    return (gpu_pipeline*) (uintptr_t) value;
  }

  // If it's not in the lobby, add it
  // TODO better allocator
  // TODO eviction
  gpu_pipeline* pipeline = calloc(1, gpu_sizeof_pipeline());
  lovrAssert(pipeline, "Out of memory");
  gpu_pipeline_init_graphics(pipeline, &thread.pipeline.info);
  value = (uintptr_t) pipeline;
  map_set(&state.pipelineLobby, hash, value);
  mtx_unlock(&state.pipelineLock);
  return pipeline;
}
*/

bool lovrGraphicsGetAlphaToCoverage() {
  return thread.pipeline.info.alphaToCoverage;
}

void lovrGraphicsSetAlphaToCoverage(bool enabled) {
  thread.pipeline.info.alphaToCoverage = enabled;
  thread.pipeline.dirty = true;
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

void lovrGraphicsGetBlendMode(uint32_t target, BlendMode* mode, BlendAlphaMode* alphaMode) {
  gpu_blend_state* blend = &thread.pipeline.info.blend[target];

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

void lovrGraphicsSetBlendMode(uint32_t target, BlendMode mode, BlendAlphaMode alphaMode) {
  if (mode == BLEND_NONE) {
    memset(&thread.pipeline.info.blend[target], 0, sizeof(gpu_blend_state));
    return;
  }

  thread.pipeline.info.blend[target] = blendModes[mode];
  if (alphaMode == BLEND_PREMULTIPLIED && mode != BLEND_MULTIPLY) {
    thread.pipeline.info.blend[target].color.src = GPU_BLEND_ONE;
  }
  thread.pipeline.info.blend[target].enabled = true;
  thread.pipeline.dirty = true;
}

void lovrGraphicsGetColorMask(uint32_t target, bool* r, bool* g, bool* b, bool* a) {
  uint8_t mask = thread.pipeline.info.colorMask[target];
  *r = mask & 0x1;
  *g = mask & 0x2;
  *b = mask & 0x4;
  *a = mask & 0x8;
}

void lovrGraphicsSetColorMask(uint32_t target, bool r, bool g, bool b, bool a) {
  thread.pipeline.info.colorMask[target] = (r << 0) | (g << 1) | (b << 2) | (a << 3);
  thread.pipeline.dirty = true;
}

CullMode lovrGraphicsGetCullMode() {
  return (CullMode) thread.pipeline.info.rasterizer.cullMode;
}

void lovrGraphicsSetCullMode(CullMode mode) {
  thread.pipeline.info.rasterizer.cullMode = (gpu_cull_mode) mode;
  thread.pipeline.dirty = true;
}

void lovrGraphicsGetDepthTest(CompareMode* test, bool* write) {
  *test = (CompareMode) thread.pipeline.info.depth.test;
  *write = thread.pipeline.info.depth.write;
}

void lovrGraphicsSetDepthTest(CompareMode test, bool write) {
  thread.pipeline.info.depth.test = (gpu_compare_mode) test;
  thread.pipeline.info.depth.write = write;
  thread.pipeline.dirty = true;
}

void lovrGraphicsGetDepthNudge(float* nudge, float* sloped, float* clamp) {
  *nudge = thread.pipeline.info.rasterizer.depthOffset;
  *sloped = thread.pipeline.info.rasterizer.depthOffsetSloped;
  *clamp = thread.pipeline.info.rasterizer.depthOffsetClamp;
}

void lovrGraphicsSetDepthNudge(float nudge, float sloped, float clamp) {
  thread.pipeline.info.rasterizer.depthOffset = nudge;
  thread.pipeline.info.rasterizer.depthOffsetSloped = sloped;
  thread.pipeline.info.rasterizer.depthOffsetClamp = clamp;
  thread.pipeline.dirty = true;
}

bool lovrGraphicsGetDepthClamp() {
  return thread.pipeline.info.rasterizer.depthClamp;
}

void lovrGraphicsSetDepthClamp(bool clamp) {
  thread.pipeline.info.rasterizer.depthClamp = clamp;
  thread.pipeline.dirty = true;
}

void lovrGraphicsGetStencilTest(CompareMode* test, uint8_t* value) {
  switch (thread.pipeline.info.stencil.test) {
    case GPU_COMPARE_NONE: default: *test = COMPARE_NONE; break;
    case GPU_COMPARE_EQUAL: *test = COMPARE_EQUAL; break;
    case GPU_COMPARE_NEQUAL: *test = COMPARE_NEQUAL; break;
    case GPU_COMPARE_LESS: *test = COMPARE_GREATER; break;
    case GPU_COMPARE_LEQUAL: *test = COMPARE_GEQUAL; break;
    case GPU_COMPARE_GREATER: *test = COMPARE_LESS; break;
    case GPU_COMPARE_GEQUAL: *test = COMPARE_LEQUAL; break;
  }
  *value = thread.pipeline.info.stencil.value;
}

void lovrGraphicsSetStencilTest(CompareMode test, uint8_t value) {
  switch (test) {
    case COMPARE_NONE: default: thread.pipeline.info.stencil.test = GPU_COMPARE_NONE; break;
    case COMPARE_EQUAL: thread.pipeline.info.stencil.test = GPU_COMPARE_EQUAL; break;
    case COMPARE_NEQUAL: thread.pipeline.info.stencil.test = GPU_COMPARE_NEQUAL; break;
    case COMPARE_LESS: thread.pipeline.info.stencil.test = GPU_COMPARE_GREATER; break;
    case COMPARE_LEQUAL: thread.pipeline.info.stencil.test = GPU_COMPARE_GEQUAL; break;
    case COMPARE_GREATER: thread.pipeline.info.stencil.test = GPU_COMPARE_LESS; break;
    case COMPARE_GEQUAL: thread.pipeline.info.stencil.test = GPU_COMPARE_LEQUAL; break;
  }
  thread.pipeline.info.stencil.value = value;
  thread.pipeline.dirty = true;
}

Shader* lovrGraphicsGetShader() {
  return thread.shader;
}

void lovrGraphicsSetShader(Shader* shader) {
  lovrAssert(!shader || shader->info.type == SHADER_GRAPHICS, "Compute shaders can not be used with setShader");
  if (shader == thread.shader) return;
  lovrRetain(shader);
  lovrRelease(thread.shader, lovrShaderDestroy);
  thread.shader = shader;
  thread.pipeline.info.shader = shader->gpu;
  thread.pipeline.dirty = true;
}

Winding lovrGraphicsGetWinding() {
  return (Winding) thread.pipeline.info.rasterizer.winding;
}

void lovrGraphicsSetWinding(Winding winding) {
  thread.pipeline.info.rasterizer.winding = (gpu_winding) winding;
  thread.pipeline.dirty = true;
}

bool lovrGraphicsIsWireframe() {
  return thread.pipeline.info.rasterizer.wireframe;
}

void lovrGraphicsSetWireframe(bool wireframe) {
  thread.pipeline.info.rasterizer.wireframe = wireframe;
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

void lovrGraphicsTransform(mat4 transform) {
  mat4_mul(thread.transforms[thread.transform], transform);
}

void lovrGraphicsGetViewMatrix(uint32_t index, float* viewMatrix) {
  lovrAssert(index < COUNTOF(thread.viewMatrix), "Invalid view index %d", index);
  mat4_init(viewMatrix, thread.viewMatrix[index]);
}

void lovrGraphicsSetViewMatrix(uint32_t index, float* viewMatrix) {
  lovrAssert(index < COUNTOF(thread.viewMatrix), "Invalid view index %d", index);
  mat4_init(thread.viewMatrix[index], viewMatrix);
}

void lovrGraphicsGetProjection(uint32_t index, float* projection) {
  lovrAssert(index < COUNTOF(thread.projection), "Invalid view index %d", index);
  mat4_init(projection, thread.projection[index]);
}

void lovrGraphicsSetProjection(uint32_t index, float* projection) {
  lovrAssert(index < COUNTOF(thread.projection), "Invalid view index %d", index);
  mat4_init(thread.projection[index], projection);
}

void lovrGraphicsStencil(StencilAction action, StencilAction depthFailAction, uint8_t value, StencilCallback* callback, void* userdata) {
  uint8_t oldValue = thread.pipeline.info.stencil.value;
  gpu_compare_mode oldTest = thread.pipeline.info.stencil.test;
  // TODO must be in render pass
  // TODO must have stencil buffer?
  thread.pipeline.info.stencil.test = GPU_COMPARE_NONE;
  thread.pipeline.info.stencil.passOp = (gpu_stencil_op) action;
  thread.pipeline.info.stencil.depthFailOp = (gpu_stencil_op) depthFailAction;
  thread.pipeline.info.stencil.value = value;
  thread.pipeline.dirty = true;
  callback(userdata);
  thread.pipeline.info.stencil.test = oldTest;
  thread.pipeline.info.stencil.passOp = GPU_STENCIL_KEEP;
  thread.pipeline.info.stencil.depthFailOp = GPU_STENCIL_KEEP;
  thread.pipeline.info.stencil.value = oldValue;
  thread.pipeline.dirty = true;
}

// Buffer

Buffer* lovrBufferCreate(BufferInfo* info) {
  lovrAssert(info->type == BUFFER_STATIC || ~info->usage & (1 << BUFFER_COMPUTE), "Only static buffers can have the 'compute' usage flag");
  lovrAssert(info->type == BUFFER_STATIC || ~info->usage & (1 << BUFFER_COPY_DST), "Only static buffers can have the 'copyto' usage flag");
  Buffer* buffer = calloc(1, sizeof(Buffer) + gpu_sizeof_buffer());
  buffer->gpu = (gpu_buffer*) (buffer + 1);
  buffer->info = *info;
  buffer->ref = 1;

  gpu_buffer_info gpuInfo = {
    .size = ALIGN(info->size, 4),
    .type = (gpu_buffer_type) info->type,
    .usage = info->usage,
    .mapping = info->mapping,
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

void* lovrBufferMap(Buffer* buffer) {
  lovrAssert(buffer->info.type != BUFFER_STATIC, "Static Buffers can not be mapped");
  return gpu_buffer_map(buffer->gpu);
}

// Texture

Texture* lovrTextureCreate(TextureInfo* info) {
  Texture* texture = calloc(1, sizeof(Texture) + gpu_sizeof_texture());
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = *info;
  texture->ref = 1;

  if (info->mipmaps == ~0u) {
    info->mipmaps = log2(MAX(MAX(info->size[0], info->size[1]), info->size[2])) + 1;
  }

  gpu_texture_info gpuInfo = {
    .type = (gpu_texture_type) info->type,
    .format = (gpu_texture_format) info->format,
    .size[0] = info->size[0],
    .size[1] = info->size[1],
    .size[2] = info->size[2],
    .mipmaps = info->mipmaps,
    .samples = info->samples,
    .usage = info->usage,
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
  texture->ref = 1;

  gpu_texture_view_info gpuInfo = {
    .source = view->source->gpu,
    .type = (gpu_texture_type) view->type,
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
  Shader* shader = calloc(1, sizeof(Shader) + gpu_sizeof_shader());
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
  return shader;
}

void lovrShaderDestroy(void* ref) {
  Shader* shader = ref;
  gpu_shader_destroy(shader->gpu);
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

/*
static void resolveBindings() {
  Shader* shader = pass->shader;

  for (uint32_t i = 0; i < 4; i++) {
    ShaderGroup* group = &shader->groups[i];

    if (!group->slotMask) continue;

    Bundle* bundle = pass->bundles[i];
    lovrAssert(bundle, "Missing bundle for group %d", i); // UGH
    lovrAssert(bundle->group->hash == group->hash, "Incompatible bundle bound for group %d", i);

    bool rebind = false;

    if (pass->dirtyBundles & (1 << i)) {
      pass->dirtyBundles &= ~(1 << i);
      rebind = true;
    }

    if (bundle->dirty) {
      gpu_bundle_destroy(bundle->gpu);
      gpu_bundle_init(bundle->gpu, &(gpu_bundle_info) {
        .shader = shader,
        .group = i,
        .slotCount = group->slotCount,
        .slots = group->slots,
        .bindings = bundle->bindings
      });

      bundle->dirty = false;
      bundle->version++;
      rebind = true;
    }

    if (pass->bundleVersions[i] != bundle->version) {
      pass->bundleVersions[i] = bundle->version;
      rebind = true;
    }

    if (rebind) {
      gpu_batch_bind_bundle(pass->batch, shader, i, bundle->gpu, bundle->dynamicOffsets, group->dynamicOffsetCount);
    }
  }
}
*/
