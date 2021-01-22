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
  gpu_buffer* gpu;
  BufferInfo info;
};

struct Texture {
  gpu_texture* gpu;
  TextureInfo info;
};

struct Sampler {
  gpu_sampler* gpu;
  SamplerInfo info;
};

typedef struct {
  uint64_t hash;
  uint8_t baseSlot;
  uint8_t slotCount;
  uint8_t slotMap[16];
  uint32_t baseBinding;
  uint32_t bindingCount;
  uint32_t bindingMap[16];
  uint8_t baseDynamicOffset;
  uint8_t dynamicOffsetCount;
  uint8_t dynamicOffsetMap[16];
} ShaderGroup;

struct Shader {
  gpu_shader* gpu;
  ShaderInfo info;
  map_t lookup;
  gpu_slot slots[64];
  uint32_t slotCount;
  uint32_t bindingCount;
  uint32_t dynamicOffsetCount;
  ShaderGroup groups[4];
};

static LOVR_THREAD_LOCAL struct {
  struct {
    gpu_pipeline_info info;
    gpu_pipeline* instance;
    uint64_t hash;
    bool dirty;
  } pipeline;
  Shader* shader;
  gpu_binding* bindings;
  uint32_t bindingCount;
  uint32_t dynamicOffsets[64];
  uint32_t dirtyGroups;
  uint32_t filthyGroups;
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
  Sampler* defaultSamplers[MAX_DEFAULT_SAMPLERS];
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
  for (uint32_t i = 0; i < MAX_DEFAULT_SAMPLERS; i++) {
    lovrRelease(state.defaultSamplers[i], lovrSamplerDestroy);
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

    passInfo.color[i].format = (gpu_texture_format) canvas->color[i].texture->info.format;
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

    passInfo.depth.format = (gpu_texture_format) canvas->depth.texture->info.format;
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

  // TODO better allocator
  // TODO eviction
  if (value == MAP_NIL) {
    gpu_pass* pass = calloc(1, gpu_sizeof_pass());
    lovrAssert(pass, "Out of memory");
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

static void checkSlot(Shader* shader, uint32_t group, uint32_t index, uint32_t element, gpu_slot** slot, gpu_binding** binding) {
  lovrAssert(group < 4, "Attempt to bind a resource to group %d (max group is 3)", group);
  lovrAssert(index < 16, "Attempt to bind a resource to index %d (max index is 15)", index);
  uint8_t slotIndex = shader->groups[group].slotMap[index];
  *slot = &shader->slots[slotIndex];
  lovrAssert(slotIndex != 0xff, "Attempt to bind a resource to (%d,%d), but the active Shader does not have a resource there", group, index);
  lovrAssert(element < (*slot)->count, "Attempt to bind a resource to array element %d of resource (%d,%d), which exceeds its array bounds", element, group, index);
  uint32_t bindingIndex = shader->groups[group].bindingMap[index] + element;
  *binding = &thread.bindings[bindingIndex];
}

void lovrGraphicsBindBuffer(uint32_t group, uint32_t index, uint32_t element, Buffer* buffer, uint32_t offset, uint32_t extent) {
  gpu_slot* slot;
  gpu_binding* binding;
  Shader* shader = thread.shader;
  checkSlot(shader, group, index, element, &slot, &binding);
  lovrAssert(slot->type != GPU_SLOT_SAMPLED_TEXTURE && slot->type != GPU_SLOT_STORAGE_TEXTURE, "Attempt to bind a Buffer to slot (%d,%d), but only Textures can be bound here");

  if (slot->type == GPU_SLOT_UNIFORM_BUFFER_DYNAMIC || slot->type == GPU_SLOT_STORAGE_BUFFER_DYNAMIC) {
    uint8_t dynamicOffsetIndex = shader->groups[group].dynamicOffsetMap[index];
    thread.dynamicOffsets[dynamicOffsetIndex] = offset;
    offset = 0;

    if (buffer->gpu == binding->buffer.object && extent == binding->buffer.extent) {
      // TODO need to mark this set as dirty differently somehow (need to rebind, but don't need to recreate bundle)
      return;
    }
  }

  binding->buffer = (gpu_buffer_binding) { buffer->gpu, offset, extent };
  thread.dirtyGroups |= 1 << group;
  //lovrRetain(buffer)
}

void lovrGraphicsBindTexture(uint32_t group, uint32_t index, uint32_t element, Texture* texture, Sampler* sampler) {
  gpu_slot* slot;
  gpu_binding* binding;
  Shader* shader = thread.shader;
  checkSlot(shader, group, index, element, &slot, &binding);
  lovrAssert(slot->type == GPU_SLOT_SAMPLED_TEXTURE || slot->type == GPU_SLOT_STORAGE_TEXTURE, "Attempt to bind a Texture to slot (%d,%d), but only Buffers can be bound here");
  binding->texture = (gpu_texture_binding) { texture->gpu, sampler->gpu };
  thread.dirtyGroups |= 1 << group;
  //lovrRetain(texture)
}

Sampler* lovrGraphicsGetDefaultSampler(DefaultSampler type) {
  if (!state.defaultSamplers[type]) {
    SamplerInfo info = {
      .min = (type == SAMPLER_NEAREST) ? FILTER_NEAREST : FILTER_LINEAR,
      .mag = (type == SAMPLER_NEAREST) ? FILTER_NEAREST : FILTER_LINEAR,
      .mip = (type == SAMPLER_TRILINEAR || type == SAMPLER_ANISOTROPIC) ? FILTER_LINEAR : FILTER_NEAREST,
      .anisotropy = (type == SAMPLER_ANISOTROPIC) ? state.limits.anisotropy : 0.f
    };

    // TODO thread safety (just have to prevent a retain/release "leak" with cmpxchg or something)
    state.defaultSamplers[type] = lovrSamplerCreate(&info);
  }

  return state.defaultSamplers[type];
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

  // Resize bindings array if needed
  if (shader->bindingCount > thread.bindingCount) {
    thread.bindings = realloc(thread.bindings, shader->bindingCount * sizeof(gpu_binding));
    lovrAssert(thread.bindings, "Out of memory");
    memset(thread.bindings + thread.bindingCount, 0, (shader->bindingCount - thread.bindingCount) * sizeof(gpu_binding));
    thread.bindingCount = shader->bindingCount;
  }

  // Clear bindings starting at the first group that has a conflicting slot (if any)
  for (uint32_t i = 0; i < 4; i++) {
    if (shader->groups[i].hash != thread.shader->groups[i].hash) {
      // TODO lovrRelease any bound Buffers/Textures before clearing
      uint32_t base = shader->groups[i].baseBinding;
      uint32_t tail = MAX(shader->bindingCount, thread.shader->bindingCount) - base;
      memset(thread.bindings + base, 0, tail * sizeof(gpu_binding));
      thread.dirtyGroups |= ~((1 << (i + 1)) - 1) & 0xf; // Mark all cleared groups as dirty
      break;
    }
  }

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

// Buffer

Buffer* lovrBufferCreate(BufferInfo* info) {
  Buffer* buffer = calloc(1, sizeof(Buffer) + gpu_sizeof_buffer());
  buffer->gpu = (gpu_buffer*) (buffer + 1);
  buffer->info = *info;

  gpu_buffer_info gpuInfo = {
    .size = info->size,
    .usage = info->usage,
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

void* lovrBufferMap(Buffer* buffer, uint32_t offset, uint32_t size) {
  return gpu_buffer_map(buffer->gpu, offset, size);
}

// Texture

Texture* lovrTextureCreate(TextureInfo* info) {
  Texture* texture = calloc(1, sizeof(Texture) + gpu_sizeof_texture());
  texture->gpu = (gpu_texture*) (texture + 1);
  texture->info = *info;

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
// Some common unsupported capabilities are handled directly, to provide better error messages
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

// Sets the slot count (array size) and type (gpu_slot_type) from a variable instruction, throwing on failure
static void getSlotInfo(const uint32_t* words, uint32_t wordCount, uint32_t* cache, uint32_t bound, const uint32_t* instruction, gpu_slot* slot) {
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
      slot->count = size[3];
    } else {
      lovrThrow("Invalid Shader code: variable %d is an array, but the array size is not a constant", id);
    }

    // Unwrap array to get to inner array type and keep going
    lovrAssert(instruction[2] < bound && words + cache[instruction[2]] < edge, "Invalid Shader code: id overflow");
    instruction = words + cache[instruction[2]];
  } else {
    slot->count = 1;
  }

  // Use StorageClass to detect uniform/storage buffers
  switch (storageClass) {
    case 12: slot->type = GPU_SLOT_STORAGE_BUFFER; return;
    case 2: slot->type = GPU_SLOT_UNIFORM_BUFFER; return;
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
    case 1: slot->type = GPU_SLOT_SAMPLED_TEXTURE; return;
    case 2: slot->type = GPU_SLOT_STORAGE_TEXTURE; return;
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
  struct var { uint8_t group; uint8_t index; uint16_t name; };
  size_t cacheSize = bound * sizeof(uint32_t);
  void* cache = malloc(cacheSize);
  uint32_t* types = cache;
  struct var* vars = cache;
  lovrAssert(cache, "Out of memory");
  memset(cache, 0xff, cacheSize);

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
          vars[id].index = value << 16;
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

        gpu_slot slot;
        slot.group = vars[id].group;
        slot.index = vars[id].index;
        slot.stage = stage;

        // Ignore variables that aren't decorated with a group and binding (e.g. global variables)
        if (slot.group == 0xff || slot.index == 0xff) {
          break;
        }

        getSlotInfo(words, wordCount, types, bound, instruction, &slot);

        // If the variable has a name, add it to the shader's lookup
        // If the lookup already has one with the same name, it must be at the same group + index
        if (vars[id].name != 0xffff) {
          char* name = (char*) (words + vars[id].name);
          size_t len = strnlen(name, 4 * (wordCount - vars[id].name));
          uint64_t hash = hash64(name, len);
          uint64_t old = map_get(&shader->lookup, hash);
          uint64_t new = ((uint64_t) slot.index << 32) | slot.group;
          if (old == MAP_NIL) {
            map_set(&shader->lookup, hash, new);
          } else {
            lovrAssert(old == new, "Unsupported Shader code: variable named '%s' is bound to 2 different locations", name);
          }
        }

        // If another stage already added a slot with the same group and index, verify that it has
        //   the same type and size as the one here.  Merge the current stage into its stage flags.
        // Otherwise, push it onto the list of slots and update the slotIndex
        uint8_t slotIndex = shader->groups[slot.group].slotMap[slot.index];
        if (slotIndex == 0xff) {
          gpu_slot* other = &shader->slots[slotIndex];
          lovrAssert(other->type == slot.type, "Unsupported Shader code: variable (%d,%d) is in multiple shader stages with different types", slot.group, slot.index);
          lovrAssert(other->count == slot.count, "Unsupported Shader code: variable (%d,%d) is in multiple shader stages with different array sizes", slot.group, slot.index);
          other->stage |= stage;
        } else {
          shader->slots[shader->slotCount] = slot;
          shader->groups[slot.group].slotMap[slot.index] = shader->slotCount++;
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

static int compareSlots(const void* x, const void* y) {
  const gpu_slot* sx = x;
  const gpu_slot* sy = y;
  return (sy->group * 16 + sy->index) - (sx->group * 16 + sx->index);
}

Shader* lovrShaderCreate(ShaderInfo* info) {
  Shader* shader = calloc(1, sizeof(Shader) + gpu_sizeof_shader());
  shader->gpu = (gpu_shader*) (shader + 1);
  shader->info = *info;
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
  // This temporarily populates the slotMaps so variables can be merged between stages
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
    lovrAssert(exists, "Dynamic buffer #%d (%s) does not exist", i + 1, name);
    uint32_t slotIndex = shader->groups[group].slotMap[index];
    gpu_slot* slot = &shader->slots[slotIndex];
    switch (slot->type) {
      case GPU_SLOT_UNIFORM_BUFFER: slot->type = GPU_SLOT_UNIFORM_BUFFER_DYNAMIC; break;
      case GPU_SLOT_STORAGE_BUFFER: slot->type = GPU_SLOT_STORAGE_BUFFER_DYNAMIC; break;
      default: lovrThrow("Dynamic buffer #%d (%s) is not a buffer resource", i + 1, name);
    }
  }

  qsort(shader->slots, shader->slotCount, sizeof(gpu_slot), compareSlots);

  for (uint32_t i = 0; i < 4; i++) {
    ShaderGroup* group = &shader->groups[i];
    memset(&group->slotMap, 0xff, sizeof(group->slotMap));
    memset(&group->bindingMap, ~0u, sizeof(group->bindingMap));
    memset(&group->dynamicOffsetMap, 0xff, sizeof(group->dynamicOffsetMap));
  }

  for (uint32_t i = 0; i < shader->slotCount; i++) {
    gpu_slot* slot = &shader->slots[i];
    ShaderGroup* group = &shader->groups[slot->group];

    if (i > 0 && slot->group != shader->slots[i - 1].group) {
      group->baseSlot = i;
      group->baseBinding = shader->bindingCount;
      group->baseDynamicOffset = shader->dynamicOffsetCount;
    }

    if (slot->type == GPU_SLOT_UNIFORM_BUFFER_DYNAMIC || slot->type == GPU_SLOT_STORAGE_BUFFER_DYNAMIC) {
      group->dynamicOffsetMap[slot->index] = shader->dynamicOffsetCount;
      shader->dynamicOffsetCount++;
      group->dynamicOffsetCount++;
    }

    group->slotMap[slot->index] = i;
    group->slotCount++;
    group->bindingCount += slot->count;
    shader->bindingCount += slot->count;
  }

  for (uint32_t i = 0; i < 4; i++) {
    ShaderGroup* group = &shader->groups[i];
    shader->groups[i].hash = hash64(shader->slots + group->baseSlot, sizeof(gpu_slot) * group->slotCount);
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

bool lovrShaderResolveName(Shader* shader, uint64_t hash, uint32_t* group, uint32_t* index) {
  uint64_t value = map_get(&shader->lookup, hash);
  if (value == MAP_NIL) return false;
  *group = value & ~0u;
  *index = value >> 32;
  return true;
}

// Sampler

Sampler* lovrSamplerCreate(SamplerInfo* info) {
  Sampler* sampler = calloc(1, sizeof(sampler) + gpu_sizeof_sampler());
  sampler->gpu = (gpu_sampler*) (sampler + 1);
  sampler->info = *info;

  gpu_sampler_info gpuInfo = {
    .min = (gpu_filter) info->min,
    .mag = (gpu_filter) info->mag,
    .mip = (gpu_filter) info->mip,
    .wrap[0] = (gpu_wrap) info->wrap[0],
    .wrap[1] = (gpu_wrap) info->wrap[1],
    .wrap[2] = (gpu_wrap) info->wrap[2],
    .compare = (gpu_compare_mode) info->compare,
    .anisotropy = MIN(info->anisotropy, state.limits.anisotropy),
    .lodClamp[0] = info->lodClamp[0],
    .lodClamp[1] = info->lodClamp[1]
  };

  lovrAssert(gpu_sampler_init(sampler->gpu, &gpuInfo), "Could not create Sampler");
  return sampler;
}

void lovrSamplerDestroy(void* ref) {
  Sampler* sampler = ref;
  gpu_sampler_destroy(sampler->gpu);
}

const SamplerInfo* lovrSamplerGetInfo(Sampler* sampler) {
  return &sampler->info;
}
