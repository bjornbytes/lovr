#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

typedef struct gpu_buffer gpu_buffer;
typedef struct gpu_texture gpu_texture;
typedef struct gpu_sampler gpu_sampler;
typedef struct gpu_layout gpu_layout;
typedef struct gpu_shader gpu_shader;
typedef struct gpu_bundle_pool gpu_bundle_pool;
typedef struct gpu_bundle gpu_bundle;
typedef struct gpu_pass gpu_pass;
typedef struct gpu_pipeline gpu_pipeline;
typedef struct gpu_tally gpu_tally;
typedef struct gpu_stream gpu_stream;

size_t gpu_sizeof_buffer(void);
size_t gpu_sizeof_texture(void);
size_t gpu_sizeof_sampler(void);
size_t gpu_sizeof_layout(void);
size_t gpu_sizeof_shader(void);
size_t gpu_sizeof_bundle_pool(void);
size_t gpu_sizeof_bundle(void);
size_t gpu_sizeof_pass(void);
size_t gpu_sizeof_pipeline(void);
size_t gpu_sizeof_tally(void);

// Buffer

typedef enum {
  GPU_BUFFER_STATIC,
  GPU_BUFFER_STREAM,
  GPU_BUFFER_UPLOAD,
  GPU_BUFFER_DOWNLOAD
} gpu_buffer_type;

typedef struct {
  gpu_buffer_type type;
  uint32_t size;
  void** pointer;
  uintptr_t handle;
  const char* label;
} gpu_buffer_info;

bool gpu_buffer_init(gpu_buffer* buffer, gpu_buffer_info* info);
void gpu_buffer_destroy(gpu_buffer* buffer);

// Texture

enum {
  GPU_TEXTURE_SAMPLE    = (1 << 0),
  GPU_TEXTURE_RENDER    = (1 << 1),
  GPU_TEXTURE_STORAGE   = (1 << 2),
  GPU_TEXTURE_COPY_SRC  = (1 << 3),
  GPU_TEXTURE_COPY_DST  = (1 << 4),
  GPU_TEXTURE_FOVEATION = (1 << 5)
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
  GPU_FORMAT_D24,
  GPU_FORMAT_D32F,
  GPU_FORMAT_D24S8,
  GPU_FORMAT_D32FS8,
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
  GPU_FORMAT_COUNT,
  GPU_FORMAT_SURFACE = 0xff
} gpu_texture_format;

enum {
  GPU_ASPECT_COLOR = (1 << 0),
  GPU_ASPECT_DEPTH = (1 << 1),
  GPU_ASPECT_STENCIL = (1 << 2)
};

