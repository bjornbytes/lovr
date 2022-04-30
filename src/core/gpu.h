#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct gpu_buffer gpu_buffer;
typedef struct gpu_texture gpu_texture;
typedef struct gpu_stream gpu_stream;

size_t gpu_sizeof_buffer(void);
size_t gpu_sizeof_texture(void);

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

// Texture

enum {
  GPU_TEXTURE_SAMPLE    = (1 << 0),
  GPU_TEXTURE_RENDER    = (1 << 1),
  GPU_TEXTURE_STORAGE   = (1 << 2),
  GPU_TEXTURE_COPY_SRC  = (1 << 3),
  GPU_TEXTURE_COPY_DST  = (1 << 4),
  GPU_TEXTURE_TRANSIENT = (1 << 5)
};

typedef enum {
  GPU_TEXTURE_2D,
  GPU_TEXTURE_3D,
  GPU_TEXTURE_CUBE,
  GPU_TEXTURE_ARRAY
} gpu_texture_type;

typedef enum {
  GPU_FORMAT_R8,
  GPU_FORMAT_RG8,
  GPU_FORMAT_RGBA8,
  GPU_FORMAT_R16,
  GPU_FORMAT_RG16,
  GPU_FORMAT_RGBA16,
  GPU_FORMAT_R16F,
  GPU_FORMAT_RG16F,
  GPU_FORMAT_RGBA16F,
  GPU_FORMAT_R32F,
  GPU_FORMAT_RG32F,
  GPU_FORMAT_RGBA32F,
  GPU_FORMAT_RGB565,
  GPU_FORMAT_RGB5A1,
  GPU_FORMAT_RGB10A2,
  GPU_FORMAT_RG11B10F,
  GPU_FORMAT_D16,
  GPU_FORMAT_D24S8,
  GPU_FORMAT_D32F,
  GPU_FORMAT_BC1,
  GPU_FORMAT_BC2,
  GPU_FORMAT_BC3,
  GPU_FORMAT_BC4U,
  GPU_FORMAT_BC4S,
  GPU_FORMAT_BC5U,
  GPU_FORMAT_BC5S,
  GPU_FORMAT_BC6UF,
  GPU_FORMAT_BC6SF,
  GPU_FORMAT_BC7,
  GPU_FORMAT_ASTC_4x4,
  GPU_FORMAT_ASTC_5x4,
  GPU_FORMAT_ASTC_5x5,
  GPU_FORMAT_ASTC_6x5,
  GPU_FORMAT_ASTC_6x6,
  GPU_FORMAT_ASTC_8x5,
  GPU_FORMAT_ASTC_8x6,
  GPU_FORMAT_ASTC_8x8,
  GPU_FORMAT_ASTC_10x5,
  GPU_FORMAT_ASTC_10x6,
  GPU_FORMAT_ASTC_10x8,
  GPU_FORMAT_ASTC_10x10,
  GPU_FORMAT_ASTC_12x10,
  GPU_FORMAT_ASTC_12x12,
  GPU_FORMAT_COUNT
} gpu_texture_format;

typedef struct {
  gpu_texture* source;
  gpu_texture_type type;
  uint32_t layerIndex;
  uint32_t layerCount;
  uint32_t levelIndex;
  uint32_t levelCount;
} gpu_texture_view_info;

typedef struct {
  gpu_texture_type type;
  gpu_texture_format format;
  uint32_t size[3];
  uint32_t mipmaps;
  uint32_t samples;
  uint32_t usage;
  bool srgb;
  uintptr_t handle;
  const char* label;
  struct {
    gpu_stream* stream;
    gpu_buffer* buffer;
    uint32_t* levelOffsets;
    uint32_t levelCount;
    bool generateMipmaps;
  } upload;
} gpu_texture_info;

bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info);
bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info);
void gpu_texture_destroy(gpu_texture* texture);

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

enum {
  GPU_FEATURE_SAMPLE   = (1 << 0),
  GPU_FEATURE_RENDER   = (1 << 1),
  GPU_FEATURE_BLEND    = (1 << 2),
  GPU_FEATURE_FILTER   = (1 << 3),
  GPU_FEATURE_STORAGE  = (1 << 4),
  GPU_FEATURE_ATOMIC   = (1 << 5),
  GPU_FEATURE_BLIT_SRC = (1 << 6),
  GPU_FEATURE_BLIT_DST = (1 << 7)
};

typedef struct {
  uint8_t formats[GPU_FORMAT_COUNT];
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
