#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "headset/headset.h"
#include "math/math.h"
#include "core/gpu.h"
#include "core/maf.h"
#include "core/os.h"
#include "core/util.h"
#include "resources/shaders.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define MAX_BUNDLES UINT16_MAX
#define MAX_BUNCHES 256
#define BUNDLES_PER_BUNCH 1024
#define MAX_LAYOUTS 64

typedef struct {
  gpu_buffer* gpu;
  char* data;
  uint32_t index;
  uint32_t offset;
} Megaview;

struct Buffer {
  uint32_t ref;
  uint32_t size;
  Megaview mega;
  BufferInfo info;
  gpu_vertex_format format;
  uint32_t mask;
  uint64_t hash;
  bool transient;
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
  uint32_t layout;
  uint32_t computePipelineIndex;
  uint32_t resourceCount;
  uint32_t bufferMask;
  uint32_t textureMask;
  uint32_t storageMask;
  uint8_t resourceSlots[32];
  uint64_t resourceLookup[32];
  uint64_t flagLookup[32];
  gpu_shader_flag flags[32];
  uint32_t activeFlagCount;
  uint32_t flagCount;
  uint32_t attributeMask;
};

enum {
  FLAG_VERTEX = (1 << 0),
  FLAG_INDEX = (1 << 1),
  FLAG_INDEX32 = (1 << 2)
};

typedef struct {
  float depth;
  uint16_t pipeline;
  uint16_t bundle;
  uint8_t textures;
  uint8_t chunk;
  uint8_t vertexBuffer;
  uint8_t indexBuffer;
  uint32_t flags;
  uint32_t start;
  uint32_t count;
  uint32_t instances;
  uint32_t baseVertex;
} BatchDraw;

enum {
  DIRTY_PIPELINE = (1 << 0),
  DIRTY_VERTEX = (1 << 1),
  DIRTY_INDEX = (1 << 2),
  DIRTY_CHUNK = (1 << 3),
  DIRTY_BUNDLE = (1 << 4)
};

typedef struct {
  uint16_t count;
  uint16_t dirty;
} BatchGroup;

typedef struct {
  Buffer* buffer;
  uint32_t usage;
  uint32_t stage;
} BufferSync;

typedef struct {
  Texture* texture;
  uint32_t usage;
  uint32_t stage;
} TextureSync;

struct Batch {
  uint32_t ref;
  BatchInfo info;
  gpu_pass* pass;
  BatchDraw* draws;
  uint32_t drawCount;
  BatchGroup* groups;
  uint32_t groupCount;
  uint32_t groupedCount;
  uint32_t* activeDraws;
  uint32_t activeDrawCount;
  float* origins;
  gpu_bundle** bundles;
  gpu_bundle_info* bundleInfo;
  gpu_bunch* bunch;
  uint32_t bundleCount;
  uint32_t lastBundleCount;
  Megaview transforms;
  Megaview colors;
  Megaview stash;
  uint32_t stashCursor;
  arr_t(BufferSync) buffers;
  arr_t(TextureSync) textures;
};

enum {
  SHAPE_QUAD,
  SHAPE_CUBE,
  SHAPE_DISK,
  SHAPE_MAX
};

typedef struct {
  PassType type;
  uint32_t order;
  gpu_stream* stream;
} Pass;

typedef struct {
  float color[4];
  Shader* shader;
  uint64_t format;
  gpu_pipeline_info info;
  uint16_t index;
  bool dirty;
} Pipeline;

typedef struct {
  float view[6][16];
  float projection[6][16];
  float viewProjection[6][16];
  float inverseViewProjection[6][16];
} Camera;

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
  char* pointer;
  gpu_buffer* gpu;
  uint32_t size;
  uint32_t next;
  uint32_t tick;
  uint32_t refs;
} Megabuffer;

typedef struct {
  Megabuffer list[256];
  uint32_t active[3];
  uint32_t oldest[3];
  uint32_t newest[3];
  uint32_t cursor[3];
  uint32_t count;
} BufferPool;

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

typedef struct {
  gpu_texture* handle;
  uint32_t hash;
  uint32_t tick;
} ScratchTexture;

typedef struct {
  char* memory;
  uint32_t cursor;
  uint32_t length;
  uint32_t limit;
} Allocator;

static struct {
  bool initialized;
  bool active;
  uint32_t tick;
  uint32_t passCount;
  Pass passes[32];
  Pass* pass;
  Batch* batch;
  float background[4];
  Camera camera;
  uint32_t viewCount;
  bool cameraDirty;
  float* matrix;
  Pipeline* pipeline;
  uint32_t matrixIndex;
  uint32_t pipelineIndex;
  float matrixStack[16][16];
  Pipeline pipelineStack[4];
  gpu_binding bindings[32];
  uint32_t emptyBindingMask;
  bool bindingsDirty;
  Megaview cameraBuffer;
  Megaview transforms;
  Megaview colors;
  uint32_t drawCursor;
  gpu_pipeline* boundPipeline;
  gpu_bundle* boundBundle;
  gpu_buffer* boundVertexBuffer;
  gpu_buffer* boundIndexBuffer;
  gpu_index_type boundIndexType;
  uint32_t shapeOffset[SHAPE_MAX];
  Buffer* shapes;
  Buffer* abyss;
  Texture* white;
  Texture* window;
  Shader* defaultShaders[DEFAULT_SHADER_COUNT];
  Sampler* defaultSamplers[DEFAULT_SAMPLER_COUNT];
  gpu_vertex_format formats[VERTEX_FORMAT_COUNT];
  uint32_t formatMask[VERTEX_FORMAT_COUNT];
  uint64_t formatHash[VERTEX_FORMAT_COUNT];
  ScratchTexture attachmentCache[16][4];
  ReaderPool readers;
  BufferPool buffers;
  BunchPool bunches;
  uint32_t pipelineCount;
  uint64_t pipelineLookup[4096];
  gpu_pipeline* pipelines[4096];
  uint32_t gpuPassCount;
  uint64_t passKeys[256];
  gpu_pass* gpuPasses[256];
  uint64_t layoutLookup[MAX_LAYOUTS];
  gpu_layout* layouts[MAX_LAYOUTS];
  uint32_t blockSize;
  gpu_hardware hardware;
  gpu_features features;
  gpu_limits limits;
  GraphicsStats stats;
  Allocator allocator;
} state;

static void* talloc(size_t size) {
  while (state.allocator.cursor + size > state.allocator.length) {
    lovrAssert(state.allocator.length << 1 <= state.allocator.limit, "Out of memory");
    os_vm_commit(state.allocator.memory + state.allocator.length, state.allocator.length);
    state.allocator.length <<= 1;
  }

  uint32_t cursor = ALIGN(state.allocator.cursor, 8);
  state.allocator.cursor = cursor + size;
  return state.allocator.memory + cursor;
}

// Like realloc for temp memory, but only supports growing, can be used as arr_t allocator
static void* tgrow(void* p, size_t n) {
  if (n == 0) return NULL;
  void* new = talloc(n);
  if (!p) return new;
  return memcpy(new, p, n);
}

// Returns a Megabuffer to the pool
static void recycleBuffer(uint8_t index, gpu_memory_type type) {
  Megabuffer* buffer = &state.buffers.list[index];
  lovrCheck(buffer->refs == 0, "Trying to release a Buffer while people are still using it");

  // If there's a "newest" Megabuffer, make it the second newest
  if (state.buffers.newest[type] != ~0u) {
    state.buffers.list[state.buffers.newest[type]].next = index;
  }

  // If the waitlist is completely empty, this Megabuffer should become both the oldest and newest
  if (state.buffers.oldest[type] == ~0u) {
    state.buffers.oldest[type] = index;
  }

  // This Megabuffer is the newest
  state.buffers.newest[type] = index;
  buffer->next = ~0u;
  buffer->tick = state.tick;
}

// Suballocates from a Megabuffer
static Megaview allocateBuffer(gpu_memory_type type, uint32_t size, uint32_t align) {
  uint32_t active = state.buffers.active[type];
  uint32_t oldest = state.buffers.oldest[type];
  uint32_t cursor = state.buffers.cursor[type];

  if ((align & (align - 1)) == 0) {
    cursor = ALIGN(cursor, align);
  } else if (cursor % align != 0) {
    cursor += align - cursor % align;
  }

  if (type == GPU_MEMORY_CPU_WRITE) {
    state.stats.scratchMemory += (cursor - state.buffers.cursor[type]) + size;
  }

  // If there's an active Megabuffer and it has room, use it
  if (active != ~0u && cursor + size <= state.buffers.list[active].size) {
    state.buffers.cursor[type] = cursor + size;
    Megabuffer* buffer = &state.buffers.list[active];
    return (Megaview) { .gpu = buffer->gpu, .data = buffer->pointer + cursor, .index = active, .offset = cursor };
  }

  // If the active Megabuffer is full and has no users, it can be reused when GPU is done with it
  if (active != ~0u && state.buffers.list[active].refs == 0) {
    recycleBuffer(active, type);
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
  state.stats.bufferMemory += buffer->size;
  state.stats.memory += buffer->size;

  return (Megaview) { .gpu = buffer->gpu, .data = buffer->pointer, .index = active, .offset = 0 };
}

static gpu_bundle* allocateBundle(uint32_t layout) {
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

  for (uint32_t i = 0; i < state.gpuPassCount; i++) {
    if (state.passKeys[i] == id) {
      return state.gpuPasses[i];
    }
  }

  lovrCheck(state.gpuPassCount < COUNTOF(state.gpuPasses), "Too many passes, please report this encounter");

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
      .save = canvas->depth.texture ? GPU_SAVE_OP_SAVE : GPU_SAVE_OP_DISCARD,
      .stencilSave = canvas->depth.texture ? GPU_SAVE_OP_SAVE : GPU_SAVE_OP_DISCARD
    };
  }

  lovrAssert(gpu_pass_init(state.gpuPasses[state.gpuPassCount], &info), "Failed to initialize pass");
  state.passKeys[state.gpuPassCount] = id;
  return state.gpuPasses[state.gpuPassCount++];
}

static uint32_t lookupLayout(gpu_slot* slots, uint32_t count) {
  uint64_t hash = hash64(slots, count * sizeof(gpu_slot));

  uint32_t index;
  for (index = 0; index < COUNTOF(state.layouts) && state.layoutLookup[index]; index++) {
    if (state.layoutLookup[index] == hash) {
      return index;
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
  return index;
}

static void lovrBatchFinalize(Batch* batch);

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
  } else {
    buffer[count ? -1 : 0] = '\0';
  }
#else
  buffer[count ? -1 : 0] = '\0';
#endif

  return true;
}

