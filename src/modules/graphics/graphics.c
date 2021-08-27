#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "event/event.h"
#include "headset/headset.h"
#include "math/math.h"
#include "core/maf.h"
#include "core/map.h"
#include "core/gpu.h"
#include "core/os.h"
#include "core/util.h"
#include "resources/shaders.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define MAX_STREAMS 32
#define MAX_BUNDLES 4096
#define MAX_BUNCHES 256
#define BUNDLES_PER_BUNCH 1024
#define MAX_LAYOUTS 64

enum {
  SHAPE_CUBE,
  SHAPE_MAX
};

enum {
  VERTEX_FORMAT_EMPTY,
  VERTEX_FORMAT_POSITION,
  VERTEX_FORMAT_STANDARD,
  VERTEX_FORMAT_COUNT
};

typedef struct {
  char* pointer;
  gpu_buffer* gpu;
  uint32_t size;
  uint32_t type;
  uint32_t next;
  uint32_t tick;
  uint32_t refs;
} Megabuffer;

typedef struct {
  gpu_buffer* gpu;
  char* data;
  uint32_t index;
  uint32_t offset;
} Megaview;

typedef struct {
  Megabuffer list[256];
  uint32_t active[3];
  uint32_t oldest[3];
  uint32_t newest[3];
  uint32_t cursor[3];
  uint32_t count;
} BufferPool;

struct Buffer {
  uint32_t ref;
  uint32_t size;
  Megaview mega;
  BufferInfo info;
  bool scratch;
};

struct Texture {
  uint32_t ref;
  gpu_texture* gpu;
  gpu_texture* renderView;
  Sampler* sampler;
  TextureInfo info;
};

struct Sampler {
  uint32_t ref;
  gpu_sampler* gpu;
  SamplerInfo info;
};

struct Shader {
  uint32_t ref;
  ShaderInfo info;
  gpu_shader* gpu;
  gpu_layout* layout;
  gpu_pipeline* compute;
  uint32_t resourceCount;
  uint32_t bufferMask;
  uint32_t textureMask;
  uint32_t storageMask;
  uint8_t resourceSlots[32];
  uint64_t resourceLookup[32];
  uint32_t attributeMask;
};

enum {
  DIRTY_PIPELINE = (1 << 0),
  DIRTY_VERTEX_BUFFER = (1 << 1),
  DIRTY_INDEX_BUFFER = (1 << 2),
  DIRTY_UNIFORM_CHUNK = (1 << 3),
  DIRTY_TEXTURE_PAGE = (1 << 4),
  DIRTY_BUNDLE = (1 << 5)
};

typedef struct {
  uint16_t dirty;
  uint16_t count;
} BatchGroup;

typedef union {
  uint64_t u64;
  struct {
    unsigned indirect : 1;
    unsigned pipeline : 15;
    unsigned vertex : 8;
    unsigned index : 8;
    unsigned textures : 4;
    unsigned chunk : 4;
    unsigned bundle : 12;
    unsigned depth : 12;
  } bits;
} DrawKey;

typedef struct {
  DrawKey key;
  uint32_t index;
  uint32_t start;
  uint32_t count;
  uint32_t instances;
  uint32_t base;
} BatchDraw;

typedef struct {
  float color[4];
  Shader* shader;
  uint64_t vertexFormat;
  gpu_pipeline_info info;
  uint16_t index;
  bool dirty;
} BatchPipeline;

enum {
  BUFFER_TRANSFORM,
  BUFFER_DRAW_DATA,
  BUFFER_GEOMETRY,
  BUFFER_MAX
};

struct Batch {
  uint32_t ref;
  bool scratch;
  bool active;
  BatchInfo info;
  gpu_pass* pass;
  uint32_t groupCount;
  uint32_t drawCount;
  BatchGroup* groups;
  BatchDraw* draws;
  gpu_bunch* bunch;
  gpu_bundle** bundles;
  gpu_bundle_info* bundleWrites;
  gpu_binding bindings[32];
  uint32_t emptySlotMask;
  uint32_t bundleCount;
  bool bindingsDirty;
  BatchPipeline pipelines[4];
  uint32_t pipelineIndex;
  BatchPipeline* pipeline;
  float transforms[16][16];
  uint32_t transformIndex;
  float* transform;
  Megaview buffers[BUFFER_MAX];
  Megaview scratchpads[BUFFER_MAX];
  uint32_t geometryCursor;
  float viewport[4];
  float depthRange[2];
  uint32_t scissor[4];
  float viewMatrix[6][16];
  float projection[6][16];
};

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
  char* memory;
  uint32_t tip;
  uint32_t top;
  uint32_t cap;
} LinearAllocator;

typedef struct {
  gpu_texture* handle;
  uint32_t hash;
  uint32_t tick;
} ScratchTexture;

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

static struct {
  bool initialized;
  bool active;
  gpu_hardware hardware;
  gpu_features features;
  gpu_limits limits;
  float background[4];
  LinearAllocator frame;
  uint32_t blockSize;
  uint32_t baseVertex[SHAPE_MAX];
  Megaview shapes;
  Buffer* defaultBuffer;
  Texture* defaultTexture;
  Shader* defaultShader;
  Sampler* defaultSamplers[MAX_DEFAULT_SAMPLERS];
  BufferFormat vertexFormats[VERTEX_FORMAT_COUNT];
  ScratchTexture scratchTextures[16][4];
  Texture* window;
  ReaderPool readers;
  BufferPool buffers;
  BunchPool bunches;
  uint32_t pipelineCount;
  uint64_t pipelineLookup[4096];
  gpu_pipeline* pipelines[4096];
  uint64_t passKeys[256];
  gpu_pass* passes[256];
  uint64_t layoutLookup[MAX_LAYOUTS];
  gpu_layout* layouts[MAX_LAYOUTS];
  gpu_stream* transfers;
  gpu_stream* streams[MAX_STREAMS];
  int8_t streamOrder[MAX_STREAMS];
  uint32_t streamCount;
  uint32_t tick;
} state;

static void* talloc(size_t size) {
  while (state.frame.tip + size > state.frame.top) {
    lovrAssert(state.frame.top << 1 <= state.frame.cap, "Out of memory");
    os_vm_commit(state.frame.memory + state.frame.top, state.frame.top);
    state.frame.top <<= 1;
  }

  uint32_t tip = ALIGN(state.frame.tip, 8);
  state.frame.tip = tip + size;
  return state.frame.memory + tip;
}

// Returns a Megabuffer to the pool
static void bufferRecycle(uint8_t index) {
  Megabuffer* buffer = &state.buffers.list[index];
  lovrCheck(buffer->refs == 0, "Trying to release a Buffer while people are still using it");

  // If there's a "newest" Megabuffer, make it the second newest
  if (state.buffers.newest[buffer->type] != ~0u) {
    state.buffers.list[state.buffers.newest[buffer->type]].next = index;
  }

  // If the waitlist is completely empty, this Megabuffer should become both the oldest and newest
  if (state.buffers.oldest[buffer->type] == ~0u) {
    state.buffers.oldest[buffer->type] = index;
  }

  // This Megabuffer is the newest
  state.buffers.newest[buffer->type] = index;
  buffer->next = ~0u;
  buffer->tick = state.tick;
}

// Suballocates from a Megabuffer
static Megaview bufferAllocate(gpu_memory_type type, uint32_t size, uint32_t align) {
  uint32_t active = state.buffers.active[type];
  uint32_t oldest = state.buffers.oldest[type];
  uint32_t cursor = ALIGN(state.buffers.cursor[type], align);

  // If there's an active Megabuffer and it has room, use it
  if (active != ~0u && cursor + size <= state.buffers.list[active].size) {
    state.buffers.cursor[type] = cursor + size;
    Megabuffer* buffer = &state.buffers.list[active];
    return (Megaview) { .gpu = buffer->gpu, .data = buffer->pointer + cursor, .index = active, .offset = cursor };
  }

  // If the active Megabuffer is full and has no users, it can be reused when GPU is done with it
  if (active != ~0u && state.buffers.list[active].refs == 0) {
    bufferRecycle(active);
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
  buffer->size = MAX(state.blockSize, size);
  buffer->type = type;
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
    .data = (void**) &buffer->pointer
  };

  lovrAssert(gpu_buffer_init(buffer->gpu, &info), "Failed to initialize Buffer");

  return (Megaview) { .gpu = buffer->gpu, .data = buffer->pointer, .index = active, .offset = 0 };
}

static gpu_bundle* bundleAllocate(uint32_t layout) {
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
  uint64_t id = 0xaaa; // TODO get pass key

  Texture* texture = canvas->textures[0] ? canvas->textures[0] : canvas->depth.texture;
  bool resolve = texture->info.samples == 1 && canvas->samples > 1;

  uint32_t index;
  for (index = 0; index < COUNTOF(state.passes) && state.passKeys[index]; index++) {
    if (state.passKeys[index] == id) {
      return state.passes[index];
    }
  }

  lovrCheck(index < COUNTOF(state.passes), "Too many passes, please report this encounter");

  // Create new pass
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
      .srgb = canvas->textures[i]->info.srgb
    };
  }

  if (canvas->depth.texture || canvas->depth.format) {
    info.depth = (gpu_pass_depth_info) {
      .format = (gpu_texture_format) (canvas->depth.texture ? canvas->depth.texture->info.format : canvas->depth.format),
      .load = (gpu_load_op) canvas->depth.load,
      .stencilLoad = (gpu_load_op) canvas->depth.load,
      .save = canvas->depth.save ? GPU_SAVE_OP_SAVE : GPU_SAVE_OP_DISCARD,
      .stencilSave = canvas->depth.save ? GPU_SAVE_OP_SAVE : GPU_SAVE_OP_DISCARD
    };
  }

  lovrAssert(gpu_pass_init(state.passes[index], &info), "Failed to initialize pass");
  state.passKeys[index] = id;
  return state.passes[index];
}