typedef struct {
  gpu_texture* source;
  gpu_texture_type type;
  uint32_t usage;
  bool srgb;
  uint32_t aspect;
  uint32_t layerIndex;
  uint32_t layerCount;
  uint32_t levelIndex;
  uint32_t levelCount;
  const char* label;
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

// Surface

typedef struct {
  uint32_t width;
  uint32_t height;
  bool vsync;
  union {
    struct {
      uintptr_t window;
      uintptr_t instance;
    } win32;
    struct {
      uintptr_t layer;
    } macos;
    struct {
      uintptr_t connection;
      uintptr_t window;
    } xcb;
  };
} gpu_surface_info;

bool gpu_surface_init(gpu_surface_info* info);
bool gpu_surface_resize(uint32_t width, uint32_t height);
bool gpu_surface_acquire(gpu_texture** texture);
bool gpu_surface_present(void);

// Sampler

typedef enum {
  GPU_FILTER_NEAREST,
  GPU_FILTER_LINEAR
} gpu_filter;

typedef enum {
  GPU_WRAP_CLAMP,
  GPU_WRAP_REPEAT,
  GPU_WRAP_MIRROR,
  GPU_WRAP_BORDER
} gpu_wrap;

typedef enum {
  GPU_COMPARE_NONE,
  GPU_COMPARE_EQUAL,
  GPU_COMPARE_NEQUAL,
  GPU_COMPARE_LESS,
  GPU_COMPARE_LEQUAL,
  GPU_COMPARE_GREATER,
  GPU_COMPARE_GEQUAL
} gpu_compare_mode;

typedef struct {
  gpu_filter min;
  gpu_filter mag;
  gpu_filter mip;
  gpu_wrap wrap[3];
  gpu_compare_mode compare;
  float anisotropy;
  float lodClamp[2];
} gpu_sampler_info;

bool gpu_sampler_init(gpu_sampler* sampler, gpu_sampler_info* info);
void gpu_sampler_destroy(gpu_sampler* sampler);

// Layout

typedef enum {
  GPU_SLOT_UNIFORM_BUFFER,
  GPU_SLOT_STORAGE_BUFFER,
  GPU_SLOT_UNIFORM_BUFFER_DYNAMIC,
  GPU_SLOT_STORAGE_BUFFER_DYNAMIC,
  GPU_SLOT_TEXTURE_WITH_SAMPLER,
  GPU_SLOT_SAMPLED_TEXTURE,
  GPU_SLOT_STORAGE_TEXTURE,
  GPU_SLOT_SAMPLER
} gpu_slot_type;

enum {
  GPU_STAGE_VERTEX = (1 << 0),
  GPU_STAGE_FRAGMENT = (1 << 1),
  GPU_STAGE_COMPUTE = (1 << 2),
  GPU_STAGE_GRAPHICS = GPU_STAGE_VERTEX | GPU_STAGE_FRAGMENT
};

typedef struct {
  uint32_t number;
  gpu_slot_type type;
  uint32_t stages;
} gpu_slot;

typedef struct {
  uint32_t count;
  gpu_slot* slots;
} gpu_layout_info;

bool gpu_layout_init(gpu_layout* layout, gpu_layout_info* info);
void gpu_layout_destroy(gpu_layout* layout);

// Shader

typedef struct {
  uint32_t stage;
  const void* code;
  size_t length;
} gpu_shader_source;

typedef struct {
  uint32_t stageCount;
  gpu_shader_source* stages;
  uint32_t pushConstantSize;
  gpu_layout* layouts[4];
  const char* label;
} gpu_shader_info;

bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info);
void gpu_shader_destroy(gpu_shader* shader);

// Bundles

typedef struct {
  gpu_buffer* object;
  uint32_t offset;
  uint32_t extent;
} gpu_buffer_binding;

typedef struct {
  gpu_texture* object;
  gpu_sampler* sampler;
} gpu_texture_binding;

typedef struct {
  uint32_t number;
  gpu_slot_type type;
  uint32_t count;
  union {
    gpu_buffer_binding buffer;
    gpu_texture_binding texture;
    gpu_buffer_binding* buffers;
    gpu_texture_binding* textures;
  };
} gpu_binding;

typedef struct {
  gpu_layout* layout;
  gpu_binding* bindings;
  uint32_t count;
} gpu_bundle_info;

typedef struct {
  gpu_bundle* bundles;
  gpu_bundle_info* contents;
  gpu_layout* layout;
  uint32_t count;
} gpu_bundle_pool_info;

bool gpu_bundle_pool_init(gpu_bundle_pool* pool, gpu_bundle_pool_info* info);
void gpu_bundle_pool_destroy(gpu_bundle_pool* pool);
void gpu_bundle_write(gpu_bundle** bundles, gpu_bundle_info* info, uint32_t count);

// Canvas

typedef enum {
  GPU_LOAD_OP_CLEAR,
  GPU_LOAD_OP_DISCARD,
  GPU_LOAD_OP_KEEP
} gpu_load_op;

typedef enum {
  GPU_SAVE_OP_KEEP,
  GPU_SAVE_OP_DISCARD
} gpu_save_op;

typedef struct {
  gpu_texture_format format;
  bool srgb;
  gpu_load_op load;
  gpu_save_op save;
  bool resolve;
} gpu_color_info;