bool lovrGraphicsInit(bool debug, bool vsync, uint32_t blockSize) {
  lovrCheck(blockSize <= 1 << 30, "Block size can not exceed 1GB");
  state.blockSize = blockSize;

  // GPU

  gpu_config config = {
    .debug = debug,
    .hardware = &state.hardware,
    .features = &state.features,
    .limits = &state.limits,
    .callback = callback
  };

#ifdef LOVR_VK
  config.vk.getInstanceExtensions = getInstanceExtensions;

#ifndef LOVR_DISABLE_HEADSET
  if (lovrHeadsetDisplayDriver) {
    config.vk.getDeviceExtensions = lovrHeadsetDisplayDriver->getVulkanDeviceExtensions;
    config.vk.getPhysicalDevice = lovrHeadsetDisplayDriver->getVulkanPhysicalDevice;
    config.vk.createInstance = lovrHeadsetDisplayDriver->createVulkanInstance;
    config.vk.createDevice = lovrHeadsetDisplayDriver->createVulkanDevice;
  }
#endif

  if (os_window_is_open()) {
    config.vk.surface = true;
    config.vk.vsync = vsync;
    config.vk.createSurface = os_vk_create_surface;
  }
#endif

  if (!gpu_init(&config)) {
    lovrThrow("Failed to initialize GPU");
  }

  // Heaps

  state.allocator.length = 1 << 14;
  state.allocator.limit = 1 << 30;
  state.allocator.memory = os_vm_init(state.allocator.limit);
  os_vm_commit(state.allocator.memory, state.allocator.length);

  memset(state.buffers.active, 0xff, sizeof(state.buffers.active));
  memset(state.buffers.oldest, 0xff, sizeof(state.buffers.oldest));
  memset(state.buffers.newest, 0xff, sizeof(state.buffers.newest));

  state.buffers.list[0].gpu = malloc(COUNTOF(state.buffers.list) * gpu_sizeof_buffer());
  state.bunches.list[0].gpu = malloc(COUNTOF(state.bunches.list) * gpu_sizeof_bunch());
  state.pipelines[0] = malloc(COUNTOF(state.pipelines) * gpu_sizeof_pipeline());
  state.gpuPasses[0] = malloc(COUNTOF(state.gpuPasses) * gpu_sizeof_pass());
  state.layouts[0] = malloc(COUNTOF(state.layouts) * gpu_sizeof_layout());
  lovrAssert(state.buffers.list[0].gpu && state.bunches.list[0].gpu && state.pipelines[0] && state.gpuPasses[0] && state.layouts[0], "Out of memory");

  for (uint32_t i = 1; i < COUNTOF(state.buffers.list); i++) {
    state.buffers.list[i].gpu = (gpu_buffer*) ((char*) state.buffers.list[0].gpu + i * gpu_sizeof_buffer());
  }

  for (uint32_t i = 1; i < COUNTOF(state.bunches.list); i++) {
    state.bunches.list[i].gpu = (gpu_bunch*) ((char*) state.bunches.list[0].gpu + i * gpu_sizeof_bunch());
  }

  for (uint32_t i = 1; i < COUNTOF(state.pipelines); i++) {
    state.pipelines[i] = (gpu_pipeline*) ((char*) state.pipelines[0] + i * gpu_sizeof_pipeline());
  }

  for (uint32_t i = 1; i < COUNTOF(state.gpuPasses); i++) {
    state.gpuPasses[i] = (gpu_pass*) ((char*) state.gpuPasses[0] + i * gpu_sizeof_pass());
  }

  for (uint32_t i = 1; i < COUNTOF(state.layouts); i++) {
    state.layouts[i] = (gpu_layout*) ((char*) state.layouts[0] + i * gpu_sizeof_layout());
  }

  // Default resources

  // The abyss contains 0.f x 32 followed by 1.f x 32, used for fallback attribute/resource data
  state.abyss = lovrBufferCreate(&(BufferInfo) {
    .length = 256,
    .usage = BUFFER_VERTEX | BUFFER_UNIFORM | BUFFER_STORAGE,
    .label = "abyss"
  });

  Megaview abyss = allocateBuffer(GPU_MEMORY_CPU_WRITE, 256, 4);
  float* f32 = (float*) abyss.data;
  for (uint32_t i = 0; i < 32; i++) {
    f32[i] = 0.f;
    f32[i + 32] = 1.f;
  }

  state.white = lovrTextureCreate(&(TextureInfo) {
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

  lovrTextureSetSampler(state.white, lovrGraphicsGetDefaultSampler(SAMPLER_NEAREST));

  gpu_slot defaultBindings[] = {
    { 0, GPU_SLOT_UNIFORM_BUFFER_DYNAMIC, GPU_STAGE_VERTEX, 1 }, // Camera
    { 1, GPU_SLOT_UNIFORM_BUFFER_DYNAMIC, GPU_STAGE_VERTEX, 1 }, // Transforms
    { 2, GPU_SLOT_UNIFORM_BUFFER_DYNAMIC, GPU_STAGE_VERTEX | GPU_STAGE_FRAGMENT, 1 } // Colors
  };

  lookupLayout(defaultBindings, COUNTOF(defaultBindings));

  // Standard vertex formats

  typedef struct {
    struct { float x, y, z; } position;
    struct { unsigned nx: 10, ny: 10, nz: 10, pad: 2; } normal;
    struct { uint16_t u, v; } uv;
  } Vertex;

  state.formats[VERTEX_STANDARD] = (gpu_vertex_format) {
    .bufferCount = 1,
    .attributeCount = 3,
    .bufferStrides[0] = sizeof(Vertex),
    .attributes[0] = { 0, 0, offsetof(Vertex, position), GPU_TYPE_F32x3 },
    .attributes[1] = { 0, 1, offsetof(Vertex, normal), GPU_TYPE_U10Nx3 },
    .attributes[2] = { 0, 2, offsetof(Vertex, uv), GPU_TYPE_U16Nx2 }
  };

  state.formats[VERTEX_POSITION] = (gpu_vertex_format) {
    .bufferCount = 1,
    .attributeCount = 1,
    .bufferStrides[0] = 12,
    .attributes[0] = { 0, 0, 0, GPU_TYPE_F32x3 }
  };

  state.formats[VERTEX_EMPTY] = (gpu_vertex_format) { 0 };

  for (uint32_t i = 0; i < VERTEX_FORMAT_COUNT; i++) {
    for (uint32_t j = 0; j < state.formats[i].attributeCount; j++) {
      state.formatMask[i] |= (1 << state.formats[i].attributes[j].location);
    }
    state.formatHash[i] = hash64(&state.formats[i], sizeof(state.formats[i]));
  }

  // Shapes

  Vertex quad[] = { // In 10 bit land, 0x200 is 0.0, 0x3ff is 1.0, 0x000 is -1.0
    { { -.5f,  .5f, 0.f }, { 0x200, 0x200, 0x3ff, 0x0 }, { 0x0000, 0x0000 } },
    { {  .5f,  .5f, 0.f }, { 0x200, 0x200, 0x3ff, 0x0 }, { 0xffff, 0x0000 } },
    { { -.5f, -.5f, 0.f }, { 0x200, 0x200, 0x3ff, 0x0 }, { 0x0000, 0xffff } },
    { {  .5f, -.5f, 0.f }, { 0x200, 0x200, 0x3ff, 0x0 }, { 0xffff, 0xffff } }
  };

  Vertex cube[] = {
    { { -.5f, -.5f, -.5f }, { 0x200, 0x200, 0x000, 0x0 }, { 0x0000, 0x0000 } }, // Front
    { { -.5f,  .5f, -.5f }, { 0x200, 0x200, 0x000, 0x0 }, { 0x0000, 0xffff } },
    { {  .5f, -.5f, -.5f }, { 0x200, 0x200, 0x000, 0x0 }, { 0xffff, 0x0000 } },
    { {  .5f,  .5f, -.5f }, { 0x200, 0x200, 0x000, 0x0 }, { 0xffff, 0xffff } },
    { {  .5f,  .5f, -.5f }, { 0x3ff, 0x200, 0x200, 0x0 }, { 0x0000, 0xffff } }, // Right
    { {  .5f,  .5f,  .5f }, { 0x3ff, 0x200, 0x200, 0x0 }, { 0xffff, 0xffff } },
    { {  .5f, -.5f, -.5f }, { 0x3ff, 0x200, 0x200, 0x0 }, { 0x0000, 0x0000 } },
    { {  .5f, -.5f,  .5f }, { 0x3ff, 0x200, 0x200, 0x0 }, { 0xffff, 0x0000 } },
    { {  .5f, -.5f,  .5f }, { 0x200, 0x200, 0x3ff, 0x0 }, { 0x0000, 0x0000 } }, // Back
    { {  .5f,  .5f,  .5f }, { 0x200, 0x200, 0x3ff, 0x0 }, { 0x0000, 0xffff } },
    { { -.5f, -.5f,  .5f }, { 0x200, 0x200, 0x3ff, 0x0 }, { 0xffff, 0x0000 } },
    { { -.5f,  .5f,  .5f }, { 0x200, 0x200, 0x3ff, 0x0 }, { 0xffff, 0xffff } },
    { { -.5f,  .5f,  .5f }, { 0x000, 0x200, 0x200, 0x0 }, { 0x0000, 0xffff } }, // Left
    { { -.5f,  .5f, -.5f }, { 0x000, 0x200, 0x200, 0x0 }, { 0xffff, 0xffff } },
    { { -.5f, -.5f,  .5f }, { 0x000, 0x200, 0x200, 0x0 }, { 0x0000, 0x0000 } },
    { { -.5f, -.5f, -.5f }, { 0x000, 0x200, 0x200, 0x0 }, { 0xffff, 0x0000 } },
    { { -.5f, -.5f, -.5f }, { 0x200, 0x000, 0x200, 0x0 }, { 0x0000, 0x0000 } }, // Bottom
    { {  .5f, -.5f, -.5f }, { 0x200, 0x000, 0x200, 0x0 }, { 0xffff, 0x0000 } },
    { { -.5f, -.5f,  .5f }, { 0x200, 0x000, 0x200, 0x0 }, { 0x0000, 0xffff } },
    { {  .5f, -.5f,  .5f }, { 0x200, 0x000, 0x200, 0x0 }, { 0xffff, 0xffff } },
    { { -.5f,  .5f, -.5f }, { 0x200, 0x3ff, 0x200, 0x0 }, { 0x0000, 0xffff } }, // Top
    { { -.5f,  .5f,  .5f }, { 0x200, 0x3ff, 0x200, 0x0 }, { 0x0000, 0x0000 } },
    { {  .5f,  .5f, -.5f }, { 0x200, 0x3ff, 0x200, 0x0 }, { 0xffff, 0xffff } },
    { {  .5f,  .5f,  .5f }, { 0x200, 0x3ff, 0x200, 0x0 }, { 0xffff, 0x0000 } }
  };

  Vertex disk[256];
  for (uint32_t i = 0; i < 256; i++) {
    float theta = i / 256.f * 2.f * (float) M_PI;
    float x = cosf(theta) * .5f;
    float y = sinf(theta) * .5f;
    disk[i] = (Vertex) { { x, y, 0.f }, { 0x200, 0x200, 0x3ff, 0x0 }, { x * 0xffff, y * 0xffff } };
  }

  uint32_t vertexCount = 0;
  state.shapeOffset[SHAPE_QUAD] = vertexCount, vertexCount += COUNTOF(quad);
  state.shapeOffset[SHAPE_CUBE] = vertexCount, vertexCount += COUNTOF(cube);
  state.shapeOffset[SHAPE_DISK] = vertexCount, vertexCount += COUNTOF(disk);

  state.shapes = lovrBufferCreate(&(BufferInfo) {
    .usage = BUFFER_VERTEX | BUFFER_COPYTO,
    .length = vertexCount,
    .stride = sizeof(Vertex),
    .fieldCount = 3,
    .locations = { 0, 1, 2 },
    .offsets = { offsetof(Vertex, position), offsetof(Vertex, normal), offsetof(Vertex, uv) },
    .types = { FIELD_F32x3, FIELD_U10Nx3, FIELD_U16Nx2 },
    .label = "shapes"
  });
  lovrCheck(state.shapes->hash == state.formatHash[VERTEX_STANDARD], "Unreachable");

  Megaview scratch = allocateBuffer(GPU_MEMORY_CPU_WRITE, vertexCount * sizeof(Vertex), 4);
  memcpy(scratch.data, quad, sizeof(quad)), scratch.data += sizeof(quad);
  memcpy(scratch.data, cube, sizeof(cube)), scratch.data += sizeof(cube);
  memcpy(scratch.data, disk, sizeof(disk)), scratch.data += sizeof(disk);

  // Uploads

  gpu_begin();
  gpu_stream* stream = gpu_stream_begin();
  gpu_copy_buffers(stream, abyss.gpu, state.abyss->mega.gpu, abyss.offset, state.abyss->mega.offset, 256);
  //gpu_clear_texture(stream, state.white->gpu, 0, 1, 0, 1, (float[4]) { 1.f, 1.f, 1.f, 1.f });
  gpu_copy_buffers(stream, scratch.gpu, state.shapes->mega.gpu, scratch.offset, state.shapes->mega.offset, vertexCount * sizeof(Vertex));
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
  for (uint32_t i = 0; i < COUNTOF(state.gpuPasses) && state.passKeys[i]; i++) {
    gpu_pass_destroy(state.gpuPasses[i]);
  }
  for (uint32_t i = 0; i < COUNTOF(state.layouts) && state.layoutLookup[i]; i++) {
    gpu_layout_destroy(state.layouts[i]);
  }
  for (uint32_t i = 0; i < COUNTOF(state.attachmentCache); i++) {
    for (uint32_t j = 0; j < COUNTOF(state.attachmentCache[0]); j++) {
      if (state.attachmentCache[i][j].handle) {
        gpu_texture_destroy(state.attachmentCache[i][j].handle);
        free(state.attachmentCache[i][j].handle);
      }
    }
  }
  for (uint32_t i = 0; i < COUNTOF(state.defaultShaders); i++) {
    lovrRelease(state.defaultShaders[i], lovrShaderDestroy);
  }
  for (uint32_t i = 0; i < COUNTOF(state.defaultSamplers); i++) {
    lovrRelease(state.defaultSamplers[i], lovrSamplerDestroy);
  }
  lovrRelease(state.white, lovrTextureDestroy);
  lovrRelease(state.window, lovrTextureDestroy);
  gpu_destroy();
  free(state.layouts[0]);
  free(state.gpuPasses[0]);
  free(state.pipelines[0]);
  free(state.bunches.list[0].gpu);
  free(state.buffers.list[0].gpu);
  os_vm_free(state.allocator.memory, state.allocator.limit);
  memset(&state, 0, sizeof(state));
}

void lovrGraphicsGetHardware(GraphicsHardware* hardware) {
  hardware->vendorId = state.hardware.vendorId;
  hardware->deviceId = state.hardware.deviceId;
  hardware->deviceName = state.hardware.deviceName;
  hardware->driverMajor = state.hardware.driverMajor;
  hardware->driverMinor = state.hardware.driverMinor;
  hardware->driverPatch = state.hardware.driverPatch;
  hardware->discrete = state.hardware.discrete;
#ifdef LOVR_VK
  hardware->renderer = "vulkan";
#endif
}

void lovrGraphicsGetFeatures(GraphicsFeatures* features) {
  features->bptc = state.features.bptc;
  features->astc = state.features.astc;
  features->wireframe = state.features.wireframe;
  features->depthClamp = state.features.depthClamp;
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
  limits->renderSize[0] = state.limits.renderSize[0];
  limits->renderSize[1] = state.limits.renderSize[1];
  limits->renderSize[2] = MIN(state.limits.renderSize[2], 6);
  limits->uniformBufferRange = state.limits.uniformBufferRange;
  limits->storageBufferRange = state.limits.storageBufferRange;
  limits->uniformBufferAlign = state.limits.uniformBufferAlign;
  limits->storageBufferAlign = state.limits.storageBufferAlign;
  limits->vertexAttributes = state.limits.vertexAttributes;
  limits->vertexBufferStride = state.limits.vertexBufferStride;
  limits->vertexShaderOutputs = state.limits.vertexShaderOutputs;
  memcpy(limits->computeDispatchCount, state.limits.computeDispatchCount, 3 * sizeof(uint32_t));
  memcpy(limits->computeWorkgroupSize, state.limits.computeWorkgroupSize, 3 * sizeof(uint32_t));
  limits->computeWorkgroupVolume = state.limits.computeWorkgroupVolume;
  limits->computeSharedMemory = state.limits.computeSharedMemory;
  limits->indirectDrawCount = state.limits.indirectDrawCount;
  limits->instances = state.limits.instances;
  limits->anisotropy = state.limits.anisotropy;
  limits->pointSize = state.limits.pointSize;
}

void lovrGraphicsGetStats(GraphicsStats* stats) {
  state.stats.blocks = state.buffers.count / (float) COUNTOF(state.buffers.list);
  state.stats.canvases = state.gpuPassCount / (float) COUNTOF(state.gpuPasses);
  state.stats.pipelines = state.pipelineCount / (float) COUNTOF(state.pipelines);
  state.stats.bunches = state.bunches.count / (float) COUNTOF(state.bunches.list);
  *stats = state.stats;
}

void lovrGraphicsPrepare() {
  if (state.active) return;
  state.active = true;
  state.allocator.cursor = 0;

  state.tick = gpu_begin();
  state.passCount = 0;

  state.stats.scratchMemory = 0;
  state.stats.renderPasses = 0;
  state.stats.computePasses = 0;
  state.stats.transferPasses = 0;
  state.stats.pipelineBinds = 0;
  state.stats.bundleBinds = 0;
  state.stats.drawCalls = 0;
  state.stats.dispatches = 0;
  state.stats.workgroups = 0;
  state.stats.copies = 0;

  state.cameraDirty = true;

  // Process any finished readbacks
  ReaderPool* readers = &state.readers;
  while (readers->tail != readers->head && gpu_finished(readers->list[readers->tail & 0xf].tick)) {
    Reader* reader = &readers->list[readers->tail++ & 0xf];
    reader->callback(reader->data, reader->size, reader->userdata);
  }
}

static gpu_texture* getScratchTexture(uint32_t size[2], uint32_t layers, TextureFormat format, bool srgb, uint32_t samples) {
  uint16_t key[] = { size[0], size[1], layers, format, srgb, samples };
  uint32_t hash = (uint32_t) hash64(key, sizeof(key));

  // Search for matching texture in cache table
  gpu_texture* texture;
  uint32_t rows = COUNTOF(state.attachmentCache);
  uint32_t cols = COUNTOF(state.attachmentCache[0]);
  ScratchTexture* row = state.attachmentCache[0] + (hash & (rows - 1)) * cols;
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

static void lovrGraphicsReset(gpu_pass* pass) {
  state.matrixIndex = 0;
  state.matrix = state.matrixStack[0];
  mat4_identity(state.matrix);

  state.pipelineIndex = 0;
  state.pipeline = &state.pipelineStack[0];
  memset(&state.pipeline->info, 0, sizeof(state.pipeline->info));
  state.pipeline->info.pass = pass;
  state.pipeline->info.depth.test = GPU_COMPARE_LEQUAL;
  state.pipeline->info.depth.write = true;
  state.pipeline->info.colorMask = 0xf;
  state.pipeline->format = 0;
  state.pipeline->color[0] = 1.f;
  state.pipeline->color[1] = 1.f;
  state.pipeline->color[2] = 1.f;
  state.pipeline->color[3] = 1.f;
  state.pipeline->shader = NULL;
  state.pipeline->dirty = true;

  state.emptyBindingMask = ~0u;
  state.bindingsDirty = true;

  state.drawCursor = 0;

  state.boundPipeline = NULL;
  state.boundBundle = NULL;
  state.boundVertexBuffer = NULL;
  state.boundIndexBuffer = NULL;
}

void lovrGraphicsBeginRender(Canvas* canvas, uint32_t order) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!state.pass, "Can not start a new pass while one is already active");
  lovrCheck(state.passCount < COUNTOF(state.passes), "Too many passes, try combining passes or breaking work into multiple submissions");

  // Validate Canvas
  const TextureInfo* main = canvas->textures[0] ? &canvas->textures[0]->info : &canvas->depth.texture->info;
  lovrCheck(canvas->textures[0] || canvas->depth.texture, "Canvas must have at least one color or depth texture");
  lovrCheck(main->width <= state.limits.renderSize[0], "Canvas width (%d) exceeds the renderSize limit of this GPU (%d)", main->width, state.limits.renderSize[0]);
  lovrCheck(main->height <= state.limits.renderSize[1], "Canvas height (%d) exceeds the renderSize limit of this GPU (%d)", main->height, state.limits.renderSize[1]);
  lovrCheck(main->depth <= state.limits.renderSize[2], "Canvas view count (%d) exceeds the renderSize limit of this GPU (%d)", main->depth, state.limits.renderSize[2]);
  lovrCheck(canvas->samples == 1 || canvas->samples == 4, "Currently, Canvas sample count must be 1 or 4");

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

  state.viewCount = main->depth;

  state.pass = &state.passes[state.passCount++];
  state.pass->type = PASS_RENDER;
  state.pass->order = order;
  state.pass->stream = gpu_stream_begin();
  gpu_render_begin(state.pass->stream, &target);

  float viewport[4] = { 0.f, 0.f, (float) main->width, (float) main->height };
  float depthRange[2] = { 0.f, 1.f };
  gpu_set_viewport(state.pass->stream, viewport, depthRange);

  uint32_t scissor[4] = { 0, 0, main->width, main->height };
  gpu_set_scissor(state.pass->stream, scissor);

  lovrGraphicsReset(target.pass);
  state.stats.renderPasses++;
}

void lovrGraphicsBeginCompute(uint32_t order) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!state.pass, "Can not start a new pass while one is already active");
  lovrCheck(state.passCount < COUNTOF(state.passes), "Too many passes, try combining passes or breaking work into multiple submissions");
  state.pass = &state.passes[state.passCount++];
  state.pass->type = PASS_COMPUTE;
  state.pass->order = order;
  state.pass->stream = gpu_stream_begin();
  state.emptyBindingMask = ~0u;
  state.bindingsDirty = true;
  state.boundPipeline = NULL;
  state.boundBundle = NULL;
  gpu_compute_begin(state.pass->stream);
  state.stats.computePasses++;
}

