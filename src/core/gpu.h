#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct gpu_buffer gpu_buffer;
typedef struct gpu_stream gpu_stream;

size_t gpu_sizeof_buffer(void);

// Buffer

typedef struct {
  uint32_t size;
  void** pointer;
  uintptr_t handle;
  const char* label;
} gpu_buffer_info;

bool gpu_buffer_init(gpu_buffer* buffer, gpu_buffer_info* info);
void gpu_buffer_destroy(gpu_buffer* buffer);

typedef enum {
  GPU_MAP_WRITE,
  GPU_MAP_READ
} gpu_map_mode;

void* gpu_map(gpu_buffer* buffer, uint32_t size, uint32_t align, gpu_map_mode mode);

// Stream

gpu_stream* gpu_stream_begin(const char* label);
void gpu_stream_end(gpu_stream* stream);
void gpu_copy_buffers(gpu_stream* stream, gpu_buffer* src, gpu_buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size);
void gpu_clear_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t size);

// Entry

typedef struct {
  uint32_t deviceId;
  uint32_t vendorId;
  char deviceName[256];
  const char* renderer;
  uint32_t subgroupSize;
  bool discrete;
} gpu_device_info;

typedef struct {
  bool textureBC;
  bool textureASTC;
  bool wireframe;
  bool depthClamp;
  bool indirectDrawFirstInstance;
  bool float64;
  bool int64;
  bool int16;
} gpu_features;

typedef struct {
  uint32_t textureSize2D;
  uint32_t textureSize3D;
  uint32_t textureSizeCube;
  uint32_t textureLayers;
  uint32_t renderSize[3];
  uint32_t uniformBufferRange;
  uint32_t storageBufferRange;
  uint32_t uniformBufferAlign;
  uint32_t storageBufferAlign;
  uint32_t vertexAttributes;
  uint32_t vertexBuffers;
  uint32_t vertexBufferStride;
  uint32_t vertexShaderOutputs;
  uint32_t clipDistances;
  uint32_t cullDistances;
  uint32_t clipAndCullDistances;
  uint32_t computeDispatchCount[3];
  uint32_t computeWorkgroupSize[3];
  uint32_t computeWorkgroupVolume;
  uint32_t computeSharedMemory;
  uint32_t pushConstantSize;
  uint32_t indirectDrawCount;
  uint32_t instances;
  float anisotropy;
  float pointSize;
} gpu_limits;

typedef struct {
  bool debug;
  void* userdata;
  void (*callback)(void* userdata, const char* message, bool error);
  const char* engineName;
  uint32_t engineVersion[3];
  gpu_device_info* device;
  gpu_features* features;
  gpu_limits* limits;
} gpu_config;

bool gpu_init(gpu_config* config);
void gpu_destroy(void);
uint32_t gpu_begin(void);
void gpu_submit(gpu_stream** streams, uint32_t count);
bool gpu_finished(uint32_t tick);
void gpu_wait(void);