typedef struct {
  gpu_texture_format format;
  gpu_load_op load, stencilLoad;
  gpu_save_op save, stencilSave;
  bool resolve;
} gpu_depth_info;

typedef struct {
  gpu_color_info color[4];
  gpu_depth_info depth;
  uint32_t colorCount;
  uint32_t samples;
  uint32_t views;
  bool foveated;
  bool surface;
} gpu_pass_info;

bool gpu_pass_init(gpu_pass* pass, gpu_pass_info* info);
void gpu_pass_destroy(gpu_pass* pass);

// Pipeline

typedef enum {
  GPU_PIPELINE_GRAPHICS,
  GPU_PIPELINE_COMPUTE
} gpu_pipeline_type;

typedef enum {
  GPU_FLAG_B32,
  GPU_FLAG_I32,
  GPU_FLAG_U32,
  GPU_FLAG_F32
} gpu_flag_type;

typedef union {
  uint32_t b32;
  int32_t i32;
  uint32_t u32;
  float f32;
} gpu_flag_value;

typedef struct {
  uint32_t id;
  gpu_flag_type type;
  gpu_flag_value value;
} gpu_shader_flag;

typedef enum {
  GPU_DRAW_POINTS,
  GPU_DRAW_LINES,
  GPU_DRAW_TRIANGLES
} gpu_draw_mode;

typedef enum {
  GPU_TYPE_I8x4,
  GPU_TYPE_U8x4,
  GPU_TYPE_SN8x4,
  GPU_TYPE_UN8x4,
  GPU_TYPE_SN10x3,
  GPU_TYPE_UN10x3,
  GPU_TYPE_I16,
  GPU_TYPE_I16x2,
  GPU_TYPE_I16x4,
  GPU_TYPE_U16,
  GPU_TYPE_U16x2,
  GPU_TYPE_U16x4,
  GPU_TYPE_SN16x2,
  GPU_TYPE_SN16x4,
  GPU_TYPE_UN16x2,
  GPU_TYPE_UN16x4,
  GPU_TYPE_I32,
  GPU_TYPE_I32x2,
  GPU_TYPE_I32x3,
  GPU_TYPE_I32x4,
  GPU_TYPE_U32,
  GPU_TYPE_U32x2,
  GPU_TYPE_U32x3,
  GPU_TYPE_U32x4,
  GPU_TYPE_F16x2,
  GPU_TYPE_F16x4,
  GPU_TYPE_F32,
  GPU_TYPE_F32x2,
  GPU_TYPE_F32x3,
  GPU_TYPE_F32x4,
} gpu_attribute_type;

typedef struct {
  uint8_t buffer;
  uint8_t location;
  uint8_t offset;
  uint8_t type;
} gpu_attribute;

typedef struct {
  uint32_t bufferCount;
  uint32_t attributeCount;
  uint16_t instancedBuffers;
  uint16_t bufferStrides[16];
  gpu_attribute attributes[16];
} gpu_vertex_format;

typedef enum {
  GPU_CULL_NONE,
  GPU_CULL_FRONT,
  GPU_CULL_BACK
} gpu_cull_mode;

typedef enum {
  GPU_WINDING_CCW,
  GPU_WINDING_CW
} gpu_winding;

typedef struct {
  gpu_cull_mode cullMode;
  gpu_winding winding;
  float depthOffset;
  float depthOffsetSloped;
  float depthOffsetClamp;
  bool depthClamp;
  bool wireframe;
} gpu_rasterizer_state;

typedef struct {
  uint32_t count;
  bool alphaToCoverage;
  bool alphaToOne;
} gpu_multisample_state;

typedef struct {
  gpu_compare_mode test;
  bool write;
} gpu_depth_state;

