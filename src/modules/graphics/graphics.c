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

enum {
  SHAPE_CUBE,
  SHAPE_MAX
};

typedef struct {
  gpu_buffer* gpu;
  uint32_t size;
  uint32_t type;
  uint32_t next;
  uint32_t tick;
  uint32_t refs;
  char* data;
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

struct Canvas {
  uint32_t ref;
  bool temporary;
  gpu_canvas gpu;
  CanvasInfo info;
  Texture* colorTextures[4];
  Texture* resolveTextures[4];
  Texture* depthTexture;
};

struct Shader {
  uint32_t ref;
  ShaderInfo info;
  gpu_shader* gpu;
  gpu_pipeline* compute;
  /*MaterialData material;
  ShaderResource* resources;
  ShaderVariable* variables;
  ShaderConstant* constants;
  uint32_t resourceCount;
  uint32_t variableCount;
  uint32_t constantCount;
  uint32_t attributeMask;*/
  map_t lookup;
};

struct Pipeline {
  uint32_t ref;
  PipelineInfo info;
  gpu_pipeline_info gpuinfo;
  gpu_pipeline* gpu;
  gpu_pass* pass;
  uint64_t hash;
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
    unsigned pipeline : 16;
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
  gpu_pipeline* pipeline;
  gpu_buffer* vertexBuffer;
  gpu_buffer* indexBuffer;
  gpu_bundle* bundle;
  uint32_t index;
  uint32_t start;
  uint32_t count;
  uint32_t instances;
  uint32_t base;
} BatchDraw;

typedef struct {
  float color[4];
  gpu_pipeline_info info;
  gpu_pipeline* instance;
  uint64_t hash;
  bool dirty;
} BatchPipeline;

enum {
  BUFFER_TRANSFORM,
  BUFFER_DRAW_DATA,
  BUFFER_PRIMITIVE,
  BUFFER_MAX
};

struct Batch {
  uint32_t ref;
  bool scratch;
  BatchInfo info;
  gpu_pass* pass;
  uint32_t groupCount;
  uint32_t drawCount;
  BatchGroup* groups;
  BatchDraw* draws;
  uint32_t bundle;
  bool bindingsDirty;
  uint32_t transformIndex;
  uint32_t pipelineIndex;
  BatchPipeline* pipeline;
  float* transform;
  Shader* shader;
  Megaview buffers[BUFFER_MAX];
  Megaview scratchBuffers[BUFFER_MAX];
  uint32_t shapeCursor;
  uint32_t bindingCount;
  gpu_binding bindings[256];
  BatchPipeline pipelines[4];
  float transforms[16][16];
  float viewMatrix[6][16];
  float projection[6][16];
  char* bundles;
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

static struct {
  bool initialized;
  bool active;
  gpu_hardware hardware;
  gpu_features features;
  gpu_limits limits;
  LinearAllocator frame;
  uint32_t blockSize;
  uint32_t baseVertex[SHAPE_MAX];
  gpu_buffer* shapes;
  BufferPool buffers;
  ReaderPool readers;
  Sampler* defaultSamplers[MAX_DEFAULT_SAMPLERS];
  map_t pipelines;
  map_t passes;
  Canvas* window;
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

// Suballocates into a Megabuffer
static Megaview bufferAllocate(gpu_memory_type type, uint32_t size, uint32_t align) {
  uint32_t active = state.buffers.active[type];
  uint32_t oldest = state.buffers.oldest[type];
  uint32_t cursor = ALIGN(state.buffers.cursor[type], align);

  // If there's an active Megabuffer and it has room, use it
  if (active != ~0u && cursor + size <= state.buffers.list[active].size) {
    state.buffers.cursor[type] = cursor + size;
    Megabuffer* buffer = &state.buffers.list[active];
    return (Megaview) { .gpu = buffer->gpu, .data = buffer->data + cursor, .index = active, .offset = cursor };
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
    return (Megaview) { .gpu = buffer->gpu, .data = buffer->data, .index = oldest, .offset = 0 };
  }

  // No Megabuffers were available, time for a new one
  lovrAssert(state.buffers.count < COUNTOF(state.buffers.list), "Out of Buffer memory");
  state.buffers.active[type] = active = state.buffers.count++;
  state.buffers.cursor[type] = size;

  Megabuffer* buffer = &state.buffers.list[active];
  buffer->gpu = calloc(1, gpu_sizeof_buffer()); // TODO
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
    .data = (void**) &buffer->data
  };

  lovrAssert(buffer->gpu, "Out of memory");
  lovrAssert(gpu_buffer_init(buffer->gpu, &info), "Failed to initialize Buffer");

  return (Megaview) { .gpu = buffer->gpu, .data = buffer->data, .index = active, .offset = 0 };
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

bool lovrGraphicsInit(bool debug, uint32_t blockSize) {
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
    config.vk.vsync = false; // TODO
    config.vk.createSurface = os_vk_create_surface;
  }
#endif

  if (!gpu_init(&config)) {
    lovrThrow("Failed to initialize GPU");
  }

  struct Vertex {
    float x, y, z;
    unsigned pad: 2, nx: 10, ny: 10, nz: 10;
    uint16_t u, v;
  };

  // In 10 bit land, 0x200 is 0.0, 0x3ff is 1.0, 0x000 is -1.0

  struct Vertex cube[] = {
    { -.5f, -.5f, -.5f, 0x0, 0x200, 0x200, 0x000, 0x0000, 0x0000 }, // Front
    { -.5f,  .5f, -.5f, 0x0, 0x200, 0x200, 0x000, 0x0000, 0xffff },
    {  .5f, -.5f, -.5f, 0x0, 0x200, 0x200, 0x000, 0xffff, 0x0000 },
    {  .5f,  .5f, -.5f, 0x0, 0x200, 0x200, 0x000, 0xffff, 0xffff },
    {  .5f,  .5f, -.5f, 0x0, 0x3ff, 0x200, 0x200, 0x0000, 0xffff }, // Right
    {  .5f,  .5f,  .5f, 0x0, 0x3ff, 0x200, 0x200, 0xffff, 0xffff },
    {  .5f, -.5f, -.5f, 0x0, 0x3ff, 0x200, 0x200, 0x0000, 0x0000 },
    {  .5f, -.5f,  .5f, 0x0, 0x3ff, 0x200, 0x200, 0xffff, 0x0000 },
  };

  uint32_t size = 0;
  state.baseVertex[SHAPE_CUBE] = size / sizeof(Vertex), size += sizeof(cube);
  Megaview shapes = bufferAllocate(GPU_MEMORY_GPU, size, 4);
  Megaview scratch = bufferAllocate(GPU_MEMORY_CPU_WRITE, size, 4);
  lovrAssert(shapes.offset == 0, "Unreachable");
  state.shapes = shapes.gpu;

  memcpy(scratch.data, cube, sizeof(cube)), scratch.data += sizeof(cube);

  gpu_begin();
  gpu_stream* stream = gpu_stream_begin();
  gpu_copy_buffers(stream, scratch.gpu, shapes.gpu, scratch.offset, shapes.offset, size);
  gpu_stream_end(stream);
  gpu_submit(&stream, 1);

  map_init(&state.pipelines, 8);
  map_init(&state.passes, 4);

  state.limits.vertexBuffers -= INTERNAL_VERTEX_BUFFERS;

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
  for (uint32_t i = 0; i < state.passes.size; i++) {
    if (state.passes.values[i] == MAP_NIL) continue;
    gpu_pass* pass = (gpu_pass*) (uintptr_t) state.passes.values[i];
    gpu_pass_destroy(pass);
    free(pass);
  }
  map_free(&state.pipelines);
  map_free(&state.passes);
  lovrRelease(state.window, lovrCanvasDestroy);
  gpu_destroy();
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

void lovrGraphicsRender(Canvas* canvas, Batch** batches, uint32_t count) {
  lovrCheck(state.active, "Graphics is not active");

  gpu_stream* stream = gpu_stream_begin();
  gpu_render_begin(stream, &canvas->gpu);

  for (uint32_t i = 0; i < count; i++) {
    Batch* batch = batches[i];
    BatchDraw* draw = batch->draws;
    BatchGroup* group = batch->groups;

    Megaview camera = bufferAllocate(GPU_MEMORY_CPU_WRITE, sizeof(batch->viewMatrix) + sizeof(batch->projection), 256);
    memcpy(camera.data, batch->viewMatrix, sizeof(batch->viewMatrix) + sizeof(batch->projection));

    lovrCheck(batch->info.type == BATCH_RENDER, "Attempt to use a compute Batch for rendering");

    for (uint32_t j = 0; j < batch->groupCount; j++, group++) {
      if (group->dirty & DIRTY_PIPELINE) {
        gpu_bind_pipeline(stream, draw->pipeline);
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
        gpu_bind_bundle(stream, NULL, 1, draw->bundle, NULL, 0);
      }

      if (draw->key.bits.index != 0xff) {
        for (uint16_t k = 0; k < group->count; k++, draw++) {
          gpu_draw_indexed(stream, draw->count, draw->instances, draw->start, draw->base);
        }
      } else {
        for (uint16_t k = 0; k < group->count; k++, draw++) {
          gpu_draw(stream, draw->count, draw->instances, draw->start);
        }
      }
    }
  }

  gpu_render_end(stream);

  state.streamOrder[state.streamCount] = 1;
  state.streams[state.streamCount++] = stream;
}

void lovrGraphicsCompute(Batch** batches, uint32_t count) {
  // TODO
}

// Buffer

Buffer* lovrGraphicsGetBuffer(BufferInfo* info) {
  size_t size = info->length * MAX(info->stride, 1);
  lovrCheck(size > 0, "Buffer size must be positive");
  lovrCheck(size <= 1 << 30, "Max Buffer size is 1GB");
  Buffer* buffer = talloc(sizeof(Buffer));
  buffer->ref = 1;
  buffer->size = size;
  buffer->mega = bufferAllocate(GPU_MEMORY_CPU_WRITE, size, 256);
  buffer->info = *info;
  buffer->scratch = true;
  return buffer;
}

Buffer* lovrBufferCreate(BufferInfo* info) {
  size_t size = info->length * MAX(info->stride, 1);
  lovrCheck(size > 0, "Buffer size must be positive");
  lovrCheck(size <= 1 << 30, "Max Buffer size is 1GB");
  Buffer* buffer = calloc(1, sizeof(Buffer));
  lovrAssert(buffer, "Out of memory");
  buffer->ref = 1;
  buffer->size = size;
  buffer->mega = bufferAllocate(GPU_MEMORY_GPU, size, 256);
  buffer->info = *info;
  state.buffers.list[buffer->mega.index].refs++;
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
  lovrCheck(info->samples == 1 || ~info->usage & TEXTURE_STORAGE, "Currently, Textures with the 'compute' flag can not be multisampled");
  lovrCheck(info->samples == 1 || info->mipmaps == 1, "Multisampled textures can only have 1 mipmap");
  lovrCheck(~info->usage & TEXTURE_SAMPLE || (supports & GPU_FEATURE_SAMPLE), "GPU does not support the 'sample' flag for this format");
  lovrCheck(~info->usage & TEXTURE_CANVAS || (supports & GPU_FEATURE_RENDER), "GPU does not support the 'render' flag for this format");
  lovrCheck(~info->usage & TEXTURE_STORAGE || (supports & GPU_FEATURE_STORAGE), "GPU does not support the 'compute' flag for this format");
  lovrCheck(~info->usage & TEXTURE_CANVAS || info->width <= state.limits.renderSize[0], "Texture has 'render' flag but its size exceeds limits.renderSize");
  lovrCheck(~info->usage & TEXTURE_CANVAS || info->height <= state.limits.renderSize[1], "Texture has 'render' flag but its size exceeds limits.renderSize");
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
  if (info->usage & TEXTURE_CANVAS && info->type != TEXTURE_VOLUME && info->depth <= 6) {
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
  gpu_texture_destroy(texture->gpu);
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

// Canvas

Canvas* lovrCanvasCreate(CanvasInfo* info) {
  lovrCheck(info->color[0].texture || info->depth.texture, "Canvas must have at least one color or depth texture");
  const TextureInfo* first = info->color[0].texture ? &info->color[0].texture->info : &info->depth.texture->info;

  uint32_t width = first->width;
  uint32_t height = first->height;
  uint32_t samples = first->samples > 1 ? first->samples : info->samples;
  bool resolve = first->samples == 1 && info->samples > 1;

  // Validate size/samples
  lovrCheck(width <= state.limits.renderSize[0], "Canvas width exceeds the renderWidth limit of this GPU (%d)", state.limits.renderSize[0]);
  lovrCheck(height <= state.limits.renderSize[1], "Canvas width exceeds the renderHeight limit of this GPU (%d)", state.limits.renderSize[1]);
  lovrCheck(info->views <= state.limits.renderViews, "Canvas view count (%d) exceeds the renderViews limit of this GPU (%d)", info->views, state.limits.renderViews);
  lovrCheck((samples & (samples - 1)) == 0, "Canvas multisample count must be a power of 2");

  // Validate color attachments
  for (uint32_t i = 0; i < info->count && i < 4; i++) {
    Texture* texture = info->color[i].texture;
    bool renderable = texture->info.format == ~0u || (state.features.formats[texture->info.format] & GPU_FEATURE_RENDER_COLOR);
    lovrCheck(renderable, "This GPU does not support rendering to the texture format used by Canvas color attachment #%d", i + 1);
    lovrCheck(texture->info.usage & TEXTURE_CANVAS, "Texture must be created with the 'render' flag to attach it to a Canvas");
    lovrCheck(texture->info.width == first->width, "Canvas texture sizes must match");
    lovrCheck(texture->info.height == first->height, "Canvas texture sizes must match");
    lovrCheck(texture->info.depth == first->depth, "Canvas texture sizes must match");
    lovrCheck(texture->info.samples == first->samples, "Canvas texture sample counts must match");
  }

  // Validate depth attachment
  if (info->depth.enabled) {
    Texture* texture = info->depth.texture;
    TextureFormat format = texture ? texture->info.format : info->depth.format;
    bool renderable = state.features.formats[format] & GPU_FEATURE_RENDER_DEPTH;
    lovrCheck(renderable, "This GPU does not support rendering to the Canvas depth buffer's format");
    if (texture) {
      lovrCheck(texture->info.usage & TEXTURE_CANVAS, "Textures must be created with the 'render' flag to attach them to a Canvas");
      lovrCheck(texture->info.width == first->width, "Canvas texture sizes must match");
      lovrCheck(texture->info.height == first->height, "Canvas texture sizes must match");
      lovrCheck(texture->info.depth == first->depth, "Canvas texture sizes must match");
      lovrCheck(texture->info.samples == samples, "Currently, Canvas depth buffer sample count must match its main multisample count");
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

  Canvas* canvas = calloc(1, sizeof(Canvas));
  lovrAssert(canvas, "Out of memory");
  canvas->info = *info;
  canvas->ref = 1;

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

  // Color
  for (uint32_t i = 0; i < info->count; i++) {
    Texture* texture = info->color[i].texture;
    canvas->gpu.color[i].texture = texture->renderView;
    canvas->colorTextures[i] = texture;
    lovrRetain(texture);
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
      canvas->gpu.depth.texture = canvas->depthTexture->renderView;
      canvas->depthTexture = info->depth.texture;
      lovrRetain(canvas->depthTexture);
    } else {
      textureInfo.format = info->depth.format;
      canvas->depthTexture = lovrTextureCreate(&textureInfo);
      canvas->gpu.depth.texture = canvas->depthTexture->renderView;
    }
  }

  return canvas;
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

static bool parseSpirv(Shader* shader, const void* source, uint32_t size, uint8_t stage) {
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
          lovrCheck(value < 16, "Unsupported Shader code: variable %d uses binding %d, but the binding must be less than 16", id, value);
          vars[id].binding = value;
        } else if (decoration == 34) { // Group
          lovrCheck(value < 4, "Unsupported Shader code: variable %d is in group %d, but group must be less than 4", id, value);
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
        // TODO
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

  if (info->type == SHADER_COMPUTE) {
    lovrCheck(info->source[0] && !info->source[1], "Compute shaders require one stage");
    parseSpirv(shader, info->source[0], info->length[0], GPU_STAGE_COMPUTE);
  } else {
    lovrCheck(info->source[0] && info->source[1], "Currently, graphics shaders require two stages");
    parseSpirv(shader, info->source[0], info->length[0], GPU_STAGE_VERTEX);
    parseSpirv(shader, info->source[1], info->length[1], GPU_STAGE_FRAGMENT);
  }

  gpu_shader_info gpuInfo = {
    .stages[0] = { info->source[0], info->length[0], NULL },
    .stages[1] = { info->source[1], info->length[1], NULL },
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

Batch* lovrBatchInit(void* (*allocate)(size_t size), BatchInfo* info) {
  size_t sizes[3];
  size_t total = 0;
  total += sizes[0] = sizeof(Batch);
  total += sizes[1] = info->capacity * sizeof(BatchGroup);
  total += sizes[2] = info->capacity * sizeof(BatchDraw);
  char* memory = allocate(total);
  lovrAssert(memory, "Out of memory");
  Batch* batch = (Batch*) memory; memory += sizes[0];
  batch->groups = (BatchGroup*) memory, memory += sizes[1];
  batch->draws = (BatchDraw*) memory, memory += sizes[2];
  batch->ref = 1;
  batch->info = *info;
  return batch;
}

Batch* lovrGraphicsGetBatch(BatchInfo* info) {
  Batch* batch = lovrBatchInit(talloc, info);
  uint32_t bufferSizes[] = {
    [BUFFER_TRANSFORM] = 64 * info->capacity,
    [BUFFER_DRAW_DATA] = 16 * info->capacity,
    [BUFFER_PRIMITIVE] = info->primitiveMemory
  };
  for (uint32_t i = 0; i < BUFFER_MAX; i++) {
    batch->buffers[i] = bufferAllocate(GPU_MEMORY_CPU_WRITE, bufferSizes[i], 256);
  }
  batch->scratch = true;
  return batch;
}

Batch* lovrBatchCreate(BatchInfo* info) {
  Batch* batch = lovrBatchInit(malloc, info);
  uint32_t bufferSizes[] = {
    [BUFFER_TRANSFORM] = 64 * info->capacity,
    [BUFFER_DRAW_DATA] = 16 * info->capacity,
    [BUFFER_PRIMITIVE] = info->primitiveMemory
  };
  for (uint32_t i = 0; i < BUFFER_MAX; i++) {
    Megaview scratch = bufferAllocate(GPU_MEMORY_CPU_WRITE, bufferSizes[i], 256);
    Megaview buffer = bufferAllocate(GPU_MEMORY_GPU, bufferSizes[i], 256);
    gpu_copy_buffers(state.transfers, scratch.gpu, buffer.gpu, scratch.offset, buffer.offset, bufferSizes[i]);
    batch->scratchBuffers[i] = scratch;
    batch->buffers[i] = buffer;
  }
  return batch;
}

void lovrBatchDestroy(void* ref) {
  Batch* batch = ref;
  if (!batch->scratch) free(batch);
}

const BatchInfo* lovrBatchGetInfo(Batch* batch) {
  return &batch->info;
}

uint32_t lovrBatchGetCount(Batch* batch) {
  return batch->drawCount;
}

void lovrBatchReset(Batch* batch) {
  batch->groupCount = 0;
  batch->drawCount = 0;
  batch->transform = 0;
  lovrBatchOrigin(batch);
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

void lovrBatchSetBlendMode(Batch* batch, uint32_t target, BlendMode mode, BlendAlphaMode alphaMode) {
  if (mode == BLEND_NONE) {
    batch->pipeline->dirty |= batch->pipeline->info.blend[target].enabled;
    memset(&batch->pipeline->info.blend[target], 0, sizeof(gpu_blend_state));
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

  batch->pipeline->info.blend[target] = blendModes[mode];

  if (alphaMode == BLEND_PREMULTIPLIED && mode != BLEND_MULTIPLY) {
    batch->pipeline->info.blend[target].color.src = GPU_BLEND_ONE;
  }

  batch->pipeline->info.blend[target].enabled = true;
  batch->pipeline->dirty = true;
}

void lovrBatchSetColorMask(Batch* batch, uint32_t target, bool r, bool g, bool b, bool a) {
  uint8_t mask = (r << 0) | (g << 1) | (b << 2) | (a << 3);
  batch->pipeline->dirty |= batch->pipeline->info.colorMask[target] != mask;
  batch->pipeline->info.colorMask[target] = mask;
}

void lovrBatchSetCullMode(Batch* batch, CullMode mode) {
  batch->pipeline->dirty |= batch->pipeline->info.rasterizer.cullMode != (gpu_cull_mode) mode;
  batch->pipeline->info.rasterizer.cullMode = (gpu_cull_mode) mode;
}

void lovrBatchSetDepthTest(Batch* batch, CompareMode test, bool write) {
  batch->pipeline->dirty |= batch->pipeline->info.depth.test != (gpu_compare_mode) test;
  batch->pipeline->dirty |= batch->pipeline->info.depth.write != write;
  batch->pipeline->info.depth.test = (gpu_compare_mode) test;
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

void lovrBatchSetStencilTest(Batch* batch, CompareMode test, uint8_t value) {
  switch (test) { // (Reversed compare mode)
    case COMPARE_NONE: default: batch->pipeline->info.stencil.test = GPU_COMPARE_NONE; break;
    case COMPARE_EQUAL: batch->pipeline->info.stencil.test = GPU_COMPARE_EQUAL; break;
    case COMPARE_NEQUAL: batch->pipeline->info.stencil.test = GPU_COMPARE_NEQUAL; break;
    case COMPARE_LESS: batch->pipeline->info.stencil.test = GPU_COMPARE_GREATER; break;
    case COMPARE_LEQUAL: batch->pipeline->info.stencil.test = GPU_COMPARE_GEQUAL; break;
    case COMPARE_GREATER: batch->pipeline->info.stencil.test = GPU_COMPARE_LESS; break;
    case COMPARE_GEQUAL: batch->pipeline->info.stencil.test = GPU_COMPARE_LEQUAL; break;
  }
  batch->pipeline->info.stencil.value = value;
  batch->pipeline->dirty = true;
}

void lovrBatchSetShader(Batch* batch, Shader* shader) {
  if (shader == batch->shader) return;
  lovrRetain(shader);
  lovrRelease(batch->shader, lovrShaderDestroy);
  batch->shader = shader;
  batch->pipeline->info.shader = shader->gpu;
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

void lovrBatchSetPipeline(Batch* batch, Pipeline* pipeline) {
  lovrCheck(batch->pass == pipeline->pass, "Batch canvas and Pipeline canvas must match");
  batch->pipeline->dirty = false;
}

typedef struct {
  DrawMode mode;
  float* transform;
  struct {
    void* data;
    void** pointer;
    Megaview buffer;
    uint32_t offset;
    uint32_t count;
  } vertex;
  struct {
    void* data;
    void** pointer;
    Megaview buffer;
    uint32_t offset;
    uint32_t count;
  } index;
} DrawRequest;

static BatchDraw* lovrBatchDraw(Batch* batch, DrawRequest* req) {
  lovrCheck(batch->drawCount < batch->info.capacity, "Too many draws");
  BatchDraw* draw = &batch->draws[batch->drawCount];
  draw->index = batch->drawCount++;

  // Pipeline
  if (batch->pipeline->info.drawMode != (gpu_draw_mode) req->mode) {
    batch->pipeline->info.drawMode = (gpu_draw_mode) req->mode;
    batch->pipeline->dirty = true;
  }

  if (batch->pipeline->dirty) {
    batch->pipeline->dirty = false;
    batch->pipeline->hash = hash64(&batch->pipeline->info, sizeof(gpu_pipeline_info));
    uint64_t entry = map_get(&state.pipelines, batch->pipeline->hash);

    if (entry == MAP_NIL) {
      gpu_pipeline* pipeline = calloc(1, gpu_sizeof_pipeline());
      lovrAssert(gpu_pipeline_init_graphics(pipeline, &batch->pipeline->info), "Failed to create pipeline");
      map_set(&state.pipelines, batch->pipeline->hash, (uintptr_t) pipeline);
      batch->pipeline->instance = pipeline;
      entry = (uintptr_t) pipeline;
    }

    draw->pipeline = (gpu_pipeline*) (uintptr_t) entry;
  } else {
    draw->pipeline = batch->pipeline->instance;
  }

  // Vertices
  if (req->vertex.data) {

  } else if (req->vertex.pointer) {

  } else if (req->vertex.buffer) {

  }

  // Indices
  if (req->index.data) {

  } else if (req->index.pointer) {

  } else if (req->index.buffer) {

  }

  // Bundle
  if (batch->bindingsDirty) {
    // if no room in bindings array, flush (allocate a new bunch of bundles and write all the bindings to it)
    // write the bindings
    // increment bundle index (batch->bundle)
  }

  draw->bundle = (gpu_bundle*) (batch->bundles + batch->bundle * gpu_sizeof_bundle());

  // Transform
  if (transform) {
    float modelMatrix[16];
    mat4_init(modelMatrix, batch->transform);
    mat4_mul(modelMatrix, transform);
    memcpy(batch->scratchBuffers[BUFFER_TRANSFORM].data + draw->index * 64, modelMatrix, 64);
  } else {
    memcpy(batch->scratchBuffers[BUFFER_TRANSFORM].data + draw->index * 16, batch->transform, 16 * sizeof(float));
  }

  // Color
  memcpy(batch->scratchBuffers[BUFFER_DRAW_DATA].data + draw->index * 4, batch->pipeline->color, 4 * sizeof(float));

  // Key
  draw->key.bits.pipeline = batch->pipeline->hash & 0xffff;
  draw->key.bits.chunk = (draw->index >> 8) & 0xf;
  draw->key.bits.bundle = batch->bundle & 0xfff;
  draw->key.bits.depth = 0;

  return draw;
}

static void lovrBatchGetIndices(Batch* batch, BatchDraw* draw, float** vertices, uint32_t vertexCount, uint16_t** indices, uint32_t indexCount) {
  uint32_t size = count * sizeof(uint16_t);
  lovrCheck(batch->shapeCursor + size <= batch->info.primitiveMemory, "Out of geometry memory");

  draw->indexBuffer = batch->buffers[BUFFER_PRIMITIVE].gpu;
  draw->key.bits.index = batch->buffers[BUFFER_PRIMITIVE].index;

  void* data = batch->scratchBuffers[BUFFER_PRIMITIVE].data + batch->shapeCursor;
  batch->shapeCursor += size;
  return data;
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
    .index.data = indices,
    .count = COUNTOF(indices),
    .base = state.baseVertex[SHAPE_CUBE]
  });
}