static gpu_layout* lookupLayout(gpu_slot* slots, uint32_t count) {
  uint64_t hash = hash64(slots, count * sizeof(gpu_slot));

  uint32_t index;
  for (index = 0; index < COUNTOF(state.layouts) && state.layoutLookup[index]; index++) {
    if (state.layoutLookup[index] == hash) {
      return state.layouts[index];
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
  return state.layouts[index];
}

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
  buffer[count ? -1 : 0] = '\0';

  return true;
}

bool lovrGraphicsInit(bool debug, bool vsync, uint32_t blockSize) {
  lovrCheck(blockSize <= 1 << 30, "Block size can not exceed 1GB");
  state.blockSize = blockSize;

  state.frame.top = 1 << 14;
  state.frame.cap = 1 << 30;
  state.frame.memory = os_vm_init(state.frame.cap);
  os_vm_commit(state.frame.memory, state.frame.top);

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
    config.vk.vsync = vsync;
    config.vk.createSurface = os_vk_create_surface;
  }
#endif

  if (!gpu_init(&config)) {
    lovrThrow("Failed to initialize GPU");
  }

  memset(state.buffers.active, 0xff, sizeof(state.buffers.active));
  memset(state.buffers.oldest, 0xff, sizeof(state.buffers.oldest));
  memset(state.buffers.newest, 0xff, sizeof(state.buffers.newest));

  state.buffers.list[0].gpu = malloc(COUNTOF(state.buffers.list) * gpu_sizeof_buffer());
  state.bunches.list[0].gpu = malloc(COUNTOF(state.bunches.list) * gpu_sizeof_bunch());
  state.pipelines[0] = malloc(COUNTOF(state.pipelines) * gpu_sizeof_pipeline());
  state.passes[0] = malloc(COUNTOF(state.passes) * gpu_sizeof_pass());
  state.layouts[0] = malloc(COUNTOF(state.layouts) * gpu_sizeof_layout());
  lovrAssert(state.buffers.list[0].gpu && state.bunches.list[0].gpu && state.pipelines[0] && state.passes[0] && state.layouts[0], "Out of memory");

  for (uint32_t i = 1; i < COUNTOF(state.buffers.list); i++) {
    state.buffers.list[i].gpu = (gpu_buffer*) ((char*) state.buffers.list[0].gpu + i * gpu_sizeof_buffer());
  }

  for (uint32_t i = 1; i < COUNTOF(state.bunches.list); i++) {
    state.bunches.list[i].gpu = (gpu_bunch*) ((char*) state.bunches.list[0].gpu + i * gpu_sizeof_bunch());
  }

  for (uint32_t i = 1; i < COUNTOF(state.pipelines); i++) {
    state.pipelines[i] = (gpu_pipeline*) ((char*) state.pipelines[0] + i * gpu_sizeof_pipeline());
  }

  for (uint32_t i = 1; i < COUNTOF(state.passes); i++) {
    state.passes[i] = (gpu_pass*) ((char*) state.passes[0] + i * gpu_sizeof_pass());
  }

  for (uint32_t i = 1; i < COUNTOF(state.layouts); i++) {
    state.layouts[i] = (gpu_layout*) ((char*) state.layouts[0] + i * gpu_sizeof_layout());
  }

  // Create layout for builtin resources
  gpu_slot builtins[] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER, GPU_STAGE_VERTEX, 1 }, // Camera
    { 1, GPU_SLOT_UNIFORM_BUFFER_DYNAMIC, GPU_STAGE_VERTEX, 1 } // Transforms
  };

  lookupLayout(builtins, COUNTOF(builtins));

  state.defaultBuffer = lovrBufferCreate(&(BufferInfo) {
    .length = 4096,
    .usage = BUFFER_VERTEX | BUFFER_UNIFORM | BUFFER_STORAGE,
    .label = "zero"
  });

  state.defaultTexture = lovrTextureCreate(&(TextureInfo) {
    .type = TEXTURE_2D,
    .usage = TEXTURE_SAMPLE | TEXTURE_COPYTO,
    .format = FORMAT_RGBA8,
    .width = 4,
    .height = 4,
    .depth = 1,
    .mipmaps = 1,
    .samples = 1,
    .srgb = false,
    .label = "white"
  });

  lovrTextureSetSampler(state.defaultTexture, lovrGraphicsGetDefaultSampler(SAMPLER_NEAREST));

  state.defaultShader = lovrShaderCreate(&(ShaderInfo) {
    .type = SHADER_GRAPHICS,
    .source = { lovr_shader_unlit_vert, lovr_shader_unlit_frag },
    .length = { sizeof(lovr_shader_unlit_vert), sizeof(lovr_shader_unlit_frag) },
    .label = "unlit"
  });

  // Shapes buffer
  typedef struct {
    struct { float x, y, z; } position;
    struct { unsigned pad: 2, nx: 10, ny: 10, nz: 10; } normal;
    struct { uint16_t u, v; } uv;
  } Vertex;

  uint32_t stride = sizeof(Vertex);

  // In 10 bit land, 0x200 is 0.0, 0x3ff is 1.0, 0x000 is -1.0
  Vertex cube[] = {
    { { -.5f, -.5f, -.5f }, { 0x0, 0x200, 0x200, 0x000 }, { 0x0000, 0x0000 } }, // Front
    { { -.5f,  .5f, -.5f }, { 0x0, 0x200, 0x200, 0x000 }, { 0x0000, 0xffff } },
    { {  .5f, -.5f, -.5f }, { 0x0, 0x200, 0x200, 0x000 }, { 0xffff, 0x0000 } },
    { {  .5f,  .5f, -.5f }, { 0x0, 0x200, 0x200, 0x000 }, { 0xffff, 0xffff } },
    { {  .5f,  .5f, -.5f }, { 0x0, 0x3ff, 0x200, 0x200 }, { 0x0000, 0xffff } }, // Right
    { {  .5f,  .5f,  .5f }, { 0x0, 0x3ff, 0x200, 0x200 }, { 0xffff, 0xffff } },
    { {  .5f, -.5f, -.5f }, { 0x0, 0x3ff, 0x200, 0x200 }, { 0x0000, 0x0000 } },
    { {  .5f, -.5f,  .5f }, { 0x0, 0x3ff, 0x200, 0x200 }, { 0xffff, 0x0000 } },
  };

  state.vertexFormats[VERTEX_FORMAT_EMPTY] = (BufferFormat) { 0 };
  state.vertexFormats[VERTEX_FORMAT_POSITION] = (BufferFormat) {
    .count = 1,
    .stride = 12,
    .types[0] = FIELD_F32x3
  };
  state.vertexFormats[VERTEX_FORMAT_STANDARD] = (BufferFormat) {
    .count = 3,
    .stride = sizeof(Vertex),
    .locations = { 0, 1, 2 },
    .offsets = { offsetof(Vertex, position), offsetof(Vertex, normal), offsetof(Vertex, uv) },
    .types = { FIELD_F32x3, FIELD_U10Nx3, FIELD_U16Nx2 }
  };

  for (uint32_t i = 0; i < COUNTOF(state.vertexFormats); i++) {
    state.vertexFormats[i].hash = hash64(&state.vertexFormats[i], sizeof(state.vertexFormats[i]));
  }

  uint32_t size = 0;
  state.baseVertex[SHAPE_CUBE] = size / stride, size += sizeof(cube);

  state.shapes = bufferAllocate(GPU_MEMORY_GPU, size, 4);
  Megaview scratch = bufferAllocate(GPU_MEMORY_CPU_WRITE, size, 4);
  memcpy(scratch.data, cube, sizeof(cube)), scratch.data += sizeof(cube);

  gpu_begin();
  gpu_stream* stream = gpu_stream_begin();
  gpu_clear_buffer(stream, state.defaultBuffer->mega.gpu, state.defaultBuffer->mega.offset, state.defaultBuffer->info.length, 0);
  gpu_clear_texture(stream, state.defaultTexture->gpu, 0, 1, 0, 1, (float[4]) { 1.f, 1.f, 1.f, 1.f });
  gpu_copy_buffers(stream, scratch.gpu, state.shapes.gpu, scratch.offset, state.shapes.offset, size);
  gpu_stream_end(stream);
  gpu_submit(&stream, 1);

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
  for (uint32_t i = 0; i < COUNTOF(state.passes) && state.passKeys[i]; i++) {
    gpu_pass_destroy(state.passes[i]);
  }
  for (uint32_t i = 0; i < COUNTOF(state.layouts) && state.layoutLookup[i]; i++) {
    gpu_layout_destroy(state.layouts[i]);
  }
  for (uint32_t i = 0; i < COUNTOF(state.scratchTextures); i++) {
    for (uint32_t j = 0; j < COUNTOF(state.scratchTextures[0]); j++) {
      if (state.scratchTextures[i][j].handle) {
        gpu_texture_destroy(state.scratchTextures[i][j].handle);
        free(state.scratchTextures[i][j].handle);
      }
    }
  }
  lovrRelease(state.defaultShader, lovrShaderDestroy);
  lovrRelease(state.window, lovrTextureDestroy);
  gpu_destroy();
  free(state.layouts[0]);
  free(state.passes[0]);
  free(state.pipelines[0]);
  free(state.bunches.list);
  free(state.buffers.list[0].gpu);
  os_vm_free(state.frame.memory, state.frame.cap);
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

void lovrGraphicsGetBackgroundColor(float background[4]) {
  memcpy(background, state.background, 4 * sizeof(float));
}

void lovrGraphicsSetBackgroundColor(float background[4]) {
  memcpy(state.background, background, 4 * sizeof(float));
}

void lovrGraphicsBegin() {
  if (state.active) return;
  state.active = true;
  state.frame.tip = 0;
  state.tick = gpu_begin();
  state.streams[0] = state.transfers = gpu_stream_begin();
  state.streamOrder[0] = 0;
  state.streamCount = 1;

  // Process any finished readbacks
  ReaderPool* readers = &state.readers;
  while (readers->tail != readers->head && gpu_finished(readers->list[readers->tail & 0xf].tick)) {
    Reader* reader = &readers->list[readers->tail++ & 0xf];
    reader->callback(reader->data, reader->size, reader->userdata);
  }
}