typedef enum {
  GPU_STENCIL_KEEP,
  GPU_STENCIL_ZERO,
  GPU_STENCIL_REPLACE,
  GPU_STENCIL_INCREMENT,
  GPU_STENCIL_DECREMENT,
  GPU_STENCIL_INCREMENT_WRAP,
  GPU_STENCIL_DECREMENT_WRAP,
  GPU_STENCIL_INVERT
} gpu_stencil_op;

typedef struct {
  gpu_stencil_op failOp;
  gpu_stencil_op depthFailOp;
  gpu_stencil_op passOp;
  gpu_compare_mode test;
  uint8_t testMask;
  uint8_t writeMask;
  uint8_t value;
} gpu_stencil_state;

typedef enum {
  GPU_BLEND_ZERO,
  GPU_BLEND_ONE,
  GPU_BLEND_SRC_COLOR,
  GPU_BLEND_ONE_MINUS_SRC_COLOR,
  GPU_BLEND_SRC_ALPHA,
  GPU_BLEND_ONE_MINUS_SRC_ALPHA,
  GPU_BLEND_DST_COLOR,
  GPU_BLEND_ONE_MINUS_DST_COLOR,
  GPU_BLEND_DST_ALPHA,
  GPU_BLEND_ONE_MINUS_DST_ALPHA
} gpu_blend_factor;

typedef enum {
  GPU_BLEND_ADD,
  GPU_BLEND_SUB,
  GPU_BLEND_RSUB,
  GPU_BLEND_MIN,
  GPU_BLEND_MAX
} gpu_blend_op;

typedef struct {
  struct {
    gpu_blend_factor src;
    gpu_blend_factor dst;
    gpu_blend_op op;
  } color, alpha;
  bool enabled;
} gpu_blend_state;

typedef struct {
  gpu_pass* pass;
  gpu_shader* shader;
  gpu_shader_flag* flags;
  uint32_t flagCount;
  gpu_draw_mode drawMode;
  gpu_vertex_format vertex;
  gpu_rasterizer_state rasterizer;
  gpu_multisample_state multisample;
  gpu_depth_state depth;
  gpu_stencil_state stencil;
  gpu_blend_state blend[4];
  uint8_t colorMask[4];
  const char* label;
} gpu_pipeline_info;

typedef struct {
  gpu_shader* shader;
  gpu_shader_flag* flags;
  uint32_t flagCount;
  const char* label;
} gpu_compute_pipeline_info;

bool gpu_pipeline_init_graphics(gpu_pipeline* pipeline, gpu_pipeline_info* info);
bool gpu_pipeline_init_compute(gpu_pipeline* pipeline, gpu_compute_pipeline_info* info);
void gpu_pipeline_destroy(gpu_pipeline* pipeline);
void gpu_pipeline_get_cache(void* data, size_t* size);

// Tally

typedef enum {
  GPU_TALLY_TIME,
  GPU_TALLY_PIXEL
} gpu_tally_type;

typedef struct {
  gpu_tally_type type;
  uint32_t count;
} gpu_tally_info;

bool gpu_tally_init(gpu_tally* tally, gpu_tally_info* info);
void gpu_tally_destroy(gpu_tally* tally);

// Stream

typedef struct {
  gpu_texture* texture;
  gpu_texture* resolve;
  float clear[4];
} gpu_color_attachment;

typedef struct {
  gpu_texture* texture;
  gpu_texture* resolve;
  float clear;
  uint8_t stencilClear;
} gpu_depth_attachment;

typedef struct {
  gpu_color_attachment color[4];
  gpu_depth_attachment depth;
  gpu_texture* foveation;
  gpu_pass* pass;
  uint32_t width;
  uint32_t height;
  uint32_t area[4];
} gpu_canvas;

typedef enum {
  GPU_INDEX_U16,
  GPU_INDEX_U32
} gpu_index_type;

