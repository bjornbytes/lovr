#include "graphics/graphics.h"
#include "core/gpu.h"
#include "core/os.h"
#include "util.h"
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

struct Pass {
  uint32_t ref;
  PassInfo info;
  gpu_stream* stream;
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

// Buffer

Buffer* lovrGraphicsGetBuffer(BufferInfo* info, void** data) {
  uint32_t size = info->length * info->stride;
  lovrCheck(size > 0, "Buffer size can not be zero");
  lovrCheck(size <= 1 << 30, "Max buffer size is 16GB");

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

static void onMessage(void* context, const char* message, bool severe) {
  if (severe) {
    lovrLog(LOG_ERROR, "GPU", message);
  } else {
    lovrLog(LOG_DEBUG, "GPU", message);
  }
}