static int streamSort(const void* a, const void* b) {
  return state.streamOrder[*(uint32_t*) a] - state.streamOrder[*(uint32_t*) b];
}

void lovrGraphicsSubmit() {
  if (!state.active) return;
  state.active = false;

  if (state.window) {
    state.window->gpu = NULL;
    state.window->renderView = NULL;
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

static gpu_texture* getScratchTexture(uint32_t size[2], uint32_t layers, TextureFormat format, bool srgb, uint32_t samples) {
  uint16_t key[] = { size[0], size[1], layers, format, srgb, samples };
  uint32_t hash = (uint32_t) hash64(key, sizeof(key));

  // Search for matching texture in cache table
  gpu_texture* texture;
  uint32_t rows = COUNTOF(state.scratchTextures);
  uint32_t cols = COUNTOF(state.scratchTextures[0]);
  ScratchTexture* row = state.scratchTextures[0] + (hash & (rows - 1)) * cols;
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

  // TODO this has bad malloc churn
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
    .srgb = srgb
  };

  entry = &row[0];
  for (uint32_t i = 1; i < cols; i++) {
    if (!row[i].handle || row[i].tick < entry->tick) {
      entry = &row[i];
      break;
    }
  }

  if (entry->handle) {
    gpu_texture_destroy(entry->handle);
    free(entry->handle);
  }

  texture = calloc(1, gpu_sizeof_texture());
  lovrAssert(texture, "Out of memory");
  lovrAssert(gpu_texture_init(texture, &info), "Failed to create scratch texture");
  entry->handle = texture;
  entry->hash = hash;
  entry->tick = state.tick;
  return texture;
}

void lovrGraphicsRender(Canvas* canvas, Batch** batches, uint32_t count, uint32_t order) {
  lovrCheck(state.active, "Graphics is not active");

  // Validate Canvas
  const TextureInfo* main = canvas->textures[0] ? &canvas->textures[0]->info : &canvas->depth.texture->info;
  lovrCheck(canvas->textures[0] || canvas->depth.texture, "Canvas must have at least one color or depth texture");
  lovrCheck(main->width <= state.limits.renderSize[0], "Canvas width (%d) exceeds the renderWidth limit of this GPU (%d)", main->width, state.limits.renderSize[0]);
  lovrCheck(main->height <= state.limits.renderSize[1], "Canvas height (%d) exceeds the renderHeight limit of this GPU (%d)", main->height, state.limits.renderSize[1]);
  lovrCheck(main->depth <= state.limits.renderViews, "Canvas view count (%d) exceeds the renderViews limit of this GPU (%d)", main->depth, state.limits.renderViews);
  lovrCheck((canvas->samples & (canvas->samples - 1)) == 0, "Canvas sample count must be a power of 2");

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
    const TextureInfo* info = &canvas->depth.texture->info;
    TextureFormat format = canvas->depth.texture ? info->format : canvas->depth.format;
    bool renderable = state.features.formats[format] & GPU_FEATURE_RENDER_DEPTH;
    lovrCheck(renderable, "This GPU does not support rendering to the Canvas depth buffer's format");
    if (canvas->depth.texture) {
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

  // Command buffer
  gpu_stream* stream = gpu_stream_begin();
  state.streamOrder[state.streamCount] = order;
  state.streams[state.streamCount] = stream;
  state.streamCount++;

  // Render pass
  gpu_render_begin(stream, &target);

  // Batches
  for (uint32_t i = 0; i < count; i++) {
    Batch* batch = batches[i];
    BatchDraw* draw = batch->draws;
    BatchGroup* group = batch->groups;
    lovrCheck(batch->info.type == BATCH_RENDER, "Attempt to use a compute Batch for rendering");

    // Viewport
    if (batch->viewport[2] > 0.f && batch->viewport[3] > 0.f) {
      gpu_set_viewport(stream, batch->viewport, batch->depthRange);
    } else {
      gpu_set_viewport(stream, (float[4]) { 0.f, 0.f, main->width, main->height }, (float[2]) { 0.f, 1.f });
    }

    // Scissor
    if (batch->scissor[2] > 0 && batch->scissor[3] > 0) {
      gpu_set_scissor(stream, batch->scissor);
    } else {
      gpu_set_scissor(stream, (uint32_t[4]) { 0, 0, main->width, main->height });
    }

    // Camera
    uint32_t cameraSize = sizeof(batch->viewMatrix) + sizeof(batch->projection);
    Megaview camera = bufferAllocate(GPU_MEMORY_CPU_WRITE, cameraSize, 256);
    memcpy(camera.data, batch->viewMatrix, sizeof(batch->viewMatrix) + sizeof(batch->projection));

    // Builtins
    gpu_bundle* builtins = bundleAllocate(0);
    gpu_bundle_info info = {
      .layout = state.layouts[0],
      .bindings = (gpu_binding[]) {
        [0].buffer = { camera.gpu, camera.offset, cameraSize },
        [1].buffer = { batch->buffers[BUFFER_TRANSFORM].gpu, batch->buffers[BUFFER_TRANSFORM].offset, 0 } // TODO extent
      }
    };
    gpu_bundle_write(&builtins, &info, 1);
    gpu_bind_bundle(stream, state.defaultShader->gpu, 0, builtins, NULL, 0);

    // Draws
    for (uint32_t j = 0; j < batch->groupCount; j++, group++) {
      if (group->dirty & DIRTY_PIPELINE) {
        gpu_bind_pipeline(stream, state.pipelines[draw->key.bits.pipeline], false);
      }

      if (group->dirty & DIRTY_VERTEX_BUFFER) {
        uint32_t offset = 0;
        gpu_bind_vertex_buffers(stream, &state.buffers.list[draw->key.bits.vertex].gpu, &offset, 0, 1);
      }

      if (group->dirty & DIRTY_INDEX_BUFFER) {
        gpu_bind_index_buffer(stream, state.buffers.list[draw->key.bits.index].gpu, 0, GPU_INDEX_U32);
      }

      /*if (group->dirty & (DIRTY_UNIFORM_CHUNK | DIRTY_TEXTURE_PAGE)) {
        gpu_bundle* bundle = batch->texturePages + draw->key.bits.textures * gpu_sizeof_bundle();
        uint32_t dynamicOffsets[4] = { 0, 0, 0, 0 };
        gpu_bind_bundle(stream, NULL, 0, bundle, dynamicOffsets, COUNTOF(dynamicOffsets));
      }*/

      if (group->dirty & DIRTY_BUNDLE) {
        gpu_bind_bundle(stream, NULL, 1, batch->bundles[draw->key.bits.bundle], NULL, 0); // TODO pipelineLayout ahhhhhh
      }

      if (draw->key.bits.index == 0xff) {
        for (uint16_t k = 0; k < group->count; k++, draw++) {
          gpu_draw(stream, draw->count, draw->instances, draw->start);
        }
      } else {
        for (uint16_t k = 0; k < group->count; k++, draw++) {
          gpu_draw_indexed(stream, draw->count, draw->instances, draw->start, draw->base);
        }
      }
    }
  }

  gpu_render_end(stream);
}

void lovrGraphicsCompute(Batch** batches, uint32_t count, uint32_t order) {
  // TODO
}

// Buffer

Buffer* lovrGraphicsGetBuffer(BufferInfo* info) {
  size_t size = info->length * MAX(info->format.stride, 1);
  lovrCheck(size > 0, "Buffer size must be positive");
  lovrCheck(size <= 1 << 30, "Max Buffer size is 1GB");
  Buffer* buffer = talloc(sizeof(Buffer));
  buffer->ref = 1;
  buffer->size = size;
  buffer->mega = bufferAllocate(GPU_MEMORY_CPU_WRITE, size, 256);
  buffer->info = *info;
  buffer->scratch = true;
  if (info->usage & BUFFER_VERTEX) {
    BufferFormat* format = &info->format;
    lovrCheck(format->stride < state.limits.vertexBufferStride, "Buffer with 'vertex' usage has a stride of %d bytes, which exceeds limits.vertexBufferStride (%d)", format->stride, state.limits.vertexBufferStride);
    for (uint32_t i = 0; i < COUNTOF(format->locations); i++) {
      lovrCheck(format->locations[i] < 16, "Vertex buffer attribute location %d is too big (max is 15)", format->locations[i]);
      lovrCheck(format->offsets[i] < 256, "Vertex buffer attribute offset %d is too big (max is 255)", format->offsets[i]);
    }
    format->hash = 0;
    format->hash = hash64(format, sizeof(*format));
  }
  return buffer;
}

Buffer* lovrBufferCreate(BufferInfo* info) {
  size_t size = info->length * MAX(info->format.stride, 1);
  lovrCheck(size > 0, "Buffer size must be positive");
  lovrCheck(size <= 1 << 30, "Max Buffer size is 1GB");
  Buffer* buffer = calloc(1, sizeof(Buffer));
  lovrAssert(buffer, "Out of memory");
  buffer->ref = 1;
  buffer->size = size;
  buffer->mega = bufferAllocate(GPU_MEMORY_GPU, size, 256);
  buffer->info = *info;
  state.buffers.list[buffer->mega.index].refs++;
  if (info->usage & BUFFER_VERTEX) {
    BufferFormat* format = &info->format;
    lovrCheck(format->stride < state.limits.vertexBufferStride, "Buffer with 'vertex' usage has a stride of %d bytes, which exceeds limits.vertexBufferStride (%d)", format->stride, state.limits.vertexBufferStride);
    for (uint32_t i = 0; i < COUNTOF(format->locations); i++) {
      lovrCheck(format->locations[i] < 16, "Vertex buffer attribute location %d is too big (max is 15)", format->locations[i]);
      lovrCheck(format->offsets[i] < 256, "Vertex buffer attribute offset %d is too big (max is 255)", format->offsets[i]);
    }
    format->hash = 0;
    format->hash = hash64(format, sizeof(*format));
  }
  return buffer;
}

void lovrBufferDestroy(void* ref) {
  Buffer* buffer = ref;
  if (!buffer->scratch) {
    if (--state.buffers.list[buffer->mega.index].refs == 0) {
      bufferRecycle(buffer->mega.index);
    }
    free(buffer);
  }
}

const BufferInfo* lovrBufferGetInfo(Buffer* buffer) {
  return &buffer->info;
}

void* lovrBufferMap(Buffer* buffer, uint32_t offset, uint32_t size) {
  if (size == ~0u) size = buffer->size - offset;
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(offset + size <= buffer->size, "Tried to write past the end of the Buffer");
  if (buffer->scratch) {
    return buffer->mega.data + offset;
  } else {
    lovrCheck(buffer->info.usage & BUFFER_COPYTO, "Buffers must have the 'copyto' usage to write to them");
    Megaview scratch = bufferAllocate(GPU_MEMORY_CPU_WRITE, size, 4);
    gpu_copy_buffers(state.transfers, scratch.gpu, buffer->mega.gpu, scratch.offset, buffer->mega.offset + offset, size);
    return scratch.data + scratch.offset;
  }
}

void lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t size) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(offset % 4 == 0, "Buffer clear offset must be a multiple of 4");
  lovrCheck(size % 4 == 0, "Buffer clear size must be a multiple of 4");
  lovrCheck(offset + size <= buffer->size, "Tried to clear past the end of the Buffer");
  if (buffer->scratch) {
    memset(buffer->mega.data + offset, 0, size);
  } else {
    lovrCheck(buffer->info.usage & BUFFER_COPYTO, "Buffers must have the 'copyto' usage to clear them");
    gpu_clear_buffer(state.transfers, buffer->mega.gpu, buffer->mega.offset + offset, size, 0);
  }
}