typedef enum {
  GPU_PHASE_INDIRECT = (1 << 0),
  GPU_PHASE_INPUT_INDEX = (1 << 1),
  GPU_PHASE_INPUT_VERTEX = (1 << 2),
  GPU_PHASE_SHADER_VERTEX = (1 << 3),
  GPU_PHASE_SHADER_FRAGMENT = (1 << 4),
  GPU_PHASE_SHADER_COMPUTE = (1 << 5),
  GPU_PHASE_DEPTH_EARLY = (1 << 6),
  GPU_PHASE_DEPTH_LATE = (1 << 7),
  GPU_PHASE_COLOR = (1 << 8),
  GPU_PHASE_COPY = (1 << 9),
  GPU_PHASE_CLEAR = (1 << 10),
  GPU_PHASE_BLIT = (1 << 11)
} gpu_phase;

typedef enum {
  GPU_CACHE_INDIRECT = (1 << 0),
  GPU_CACHE_INDEX = (1 << 1),
  GPU_CACHE_VERTEX = (1 << 2),
  GPU_CACHE_UNIFORM = (1 << 3),
  GPU_CACHE_TEXTURE = (1 << 4),
  GPU_CACHE_STORAGE_READ = (1 << 5),
  GPU_CACHE_STORAGE_WRITE = (1 << 6),
  GPU_CACHE_DEPTH_READ = (1 << 7),
  GPU_CACHE_DEPTH_WRITE = (1 << 8),
  GPU_CACHE_COLOR_READ = (1 << 9),
  GPU_CACHE_COLOR_WRITE = (1 << 10),
  GPU_CACHE_TRANSFER_READ = (1 << 11),
  GPU_CACHE_TRANSFER_WRITE = (1 << 12),
  GPU_CACHE_WRITE_MASK = GPU_CACHE_STORAGE_WRITE | GPU_CACHE_DEPTH_WRITE | GPU_CACHE_COLOR_WRITE | GPU_CACHE_TRANSFER_WRITE,
  GPU_CACHE_READ_MASK = ~GPU_CACHE_WRITE_MASK
} gpu_cache;

typedef struct {
  gpu_phase prev;
  gpu_phase next;
  gpu_cache flush;
  gpu_cache clear;
} gpu_barrier;

gpu_stream* gpu_stream_begin(const char* label);
bool gpu_stream_end(gpu_stream* stream);
void gpu_render_begin(gpu_stream* stream, gpu_canvas* canvas);
void gpu_render_end(gpu_stream* stream, gpu_canvas* canvas);
void gpu_compute_begin(gpu_stream* stream);
void gpu_compute_end(gpu_stream* stream);
void gpu_set_viewport(gpu_stream* stream, float viewport[4], float depthRange[2]);
void gpu_set_scissor(gpu_stream* stream, uint32_t scissor[4]);
void gpu_push_constants(gpu_stream* stream, gpu_shader* shader, void* data, uint32_t size);
void gpu_bind_pipeline(gpu_stream* stream, gpu_pipeline* pipeline, gpu_pipeline_type type);
void gpu_bind_bundles(gpu_stream* stream, gpu_shader* shader, gpu_bundle** bundles, uint32_t first, uint32_t count, uint32_t* dynamicOffsets, uint32_t dynamicOffsetCount);
void gpu_bind_vertex_buffers(gpu_stream* stream, gpu_buffer** buffers, uint32_t* offsets, uint32_t first, uint32_t count);
void gpu_bind_index_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, gpu_index_type type);
void gpu_draw(gpu_stream* stream, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t baseInstance);
void gpu_draw_indexed(gpu_stream* stream, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex, uint32_t baseInstance);
void gpu_draw_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount, uint32_t stride);
void gpu_draw_indirect_indexed(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount, uint32_t stride);
void gpu_compute(gpu_stream* stream, uint32_t x, uint32_t y, uint32_t z);
void gpu_compute_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset);
void gpu_copy_buffers(gpu_stream* stream, gpu_buffer* src, gpu_buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t extent);
void gpu_copy_textures(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t extent[3]);
void gpu_copy_buffer_texture(gpu_stream* stream, gpu_buffer* src, gpu_texture* dst, uint32_t srcOffset, uint32_t dstOffset[4], uint32_t extent[3]);
void gpu_copy_texture_buffer(gpu_stream* stream, gpu_texture* src, gpu_buffer* dst, uint32_t srcOffset[4], uint32_t dstOffset, uint32_t extent[3]);
void gpu_copy_tally_buffer(gpu_stream* stream, gpu_tally* src, gpu_buffer* dst, uint32_t srcIndex, uint32_t dstOffset, uint32_t count);
void gpu_clear_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t extent, uint32_t value);
void gpu_clear_texture(gpu_stream* stream, gpu_texture* texture, float value[4], uint32_t layer, uint32_t layerCount, uint32_t level, uint32_t levelCount);
void gpu_clear_tally(gpu_stream* stream, gpu_tally* tally, uint32_t index, uint32_t count);
void gpu_blit(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t srcExtent[3], uint32_t dstExtent[3], gpu_filter filter);
void gpu_sync(gpu_stream* stream, gpu_barrier* barriers, uint32_t count);
void gpu_tally_begin(gpu_stream* stream, gpu_tally* tally, uint32_t index);
void gpu_tally_finish(gpu_stream* stream, gpu_tally* tally, uint32_t index);
void gpu_tally_mark(gpu_stream* stream, gpu_tally* tally, uint32_t index);
void gpu_xr_acquire(gpu_stream* stream, gpu_texture* texture);
void gpu_xr_release(gpu_stream* stream, gpu_texture* texture);

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
  GPU_FEATURE_SAMPLE  = (1 << 0),
  GPU_FEATURE_RENDER  = (1 << 1),
  GPU_FEATURE_STORAGE = (1 << 2),
  GPU_FEATURE_BLIT    = (1 << 3)
};