void lovrGraphicsBeginTransfer(uint32_t order) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!state.pass, "Can not start a new pass while one is already active");
  lovrCheck(state.passCount < COUNTOF(state.passes), "Too many passes, try combining passes or breaking work into multiple submissions");
  state.pass = &state.passes[state.passCount++];
  state.pass->type = PASS_TRANSFER;
  state.pass->order = order;
  state.pass->stream = gpu_stream_begin();
  state.stats.transferPasses++;
}

void lovrGraphicsBeginBatch(Batch* batch) {
  lovrCheck(state.active, "Graphics is not active");
  lovrCheck(!state.pass, "Can not start a new pass while one is already active");
  lovrCheck(state.passCount < COUNTOF(state.passes), "Too many passes, try combining passes or breaking work into multiple submissions");
  state.pass = &state.passes[state.passCount++];
  state.pass->type = PASS_BATCH;
  state.pass->order = 0;
  state.pass->stream = batch->info.transient ? NULL : gpu_stream_begin();
  state.batch = batch;
  lovrRetain(batch);
  lovrGraphicsReset(batch->pass);
  if (batch->info.transient) {
    lovrBatchReset(batch);
    batch->transforms = allocateBuffer(GPU_MEMORY_CPU_WRITE, batch->info.capacity * 64, state.limits.uniformBufferAlign);
    batch->colors = allocateBuffer(GPU_MEMORY_CPU_WRITE, batch->info.capacity * 16, state.limits.uniformBufferAlign);
    state.transforms = batch->transforms;
    state.colors = batch->colors;
  }
}

void lovrGraphicsFinish() {
  lovrCheck(state.pass, "No pass is active");
  for (uint32_t i = 0; i <= state.pipelineIndex; i++) {
    lovrRelease(state.pipelineStack[i].shader, lovrShaderDestroy);
    state.pipelineStack[i].shader = NULL;
  }
  switch (state.pass->type) {
    case PASS_RENDER: gpu_render_end(state.pass->stream); break;
    case PASS_COMPUTE: gpu_compute_end(state.pass->stream); break;
    case PASS_TRANSFER: break;
    case PASS_BATCH: {
      lovrBatchFinalize(state.batch);
      lovrRelease(state.batch, lovrBatchDestroy);
      state.batch = NULL;
      break;
    }
    default: break;
  }
  state.pass = NULL;
}

static int passcmp(const void* a, const void* b) {
  const Pass* p = a, *q = b;
  return p->order > q->order - p->order < q->order;
}

void lovrGraphicsSubmit() {
  if (!state.active) return;
  if (state.pass) lovrGraphicsFinish();
  state.active = false;

  if (state.window) {
    state.window->gpu = NULL;
    state.window->renderView = NULL;
  }

  qsort(&state.passes, state.passCount, sizeof(Pass), passcmp);

  uint32_t streamCount = 0;
  gpu_stream* streams[COUNTOF(state.passes)];
  for (uint32_t i = 0; i < state.passCount; i++) {
    Pass* pass = &state.passes[i];
    if (!pass->stream) continue;
    gpu_stream* stream = pass->stream;
    streams[streamCount++] = stream;
    gpu_stream_end(stream);
  }

  gpu_submit(streams, streamCount);
}

void lovrGraphicsWait() {
  gpu_wait();
}

void lovrGraphicsGetBackgroundColor(float background[4]) {
  memcpy(background, state.background, 4 * sizeof(float));
}

void lovrGraphicsSetBackgroundColor(float background[4]) {
  memcpy(state.background, background, 4 * sizeof(float));
}

void lovrGraphicsGetViewMatrix(uint32_t index, float* view) {
  lovrCheck(index < COUNTOF(state.camera.view), "Invalid view index %d", index);
  mat4_init(view, state.camera.view[index]);
}

void lovrGraphicsSetViewMatrix(uint32_t index, float* view) {
  lovrCheck(index < COUNTOF(state.camera.view), "Invalid view index %d", index);
  mat4_init(state.camera.view[index], view);
}

void lovrGraphicsGetProjection(uint32_t index, float* projection) {
  lovrCheck(index < COUNTOF(state.camera.projection), "Invalid view index %d", index);
  mat4_init(projection, state.camera.projection[index]);
}

void lovrGraphicsSetProjection(uint32_t index, float* projection) {
  lovrCheck(index < COUNTOF(state.camera.projection), "Invalid view index %d", index);
  mat4_init(state.camera.projection[index], projection);
}

void lovrGraphicsSetViewport(float viewport[4], float depthRange[2]) {
  lovrCheck(state.pass && state.pass->type == PASS_RENDER, "The viewport can only be changed during a render pass");
  lovrCheck(viewport[2] > 0.f && viewport[3] > 0.f, "Viewport dimensions must be greater than zero");
  lovrCheck(depthRange[0] >= 0.f && depthRange[0] <= 1.f, "Depth range values must be between 0 and 1");
  lovrCheck(depthRange[1] >= 0.f && depthRange[1] <= 1.f, "Depth range values must be between 0 and 1");
  gpu_set_viewport(state.pass->stream, viewport, depthRange);
}

void lovrGraphicsSetScissor(uint32_t scissor[4]) {
  lovrCheck(state.pass && state.pass->type == PASS_RENDER, "The scissor can only be changed during a render pass");
  lovrCheck(scissor[2] > 0 && scissor[3] > 0, "Scissor dimensions must be greater than zero");
  gpu_set_scissor(state.pass->stream, scissor);
}

void lovrGraphicsPush(StackType type) {
  if (type == STACK_TRANSFORM) {
    state.matrix = state.matrixStack[++state.matrixIndex];
    lovrCheck(state.matrixIndex < COUNTOF(state.matrixStack), "Transform stack overflow (more pushes than pops?)");
    mat4_init(state.matrix, state.matrixStack[state.matrixIndex - 1]);
  } else {
    state.pipeline = &state.pipelineStack[++state.pipelineIndex];
    lovrCheck(state.pipelineIndex < COUNTOF(state.pipelineStack), "Pipeline stack overflow (more pushes than pops?)");
    memcpy(state.pipeline, &state.pipelineStack[state.pipelineIndex - 1], sizeof(Pipeline));
    lovrRetain(state.pipeline->shader);
  }
}

void lovrGraphicsPop(StackType type) {
  if (type == STACK_TRANSFORM) {
    state.matrix = state.matrixStack[--state.matrixIndex];
    lovrCheck(state.matrixIndex < COUNTOF(state.matrixStack), "Transform stack underflow (more pops than pushes?)");
  } else {
    lovrRelease(state.pipeline->shader, lovrShaderDestroy);
    state.pipeline = &state.pipelineStack[--state.pipelineIndex];
    lovrCheck(state.pipelineIndex < COUNTOF(state.pipelineStack), "Pipeline stack underflow (more pops than pushes?)");
  }
}

void lovrGraphicsOrigin(void) {
  mat4_identity(state.matrix);
}

void lovrGraphicsTranslate(vec3 translation) {
  mat4_translate(state.matrix, translation[0], translation[1], translation[2]);
}

void lovrGraphicsRotate(quat rotation) {
  mat4_rotateQuat(state.matrix, rotation);
}

void lovrGraphicsScale(vec3 scale) {
  mat4_scale(state.matrix, scale[0], scale[1], scale[2]);
}

void lovrGraphicsTransform(mat4 transform) {
  mat4_mul(state.matrix, transform);
}

void lovrGraphicsSetAlphaToCoverage(bool enabled) {
  state.pipeline->dirty |= enabled != state.pipeline->info.alphaToCoverage;
  state.pipeline->info.alphaToCoverage = enabled;
}