void lovrBufferCopy(Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(src->info.usage & BUFFER_COPYFROM, "Buffer must have the 'copyfrom' usage to copy from it");
  lovrCheck(dst->info.usage & BUFFER_COPYTO, "Buffer must have the 'copyto' usage to copy to it");
  lovrCheck(srcOffset + size <= src->size, "Tried to read past the end of the source Buffer");
  lovrCheck(dstOffset + size <= dst->size, "Tried to copy past the end of the destination Buffer");
  gpu_copy_buffers(state.transfers, src->mega.gpu, dst->mega.gpu, src->mega.offset + srcOffset, dst->mega.offset + dstOffset, size);
}

void lovrBufferRead(Buffer* buffer, uint32_t offset, uint32_t size, void (*callback)(void*, uint32_t, void*), void* userdata) {
  ReaderPool* readers = &state.readers;
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(buffer->info.usage & BUFFER_COPYFROM, "Buffer must have the 'copyfrom' usage to read from it");
  lovrCheck(offset + size <= buffer->size, "Tried to read past the end of the Buffer");
  lovrCheck(readers->head - readers->tail != COUNTOF(readers->list), "Too many readbacks"); // TODO emergency waitIdle instead
  Megaview scratch = bufferAllocate(GPU_MEMORY_CPU_READ, size, 4);
  gpu_copy_buffers(state.transfers, buffer->mega.gpu, scratch.gpu, buffer->mega.offset + offset, scratch.offset, size);
  readers->list[readers->head++ & 0xf] = (Reader) {
    .callback = callback,
    .userdata = userdata,
    .data = scratch.data,
    .size = size,
    .tick = state.tick
  };
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
  lovrCheck(offset[0] + extent[0] <= maxWidth, "Texture x range [%d,%d] exceeds width (%d)", offset[0], offset[0] + extent[0], maxWidth);
  lovrCheck(offset[1] + extent[1] <= maxHeight, "Texture y range [%d,%d] exceeds height (%d)", offset[1], offset[1] + extent[1], maxHeight);
  lovrCheck(offset[2] + extent[2] <= maxDepth, "Texture z range [%d,%d] exceeds depth (%d)", offset[2], offset[2] + extent[2], maxDepth);
  lovrCheck(offset[3] < info->mipmaps, "Texture mipmap %d exceeds its mipmap count (%d)", offset[3] + 1, info->mipmaps);
}

Texture* lovrGraphicsGetWindowTexture() {
  if (!state.window) {
    state.window = calloc(1, sizeof(Texture));
    lovrAssert(state.window, "Out of memory");
    state.window->ref = 1;
    state.window->info.type = TEXTURE_2D;
    state.window->info.usage = TEXTURE_RENDER;
    state.window->info.format = ~0u;
    state.window->info.mipmaps = 1;
    state.window->info.samples = 1;
    state.window->info.srgb = true;
  }

  if (!state.window->gpu) {
    state.window->gpu = gpu_surface_acquire();
    state.window->renderView = state.window->gpu;
    int width, height;
    os_window_get_fbsize(&width, &height);
    state.window->info.width = width;
    state.window->info.height = height;
    state.window->info.depth = 1;
  }

  return state.window;
}

Texture* lovrTextureCreate(TextureInfo* info) {
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
  lovrCheck(getTextureRegionSize(info->format, info->width, info->height, info->depth) < 1 << 30, "Memory for a Texture can not exceed 1GB");
  lovrCheck((info->samples & (info->samples - 1)) == 0, "Texture multisample count must be a power of 2");
  lovrCheck(info->samples == 1 || info->type != TEXTURE_CUBE, "Cubemaps can not be multisampled");
  lovrCheck(info->samples == 1 || info->type != TEXTURE_VOLUME, "Volume textures can not be multisampled");
  lovrCheck(info->samples == 1 || ~info->usage & TEXTURE_STORAGE, "Currently, Textures with the 'storage' flag can not be multisampled");
  lovrCheck(info->samples == 1 || info->mipmaps == 1, "Multisampled textures can only have 1 mipmap");
  lovrCheck(~info->usage & TEXTURE_SAMPLE || (supports & GPU_FEATURE_SAMPLE), "GPU does not support the 'sample' flag for this format");
  lovrCheck(~info->usage & TEXTURE_RENDER || (supports & GPU_FEATURE_RENDER), "GPU does not support the 'render' flag for this format");
  lovrCheck(~info->usage & TEXTURE_STORAGE || (supports & GPU_FEATURE_STORAGE), "GPU does not support the 'storage' flag for this format");
  lovrCheck(~info->usage & TEXTURE_RENDER || info->width <= state.limits.renderSize[0], "Texture has 'render' flag but its size exceeds limits.renderSize");
  lovrCheck(~info->usage & TEXTURE_RENDER || info->height <= state.limits.renderSize[1], "Texture has 'render' flag but its size exceeds limits.renderSize");
  lovrCheck(info->mipmaps == ~0u || info->mipmaps <= mips, "Texture has more than the max number of mipmap levels for its size (%d)", mips);

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

  if (info->usage & TEXTURE_SAMPLE) {
    lovrTextureSetSampler(texture, lovrGraphicsGetDefaultSampler(SAMPLER_TRILINEAR));
  }

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

  lovrRetain(view->parent);
  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  lovrRelease(texture->info.parent, lovrTextureDestroy);
  lovrRelease(texture->sampler, lovrSamplerDestroy);
  if (texture->renderView && texture->renderView != texture->gpu) gpu_texture_destroy(texture->renderView);
  if (texture->gpu) gpu_texture_destroy(texture->gpu);
  free(texture);
}

const TextureInfo* lovrTextureGetInfo(Texture* texture) {
  return &texture->info;
}

Sampler* lovrTextureGetSampler(Texture* texture) {
  if (~texture->info.usage & TEXTURE_SAMPLE) return NULL;
  return texture->sampler;
}

void lovrTextureSetSampler(Texture* texture, Sampler* sampler) {
  lovrCheck(texture->info.usage & TEXTURE_SAMPLE, "Textures must have the 'sample' usage to apply Samplers to them");
  lovrRetain(sampler);
  lovrRelease(texture->sampler, lovrSamplerDestroy);
  texture->sampler = sampler;
}

void lovrTextureWrite(Texture* texture, uint16_t offset[4], uint16_t extent[3], void* data, uint32_t step[2]) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!texture->info.parent, "Texture views can not be written to");
  lovrCheck(texture->info.usage & TEXTURE_COPYTO, "Texture must have the 'copyto' flag to write to it");
  lovrCheck(texture->info.samples == 1, "Multisampled Textures can not be written to");
  checkTextureBounds(&texture->info, offset, extent);

  size_t fullSize = getTextureRegionSize(texture->info.format, extent[0], extent[1], extent[2]);
  size_t rowSize = getTextureRegionSize(texture->info.format, extent[0], 1, 1);
  size_t imgSize = getTextureRegionSize(texture->info.format, extent[0], extent[1], 1);
  Megaview scratch = bufferAllocate(GPU_MEMORY_CPU_WRITE, fullSize, 64);
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

  gpu_copy_buffer_texture(state.transfers, scratch.gpu, texture->gpu, scratch.offset, offset, extent);
}

void lovrTexturePaste(Texture* texture, Image* image, uint16_t srcOffset[2], uint16_t dstOffset[4], uint16_t extent[2]) {
  lovrCheck(texture->info.format == image->format, "Texture and Image formats must match");
  lovrCheck(srcOffset[0] + extent[0] <= image->width, "Tried to read pixels past the width of the Image");
  lovrCheck(srcOffset[1] + extent[1] <= image->height, "Tried to read pixels past the height of the Image");
  uint16_t fullExtent[3] = { extent[0], extent[1], 1 };
  uint32_t step[2] = { getTextureRegionSize(image->format, image->width, 1, 1), 0 };
  size_t offsetx = getTextureRegionSize(image->format, srcOffset[0], 1, 1);
  size_t offsety = srcOffset[1] * step[0];
  char* data = (char*) image->blob->data + offsety + offsetx;
  lovrTextureWrite(texture, dstOffset, fullExtent, data, step);
}

void lovrTextureClear(Texture* texture, uint16_t layer, uint16_t layerCount, uint16_t level, uint16_t levelCount, float color[4]) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!texture->info.parent, "Texture views can not be cleared");
  lovrCheck(!isDepthFormat(texture->info.format), "Currently only color textures can be cleared");
  lovrCheck(texture->info.type == TEXTURE_VOLUME || layer + layerCount <= texture->info.depth, "Texture clear range exceeds texture layer count");
  lovrCheck(level + levelCount <= texture->info.mipmaps, "Texture clear range exceeds texture mipmap count");
  gpu_clear_texture(state.transfers, texture->gpu, layer, layerCount, level, levelCount, color);
}