typedef struct {
  uint8_t formats[GPU_FORMAT_COUNT][2];
  uint32_t sampleCounts;
  bool textureBC;
  bool textureASTC;
  bool wireframe;
  bool depthClamp;
  bool depthResolve;
  bool foveation;
  bool indirectDrawFirstInstance;
  bool packedBuffers;
  bool shaderDebug;
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
  uint32_t uniformBuffersPerStage;
  uint32_t storageBuffersPerStage;
  uint32_t sampledTexturesPerStage;
  uint32_t storageTexturesPerStage;
  uint32_t samplersPerStage;
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
  uint32_t workgroupCount[3];
  uint32_t workgroupSize[3];
  uint32_t totalWorkgroupSize;
  uint32_t computeSharedMemory;
  uint32_t pushConstantSize;
  uint32_t indirectDrawCount;
  uint32_t instances;
  float timestampPeriod;
  float anisotropy;
  float pointSize;
} gpu_limits;

typedef struct {
  bool debug;
  void* userdata;
  void (*fnLog)(void* userdata, const char* message);
  void* (*fnAlloc)(size_t size);
  void (*fnFree)(void* data);
  const char* engineName;
  uint32_t engineVersion[3];
  gpu_device_info* device;
  gpu_features* features;
  gpu_limits* limits;
  struct {
    uint32_t (*createInstance)(void* instanceCreateInfo, void* allocator, uintptr_t instance, void* getInstanceProcAddr);
    void (*getPhysicalDevice)(void* instance, uintptr_t physicalDevice);
    uint32_t (*createDevice)(void* instance, void* devceCreateInfo, void* allocator, uintptr_t device, void* getInstanceProcAddr);
    void* cacheData;
    size_t cacheSize;
  } vk;
} gpu_config;

bool gpu_init(gpu_config* config);
void gpu_destroy(void);
const char* gpu_get_error(void);
bool gpu_begin(uint32_t* tick);
bool gpu_submit(gpu_stream** streams, uint32_t count);
bool gpu_is_complete(uint32_t tick);
bool gpu_wait_tick(uint32_t tick, bool* waited);
bool gpu_wait_idle(void);