void lovrGraphicsSetBlendMode(BlendMode mode, BlendAlphaMode alphaMode) {
  if (mode == BLEND_NONE) {
    state.pipeline->dirty |= state.pipeline->info.blend.enabled;
    memset(&state.pipeline->info.blend, 0, sizeof(gpu_blend_state));
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

  state.pipeline->info.blend = blendModes[mode];

  if (alphaMode == BLEND_PREMULTIPLIED && mode != BLEND_MULTIPLY) {
    state.pipeline->info.blend.color.src = GPU_BLEND_ONE;
  }

  state.pipeline->info.blend.enabled = true;
  state.pipeline->dirty = true;
}

void lovrGraphicsSetColorMask(bool r, bool g, bool b, bool a) {
  uint8_t mask = (r << 0) | (g << 1) | (b << 2) | (a << 3);
  state.pipeline->dirty |= state.pipeline->info.colorMask != mask;
  state.pipeline->info.colorMask = mask;
}

void lovrGraphicsSetColor(float color[4]) {
  memcpy(state.pipeline->color, color, 4 * sizeof(float));
}

void lovrGraphicsSetCullMode(CullMode mode) {
  state.pipeline->dirty |= state.pipeline->info.rasterizer.cullMode != (gpu_cull_mode) mode;
  state.pipeline->info.rasterizer.cullMode = (gpu_cull_mode) mode;
}

void lovrGraphicsSetDepthTest(CompareMode test) {
  state.pipeline->dirty |= state.pipeline->info.depth.test != (gpu_compare_mode) test;
  state.pipeline->info.depth.test = (gpu_compare_mode) test;
}

void lovrGraphicsSetDepthWrite(bool write) {
  state.pipeline->dirty |= state.pipeline->info.depth.write != write;
  state.pipeline->info.depth.write = write;
}

void lovrGraphicsSetDepthNudge(float nudge, float sloped) {
  state.pipeline->info.rasterizer.depthOffset = nudge;
  state.pipeline->info.rasterizer.depthOffsetSloped = sloped;
  state.pipeline->dirty = true;
}

void lovrGraphicsSetDepthClamp(bool clamp) {
  if (state.features.depthClamp) {
    state.pipeline->dirty |= state.pipeline->info.rasterizer.depthClamp != clamp;
    state.pipeline->info.rasterizer.depthClamp = clamp;
  }
}

void lovrGraphicsSetStencilTest(CompareMode test, uint8_t value, uint8_t mask) {
  bool hasReplace = false;
  hasReplace |= state.pipeline->info.stencil.failOp == GPU_STENCIL_REPLACE;
  hasReplace |= state.pipeline->info.stencil.depthFailOp == GPU_STENCIL_REPLACE;
  hasReplace |= state.pipeline->info.stencil.passOp == GPU_STENCIL_REPLACE;
  if (hasReplace && test != COMPARE_NONE) {
    lovrCheck(value == state.pipeline->info.stencil.value, "When stencil write is 'replace' and stencil test is active, their values must match");
  }
  switch (test) { // (Reversed compare mode)
    case COMPARE_NONE: default: state.pipeline->info.stencil.test = GPU_COMPARE_NONE; break;
    case COMPARE_EQUAL: state.pipeline->info.stencil.test = GPU_COMPARE_EQUAL; break;
    case COMPARE_NEQUAL: state.pipeline->info.stencil.test = GPU_COMPARE_NEQUAL; break;
    case COMPARE_LESS: state.pipeline->info.stencil.test = GPU_COMPARE_GREATER; break;
    case COMPARE_LEQUAL: state.pipeline->info.stencil.test = GPU_COMPARE_GEQUAL; break;
    case COMPARE_GREATER: state.pipeline->info.stencil.test = GPU_COMPARE_LESS; break;
    case COMPARE_GEQUAL: state.pipeline->info.stencil.test = GPU_COMPARE_LEQUAL; break;
  }
  state.pipeline->info.stencil.testMask = mask;
  if (test != COMPARE_NONE) state.pipeline->info.stencil.value = value;
  state.pipeline->dirty = true;
}

void lovrGraphicsSetStencilWrite(StencilAction actions[3], uint8_t value, uint8_t mask) {
  bool hasReplace = actions[0] == STENCIL_REPLACE || actions[1] == STENCIL_REPLACE || actions[2] == STENCIL_REPLACE;
  if (hasReplace && state.pipeline->info.stencil.test != GPU_COMPARE_NONE) {
    lovrCheck(value == state.pipeline->info.stencil.value, "When stencil write is 'replace' and stencil test is active, their values must match");
  }
  state.pipeline->info.stencil.failOp = (gpu_stencil_op) actions[0];
  state.pipeline->info.stencil.depthFailOp = (gpu_stencil_op) actions[1];
  state.pipeline->info.stencil.passOp = (gpu_stencil_op) actions[2];
  state.pipeline->info.stencil.writeMask = mask;
  if (hasReplace) state.pipeline->info.stencil.value = value;
  state.pipeline->dirty = true;
}

void lovrGraphicsSetWinding(Winding winding) {
  state.pipeline->dirty |= state.pipeline->info.rasterizer.winding != (gpu_winding) winding;
  state.pipeline->info.rasterizer.winding = (gpu_winding) winding;
}

void lovrGraphicsSetWireframe(bool wireframe) {
  if (state.features.wireframe) {
    state.pipeline->dirty |= state.pipeline->info.rasterizer.wireframe != (gpu_winding) wireframe;
    state.pipeline->info.rasterizer.wireframe = wireframe;
  }
}

void lovrGraphicsSetShader(Shader* shader) {
  Shader* previous = state.pipeline->shader;
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
        state.emptyBindingMask |= (differentType || differentStorage) << shader->resourceSlots[j];
        i++;
        j++;
      }
    }
  }

  uint32_t empties = (shader->bufferMask | shader->textureMask) & state.emptyBindingMask;

  // Assign default bindings to any empty slots used by the shader (TODO biterationtrinsics)
  if (empties) {
    for (uint32_t i = 0; i < 32; i++) {
      if (~empties & (1 << i)) continue;

      if (shader->bufferMask) {
        state.bindings[i].buffer = (gpu_buffer_binding) {
          .object = state.abyss->mega.gpu,
          .offset = 0,
          .extent = state.abyss->info.length
        };
      } else {
        state.bindings[i].texture = (gpu_texture_binding) {
          .object = state.white->gpu,
          .sampler = state.white->sampler->gpu
        };
      }

      state.emptyBindingMask &= ~(1 << i);
    }

    state.bindingsDirty = true;
  }

  lovrRetain(shader);
  lovrRelease(previous, lovrShaderDestroy);

  state.pipeline->shader = shader;
  state.pipeline->info.shader = shader ? shader->gpu : NULL;
  state.pipeline->info.flags = shader ? shader->flags : NULL;
  state.pipeline->info.flagCount = shader ? shader->activeFlagCount : 0;
  state.pipeline->dirty = true;
}

void lovrGraphicsBind(const char* name, size_t length, uint32_t slot, Buffer* buffer, uint32_t offset, Texture* texture) {
  Shader* shader = state.pipeline->shader;
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
    lovrCheck(slot < COUNTOF(state.bindings), "Binding slot index (%d) is too big (max is %d)", slot + 1, COUNTOF(state.bindings) - 1);
    for (index = 0; index < shader->resourceCount; index++) {
      if (shader->resourceSlots[index] == slot) {
        found = true;
        break;
      }
    }
    lovrCheck(found, "Active Shader has no variable in slot %d", slot + 1);
  }

  bool storage = shader->storageMask & (1 << index);

  if (buffer) {
    lovrCheck(shader->bufferMask & (1 << index), "Unable to bind a Buffer to a Texture");
    lovrCheck(offset < buffer->size, "Buffer bind offset is past the end of the Buffer");
    //lovrCheck(batch->transient || !buffer->transient, "Transient buffers can only be bound to transient batches");

    if (storage) {
      lovrCheck(buffer->info.usage & BUFFER_STORAGE, "Buffers must be created with the 'storage' usage to use them as storage buffers");
      lovrCheck(offset % state.limits.storageBufferAlign == 0, "Storage buffer offset (%d) is not aligned to limits.storageBufferAlign (%d)", offset, state.limits.storageBufferAlign);
    } else {
      lovrCheck(buffer->info.usage & BUFFER_UNIFORM, "Buffers must be created with the 'uniform' usage to use them as uniform buffers");
      lovrCheck(offset % state.limits.uniformBufferAlign == 0, "Uniform buffer offset (%d) is not aligned to limits.uniformBufferAlign (%d)", offset, state.limits.uniformBufferAlign);
    }

    state.bindings[slot].buffer.object = buffer->mega.gpu;
    state.bindings[slot].buffer.offset = buffer->mega.offset + offset;
    state.bindings[slot].buffer.extent = MIN(buffer->size - offset, storage ? state.limits.storageBufferAlign : state.limits.uniformBufferAlign);
    //lovrBatchTrackBuffer(buffer, storage ? GPU_BUFFER_STORAGE : GPU_BUFFER_UNIFORM, 0);
  } else if (texture) {
    lovrCheck(shader->textureMask & (1 << index), "Unable to bind a Texture to a Buffer");

    if (storage) {
      lovrCheck(texture->info.usage & TEXTURE_STORAGE, "Textures must be created with the 'storage' flag to use them as storage textures");
    } else {
      lovrCheck(texture->info.usage & TEXTURE_SAMPLE, "Textures must be created with the 'sample' flag to sample them in shaders");
    }

    state.bindings[slot].texture.object = texture->gpu;
    state.bindings[slot].texture.sampler = texture->sampler->gpu;
    //lovrBatchTrackTexture(batch, texture, storage ? GPU_TEXTURE_STORAGE : GPU_TEXTURE_SAMPLE, 0);
  }

  state.emptyBindingMask &= ~(1 << slot);
  state.bindingsDirty = true;
}

uint32_t lovrGraphicsDraw(DrawInfo* info, float* transform) {
  lovrCheck(state.pass && (state.pass->type == PASS_RENDER || state.pass->type == PASS_BATCH), "Drawing can only happen inside of a render pass or batch pass");
  Shader* shader = state.pipeline->shader ? state.pipeline->shader : lovrGraphicsGetDefaultShader(info->shader);
  Batch* batch = state.batch;

  // Pipeline

  gpu_pipeline* pipeline;
  uint32_t pipelineIndex;

  state.pipeline->dirty |= state.pipeline->info.drawMode != (gpu_draw_mode) info->mode;
  state.pipeline->info.drawMode = (gpu_draw_mode) info->mode;

  if (!state.pipeline->shader && shader->gpu != state.pipeline->info.shader) {
    state.pipeline->info.shader = shader->gpu;
    state.pipeline->info.flags = NULL;
    state.pipeline->info.flagCount = 0;
    state.pipeline->dirty = true;
  }

  gpu_vertex_format* format;
  uint64_t formatHash;
  uint32_t formatMask;

  if (info->vertex.buffer) {
    format = &info->vertex.buffer->format;
    formatHash = info->vertex.buffer->hash;
    formatMask = info->vertex.buffer->mask;
  } else {
    format = &state.formats[info->vertex.format];
    formatHash = state.formatHash[info->vertex.format];
    formatMask = state.formatMask[info->vertex.format];
  }

  if (state.pipeline->format != formatHash) {
    state.pipeline->format = formatHash;
    state.pipeline->info.vertex = *format;
    uint32_t missingLocations = shader->attributeMask & ~formatMask;

    if (missingLocations) {
      gpu_vertex_format* vertex = &state.pipeline->info.vertex;
      vertex->bufferCount++;
      vertex->bufferStrides[1] = 0;
      for (uint32_t i = 0; i < 32; i++) {
        if (missingLocations & (1 << i)) {
          vertex->attributes[vertex->attributeCount++] = (gpu_attribute) {
            .buffer = 1,
            .location = i,
            .offset = i == ATTRIBUTE_COLOR ? 128 : 0, // abyss
            .type = GPU_TYPE_F32x4
          };
        }
      }
    }

    state.pipeline->dirty = true;
  }

  if (state.pipeline->dirty) {
    uint32_t hash = hash64(&state.pipeline->info, sizeof(gpu_pipeline_info)) & ~0u;
    uint32_t mask = COUNTOF(state.pipelines) - 1;
    uint32_t bucket = hash & mask;

    while (state.pipelineLookup[bucket] && state.pipelineLookup[bucket] >> 32 != hash) {
      bucket = (bucket + 1) & mask;
    }

    if (!state.pipelineLookup[bucket]) {
      uint32_t index = state.pipelineCount++;
      lovrCheck(index < COUNTOF(state.pipelines), "Too many pipelines, please report this encounter");
      lovrAssert(gpu_pipeline_init_graphics(state.pipelines[index], &state.pipeline->info, 1), "Failed to initialize pipeline");
      state.pipelineLookup[bucket] = ((uint64_t) hash << 32) | index;
      state.pipeline->index = pipelineIndex = index;
      pipeline = state.pipelines[pipelineIndex];
    } else {
      state.pipeline->index = pipelineIndex = state.pipelineLookup[bucket] & 0xffff;
      pipeline = state.pipelines[pipelineIndex];
    }

    state.pipeline->dirty = false;
  } else {
    pipelineIndex = state.pipeline->index;
    pipeline = state.pipelines[pipelineIndex];
  }

  // Bundle

  gpu_bundle* bundle;
  uint32_t bundleIndex;

  if (state.bindingsDirty && shader->resourceCount > 0) {
    gpu_binding* bindings = talloc(shader->resourceCount * sizeof(gpu_binding));

    for (uint32_t i = 0; i < shader->resourceCount; i++) {
      bindings[i] = state.bindings[shader->resourceSlots[i]];
    }

    gpu_bundle_info info = {
      .layout = state.layouts[shader->layout],
      .bindings = bindings
    };

    if (batch) {
      bundleIndex = batch->bundleCount++;
      batch->bundleInfo[bundleIndex] = info;
    } else {
      bundle = allocateBundle(shader->layout);
      gpu_bundle_write(&bundle, &info, 1);
    }

    state.bindingsDirty = false;
  } else {
    bundle = state.boundBundle;
    bundleIndex = batch ? batch->bundleCount - 1 : 0;
  }

  // Buffers

  Megaview vertexBuffer;
  Megaview indexBuffer;

  bool hasVertices = info->vertex.buffer || info->vertex.data || info->vertex.pointer;
  bool hasIndices = info->index.buffer || info->index.data || info->index.pointer;
  gpu_index_type indexType;

  uint32_t start;
  uint32_t count;
  uint32_t baseVertex;

  if (hasVertices) {
    if (info->vertex.buffer) {
      vertexBuffer = info->vertex.buffer->mega;
      start = info->start + vertexBuffer.offset / format->bufferStrides[0];
      count = info->count;
    } else {
      uint32_t stride = format->bufferStrides[0];
      uint32_t size = info->vertex.count * stride;

      vertexBuffer = allocateBuffer(GPU_MEMORY_CPU_WRITE, size, stride);
      start = vertexBuffer.offset / stride;
      count = info->vertex.count;

      if (info->vertex.pointer) {
        *info->vertex.pointer = vertexBuffer.data;
      } else {
        memcpy(vertexBuffer.data, info->vertex.data, size);
      }
    }
  } else if (!hasIndices) {
    start = info->start;
    count = info->count;
  }

  if (hasIndices) {
    baseVertex = info->base + (hasVertices ? start : 0);
    if (info->index.buffer) {
      indexBuffer = info->index.buffer->mega;
      indexType = info->index.buffer->info.stride == 4 ? GPU_INDEX_U32 : GPU_INDEX_U16;
      start = info->start + indexBuffer.offset / info->index.buffer->info.stride;
      count = info->count;
    } else {
      uint32_t stride = info->index.stride;
      uint32_t size = info->index.count * stride;

      indexBuffer = allocateBuffer(GPU_MEMORY_CPU_WRITE, size, stride);
      indexType = stride == 4 ? GPU_INDEX_U32 : GPU_INDEX_U16;
      start = indexBuffer.offset / stride;
      count = info->index.count;

      if (info->index.pointer) {
        *info->index.pointer = indexBuffer.data;
      } else {
        memcpy(indexBuffer.data, info->index.data, size);
      }
    }
  }

  // Persistent batches copy immediate vertex/index data their stash, a device-local buffer
  /*if (batch && !batch->transient) {
    if (!info->vertex.buffer) {
      uint32_t stride = format->bufferStrides[0];
      uint32_t size = info->vertex.count * stride;

      if (batch->stashCursor % stride != 0) {
        batch->stashCursor += stride - batch->stashCursor % stride;
      }

      gpu_buffer_copy(state.pass->stream, vertexBuffer.gpu, batch->stash.gpu, vertexBuffer.offset, batch->stash.offset, 256 * 64);
      vertexBuffer = batch->stash;
      batch->stashCursor += size;
    }

    if (!info->index.buffer) {
      gpu_buffer_copy(state.pass->stream, src->gpu, dst->gpu, src->offset, dst->offset, 256 * 16);
      indexBuffer = batch->stash;
    }
  }*/

  // Uniforms

  // Grab new scratch buffers if they're full/non-existent (does not apply to transient batches)
  if ((state.drawCursor & 0xff) == 0 && (!batch || !batch->info.transient)) {
    if (batch) {
      Megaview *src, *dst;
      src = &state.transforms;
      dst = &batch->transforms;
      gpu_copy_buffers(state.pass->stream, src->gpu, dst->gpu, src->offset, dst->offset, 256 * 64);
      src = &state.colors;
      dst = &batch->colors;
      gpu_copy_buffers(state.pass->stream, src->gpu, dst->gpu, src->offset, dst->offset, 256 * 16);
    }

    state.transforms = allocateBuffer(GPU_MEMORY_CPU_WRITE, 256 * 64, state.limits.uniformBufferAlign);
    state.colors = allocateBuffer(GPU_MEMORY_CPU_WRITE, 256 * 16, state.limits.uniformBufferAlign);
  }

  float m[16];
  if (transform) {
    transform = mat4_mul(mat4_init(m, state.matrix), transform);
  } else {
    transform = state.matrix;
  }

  memcpy(state.transforms.data, transform, 64);
  memcpy(state.colors.data, state.pipeline->color, 16);
  state.transforms.data += 64;
  state.colors.data += 16;

  // Draw

  uint32_t instances = MAX(info->instances, 1);
  uint32_t baseInstance = state.drawCursor;

  if (batch) {
    lovrCheck(batch->drawCount < batch->info.capacity, "Batch is out of draws, try creating it with a higher capacity");

    uint32_t id = batch->drawCount++;
    batch->activeDraws[batch->activeDrawCount++] = id;

    BatchDraw* draw = &batch->draws[id];
    draw->pipeline = (uint16_t) pipelineIndex;
    draw->bundle = (uint16_t) bundleIndex;
    draw->vertexBuffer = hasVertices ? (uint8_t) vertexBuffer.index : 0xff;
    draw->indexBuffer = hasIndices ? (uint8_t) indexBuffer.index : 0xff;
    draw->flags = 0;
    draw->flags |= hasVertices ? FLAG_VERTEX : 0;
    draw->flags |= hasIndices ? FLAG_INDEX : 0;
    draw->flags |= indexType == GPU_INDEX_U32 ? FLAG_INDEX32 : 0;
    draw->start = start;
    draw->count = count;
    draw->instances = instances;
    draw->baseVertex = baseVertex;
    return id;
  } else {
    if (pipeline != state.boundPipeline) {
      gpu_bind_pipeline_graphics(state.pass->stream, pipeline);
      state.boundPipeline = pipeline;
      state.stats.pipelineBinds++;
    }

    // TODO cache this better and use dynamic offsets more and dedupe with replay and stuff
    if (state.cameraDirty || (state.drawCursor & 0xff) == 0) {
      if (state.cameraDirty) {
        for (uint32_t i = 0; i < state.viewCount; i++) {
          mat4_init(state.camera.viewProjection[i], state.camera.projection[i]);
          mat4_mul(state.camera.viewProjection[i], state.camera.view[i]);
          mat4_init(state.camera.inverseViewProjection[i], state.camera.viewProjection[i]);
          mat4_invert(state.camera.inverseViewProjection[i]);
        }

        state.cameraBuffer = allocateBuffer(GPU_MEMORY_CPU_WRITE, sizeof(Camera), state.limits.uniformBufferAlign);
        memcpy(state.cameraBuffer.data, &state.camera, sizeof(Camera));
        state.cameraDirty = false;
      }

      gpu_bundle_info uniforms = {
        .layout = state.layouts[0],
        .bindings = (gpu_binding[]) {
          [0].buffer = { state.cameraBuffer.gpu, state.cameraBuffer.offset, sizeof(Camera) },
          [1].buffer = { state.transforms.gpu, state.transforms.offset, 256 * 64 },
          [2].buffer = { state.colors.gpu, state.colors.offset, 256 * 16 }
        }
      };
      gpu_bundle* uniformBundle = allocateBundle(0);
      gpu_bundle_write(&uniformBundle, &uniforms, 1);
      uint32_t dynamicOffsets[3] = { 0 };
      gpu_bind_bundle(state.pass->stream, pipeline, 0, uniformBundle, dynamicOffsets, 3);
    }

    if (bundle != state.boundBundle) {
      gpu_bind_bundle(state.pass->stream, pipeline, 1, bundle, NULL, 0);
      state.boundBundle = bundle;
      state.stats.bundleBinds++;
    }

    if (hasVertices && vertexBuffer.gpu != state.boundVertexBuffer) {
      gpu_buffer* buffers[2] = { vertexBuffer.gpu, state.abyss->mega.gpu };
      gpu_bind_vertex_buffers(state.pass->stream, buffers, (uint32_t[2]) { 0, 0 }, 0, 2);
      state.boundVertexBuffer = vertexBuffer.gpu;
    }

    if (hasIndices && (indexBuffer.gpu != state.boundIndexBuffer || indexType != state.boundIndexType)) {
      gpu_bind_index_buffer(state.pass->stream, indexBuffer.gpu, 0, indexType);
      state.boundIndexBuffer = indexBuffer.gpu;
      state.boundIndexType = indexType;
    }

    if (hasIndices) {
      gpu_draw_indexed(state.pass->stream, count, instances, start, baseVertex, baseInstance);
    } else {
      gpu_draw(state.pass->stream, count, instances, start, baseInstance);
    }

    return state.drawCursor++;
  }
}