void lovrTextureRead(Texture* texture, uint16_t offset[4], uint16_t extent[3], void (*callback)(void* data, uint32_t size, void* userdata), void* userdata) {
  ReaderPool* readers = &state.readers;
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!texture->info.parent, "Texture views can not be read");
  lovrCheck(texture->info.usage & TEXTURE_COPYFROM, "Texture must have the 'copyfrom' flag to read from it");
  lovrCheck(texture->info.samples == 1, "Multisampled Textures can not be read");
  checkTextureBounds(&texture->info, offset, extent);
  lovrCheck(readers->head - readers->tail != COUNTOF(readers->list), "Too many readbacks"); // TODO emergency waitIdle instead
  size_t size = getTextureRegionSize(texture->info.format, extent[0], extent[1], extent[2]);
  Megaview scratch = bufferAllocate(GPU_MEMORY_CPU_READ, size, 64);
  gpu_copy_texture_buffer(state.transfers, texture->gpu, scratch.gpu, offset, scratch.offset, extent);
  readers->list[readers->head++ & 0xf] = (Reader) {
    .callback = callback,
    .userdata = userdata,
    .data = scratch.data,
    .size = size,
    .tick = state.tick
  };
}

void lovrTextureCopy(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t extent[3]) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!src->info.parent && !dst->info.parent, "Can not copy Texture views");
  lovrCheck(src->info.usage & TEXTURE_COPYFROM, "Texture must have the 'copyfrom' flag to copy from it");
  lovrCheck(dst->info.usage & TEXTURE_COPYTO, "Texture must have the 'copyto' flag to copy to it");
  lovrCheck(src->info.format == dst->info.format, "Copying between Textures requires them to have the same format");
  lovrCheck(src->info.samples == dst->info.samples, "Textures must have the same sample counts to copy between them");
  checkTextureBounds(&src->info, srcOffset, extent);
  checkTextureBounds(&dst->info, dstOffset, extent);
  gpu_copy_textures(state.transfers, src->gpu, dst->gpu, srcOffset, dstOffset, extent);
}

void lovrTextureBlit(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t srcExtent[3], uint16_t dstExtent[3], bool nearest) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!src->info.parent && !dst->info.parent, "Can not blit Texture views");
  lovrCheck(src->info.samples == 1 && dst->info.samples == 1, "Multisampled textures can not be used for blits");
  lovrCheck(src->info.usage & TEXTURE_COPYFROM, "Texture must have the 'copyfrom' flag to blit from it");
  lovrCheck(dst->info.usage & TEXTURE_COPYTO, "Texture must have the 'copyto' flag to blit to it");
  lovrCheck(state.features.formats[src->info.format] & GPU_FEATURE_BLIT, "This GPU does not support blits for the source texture's format");
  lovrCheck(state.features.formats[dst->info.format] & GPU_FEATURE_BLIT, "This GPU does not support blits for the destination texture's format");
  lovrCheck(src->info.format == dst->info.format, "Texture formats must match to blit between them");
  checkTextureBounds(&src->info, srcOffset, srcExtent);
  checkTextureBounds(&dst->info, dstOffset, dstExtent);
  gpu_blit(state.transfers, src->gpu, dst->gpu, srcOffset, dstOffset, srcExtent, dstExtent, nearest);
}

void lovrTextureGenerateMipmaps(Texture* texture) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!texture->info.parent, "Can not generate mipmaps on texture views");
  lovrCheck(texture->info.usage & TEXTURE_COPYFROM, "Texture must have the 'copyfrom' flag to generate mipmaps");
  lovrCheck(texture->info.usage & TEXTURE_COPYTO, "Texture must have the 'copyto' flag to generate mipmaps");
  lovrCheck(state.features.formats[texture->info.format] & GPU_FEATURE_BLIT, "This GPU does not support blits for the texture's format, which is required for mipmap generation");
  gpu_mipgen(state.transfers, texture->gpu);
}

// Sampler

Sampler* lovrSamplerCreate(SamplerInfo* info) {
  lovrCheck(info->range[1] < 0.f || info->range[1] >= info->range[0], "Invalid Sampler mipmap range");

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
    .anisotropy = state.features.anisotropy ? MIN(info->anisotropy, state.limits.anisotropy) : 0.f,
    .lodClamp = { info->range[0], info->range[1] }
  };

  lovrAssert(gpu_sampler_init(sampler->gpu, &gpu), "Failed to initialize sampler");
  return sampler;
}

