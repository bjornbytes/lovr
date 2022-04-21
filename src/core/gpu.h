#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool discrete;
  uint32_t serial;
  uint32_t vendor;
  uint32_t version;
  const char name[256];
  const char* renderer;
  uint32_t subgroupSize;
} gpu_device;

typedef struct {
  bool bptc;
  bool astc;
  bool wireframe;
  bool depthClamp;
  bool clipDistance;
  bool cullDistance;
  bool fullIndexBufferRange;
  bool indirectDrawFirstInstance;
  bool dynamicIndexing;
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
  gpu_device* device;
  gpu_features* features;
  gpu_limits* limits;
} gpu_config;

bool gpu_init(gpu_config* config);
void gpu_destroy(void);