uint32_t lovrGraphicsPoints(uint32_t count, float** vertices) {
  return lovrGraphicsDraw(&(DrawInfo) {
    .mode = DRAW_POINTS,
    .vertex.format = VERTEX_POSITION,
    .vertex.pointer = (void**) vertices,
    .vertex.count = count
  }, NULL);
}

uint32_t lovrGraphicsLine(uint32_t count, float** vertices) {
  uint16_t* indices;

  uint32_t id = lovrGraphicsDraw(&(DrawInfo) {
    .mode = DRAW_LINES,
    .vertex.format = VERTEX_POSITION,
    .vertex.pointer = (void**) vertices,
    .vertex.count = count,
    .index.pointer = (void**) &indices,
    .index.count = 2 * (count - 1),
    .index.stride = sizeof(*indices)
  }, NULL);

  for (uint32_t i = 0; i < count; i++) {
    indices[2 * i + 0] = i;
    indices[2 * i + 1] = i + 1;
  }

  return id;
}

uint32_t lovrGraphicsPlane(float* transform, uint32_t segments) {
  static const uint16_t indices[] = { 0,  1,  2,  1, 2, 3 };
  return lovrGraphicsDraw(&(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .vertex.buffer = state.shapes,
    .index.data = indices,
    .index.count = COUNTOF(indices),
    .index.stride = sizeof(uint16_t),
    .base = state.shapeOffset[SHAPE_QUAD]
  }, transform);
}

uint32_t lovrGraphicsBox(float* transform) {
  static const uint16_t indices[] = {
     0,  1,  2,  2,  1,  3,
     4,  5,  6,  6,  5,  7,
     8,  9, 10, 10,  9, 11,
    12, 13, 14, 14, 13, 15,
    16, 17, 18, 18, 17, 19,
    20, 21, 22, 22, 21, 23
  };

  return lovrGraphicsDraw(&(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .vertex.buffer = state.shapes,
    .index.data = indices,
    .index.count = COUNTOF(indices),
    .index.stride = sizeof(uint16_t),
    .base = state.shapeOffset[SHAPE_CUBE]
  }, transform);
}

uint32_t lovrGraphicsCircle(float* transform, uint32_t detail) {
  detail = MIN(detail, 6);
  uint16_t vertexCount = 4 << detail; // 4, 8, 16, 32, 64, 128, 256
  uint16_t vertexSkip = 64 >> detail; // 64, 32, 16, 8, 4, 2, 1
  uint16_t indexCount = 3 * (vertexCount - 2);

  uint16_t* indices;
  uint32_t id = lovrGraphicsDraw(&(DrawInfo) {
    .mode = DRAW_TRIANGLES,
    .vertex.buffer = state.shapes,
    .index.pointer = (void**) &indices,
    .index.count = indexCount,
    .index.stride = sizeof(uint16_t),
    .base = state.shapeOffset[SHAPE_DISK]
  }, transform);

  for (uint16_t i = 0, j = vertexSkip; i < indexCount; i += 3, j += vertexSkip) {
    indices[i + 0] = 0;
    indices[i + 1] = j;
    indices[i + 2] = j + vertexSkip;
  }

  return id;
}

uint32_t lovrGraphicsCylinder(mat4 transform, float r1, float r2, bool capped, uint32_t detail) {
  lovrThrow("TODO");
}

uint32_t lovrGraphicsSphere(mat4 transform, uint32_t segments) {
  lovrThrow("TODO");
}

uint32_t lovrGraphicsSkybox(Texture* texture) {
  lovrThrow("TODO");
}

uint32_t lovrGraphicsFill(Texture* texture) {
  lovrThrow("TODO");
}

uint32_t lovrGraphicsModel(Model* model, mat4 transform, uint32_t node, bool children, uint32_t instances) {
  lovrThrow("TODO");
}

uint32_t lovrGraphicsPrint(Font* font, const char* text, uint32_t length, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign) {
  lovrThrow("TODO");
}

void lovrGraphicsReplay(Batch* batch) {
  lovrCheck(state.pass && state.pass->type == PASS_RENDER, "Replaying a Batch can only happen inside a render pass");
  if (batch->activeDrawCount == 0) return;

  // Uniforms

  if (state.cameraDirty) {
    for (uint32_t i = 0; i < state.viewCount; i++) {
      mat4_init(state.camera.viewProjection[i], state.camera.projection[i]);
      mat4_mul(state.camera.viewProjection[i], state.camera.view[i]);
      mat4_init(state.camera.inverseViewProjection[i], state.camera.viewProjection[i]);
      mat4_invert(state.camera.inverseViewProjection[i]);
    }

    state.cameraBuffer = allocateBuffer(GPU_MEMORY_CPU_WRITE, sizeof(Camera), state.limits.uniformBufferAlign);
    memcpy(state.cameraBuffer.data, &state.camera, sizeof(Camera));
    state.cameraDirty = false;
  }

  gpu_bundle_info uniforms = {
    .layout = state.layouts[0],
    .bindings = (gpu_binding[]) {
      [0].buffer = { state.cameraBuffer.gpu, state.cameraBuffer.offset, sizeof(Camera) },
      [1].buffer = { batch->transforms.gpu, batch->transforms.offset, 256 * 64 },
      [2].buffer = { batch->colors.gpu, batch->colors.offset, 256 * 16 }
    }
  };
  gpu_bundle* uniformBundle = allocateBundle(0);
  gpu_bundle_write(&uniformBundle, &uniforms, 1);
  uint32_t dynamicOffsets[3] = { 0 };

  // Group draws

  if (batch->groupedCount == 0) {
    BatchGroup* group = &batch->groups[0];
    BatchDraw* draw = &batch->draws[batch->activeDraws[0]];
    group->count = 1;
    group->dirty = 0;
    group->dirty |= DIRTY_PIPELINE;
    group->dirty |= (draw->flags & FLAG_VERTEX) ? DIRTY_VERTEX : 0;
    group->dirty |= (draw->flags & FLAG_INDEX) ? DIRTY_INDEX : 0;
    group->dirty |= DIRTY_CHUNK;
    group->dirty |= batch->bundleCount > 0 ? DIRTY_BUNDLE : 0;
    batch->groupedCount = 1;
    batch->groupCount = 1;
  }

  for (uint32_t i = batch->groupedCount; i < batch->activeDrawCount; i++, batch->groupedCount++) {
    BatchDraw* a = &batch->draws[batch->activeDraws[i - 0]];
    BatchDraw* b = &batch->draws[batch->activeDraws[i - 1]];

    bool pipelineChanged = a->pipeline != b->pipeline;
    bool vertexBufferChanged = (a->flags & FLAG_VERTEX) > (b->flags & FLAG_VERTEX) || a->vertexBuffer != b->vertexBuffer;
    bool indexBufferChanged = (a->flags & FLAG_INDEX) > (b->flags & FLAG_INDEX) || a->indexBuffer != b->indexBuffer;
    bool indexTypeChanged = (a->flags & FLAG_INDEX32) != (b->flags & FLAG_INDEX32);
    bool chunkChanged = batch->activeDraws[i] >> 8 != batch->activeDraws[i - 1] >> 8;
    bool bundleChanged = a->bundle != b->bundle;

    uint16_t dirty = 0;
    dirty |= -pipelineChanged & DIRTY_PIPELINE;
    dirty |= -vertexBufferChanged & DIRTY_VERTEX;
    dirty |= -(indexBufferChanged || indexTypeChanged) & DIRTY_INDEX;
    dirty |= -chunkChanged & DIRTY_CHUNK;
    dirty |= -bundleChanged & DIRTY_BUNDLE;

    if (dirty) {
      batch->groups[batch->groupCount++] = (BatchGroup) { .dirty = dirty, .count = 1 };
    } else {
      batch->groups[batch->groupCount - 1].count++;
    }
  }

  // Draws

  uint32_t activeDrawIndex = 0;
  BatchGroup* group = batch->groups;
  for (uint32_t i = 0; i < batch->groupCount; i++, group++) {
    uint32_t index = batch->activeDraws[activeDrawIndex];
    BatchDraw* first = &batch->draws[index];
    gpu_pipeline* pipeline = state.pipelines[first->pipeline];

    if (group->dirty & DIRTY_PIPELINE) {
      gpu_bind_pipeline_graphics(state.pass->stream, pipeline);
      state.boundPipeline = pipeline;
      state.stats.pipelineBinds++;
    }

    if (group->dirty & DIRTY_VERTEX) {
      uint32_t offsets[2] = { 0, 0 };
      gpu_buffer* buffers[2] = { state.buffers.list[first->vertexBuffer].gpu, state.abyss->mega.gpu };
      gpu_bind_vertex_buffers(state.pass->stream, buffers, offsets, 0, 2);
      state.boundVertexBuffer = buffers[0];
    }

    if (group->dirty & DIRTY_INDEX) {
      gpu_index_type type = (first->flags & FLAG_INDEX32) ? GPU_INDEX_U32 : GPU_INDEX_U16;
      gpu_bind_index_buffer(state.pass->stream, state.buffers.list[first->indexBuffer].gpu, 0, type);
      state.boundIndexBuffer = state.buffers.list[first->indexBuffer].gpu;
      state.boundIndexType = type;
    }

    if (group->dirty & DIRTY_CHUNK) {
      dynamicOffsets[0] = (index >> 8) * 256 * 64;
      dynamicOffsets[1] = (index >> 8) * 256 * 16;
      gpu_bind_bundle(state.pass->stream, pipeline, 0, uniformBundle, dynamicOffsets, 3);
    }

    if (group->dirty & DIRTY_BUNDLE) {
      gpu_bind_bundle(state.pass->stream, pipeline, 1, batch->bundles[first->bundle], NULL, 0);
      state.boundBundle = batch->bundles[first->bundle];
      state.stats.bundleBinds++;
    }

    if (first->flags & FLAG_INDEX) {
      for (uint16_t j = 0; j < group->count; j++) {
        index = batch->activeDraws[activeDrawIndex++];
        BatchDraw* draw = &batch->draws[index];
        gpu_draw_indexed(state.pass->stream, draw->count, draw->instances, draw->start, draw->baseVertex, index);
      }
    } else {
      for (uint16_t j = 0; j < group->count; j++) {
        index = batch->activeDraws[activeDrawIndex++];
        BatchDraw* draw = &batch->draws[index];
        gpu_draw(state.pass->stream, draw->count, draw->instances, draw->start, index);
      }
    }
  }

  state.stats.drawCalls += batch->activeDrawCount;
}