Sampler* lovrGraphicsGetDefaultSampler(DefaultSampler type) {
  if (state.defaultSamplers[type]) return state.defaultSamplers[type];
  return state.defaultSamplers[type] = lovrSamplerCreate(&(SamplerInfo) {
    .min = type == SAMPLER_NEAREST ? FILTER_NEAREST : FILTER_LINEAR,
    .mag = type == SAMPLER_NEAREST ? FILTER_NEAREST : FILTER_LINEAR,
    .mip = type >= SAMPLER_TRILINEAR ? FILTER_LINEAR : FILTER_NEAREST,
    .wrap = { WRAP_REPEAT, WRAP_REPEAT, WRAP_REPEAT },
    .anisotropy = (type == SAMPLER_ANISOTROPIC && state.features.anisotropy) ? MIN(2.f, state.limits.anisotropy) : 0.f
  });
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
    case 4427: lovrCheck(state.features.extraShaderInputs, "GPU does not support shader feature #%d: %s", capability, "extra shader inputs"); break;
    case 4437: lovrThrow("Shader uses unsupported feature #%d: %s", capability, "multigpu");
    case 4439: lovrCheck(state.limits.renderViews > 1, "GPU does not support shader feature #%d: %s", capability, "multiview"); break;
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
  lovrCheck(instruction < edge && instruction[3] < bound, "Invalid Shader code: id overflow");
  instruction = words + cache[instruction[3]];
  lovrCheck(instruction < edge, "Invalid Shader code: id overflow");

  if ((instruction[0] & 0xffff) == 28) { // OpTypeArray
    // Read array size
    lovrCheck(instruction[3] < bound && words + cache[instruction[3]] < edge, "Invalid Shader code: id overflow");
    const uint32_t* size = words + cache[instruction[3]];
    if ((size[0] & 0xffff) == 43 || (size[0] & 0xffff) == 50) { // OpConstant || OpSpecConstant
      *count = size[3];
    } else {
      lovrThrow("Invalid Shader code: variable %d is an array, but the array size is not a constant", id);
    }

    // Unwrap array to get to inner array type and keep going
    lovrCheck(instruction[2] < bound && words + cache[instruction[2]] < edge, "Invalid Shader code: id overflow");
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
    lovrCheck(instruction < edge, "Invalid Shader code: id overflow");
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

static bool parseSpirv(Shader* shader, const void* source, uint32_t size, uint8_t stage, gpu_slot slots[2][32], uint64_t names[32]) {
  const uint32_t* words = source;
  uint32_t wordCount = size / sizeof(uint32_t);

  if (wordCount < MIN_SPIRV_WORDS || words[0] != 0x07230203) {
    return false;
  }

  uint32_t bound = words[3];
  lovrCheck(bound <= 0xffff, "Unsupported Shader code: id bound is too big (max is 65535)");

  // The cache stores information for spirv ids, allowing everything to be parsed in a single pass
  // - For bundle variables, stores the slot's group, index, and name offset
  // - For vertex attributes, stores the attribute location decoration
  // - For types, stores the index of its declaration instruction
  // Need to study whether aliasing cache as u32 and struct var violates strict aliasing
  struct var { uint8_t group; uint8_t binding; uint16_t name; };
  size_t cacheSize = bound * sizeof(uint32_t);
  void* cache = talloc(cacheSize);
  memset(cache, 0xff, cacheSize);
  struct var* vars = cache;
  uint32_t* locations = cache;
  uint32_t* types = cache;

  const uint32_t* instruction = words + 5;

  while (instruction < words + wordCount) {
    uint16_t opcode = instruction[0] & 0xffff;
    uint16_t length = instruction[0] >> 16;
    uint32_t id;

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
        vars[id].name = instruction - words + 2;
        break;
      case 71: // OpDecorate
        if (length < 4 || instruction[1] >= bound) break;

        id = instruction[1];
        uint32_t decoration = instruction[2];
        uint32_t value = instruction[3];

        if (decoration == 33) { // Binding
          lovrCheck(value < 32, "Unsupported Shader code: variable %d uses binding %d, but the binding must be less than 32", id, value);
          vars[id].binding = value;
        } else if (decoration == 34) { // Group
          lovrCheck(value < 2, "Unsupported Shader code: variable %d is in group %d, but group must be less than 2", id, value);
          vars[id].group = value;
        } else if (decoration == 30) { // Location
          lovrCheck(value < 16, "Unsupported Shader code: vertex shader uses attribute location %d, but locations must be less than 16", value);
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
        uint32_t storageClass = instruction[3];

        // Ignore inputs/outputs and variables that aren't decorated with a group and binding (e.g. globals)
        if (storageClass == 1 || storageClass == 3 || vars[id].group == 0xff || vars[id].binding == 0xff) {
          break;
        }

        uint32_t count;
        gpu_slot_type type;
        parseTypeInfo(words, wordCount, types, bound, instruction, &type, &count);

        uint32_t group = vars[id].group;
        uint32_t number = vars[id].binding;
        gpu_slot* slot = &slots[group][number];

        // Name
        if (group == 1 && !names[number] && vars[id].name != 0xffff) {
          char* name = (char*) (words + vars[id].name);
          names[number] = hash64(name, strnlen(name, 4 * (wordCount - vars[id].name)));
        }

        // Either merge our info into an existing slot, or add the slot
        if (slot->stage != 0) {
          lovrCheck(slot->type == type, "Variable (%d,%d) is in multiple shader stages with different types");
          lovrCheck(slot->count == count, "Variable (%d,%d) is in multiple shader stages with different array lengths");
          slot->stage |= stage;
        } else {
          lovrCheck(count > 0, "Variable (%d,%d) has array length of zero");
          lovrCheck(count < 256, "Variable (%d,%d) has array length of %d, but the max is 255", count);
          slot->number = number;
          slot->type = type;
          slot->stage = stage;
          slot->count = count;
        }
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

Shader* lovrShaderCreate(ShaderInfo* info) {
  Shader* shader = calloc(1, sizeof(Shader) + gpu_sizeof_shader() + gpu_sizeof_pipeline());
  lovrAssert(shader, "Out of memory");
  shader->gpu = (gpu_shader*) (shader + 1);
  shader->info = *info;
  shader->ref = 1;

  gpu_slot slots[2][32] = { 0 };
  uint64_t names[32] = { 0 };

  if (info->type == SHADER_COMPUTE) {
    lovrCheck(info->source[0] && !info->source[1], "Compute shaders require one stage");
    parseSpirv(shader, info->source[0], info->length[0], GPU_STAGE_COMPUTE, slots, names);
  } else {
    lovrCheck(info->source[0] && info->source[1], "Currently, graphics shaders require two stages");
    parseSpirv(shader, info->source[0], info->length[0], GPU_STAGE_VERTEX, slots, names);
    parseSpirv(shader, info->source[1], info->length[1], GPU_STAGE_FRAGMENT, slots, names);
  }

  // Validate built in bindings
  lovrCheck(slots[0][0].type == GPU_SLOT_UNIFORM_BUFFER, "Expected uniform buffer for camera matrices");

  // Preprocess user bindings
  for (uint32_t i = 0; i < COUNTOF(slots[1]); i++) {
    gpu_slot slot = slots[1][i];
    if (!slot.stage) continue;
    uint32_t index = shader->resourceCount++;
    bool buffer = slot.type == GPU_SLOT_UNIFORM_BUFFER || slot.type == GPU_SLOT_STORAGE_BUFFER;
    bool storage = slot.type == GPU_SLOT_STORAGE_BUFFER || slot.type == GPU_SLOT_STORAGE_TEXTURE;
    shader->bufferMask |= (buffer << i);
    shader->textureMask |= (!buffer << i);
    shader->storageMask |= (storage << i);
    shader->resourceSlots[index] = i;
    shader->resourceLookup[index] = names[i];
  }

  // Filter empties from slots, so non-sparse slot list can be used to hash/create layout
  for (uint32_t i = 0; i < shader->resourceCount; i++) {
    if (shader->resourceSlots[i] > i) {
      slots[1][i] = slots[1][shader->resourceSlots[i]];
    }
  }

  if (shader->resourceCount > 0) {
    shader->layout = lookupLayout(slots[1], shader->resourceCount);
  }

  gpu_shader_info gpuInfo = {
    .stages[0] = { info->source[0], info->length[0], NULL },
    .stages[1] = { info->source[1], info->length[1], NULL },
    .layouts[0] = state.layouts[0],
    .layouts[1] = shader->layout,
    .label = info->label
  };

  lovrAssert(gpu_shader_init(shader->gpu, &gpuInfo), "Could not create Shader");

  if (info->type == SHADER_COMPUTE) {
    shader->compute = (gpu_pipeline*) ((char*) shader + gpu_sizeof_shader());
    lovrAssert(gpu_pipeline_init_compute(shader->compute, shader->gpu, NULL), "Could not create Shader compute pipeline");
  }

  return shader;
}

void lovrShaderDestroy(void* ref) {
  Shader* shader = ref;
  gpu_shader_destroy(shader->gpu);
  if (shader->compute) gpu_pipeline_destroy(shader->compute);
  free(shader);
}

const ShaderInfo* lovrShaderGetInfo(Shader* shader) {
  return &shader->info;
}

// Batch

Batch* lovrBatchInit(BatchInfo* info, bool scratch) {
  uint32_t maxBundles = MIN(info->capacity, MAX_BUNDLES);
  size_t sizes[7];
  size_t total = 0;
  total += sizes[0] = sizeof(Batch);
  total += sizes[1] = info->capacity * sizeof(BatchGroup);
  total += sizes[2] = info->capacity * sizeof(BatchDraw);
  total += sizes[3] = maxBundles * sizeof(gpu_bundle_info);
  total += sizes[4] = maxBundles * sizeof(gpu_bundle*);
  total += sizes[5] = scratch ? 0 : maxBundles * gpu_sizeof_bundle();
  total += sizes[6] = scratch ? 0 : gpu_sizeof_bunch();
  char* memory = scratch ? talloc(total) : malloc(total);
  lovrAssert(memory, "Out of memory");
  Batch* batch = (Batch*) memory; memory += sizes[0];
  batch->pass = lookupPass(&info->canvas);
  batch->groups = (BatchGroup*) memory, memory += sizes[1];
  batch->draws = (BatchDraw*) memory, memory += sizes[2];
  batch->bundleWrites = (gpu_bundle_info*) memory, memory += sizes[3];
  batch->bundles = (gpu_bundle**) memory, memory += sizes[4];
  batch->bunch = (gpu_bunch*) memory, memory += sizes[5];
  if (!scratch) {
    for (uint32_t i = 0; i < maxBundles; i++) {
      batch->bundles[i] = (gpu_bundle*) memory, memory += gpu_sizeof_bundle();
    }
  }
  batch->ref = 1;
  batch->scratch = scratch;
  batch->info = *info;
  uint32_t bufferSizes[] = {
    [BUFFER_TRANSFORM] = 64 * info->capacity,
    [BUFFER_DRAW_DATA] = 16 * info->capacity,
    [BUFFER_GEOMETRY] = info->scratchMemory
  };
  for (uint32_t i = 0; i < BUFFER_MAX; i++) {
    batch->scratchpads[i] = bufferAllocate(GPU_MEMORY_CPU_WRITE, bufferSizes[i], 256);
    if (scratch) {
      batch->buffers[i] = batch->scratchpads[i];
    } else {
      batch->buffers[i] = bufferAllocate(GPU_MEMORY_GPU, bufferSizes[i], 256);
    }
  }
  lovrBatchBegin(batch);
  return batch;
}

Batch* lovrGraphicsGetBatch(BatchInfo* info) {
  return lovrBatchInit(info, true);
}

Batch* lovrBatchCreate(BatchInfo* info) {
  return lovrBatchInit(info, false);
}

void lovrBatchDestroy(void* ref) {
  Batch* batch = ref;
  if (!batch->scratch) {
    if (!batch->active && batch->bundleCount > 0) {
      gpu_bunch_destroy(batch->bunch);
    }
    free(batch);
  }
}

const BatchInfo* lovrBatchGetInfo(Batch* batch) {
  return &batch->info;
}

uint32_t lovrBatchGetCount(Batch* batch) {
  return batch->drawCount;
}

void lovrBatchBegin(Batch* batch) {
  batch->active = true;

  batch->groupCount = 0;
  batch->drawCount = 0;

  // Destroy old bunch if it exists (maybe this test for detecting if bunch exists can be improved)
  if (!batch->scratch && !batch->active && batch->bundleCount > 0) {
    gpu_bunch_destroy(batch->bunch);
  }

  batch->bundleCount = 0;
  batch->bindingsDirty = true;
  memset(batch->bindings, 0, sizeof(batch->bindings));
  batch->emptySlotMask = ~0u;

  batch->transformIndex = 0;
  batch->pipelineIndex = 0;

  batch->transform = batch->transforms[batch->transformIndex];
  mat4_identity(batch->transform);

  batch->pipeline = &batch->pipelines[batch->pipelineIndex];
  memset(&batch->pipeline->info, 0, sizeof(batch->pipeline->info));
  batch->pipeline->info.pass = batch->pass;
  batch->pipeline->info.depth.test = GPU_COMPARE_LEQUAL;
  batch->pipeline->info.depth.write = true;
  batch->pipeline->info.colorMask = 0xf;
  batch->pipeline->vertexFormat = 0;
  batch->pipeline->color[0] = 1.f;
  batch->pipeline->color[1] = 1.f;
  batch->pipeline->color[2] = 1.f;
  batch->pipeline->color[3] = 1.f;
  batch->pipeline->dirty = true;
}

void lovrBatchFinish(Batch* batch) {
  if (!batch->active) return;
  batch->active = false;

  // Allocate and write bundles
  if (batch->scratch) {
    for (uint32_t i = 0; i < batch->bundleCount; i++) {
      batch->bundles[i] = bundleAllocate(0);//batch->bundleWrites[i].layout - state.layouts[0]);
    }
  } else {
    gpu_bunch_info info = {
      .bundles = batch->bundles[0],
      .contents = batch->bundleWrites,
      .count = batch->bundleCount
    };

    lovrAssert(gpu_bunch_init(batch->bunch, &info), "Failed to initialize bunch");
  }

  gpu_bundle_write(batch->bundles, batch->bundleWrites, batch->bundleCount);
}

bool lovrBatchIsActive(Batch* batch) {
  return batch->active;
}

void lovrBatchGetViewport(Batch* batch, float viewport[4], float depthRange[2]) {
  memcpy(viewport, batch->viewport, sizeof(batch->viewport));
  memcpy(depthRange, batch->depthRange, sizeof(batch->depthRange));
}

void lovrBatchSetViewport(Batch* batch, float viewport[4], float depthRange[2]) {
  // TODO validate viewport dimensions
  lovrCheck(depthRange[0] >= 0.f && depthRange[0] <= 1.f, "Depth range values must be between 0 and 1");
  lovrCheck(depthRange[1] >= 0.f && depthRange[1] <= 1.f, "Depth range values must be between 0 and 1");
  memcpy(batch->viewport, viewport, sizeof(batch->viewport));
  memcpy(batch->depthRange, depthRange, sizeof(batch->depthRange));
}

void lovrBatchGetScissor(Batch* batch, uint32_t scissor[4]) {
  memcpy(scissor, batch->scissor, sizeof(batch->scissor));
}

void lovrBatchSetScissor(Batch* batch, uint32_t scissor[4]) {
  // TODO validate scissor range
  memcpy(batch->scissor, scissor, sizeof(batch->scissor));
}

void lovrBatchGetViewMatrix(Batch* batch, uint32_t index, float* viewMatrix) {
  lovrCheck(index < COUNTOF(batch->viewMatrix), "Invalid view index %d", index);
  mat4_init(viewMatrix, batch->viewMatrix[index]);
}

void lovrBatchSetViewMatrix(Batch* batch, uint32_t index, float* viewMatrix) {
  lovrCheck(index < COUNTOF(batch->viewMatrix), "Invalid view index %d", index);
  mat4_init(batch->viewMatrix[index], viewMatrix);
}

void lovrBatchGetProjection(Batch* batch, uint32_t index, float* projection) {
  lovrCheck(index < COUNTOF(batch->projection), "Invalid view index %d", index);
  mat4_init(batch->projection[index], projection);
}

void lovrBatchSetProjection(Batch* batch, uint32_t index, float* projection) {
  lovrCheck(index < COUNTOF(batch->projection), "Invalid view index %d", index);
  mat4_init(projection, batch->projection[index]);
}

void lovrBatchPush(Batch* batch, StackType type) {
  if (type == STACK_TRANSFORM) {
    batch->transform = batch->transforms[++batch->transformIndex];
    lovrCheck(batch->transformIndex < COUNTOF(batch->transforms), "Matrix stack overflow (more pushes than pops?)");
    mat4_init(batch->transform, batch->transforms[batch->transformIndex - 1]);
  } else {
    batch->pipeline = &batch->pipelines[++batch->pipelineIndex];
    lovrCheck(batch->pipelineIndex < COUNTOF(batch->pipelines), "Pipeline stack overflow (more pushes than pops?)");
    batch->pipeline = &batch->pipelines[batch->pipelineIndex - 1];
  }
}

void lovrBatchPop(Batch* batch, StackType type) {
  if (type == STACK_TRANSFORM) {
    batch->transform = batch->transforms[--batch->transformIndex];
    lovrCheck(batch->transformIndex < COUNTOF(batch->transforms), "Matrix stack overflow (more pops than pushes?)");
  } else {
    batch->pipeline = &batch->pipelines[--batch->pipelineIndex];
    lovrCheck(batch->pipelineIndex < COUNTOF(batch->pipelines), "Pipeline stack overflow (more pops than pushes?)");
  }
}

void lovrBatchOrigin(Batch* batch) {
  mat4_identity(batch->transform);
}

void lovrBatchTranslate(Batch* batch, vec3 translation) {
  mat4_translate(batch->transform, translation[0], translation[1], translation[2]);
}

void lovrBatchRotate(Batch* batch, quat rotation) {
  mat4_rotateQuat(batch->transform, rotation);
}

void lovrBatchScale(Batch* batch, vec3 scale) {
  mat4_scale(batch->transform, scale[0], scale[1], scale[2]);
}

void lovrBatchTransform(Batch* batch, mat4 transform) {
  mat4_mul(batch->transform, transform);
}

void lovrBatchSetAlphaToCoverage(Batch* batch, bool enabled) {
  batch->pipeline->dirty |= enabled != batch->pipeline->info.alphaToCoverage;
  batch->pipeline->info.alphaToCoverage = enabled;
}

void lovrBatchSetBlendMode(Batch* batch, BlendMode mode, BlendAlphaMode alphaMode) {
  if (mode == BLEND_NONE) {
    batch->pipeline->dirty |= batch->pipeline->info.blend.enabled;
    memset(&batch->pipeline->info.blend, 0, sizeof(gpu_blend_state));
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

  batch->pipeline->info.blend = blendModes[mode];

  if (alphaMode == BLEND_PREMULTIPLIED && mode != BLEND_MULTIPLY) {
    batch->pipeline->info.blend.color.src = GPU_BLEND_ONE;
  }

  batch->pipeline->info.blend.enabled = true;
  batch->pipeline->dirty = true;
}

void lovrBatchSetColorMask(Batch* batch, bool r, bool g, bool b, bool a) {
  uint8_t mask = (r << 0) | (g << 1) | (b << 2) | (a << 3);
  batch->pipeline->dirty |= batch->pipeline->info.colorMask != mask;
  batch->pipeline->info.colorMask = mask;
}

void lovrBatchSetCullMode(Batch* batch, CullMode mode) {
  batch->pipeline->dirty |= batch->pipeline->info.rasterizer.cullMode != (gpu_cull_mode) mode;
  batch->pipeline->info.rasterizer.cullMode = (gpu_cull_mode) mode;
}

void lovrBatchSetDepthTest(Batch* batch, CompareMode test) {
  batch->pipeline->dirty |= batch->pipeline->info.depth.test != (gpu_compare_mode) test;
  batch->pipeline->info.depth.test = (gpu_compare_mode) test;
}

void lovrBatchSetDepthWrite(Batch* batch, bool write) {
  batch->pipeline->dirty |= batch->pipeline->info.depth.write != write;
  batch->pipeline->info.depth.write = write;
}

void lovrBatchSetDepthNudge(Batch* batch, float nudge, float sloped, float clamp) {
  batch->pipeline->info.rasterizer.depthOffset = nudge;
  batch->pipeline->info.rasterizer.depthOffsetSloped = sloped;
  batch->pipeline->info.rasterizer.depthOffsetClamp = clamp;
  batch->pipeline->dirty = true;
}

void lovrBatchSetDepthClamp(Batch* batch, bool clamp) {
  batch->pipeline->dirty |= batch->pipeline->info.rasterizer.depthClamp != clamp;
  batch->pipeline->info.rasterizer.depthClamp = clamp;
}

void lovrBatchSetStencilTest(Batch* batch, CompareMode test, uint8_t value, uint8_t mask) {
  bool hasReplace = false;
  hasReplace |= batch->pipeline->info.stencil.failOp == GPU_STENCIL_REPLACE;
  hasReplace |= batch->pipeline->info.stencil.depthFailOp == GPU_STENCIL_REPLACE;
  hasReplace |= batch->pipeline->info.stencil.passOp == GPU_STENCIL_REPLACE;
  if (hasReplace && test != COMPARE_NONE) {
    lovrCheck(value == batch->pipeline->info.stencil.value, "When stencil write is 'replace' and stencil test is active, their values must match");
  }
  switch (test) { // (Reversed compare mode)
    case COMPARE_NONE: default: batch->pipeline->info.stencil.test = GPU_COMPARE_NONE; break;
    case COMPARE_EQUAL: batch->pipeline->info.stencil.test = GPU_COMPARE_EQUAL; break;
    case COMPARE_NEQUAL: batch->pipeline->info.stencil.test = GPU_COMPARE_NEQUAL; break;
    case COMPARE_LESS: batch->pipeline->info.stencil.test = GPU_COMPARE_GREATER; break;
    case COMPARE_LEQUAL: batch->pipeline->info.stencil.test = GPU_COMPARE_GEQUAL; break;
    case COMPARE_GREATER: batch->pipeline->info.stencil.test = GPU_COMPARE_LESS; break;
    case COMPARE_GEQUAL: batch->pipeline->info.stencil.test = GPU_COMPARE_LEQUAL; break;
  }
  batch->pipeline->info.stencil.testMask = mask;
  if (test != COMPARE_NONE) batch->pipeline->info.stencil.value = value;
  batch->pipeline->dirty = true;
}

void lovrBatchSetStencilWrite(Batch* batch, StencilAction actions[3], uint8_t value, uint8_t mask) {
  bool hasReplace = actions[0] == STENCIL_REPLACE || actions[1] == STENCIL_REPLACE || actions[2] == STENCIL_REPLACE;
  if (hasReplace && batch->pipeline->info.stencil.test != GPU_COMPARE_NONE) {
    lovrCheck(value == batch->pipeline->info.stencil.value, "When stencil write is 'replace' and stencil test is active, their values must match");
  }
  batch->pipeline->info.stencil.failOp = (gpu_stencil_op) actions[0];
  batch->pipeline->info.stencil.depthFailOp = (gpu_stencil_op) actions[1];
  batch->pipeline->info.stencil.passOp = (gpu_stencil_op) actions[2];
  batch->pipeline->info.stencil.writeMask = mask;
  if (hasReplace) batch->pipeline->info.stencil.value = value;
  batch->pipeline->dirty = true;
}

void lovrBatchSetWinding(Batch* batch, Winding winding) {
  batch->pipeline->dirty |= batch->pipeline->info.rasterizer.winding != (gpu_winding) winding;
  batch->pipeline->info.rasterizer.winding = (gpu_winding) winding;
}

void lovrBatchSetWireframe(Batch* batch, bool wireframe) {
  batch->pipeline->dirty |= batch->pipeline->info.rasterizer.wireframe != (gpu_winding) wireframe;
  batch->pipeline->info.rasterizer.wireframe = wireframe;
}

void lovrBatchSetShader(Batch* batch, Shader* shader) {
  Shader* previous = batch->pipeline->shader;
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
        bool differentType = !(previous->bufferMask & mask) == !(shader->bufferMask & mask);
        bool differentStorage = !(previous->storageMask & mask) == !(shader->storageMask & mask);
        batch->emptySlotMask |= (differentType || differentStorage) << shader->resourceSlots[j];
        i++;
        j++;
      }
    }
  }

  uint32_t empties = (shader->bufferMask | shader->textureMask) & batch->emptySlotMask;

  // Assign default bindings to any empty slots used by the shader (TODO biterationtrinsics)
  if (empties) {
    for (uint32_t i = 0; i < 32; i++) {
      if (~empties & (1 << i)) continue;

      if (shader->bufferMask) {
        batch->bindings[i].buffer = (gpu_buffer_binding) {
          .object = state.defaultBuffer->mega.gpu,
          .offset = 0,
          .extent = state.defaultBuffer->info.length
        };
      } else {
        batch->bindings[i].texture = (gpu_texture_binding) {
          .object = state.defaultTexture->gpu,
          .sampler = state.defaultTexture->sampler->gpu
        };
      }

      batch->emptySlotMask &= ~(1 << i);
    }

    batch->bindingsDirty = true;
  }

  lovrRetain(shader);
  lovrRelease(previous, lovrShaderDestroy);
  batch->pipeline->shader = shader;
  batch->pipeline->info.shader = shader ? shader->gpu : NULL;
  batch->pipeline->dirty = true;
}

void lovrBatchBind(Batch* batch, const char* name, size_t length, uint32_t slot, Buffer* buffer, uint32_t offset, Texture* texture) {
  Shader* shader = batch->pipeline->shader;
  uint32_t index;

  lovrCheck(shader, "Batch:bind requires a Shader to be active");

  if (name) {
    bool found = false;
    uint64_t hash = hash64(name, length);
    for (index = 0; index < shader->resourceCount; index++) {
      if (shader->resourceLookup[index] == hash) {
        slot = shader->resourceSlots[index];
        found = true;
        break;
      }
    }
    lovrCheck(found, "Active Shader has no variable named '%s'", name);
  } else {
    bool found = false;
    lovrCheck(slot < COUNTOF(batch->bindings), "Binding slot index (%d) is too big (max is %d)", slot + 1, COUNTOF(batch->bindings) - 1);
    for (index = 0; index < shader->resourceCount; index++) {
      if (shader->resourceSlots[index] == slot) {
        found = true;
        break;
      }
    }
    lovrCheck(found, "Active Shader has no variable in slot %d", slot + 1);
  }

  if (buffer) {
    lovrCheck(shader->bufferMask & (1 << index), "Unable to bind a Buffer to a Texture");
    lovrCheck(offset < buffer->size, "Buffer bind offset is past the end of the Buffer");

    if (shader->storageMask & (1 << index)) {
      lovrCheck(buffer->info.usage & BUFFER_STORAGE, "Buffers must be created with the 'storage' usage to use them as storage buffers");
      lovrCheck(offset % state.limits.storageBufferAlign == 0, "Storage buffer offset (%d) is not aligned to limits.storageBufferAlign (%d)", offset, state.limits.storageBufferAlign);
    } else {
      lovrCheck(buffer->info.usage & BUFFER_UNIFORM, "Buffers must be created with the 'uniform' usage to use them as uniform buffers");
      lovrCheck(offset % state.limits.uniformBufferAlign == 0, "Uniform buffer offset (%d) is not aligned to limits.uniformBufferAlign (%d)", offset, state.limits.uniformBufferAlign);
    }

    batch->bindings[slot].buffer.object = buffer->mega.gpu;
    batch->bindings[slot].buffer.offset = buffer->mega.offset + offset;
    batch->bindings[slot].buffer.extent = MIN(buffer->size - offset, (shader->storageMask & (1 << index)) ? state.limits.storageBufferAlign : state.limits.uniformBufferAlign);
  } else if (texture) {
    lovrCheck(shader->textureMask & (1 << index), "Unable to bind a Texture to a Buffer");

    if (shader->storageMask & (1 << index)) {
      lovrCheck(texture->info.usage & TEXTURE_STORAGE, "Textures must be created with the 'storage' flag to use them as storage textures");
    } else {
      lovrCheck(texture->info.usage & TEXTURE_SAMPLE, "Textures must be created with the 'sample' flag to sample them in shaders");
    }

    batch->bindings[slot].texture.object = texture->gpu;
    batch->bindings[slot].texture.sampler = texture->sampler->gpu;
  }

  batch->emptySlotMask &= ~(1 << slot);
  batch->bindingsDirty = true;
}

typedef struct {
  DrawMode mode;
  float* transform;
  struct {
    Megaview* buffer;
    BufferFormat* format;
    void* data;
    void** pointer;
    uint32_t size;
  } vertex;
  struct {
    Megaview* buffer;
    void* data;
    void** pointer;
    uint32_t size;
    uint32_t stride;
  } index;
  uint32_t start;
  uint32_t count;
  uint32_t instances;
  uint32_t base;
} DrawRequest;

static BatchDraw* lovrBatchDraw(Batch* batch, DrawRequest* req) {
  lovrCheck(batch->drawCount < batch->info.capacity, "Too many draws");
  BatchDraw* draw = &batch->draws[batch->drawCount];
  draw->index = batch->drawCount++;
  draw->key.bits.indirect = 0;
  draw->key.bits.chunk = (draw->index >> 8) & 0xf;

  // Pipeline
  if (batch->pipeline->info.drawMode != (gpu_draw_mode) req->mode) {
    batch->pipeline->info.drawMode = (gpu_draw_mode) req->mode;
    batch->pipeline->dirty = true;
  }

  Shader* shader = batch->pipeline->shader;

  if (!shader) {
    shader = state.defaultShader;
    batch->pipeline->dirty = shader->gpu != batch->pipeline->info.shader;
    batch->pipeline->info.shader = shader->gpu;
  }

  if (req->vertex.format->hash != batch->pipeline->vertexFormat) {
    batch->pipeline->dirty = true;
  }

  if (batch->pipeline->dirty) {
    uint32_t hash = hash64(&batch->pipeline->info, sizeof(gpu_pipeline_info)) & ~0u;
    uint32_t mask = COUNTOF(state.pipelines) - 1;
    uint32_t bucket = hash & mask;

    while (state.pipelineLookup[bucket] && state.pipelineLookup[bucket] >> 32 != hash) {
      bucket = (bucket + 1) & mask;
    }

    if (!state.pipelineLookup[bucket]) {
      uint32_t index = state.pipelineCount++;
      lovrCheck(index < COUNTOF(state.pipelines), "Too many pipelines, please report this encounter");
      lovrAssert(gpu_pipeline_init_graphics(state.pipelines[index], &batch->pipeline->info, 1), "Failed to initialize pipeline");
      state.pipelineLookup[bucket] = ((uint64_t) hash << 32) | index;
    }

    batch->pipeline->index = state.pipelineLookup[bucket] & 0xffff;
    batch->pipeline->dirty = false;
  }

  draw->key.bits.pipeline = batch->pipeline->index;

  // Vertices
  uint32_t vertexOffset;
  if (req->vertex.buffer) {
    draw->key.bits.vertex = req->vertex.buffer->index;
    vertexOffset = req->vertex.buffer->offset / req->vertex.format->stride;
  } else {
    draw->key.bits.vertex = batch->buffers[BUFFER_GEOMETRY].index;

    uint32_t cursor = ALIGN(batch->geometryCursor, req->vertex.format->stride);
    lovrCheck(cursor + req->vertex.size <= batch->info.scratchMemory, "Out of vertex data memory");
    batch->geometryCursor = cursor + req->vertex.size;
    vertexOffset = cursor / req->vertex.format->stride;

    if (req->vertex.pointer) {
      *req->vertex.pointer = batch->scratchpads[BUFFER_GEOMETRY].data + cursor;
    } else {
      memcpy(batch->scratchpads[BUFFER_GEOMETRY].data + cursor, req->vertex.data, req->vertex.size);
    }
  }

  // Indices
  uint32_t indexOffset;
  if (req->index.buffer) {
    draw->key.bits.index = req->index.buffer->index;
    indexOffset = req->index.buffer->offset / req->index.stride;
  } else if (req->index.size > 0) {
    draw->key.bits.index = batch->buffers[BUFFER_GEOMETRY].index;

    uint32_t cursor = ALIGN(batch->geometryCursor, req->index.stride);
    lovrCheck(cursor + req->index.size <= batch->info.scratchMemory, "Out of index data memory");
    batch->geometryCursor = cursor + req->index.size;
    indexOffset = cursor / req->index.stride;

    if (req->index.pointer) {
      *req->index.pointer = batch->scratchpads[BUFFER_GEOMETRY].data + cursor;
    } else {
      memcpy(batch->scratchpads[BUFFER_GEOMETRY].data + cursor, req->index.data, req->index.size);
    }
  }

  // Bundle
  if (batch->bindingsDirty) {
    batch->bindingsDirty = false;
    if (shader->resourceCount > 0) {
      gpu_bundle_info* info = &batch->bundleWrites[batch->bundleCount++];
      info->layout = shader->layout;
      info->bindings = talloc(shader->resourceCount * sizeof(gpu_binding));
      for (uint32_t i = 0; i < shader->resourceCount; i++) {
        info->bindings[i] = batch->bindings[shader->resourceSlots[i]];
      }
    }
  }

  draw->key.bits.bundle = batch->bundleCount - 1;

  // Params
  bool indexed = req->index.buffer || req->index.pointer || req->index.data;
  draw->start = req->start + (indexed ? indexOffset : vertexOffset);
  draw->count = req->count;
  draw->instances = MAX(req->instances, 1);
  draw->base = indexed ? draw->base : 0;

  // Transform
  if (req->transform) {
    float transform[16];
    mat4_init(transform, batch->transform);
    mat4_mul(transform, req->transform);
    memcpy(batch->scratchpads[BUFFER_TRANSFORM].data + draw->index * 64, transform, 64);
  } else {
    memcpy(batch->scratchpads[BUFFER_TRANSFORM].data + draw->index * 64, batch->transform, 16 * sizeof(float));
  }

  // Color
  memcpy(batch->scratchpads[BUFFER_DRAW_DATA].data + draw->index * 4, batch->pipeline->color, 4 * sizeof(float));

  return draw;
}

void lovrBatchCube(Batch* batch, DrawStyle style, float* transform) {
  uint16_t indices[] = {
     0,  1,  2,  2,  1,  3,
     4,  5,  6,  6,  5,  7,
     8,  9, 10, 10,  9, 11,
    12, 13, 14, 14, 13, 15,
    16, 17, 18, 18, 17, 19,
    20, 21, 22, 22, 21, 23
  };

  lovrBatchDraw(batch, &(DrawRequest) {
    .mode = DRAW_TRIANGLES,
    .transform = transform,
    .vertex.buffer = &state.shapes,
    .vertex.format = &state.vertexFormats[VERTEX_FORMAT_STANDARD],
    .index.data = indices,
    .index.size = sizeof(indices),
    .index.stride = sizeof(uint16_t),
    .count = COUNTOF(indices),
    .base = state.baseVertex[SHAPE_CUBE]
  });
}