void lovrGraphicsCompute(uint32_t x, uint32_t y, uint32_t z, Buffer* buffer, uint32_t offset) {
  lovrCheck(state.pass && state.pass->type == PASS_COMPUTE, "Compute shaders can only run inside of a compute pass");

  Shader* shader = state.pipeline->shader;
  lovrCheck(shader && shader->info.type == SHADER_COMPUTE, "A compute shader must be bound before dispatching compute work");

  lovrCheck(x <= state.limits.computeDispatchCount[0], "Compute %s count exceeds computeDispatchCount limit", "x");
  lovrCheck(y <= state.limits.computeDispatchCount[1], "Compute %s count exceeds computeDispatchCount limit", "y");
  lovrCheck(z <= state.limits.computeDispatchCount[2], "Compute %s count exceeds computeDispatchCount limit", "z");

  gpu_pipeline* pipeline = state.pipelines[shader->computePipelineIndex];

  if (pipeline != state.boundPipeline) {
    gpu_bind_pipeline_compute(state.pass->stream, pipeline);
    state.boundPipeline = pipeline;
  }

  if (state.bindingsDirty && shader->resourceCount > 0) {
    gpu_binding* bindings = talloc(shader->resourceCount * sizeof(gpu_binding));

    for (uint32_t i = 0; i < shader->resourceCount; i++) {
      bindings[i] = state.bindings[shader->resourceSlots[i]];
    }

    gpu_bundle_info info = {
      .layout = state.layouts[shader->layout],
      .bindings = bindings
    };

    gpu_bundle* bundle = allocateBundle(shader->layout);
    gpu_bundle_write(&bundle, &info, 1);
    gpu_bind_bundle(state.pass->stream, pipeline, 1, bundle, NULL, 0);
    state.boundBundle = bundle;
    state.bindingsDirty = false;
  }

  if (buffer) {
    lovrCheck(buffer->info.usage & BUFFER_INDIRECT, "A Buffer must be created with the 'indirect' usage to use it for indirect compute");
    lovrCheck(offset % 4 == 0, "Indirect compute offset must be a multiple of 4");
    lovrCheck(offset + 12 <= buffer->size, "Indirect compute offset overflows the Buffer");
    gpu_compute_indirect(state.pass->stream, buffer->mega.gpu, buffer->mega.offset + offset);
  } else {
    gpu_compute(state.pass->stream, x, y, z);
  }
}

// Buffer

Buffer* lovrBufferInit(BufferInfo* info, bool transient) {
  bool po2 = (info->stride & (info->stride - 1)) == 0;
  size_t size = info->length * MAX(info->stride, 1);
  lovrCheck(size > 0, "Buffer size must be positive");
  lovrCheck(size <= 1 << 30, "Max Buffer size is 1GB");
  Buffer* buffer = transient ? talloc(sizeof(Buffer)) : calloc(1, sizeof(Buffer));
  lovrAssert(buffer, "Out of memory");
  buffer->ref = 1;
  buffer->size = size;
  uint32_t align = 1;
  if (info->usage & BUFFER_UNIFORM) align = MAX(align, state.limits.uniformBufferAlign);
  if (info->usage & BUFFER_STORAGE) align = MAX(align, state.limits.storageBufferAlign);
  if (info->usage & BUFFER_VERTEX) {
    if (po2) {
      align = MAX(align, info->stride);
    } else {
      lovrCheck(~info->usage & (BUFFER_UNIFORM | BUFFER_STORAGE), "Buffers with the 'vertex' usage and either 'uniform' or 'storage' must have a power of 2 stride");
      align = info->stride;
    }
  }
  buffer->mega = allocateBuffer(transient ? GPU_MEMORY_CPU_WRITE : GPU_MEMORY_GPU, size, align);
  buffer->info = *info;
  buffer->transient = transient;
  if (!transient) {
    state.buffers.list[buffer->mega.index].refs++;
  }
  if (info->usage & BUFFER_VERTEX) {
    lovrCheck(info->stride < state.limits.vertexBufferStride, "Buffer with 'vertex' usage has a stride of %d bytes, which exceeds limits.vertexBufferStride (%d)", info->stride, state.limits.vertexBufferStride);
    buffer->mask = 0;
    buffer->format.bufferCount = 1;
    buffer->format.attributeCount = info->fieldCount;
    buffer->format.bufferStrides[0] = info->stride;
    for (uint32_t i = 0; i < info->fieldCount; i++) {
      lovrCheck(info->locations[i] < 16, "Vertex buffer attribute location %d is too big (max is 15)", info->locations[i]);
      lovrCheck(info->offsets[i] < 256, "Vertex buffer attribute offset %d is too big (max is 255)", info->offsets[i]);
      buffer->format.attributes[i] = (gpu_attribute) {
        .buffer = 0,
        .location = info->locations[i],
        .offset = info->offsets[i],
        .type = (gpu_type) info->types[i]
      };
      buffer->mask |= 1 << info->locations[i];
    }
    buffer->hash = hash64(&buffer->format, sizeof(buffer->format));
  }
  return buffer;
}

Buffer* lovrGraphicsGetBuffer(BufferInfo* info) {
  return lovrBufferInit(info, true);
}

Buffer* lovrBufferCreate(BufferInfo* info) {
  state.stats.buffers++;
  return lovrBufferInit(info, false);
}

void lovrBufferDestroy(void* ref) {
  Buffer* buffer = ref;
  if (!buffer->transient) {
    if (--state.buffers.list[buffer->mega.index].refs == 0) {
      recycleBuffer(buffer->mega.index, GPU_MEMORY_GPU);
    }
    state.stats.buffers--;
    free(buffer);
  }
}

const BufferInfo* lovrBufferGetInfo(Buffer* buffer) {
  return &buffer->info;
}

void* lovrBufferMap(Buffer* buffer, uint32_t offset, uint32_t size) {
  if (size == ~0u) size = buffer->size - offset;
  lovrCheck(offset + size <= buffer->size, "Tried to write past the end of the Buffer");
  if (buffer->transient) {
    lovrCheck(state.active, "Graphics is not active");
    return buffer->mega.data + offset;
  } else {
    lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Writing to persistent buffers can only happen in a transfer pass");
    lovrCheck(buffer->info.usage & BUFFER_COPYTO, "Buffers must have the 'copyto' usage to write to them");
    Megaview scratch = allocateBuffer(GPU_MEMORY_CPU_WRITE, size, 4);
    gpu_copy_buffers(state.pass->stream, scratch.gpu, buffer->mega.gpu, scratch.offset, buffer->mega.offset + offset, size);
    state.stats.copies++;
    return scratch.data;
  }
}

void lovrBufferClear(Buffer* buffer, uint32_t offset, uint32_t size) {
  lovrCheck(offset % 4 == 0, "Buffer clear offset must be a multiple of 4");
  lovrCheck(size % 4 == 0, "Buffer clear size must be a multiple of 4");
  lovrCheck(offset + size <= buffer->size, "Tried to clear past the end of the Buffer");
  if (buffer->transient) {
    lovrCheck(state.active, "Graphics is not active");
    memset(buffer->mega.data + offset, 0, size);
  } else {
    lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Clearing persistent buffers can only happen in a transfer pass");
    lovrCheck(buffer->info.usage & BUFFER_COPYTO, "Buffers must have the 'copyto' usage to clear them");
    gpu_clear_buffer(state.pass->stream, buffer->mega.gpu, buffer->mega.offset + offset, size, 0);
    state.stats.copies++;
  }
}

void lovrBufferCopy(Buffer* src, Buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size) {
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Buffer copies can only happen in a transfer pass");
  lovrCheck(src->info.usage & BUFFER_COPYFROM, "Buffer must have the 'copyfrom' usage to copy from it");
  lovrCheck(dst->info.usage & BUFFER_COPYTO, "Buffer must have the 'copyto' usage to copy to it");
  lovrCheck(srcOffset + size <= src->size, "Tried to read past the end of the source Buffer");
  lovrCheck(dstOffset + size <= dst->size, "Tried to copy past the end of the destination Buffer");
  gpu_copy_buffers(state.pass->stream, src->mega.gpu, dst->mega.gpu, src->mega.offset + srcOffset, dst->mega.offset + dstOffset, size);
  state.stats.copies++;
}

void lovrBufferRead(Buffer* buffer, uint32_t offset, uint32_t size, void (*callback)(void*, uint32_t, void*), void* userdata) {
  ReaderPool* readers = &state.readers;
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Reading buffer data can only happen in a transfer pass");
  lovrCheck(buffer->info.usage & BUFFER_COPYFROM, "Buffer must have the 'copyfrom' usage to read from it");
  lovrCheck(offset + size <= buffer->size, "Tried to read past the end of the Buffer");
  lovrCheck(readers->head - readers->tail != COUNTOF(readers->list), "Too many readbacks"); // TODO emergency waitIdle instead
  Megaview scratch = allocateBuffer(GPU_MEMORY_CPU_READ, size, 4);
  gpu_copy_buffers(state.pass->stream, buffer->mega.gpu, scratch.gpu, buffer->mega.offset + offset, scratch.offset, size);
  readers->list[readers->head++ & 0xf] = (Reader) {
    .callback = callback,
    .userdata = userdata,
    .data = scratch.data,
    .size = size,
    .tick = state.tick
  };
  state.stats.copies++;
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
    int width, height;
    os_window_get_fbsize(&width, &height); // Currently immutable
    state.window->ref = 1;
    state.window->info.type = TEXTURE_2D;
    state.window->info.usage = TEXTURE_RENDER;
    state.window->info.format = ~0u;
    state.window->info.width = width;
    state.window->info.height = height;
    state.window->info.depth = 1;
    state.window->info.mipmaps = 1;
    state.window->info.samples = 1;
    state.window->info.srgb = true;
  }

  if (!state.window->gpu) {
    state.window->gpu = gpu_surface_acquire();
    state.window->renderView = state.window->gpu;
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
  lovrCheck(info->samples == 1 || info->samples == 4, "Currently, Texture multisample count must be 1 or 4");
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
  lovrCheck((info->format != FORMAT_BC6 && info->format != FORMAT_BC7) || state.features.bptc, "BC6/BC7 textures are not supported on this GPU");
  lovrCheck(info->format < FORMAT_ASTC_4x4 || state.features.astc, "ASTC textures are not supported on this GPU");

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
    .samples = MAX(info->samples, 1),
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

  if (!info->handle) {
    uint32_t size = getTextureRegionSize(info->format, info->width, info->height, info->depth);
    state.stats.memory += size;
    state.stats.textureMemory += size;
  }

  state.stats.textures++;
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

  state.stats.textures++;
  lovrRetain(view->parent);
  return texture;
}

void lovrTextureDestroy(void* ref) {
  Texture* texture = ref;
  const TextureInfo* info = &texture->info;
  lovrRelease(texture->info.parent, lovrTextureDestroy);
  lovrRelease(texture->sampler, lovrSamplerDestroy);
  if (texture->renderView && texture->renderView != texture->gpu) gpu_texture_destroy(texture->renderView);
  if (texture->gpu) gpu_texture_destroy(texture->gpu);
  if (!info->parent && !info->handle && info->format != ~0u) {
    uint32_t size = getTextureRegionSize(info->format, info->width, info->height, info->depth);
    state.stats.memory -= size;
    state.stats.textureMemory -= size;
  }
  state.stats.textures--;
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
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Writing to a Texture can only happen in a transfer pass");
  lovrCheck(!texture->info.parent, "Texture views can not be written to");
  lovrCheck(texture->info.usage & TEXTURE_COPYTO, "Texture must have the 'copyto' flag to write to it");
  lovrCheck(texture->info.samples == 1, "Multisampled Textures can not be written to");
  checkTextureBounds(&texture->info, offset, extent);

  size_t fullSize = getTextureRegionSize(texture->info.format, extent[0], extent[1], extent[2]);
  size_t rowSize = getTextureRegionSize(texture->info.format, extent[0], 1, 1);
  size_t imgSize = getTextureRegionSize(texture->info.format, extent[0], extent[1], 1);
  Megaview scratch = allocateBuffer(GPU_MEMORY_CPU_WRITE, fullSize, 64);
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

  gpu_copy_buffer_texture(state.pass->stream, scratch.gpu, texture->gpu, scratch.offset, offset, extent);
  state.stats.copies++;
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
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Clearing a Texture can only happen in a transfer pass");
  lovrCheck(!texture->info.parent, "Texture views can not be cleared");
  lovrCheck(!isDepthFormat(texture->info.format), "Currently only color textures can be cleared");
  lovrCheck(texture->info.type == TEXTURE_VOLUME || layer + layerCount <= texture->info.depth, "Texture clear range exceeds texture layer count");
  lovrCheck(level + levelCount <= texture->info.mipmaps, "Texture clear range exceeds texture mipmap count");
  gpu_clear_texture(state.pass->stream, texture->gpu, layer, layerCount, level, levelCount, color);
  state.stats.copies++;
}

void lovrTextureRead(Texture* texture, uint16_t offset[4], uint16_t extent[3], void (*callback)(void* data, uint32_t size, void* userdata), void* userdata) {
  ReaderPool* readers = &state.readers;
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Downloading a Texture can only happen in a transfer pass");
  lovrCheck(!texture->info.parent, "Texture views can not be read");
  lovrCheck(texture->info.usage & TEXTURE_COPYFROM, "Texture must have the 'copyfrom' flag to read from it");
  lovrCheck(texture->info.samples == 1, "Multisampled Textures can not be read");
  checkTextureBounds(&texture->info, offset, extent);
  lovrCheck(readers->head - readers->tail != COUNTOF(readers->list), "Too many readbacks"); // TODO emergency waitIdle instead
  size_t size = getTextureRegionSize(texture->info.format, extent[0], extent[1], extent[2]);
  Megaview scratch = allocateBuffer(GPU_MEMORY_CPU_READ, size, 64);
  gpu_copy_texture_buffer(state.pass->stream, texture->gpu, scratch.gpu, offset, scratch.offset, extent);
  readers->list[readers->head++ & 0xf] = (Reader) {
    .callback = callback,
    .userdata = userdata,
    .data = scratch.data,
    .size = size,
    .tick = state.tick
  };
  state.stats.copies++;
}

void lovrTextureCopy(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t extent[3]) {
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Texture copies can only happen in a transfer pass");
  lovrCheck(!src->info.parent && !dst->info.parent, "Can not copy Texture views");
  lovrCheck(src->info.usage & TEXTURE_COPYFROM, "Texture must have the 'copyfrom' flag to copy from it");
  lovrCheck(dst->info.usage & TEXTURE_COPYTO, "Texture must have the 'copyto' flag to copy to it");
  lovrCheck(src->info.format == dst->info.format, "Copying between Textures requires them to have the same format");
  lovrCheck(src->info.samples == dst->info.samples, "Textures must have the same sample counts to copy between them");
  checkTextureBounds(&src->info, srcOffset, extent);
  checkTextureBounds(&dst->info, dstOffset, extent);
  gpu_copy_textures(state.pass->stream, src->gpu, dst->gpu, srcOffset, dstOffset, extent);
  state.stats.copies++;
}

void lovrTextureBlit(Texture* src, Texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t srcExtent[3], uint16_t dstExtent[3], bool nearest) {
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Texture blits can only happen in a transfer pass");
  lovrCheck(!src->info.parent && !dst->info.parent, "Can not blit Texture views");
  lovrCheck(src->info.samples == 1 && dst->info.samples == 1, "Multisampled textures can not be used for blits");
  lovrCheck(src->info.usage & TEXTURE_COPYFROM, "Texture must have the 'copyfrom' flag to blit from it");
  lovrCheck(dst->info.usage & TEXTURE_COPYTO, "Texture must have the 'copyto' flag to blit to it");
  lovrCheck(state.features.formats[src->info.format] & GPU_FEATURE_BLIT, "This GPU does not support blits for the source texture's format");
  lovrCheck(state.features.formats[dst->info.format] & GPU_FEATURE_BLIT, "This GPU does not support blits for the destination texture's format");
  lovrCheck(src->info.format == dst->info.format, "Texture formats must match to blit between them");
  checkTextureBounds(&src->info, srcOffset, srcExtent);
  checkTextureBounds(&dst->info, dstOffset, dstExtent);
  gpu_blit(state.pass->stream, src->gpu, dst->gpu, srcOffset, dstOffset, srcExtent, dstExtent, nearest);
  state.stats.copies++;
}

void lovrTextureGenerateMipmaps(Texture* texture) {
  lovrCheck(state.pass && state.pass->type == PASS_TRANSFER, "Texture mipmap generation can only happen in a transfer pass");
  lovrCheck(!texture->info.parent, "Can not generate mipmaps on texture views");
  lovrCheck(texture->info.usage & TEXTURE_COPYFROM, "Texture must have the 'copyfrom' flag to generate mipmaps");
  lovrCheck(texture->info.usage & TEXTURE_COPYTO, "Texture must have the 'copyto' flag to generate mipmaps");
  lovrCheck(state.features.formats[texture->info.format] & GPU_FEATURE_BLIT, "This GPU does not support blits for the texture's format, which is required for mipmap generation");
  gpu_mipgen(state.pass->stream, texture->gpu);
  state.stats.copies++;
}

// Sampler

Sampler* lovrSamplerCreate(SamplerInfo* info) {
  lovrCheck(info->range[1] < 0.f || info->range[1] >= info->range[0], "Invalid Sampler mipmap range");
  lovrCheck(info->anisotropy <= state.limits.anisotropy, "Sampler anisotropy (%f) exceeds limits.anisotropy (%f)", info->anisotropy, state.limits.anisotropy);

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
  state.stats.samplers++;
  return sampler;
}

Sampler* lovrGraphicsGetDefaultSampler(DefaultSampler type) {
  if (state.defaultSamplers[type]) return state.defaultSamplers[type];
  state.stats.samplers++;
  return state.defaultSamplers[type] = lovrSamplerCreate(&(SamplerInfo) {
    .min = type == SAMPLER_NEAREST ? FILTER_NEAREST : FILTER_LINEAR,
    .mag = type == SAMPLER_NEAREST ? FILTER_NEAREST : FILTER_LINEAR,
    .mip = type >= SAMPLER_TRILINEAR ? FILTER_LINEAR : FILTER_NEAREST,
    .wrap = { WRAP_REPEAT, WRAP_REPEAT, WRAP_REPEAT },
    .anisotropy = type == SAMPLER_ANISOTROPIC ? MIN(2.f, state.limits.anisotropy) : 0.f
  });
}

void lovrSamplerDestroy(void* ref) {
  Sampler* sampler = ref;
  gpu_sampler_destroy(sampler->gpu);
  state.stats.samplers--;
  free(sampler);
}

const SamplerInfo* lovrSamplerGetInfo(Sampler* sampler) {
  return &sampler->info;
}

// Shader

#define MIN_SPIRV_WORDS 8

typedef union {
  struct {
    uint16_t location;
    uint16_t name;
  } attribute;
  struct {
    uint16_t group;
    uint16_t binding;
  } resource;
  struct {
    uint16_t number;
    uint16_t name;
  } flag;
  struct {
    uint16_t inner;
    uint16_t name;
  } type;
} CacheData;

typedef struct {
  uint32_t attributeMask;
  gpu_slot slots[2][32];
  uint64_t slotNames[32];
  uint64_t flagNames[32];
  gpu_shader_flag flags[32];
  uint32_t flagCount;
} ReflectionInfo;

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
    case 4439: lovrCheck(state.limits.renderSize[2] > 1, "GPU does not support shader feature #%d: %s", capability, "multiview"); break;
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
static void parseTypeInfo(const uint32_t* words, uint32_t wordCount, CacheData* cache, uint32_t bound, const uint32_t* instruction, gpu_slot_type* slotType, uint32_t* count) {
  const uint32_t* edge = words + wordCount - MIN_SPIRV_WORDS;
  uint32_t type = instruction[1];
  uint32_t id = instruction[2];
  uint32_t storageClass = instruction[3];

  // Follow the variable's type id to the OpTypePointer instruction that declares its pointer type
  // Then unwrap the pointer to get to the inner type of the variable
  instruction = words + cache[type].type.inner;
  lovrCheck(instruction < edge && instruction[3] < bound, "Invalid Shader code: id overflow");
  instruction = words + cache[instruction[3]].type.inner;
  lovrCheck(instruction < edge, "Invalid Shader code: id overflow");

  if ((instruction[0] & 0xffff) == 28) { // OpTypeArray
    // Read array size
    lovrCheck(instruction[3] < bound && words + cache[instruction[3]].type.inner < edge, "Invalid Shader code: id overflow");
    const uint32_t* size = words + cache[instruction[3]].type.inner;
    if ((size[0] & 0xffff) == 43 || (size[0] & 0xffff) == 50) { // OpConstant || OpSpecConstant
      *count = size[3];
    } else {
      lovrThrow("Invalid Shader code: variable %d is an array, but the array size is not a constant", id);
    }

    // Unwrap array to get to inner array type and keep going
    lovrCheck(instruction[2] < bound && words + cache[instruction[2]].type.inner < edge, "Invalid Shader code: id overflow");
    instruction = words + cache[instruction[2]].type.inner;
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
    instruction = words + cache[instruction[2]].type.inner;
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

static bool parseSpirv(const void* source, uint32_t size, uint8_t stage, ReflectionInfo* reflection) {
  const uint32_t* words = source;
  uint32_t wordCount = size / sizeof(uint32_t);
  const uint32_t* edge = words + wordCount - MIN_SPIRV_WORDS;

  if (wordCount < MIN_SPIRV_WORDS || words[0] != 0x07230203) {
    return false;
  }

  uint32_t bound = words[3];
  lovrCheck(bound < 0xffff, "Unsupported Shader code: id bound is too big (max is 65534)");

  // The cache stores information for spirv ids, allowing everything to be parsed in a single pass
  // - For buffer/texture resources, stores the slot's group and binding number
  // - For vertex attributes, stores the attribute location decoration
  // - For specialization constants, stores the constant's name and its constant id
  // - For types, stores the id of its declaration instruction and its name (for block resources)
  size_t cacheSize = bound * sizeof(CacheData);
  void* cacheData = talloc(cacheSize);
  memset(cacheData, 0xff, cacheSize);
  CacheData* cache = cacheData;

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
        cache[id].type.name = instruction - words + 2;
        break;
      case 71: // OpDecorate
        if (length < 4 || instruction[1] >= bound) break;

        id = instruction[1];
        uint32_t decoration = instruction[2];
        uint32_t value = instruction[3];

        if (decoration == 33) { // Binding
          lovrCheck(value < 32, "Unsupported Shader code: variable %d uses binding %d, but the binding must be less than 32", id, value);
          cache[id].resource.binding = value;
        } else if (decoration == 34) { // Group
          lovrCheck(value < 2, "Unsupported Shader code: variable %d is in group %d, but group must be less than 2", id, value);
          cache[id].resource.group = value;
        } else if (decoration == 30) { // Location
          lovrCheck(value < 32, "Unsupported Shader code: vertex shader uses attribute location %d, but locations must be less than 16", value);
          cache[id].attribute.location = value;
        } else if (decoration == 1) { // SpecId
          lovrCheck(value < 2000, "Unsupported Shader code: specialization constant id is too big (max is 2000)");
          cache[id].flag.number = value;
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
        cache[instruction[1]].type.inner = instruction - words;
        break;
      case 48: // OpSpecConstantTrue
      case 49: // OpSpecConstantFalse
      case 50: // OpSpecConstant
        if (length < 2 || instruction[2] >= bound) break;
        uint32_t id = instruction[2];

        lovrCheck(reflection->flagCount < COUNTOF(reflection->flags), "Shader has too many flags");
        uint32_t index = reflection->flagCount++;

        lovrCheck(cache[id].flag.number != 0xffff, "Invalid Shader code: Specialization constant has no ID");
        reflection->flags[index].id = cache[id].flag.number;

        // If it's a regular SpecConstant, parse its type for i32/u32/f32, otherwise use b32
        if (opcode == 50) {
          const uint32_t* type = words + cache[instruction[1]].type.inner;
          lovrCheck(type < edge, "Invalid Shader code: Specialization constant has invalid type");
          if ((type[0] & 0xffff) == 21 && type[2] == 32) { // OpTypeInt
            if (type[3] == 0) {
              reflection->flags[index].type = GPU_FLAG_U32;
            } else {
              reflection->flags[index].type = GPU_FLAG_I32;
            }
          } else if ((type[0] & 0xffff) == 22 && type[2] == 32) { // OpTypeFloat
            reflection->flags[index].type = GPU_FLAG_F32;
          } else {
            lovrThrow("Invalid Shader code: Specialization constant has unsupported type (use bool, int, uint, or float)");
          }
        } else {
          reflection->flags[index].type = GPU_FLAG_B32;
        }

        if (cache[id].flag.name != 0xffff) {
          uint32_t nameWord = cache[id].flag.name;
          char* name = (char*) (words + nameWord);
          size_t length = strnlen(name, (wordCount - nameWord) * sizeof(uint32_t));
          reflection->flagNames[index] = hash64(name, length);
        }
        break;
      case 59: // OpVariable
        if (length < 4 || instruction[2] >= bound) break;
        id = instruction[2];
        uint32_t type = instruction[1];
        uint32_t storageClass = instruction[3];

        // Vertex shaders track attribute locations (Input StorageClass (1) decorated with Location)
        if (stage == GPU_STAGE_VERTEX && storageClass == 1 && cache[id].attribute.location < 32) {
          reflection->attributeMask |= (1 << cache[id].attribute.location);
          break;
        }

        // Ignore inputs/outputs and variables that aren't decorated with a group and binding (e.g. globals)
        if (storageClass == 1 || storageClass == 3 || cache[id].resource.group == 0xff || cache[id].resource.binding == 0xff) {
          break;
        }

        uint32_t count;
        gpu_slot_type slotType;
        parseTypeInfo(words, wordCount, cache, bound, instruction, &slotType, &count);

        uint32_t group = cache[id].resource.group;
        uint32_t number = cache[id].resource.binding;
        gpu_slot* slot = &reflection->slots[group][number];

        // Name (type of variable is pointer, follow pointer's inner type to get to struct type)
        // The struct type is what actually has the block name, used for the resource's name
        uint32_t blockType = (words + cache[type].type.inner)[3];
        if (group == 1 && !reflection->slotNames[number] && cache[blockType].type.name != 0xffff) {
          uint32_t nameWord = cache[blockType].type.name;
          char* name = (char*) (words + nameWord);
          size_t length = strnlen(name, (wordCount - nameWord) * sizeof(uint32_t));
          reflection->slotNames[number] = hash64(name, length);
        }

        // Either merge our info into an existing slot, or add the slot
        if (slot->stage != 0) {
          lovrCheck(slot->type == slotType, "Variable (%d,%d) is in multiple shader stages with different types");
          lovrCheck(slot->count == count, "Variable (%d,%d) is in multiple shader stages with different array lengths");
          slot->stage |= stage;
        } else {
          lovrCheck(count > 0, "Variable (%d,%d) has array length of zero");
          lovrCheck(count < 256, "Variable (%d,%d) has array length of %d, but the max is 255", count);
          slot->number = number;
          slot->type = slotType;
          slot->stage = stage;
          slot->count = count;
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

  return true;
}

Shader* lovrShaderCreate(ShaderInfo* info) {
  Shader* shader = calloc(1, sizeof(Shader) + gpu_sizeof_shader() + gpu_sizeof_pipeline());
  lovrAssert(shader, "Out of memory");
  shader->gpu = (gpu_shader*) (shader + 1);
  shader->info = *info;
  shader->ref = 1;

  ReflectionInfo reflection;
  memset(&reflection, 0, sizeof(reflection));

  if (info->type == SHADER_COMPUTE) {
    lovrCheck(info->source[0] && !info->source[1], "Compute shaders require one stage");
    parseSpirv(info->source[0], info->length[0], GPU_STAGE_COMPUTE, &reflection);
  } else {
    lovrCheck(info->source[0] && info->source[1], "Currently, graphics shaders require two stages");
    parseSpirv(info->source[0], info->length[0], GPU_STAGE_VERTEX, &reflection);
    parseSpirv(info->source[1], info->length[1], GPU_STAGE_FRAGMENT, &reflection);
  }

  // Copy some of the reflection info (maybe it should just write directly to Shader*)?
  memcpy(shader->flagLookup, reflection.flagNames, sizeof(reflection.flagNames));
  memcpy(shader->flags, reflection.flags, sizeof(reflection.flags));
  shader->flagCount = reflection.flagCount;
  shader->attributeMask = reflection.attributeMask;

  // Validate built in bindings (TODO?)
  lovrCheck(reflection.slots[0][0].type == GPU_SLOT_UNIFORM_BUFFER, "Expected uniform buffer for camera matrices");

  // Preprocess user bindings
  for (uint32_t i = 0; i < COUNTOF(reflection.slots[1]); i++) {
    gpu_slot slot = reflection.slots[1][i];
    if (!slot.stage) continue;
    uint32_t index = shader->resourceCount++;
    bool buffer = slot.type == GPU_SLOT_UNIFORM_BUFFER || slot.type == GPU_SLOT_STORAGE_BUFFER;
    bool storage = slot.type == GPU_SLOT_STORAGE_BUFFER || slot.type == GPU_SLOT_STORAGE_TEXTURE;
    shader->bufferMask |= (buffer << i);
    shader->textureMask |= (!buffer << i);
    shader->storageMask |= (storage << i);
    shader->resourceSlots[index] = i;
    shader->resourceLookup[index] = reflection.slotNames[i];
  }

  // Filter empties from slots, so non-sparse slot list can be used to hash/create layout
  for (uint32_t i = 0; i < shader->resourceCount; i++) {
    if (shader->resourceSlots[i] > i) {
      reflection.slots[1][i] = reflection.slots[1][shader->resourceSlots[i]];
    }
  }

  if (shader->resourceCount > 0) {
    shader->layout = lookupLayout(reflection.slots[1], shader->resourceCount);
  }

  gpu_shader_info gpuInfo = {
    .stages[0] = { info->source[0], info->length[0] },
    .stages[1] = { info->source[1], info->length[1] },
    .layouts[0] = state.layouts[0],
    .layouts[1] = state.layouts[shader->layout],
    .label = info->label
  };

  lovrAssert(gpu_shader_init(shader->gpu, &gpuInfo), "Could not create Shader");

  // Flags: Shader stores full list, but reorders them to put active (overridden) ones first
  shader->activeFlagCount = 0;
  for (uint32_t i = 0; i < info->flagCount; i++) {
    ShaderFlag* flag = &info->flags[i];
    uint64_t hash = flag->name ? hash64(flag->name, strlen(flag->name)) : 0;
    for (uint32_t j = 0; j < shader->flagCount; j++) {
      if (hash ? (hash != shader->flagLookup[j]) : (flag->id != shader->flags[j].id)) continue;
      uint32_t index = shader->activeFlagCount++;
      if (index != j) {
        gpu_shader_flag temp = shader->flags[index];
        shader->flags[index] = shader->flags[j];
        shader->flags[j] = temp;

        uint64_t tempName = shader->flagLookup[index];
        shader->flagLookup[index] = shader->flagLookup[j];
        shader->flagLookup[j] = tempName;
      }
      shader->flags[index].value = flag->value;
    }
  }

  if (info->type == SHADER_COMPUTE) {
    gpu_compute_pipeline_info compute = {
      .shader = shader->gpu,
      .flags = shader->flags,
      .flagCount = shader->activeFlagCount
    };
    uint32_t index = state.pipelineCount++;
    lovrCheck(index < COUNTOF(state.pipelines), "Too many pipelines, please report this encounter");
    lovrAssert(gpu_pipeline_init_compute(state.pipelines[index], &compute), "Failed to initialize compute pipeline");
    shader->computePipelineIndex = index;
  }

  state.stats.shaders++;
  return shader;
}

Shader* lovrShaderClone(Shader* parent, ShaderFlag* flags, uint32_t count) {
  Shader* shader = calloc(1, sizeof(Shader) + gpu_sizeof_shader() + gpu_sizeof_pipeline());
  lovrAssert(shader, "Out of memory");
  shader->ref = 1;
  shader->gpu = parent->gpu;
  shader->info = parent->info;
  shader->info.flags = flags;
  shader->info.flagCount = count;
  shader->layout = parent->layout;
  shader->resourceCount = parent->resourceCount;
  shader->bufferMask = parent->bufferMask;
  shader->textureMask = parent->textureMask;
  shader->storageMask = parent->storageMask;
  memcpy(shader->resourceSlots, parent->resourceSlots, sizeof(shader->resourceSlots));
  memcpy(shader->resourceLookup, parent->resourceLookup, sizeof(shader->resourceLookup));
  memcpy(shader->flagLookup, parent->flagLookup, sizeof(shader->flagLookup));
  memcpy(shader->flags, parent->flags, sizeof(shader->flags));
  shader->flagCount = parent->flagCount;
  shader->attributeMask = parent->attributeMask;

  // Reorder the flags to put the overrides at the beginning
  shader->activeFlagCount = 0;
  for (uint32_t i = 0; i < count; i++) {
    ShaderFlag* flag = &flags[i];
    uint64_t hash = flag->name ? hash64(flag->name, strlen(flag->name)) : 0;
    for (uint32_t j = 0; j < shader->flagCount; j++) {
      if (hash ? (hash != shader->flagLookup[j]) : (flag->id != shader->flags[j].id)) continue;
      uint32_t index = shader->activeFlagCount++;
      if (index != j) {
        gpu_shader_flag temp = shader->flags[index];
        shader->flags[index] = shader->flags[j];
        shader->flags[j] = temp;

        uint64_t tempName = shader->flagLookup[index];
        shader->flagLookup[index] = shader->flagLookup[j];
        shader->flagLookup[j] = tempName;
      }
      shader->flags[index].value = flag->value;
    }
  }

  if (shader->info.type == SHADER_COMPUTE) {
    gpu_compute_pipeline_info compute = {
      .shader = shader->gpu,
      .flags = shader->flags,
      .flagCount = shader->activeFlagCount
    };
    uint32_t index = state.pipelineCount++;
    lovrCheck(index < COUNTOF(state.pipelines), "Too many pipelines, please report this encounter");
    lovrAssert(gpu_pipeline_init_compute(state.pipelines[index], &compute), "Failed to initialize compute pipeline");
    shader->computePipelineIndex = index;
  }

  state.stats.shaders++;
  return shader;
}

Shader* lovrShaderCreateDefault(DefaultShader type, ShaderFlag* flags, uint32_t count) {
  return lovrShaderClone(lovrGraphicsGetDefaultShader(type), flags, count);
}

Shader* lovrGraphicsGetDefaultShader(DefaultShader type) {
  if (state.defaultShaders[type]) return state.defaultShaders[type];
  switch (type) {
    case SHADER_UNLIT:
      return state.defaultShaders[type] = lovrShaderCreate(&(ShaderInfo) {
        .type = SHADER_GRAPHICS,
        .source = { lovr_shader_unlit_vert, lovr_shader_unlit_frag },
        .length = { sizeof(lovr_shader_unlit_vert), sizeof(lovr_shader_unlit_frag) },
        .label = "unlit"
      });
    default: lovrThrow("Unreachable");
  }
}

void lovrShaderDestroy(void* ref) {
  Shader* shader = ref;
  gpu_shader_destroy(shader->gpu);
  state.stats.shaders--;
  free(shader);
}

const ShaderInfo* lovrShaderGetInfo(Shader* shader) {
  return &shader->info;
}

// Batch

Batch* lovrBatchCreate(BatchInfo* info) {
  lovrCheck(info->capacity <= 0xffff, "Currently, the maximum batch capacity is %d", 0xffff);
  Batch* batch = calloc(1, sizeof(Batch));
  lovrAssert(batch, "Out of memory");
  batch->ref = 1;
  batch->info = *info;
  batch->pass = lookupPass(info->canvas);
  batch->draws = malloc(info->capacity * sizeof(BatchDraw));
  batch->groups = malloc(info->capacity * sizeof(BatchGroup));
  batch->activeDraws = malloc(info->capacity * sizeof(uint32_t));
  batch->origins = malloc(info->capacity * 4 * sizeof(float));
  batch->bundles = malloc(info->capacity * sizeof(gpu_bundle*));
  batch->bundleInfo = malloc(info->capacity * sizeof(gpu_bundle_info));
  lovrAssert(batch->draws, "Out of memory");
  lovrAssert(batch->groups, "Out of memory");
  lovrAssert(batch->activeDraws, "Out of memory");
  lovrAssert(batch->origins, "Out of memory");
  lovrAssert(batch->bundles, "Out of memory");
  lovrAssert(batch->bundleInfo, "Out of memory");

  if (!info->transient) {
    batch->bunch = malloc(gpu_sizeof_bunch());
    batch->bundles[0] = malloc(info->capacity * gpu_sizeof_bundle());
    lovrAssert(batch->bunch, "Out of memory");
    lovrAssert(batch->bundles[0], "Out of memory");

    for (uint32_t i = 1; i < info->capacity; i++) {
      batch->bundles[i] = (gpu_bundle*) ((char*) batch->bundles[0] + i * gpu_sizeof_bundle());
    }

    uint32_t alignment = state.limits.uniformBufferAlign;
    batch->transforms = allocateBuffer(GPU_MEMORY_GPU, info->capacity * 64, alignment);
    batch->colors = allocateBuffer(GPU_MEMORY_GPU, info->capacity * 16, alignment);
    batch->stash = allocateBuffer(GPU_MEMORY_GPU, info->bufferSize, 4);
  }

  arr_init(&batch->buffers, info->transient ? tgrow : realloc);
  arr_init(&batch->textures, info->transient ? tgrow : realloc);
  return batch;
}

void lovrBatchDestroy(void* ref) {
  Batch* batch = ref;
  for (size_t i = 0; i < batch->buffers.length; i++) {
    lovrRelease(batch->buffers.data[i].buffer, lovrBufferDestroy);
  }
  for (size_t i = 0; i < batch->textures.length; i++) {
    lovrRelease(batch->textures.data[i].texture, lovrTextureDestroy);
  }
  arr_free(&batch->buffers);
  arr_free(&batch->textures);
  if (!batch->info.transient) {
    if (--state.buffers.list[batch->transforms.index].refs == 0) {
      recycleBuffer(batch->transforms.index, GPU_MEMORY_GPU);
    }
    if (--state.buffers.list[batch->colors.index].refs == 0) {
      recycleBuffer(batch->transforms.index, GPU_MEMORY_GPU);
    }
    if (--state.buffers.list[batch->stash.index].refs == 0) {
      recycleBuffer(batch->stash.index, GPU_MEMORY_GPU);
    }
    if (batch->bundleCount > 0) {
      gpu_bunch_destroy(batch->bunch);
    }
    free(batch->bundles[0]);
    free(batch->bunch);
  }
  free(batch->bundleInfo);
  free(batch->bundles);
  free(batch->origins);
  free(batch->activeDraws);
  free(batch->groups);
  free(batch->draws);
  free(batch);
}

const BatchInfo* lovrBatchGetInfo(Batch* batch) {
  return &batch->info;
}

uint32_t lovrBatchGetCount(Batch* batch) {
  return batch->drawCount;
}

void lovrBatchReset(Batch* batch) {
  batch->drawCount = 0;
  batch->groupCount = 0;
  batch->activeDrawCount = 0;
  batch->groupedCount = 0;
  batch->stashCursor = 0;

  if (!batch->info.transient && batch->lastBundleCount > 0) {
    gpu_bunch_destroy(batch->bunch);
  }

  batch->bundleCount = 0;
  batch->lastBundleCount = 0;

  for (size_t i = 0; i < batch->buffers.length; i++) {
    lovrRelease(batch->buffers.data[i].buffer, lovrBufferDestroy);
  }

  for (size_t i = 0; i < batch->textures.length; i++) {
    lovrRelease(batch->textures.data[i].texture, lovrTextureDestroy);
  }

  arr_clear(&batch->buffers);
  arr_clear(&batch->textures);
}

static LOVR_THREAD_LOCAL Batch* sortBatch;

// Opaque sort groups by state change, and uses front-to-back depth as a tie breaker
static int cmpOpaque(const void* a, const void* b) {
  uint32_t i = *(uint32_t*) a;
  uint32_t j = *(uint32_t*) b;
  uint64_t key1, key2;
  memcpy(&key1, &sortBatch->draws[i].pipeline, sizeof(uint64_t));
  memcpy(&key2, &sortBatch->draws[j].pipeline, sizeof(uint64_t));
  if (key1 == key2) {
    return sortBatch->draws[i].depth > sortBatch->draws[j].depth - sortBatch->draws[i].depth > sortBatch->draws[j].depth;
  }
  return (key1 > key2) - (key1 < key2);
}

// Transparent sort orders by depth and the 32 bits after depth (e.g. pipeline, doesn't matter much)
static int cmpTransparent(const void* a, const void* b) {
  uint32_t i = *(uint32_t*) a;
  uint32_t j = *(uint32_t*) b;
  uint64_t key1, key2;
  memcpy(&key1, &sortBatch->draws[i].depth, sizeof(uint64_t));
  memcpy(&key2, &sortBatch->draws[j].depth, sizeof(uint64_t));
  return (key1 > key2) - (key1 < key2);
}

void lovrBatchSort(Batch* batch, SortMode mode) {
  /*
  for (uint32_t i = 0; i < batch->activeDrawCount; i++) {
    float v[4];
    vec3_init(v, batch->origins + 4 * batch->activeDraws[i]);
    mat4_mulVec4(batch->camera.view[0], v);
    batch->draws[batch->activeDraws[i]].depth = -v[2];
  }
  */

  sortBatch = batch;
  qsort(batch->activeDraws, batch->activeDrawCount, sizeof(uint32_t), mode == SORT_OPAQUE ? cmpOpaque : cmpTransparent);
  batch->groupedCount = 0;
}

void lovrBatchFilter(Batch* batch, bool (*predicate)(void* context, uint32_t i), void* context) {
  batch->activeDrawCount = 0;
  for (uint32_t i = 0; i < batch->drawCount; i++) {
    if (predicate(context, i)) {
      batch->activeDraws[batch->activeDrawCount++] = i;
    }
  }
  batch->groupedCount = 0;
}

static void lovrBatchFinalize(Batch* batch) {
  // Allocate bundles
  if (batch->bundleCount > 0) {
    if (batch->info.transient) {
      for (uint32_t i = 0; i < batch->bundleCount; i++) {
        uint32_t layoutIndex = ((char*) batch->bundleInfo[i].layout - (char*) state.layouts[0]) / gpu_sizeof_layout();
        batch->bundles[i] = allocateBundle(layoutIndex);
      }
    } else {
      gpu_bunch_info info = {
        .bundles = batch->bundles[0],
        .contents = batch->bundleInfo,
        .count = batch->bundleCount
      };

      lovrAssert(gpu_bunch_init(batch->bunch, &info), "Failed to initialize bunch");
    }

    gpu_bundle_write(batch->bundles, batch->bundleInfo, batch->bundleCount);
  }

}
