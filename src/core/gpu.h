#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

typedef struct gpu_buffer gpu_buffer;
typedef struct gpu_texture gpu_texture;
typedef struct gpu_sampler gpu_sampler;
typedef struct gpu_pass gpu_pass;
typedef struct gpu_shader gpu_shader;
typedef struct gpu_bunch gpu_bunch;
typedef struct gpu_bundle gpu_bundle;
typedef struct gpu_pipeline gpu_pipeline;
typedef struct gpu_stream gpu_stream;

size_t gpu_sizeof_buffer(void);
size_t gpu_sizeof_texture(void);
size_t gpu_sizeof_sampler(void);
size_t gpu_sizeof_pass(void);
size_t gpu_sizeof_shader(void);
size_t gpu_sizeof_bunch(void);
size_t gpu_sizeof_bundle(void);
size_t gpu_sizeof_pipeline(void);

// Buffer

enum {
  GPU_BUFFER_VERTEX   = (1 << 0),
  GPU_BUFFER_INDEX    = (1 << 1),
  GPU_BUFFER_UNIFORM  = (1 << 2),
  GPU_BUFFER_STORAGE  = (1 << 3),
  GPU_BUFFER_INDIRECT = (1 << 4),
  GPU_BUFFER_COPY_SRC = (1 << 5),
  GPU_BUFFER_COPY_DST = (1 << 6)
};

typedef enum {
  GPU_MEMORY_GPU,
  GPU_MEMORY_CPU_WRITE,
  GPU_MEMORY_CPU_READ
} gpu_memory_type;

typedef struct {
  uint32_t size;
  uint32_t usage;
  gpu_memory_type memory;
  void** data;
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
  GPU_FORMAT_BC6,
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
} gpu_texture_info;

bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info);
bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info);
void gpu_texture_destroy(gpu_texture* texture);
gpu_texture* gpu_surface_acquire(void);

// Sampler

typedef enum {
  GPU_FILTER_NEAREST,
  GPU_FILTER_LINEAR
} gpu_filter;

typedef enum {
  GPU_WRAP_CLAMP,
  GPU_WRAP_REPEAT,
  GPU_WRAP_MIRROR
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

// Pass

typedef enum {
  GPU_LOAD_OP_LOAD,
  GPU_LOAD_OP_CLEAR,
  GPU_LOAD_OP_DISCARD
} gpu_load_op;

typedef enum {
  GPU_SAVE_OP_SAVE,
  GPU_SAVE_OP_DISCARD
} gpu_save_op;

typedef struct {
  gpu_texture_format format;
  gpu_load_op load;
  gpu_save_op save;
  bool srgb;
} gpu_pass_color_info;

typedef struct {
  gpu_texture_format format;
  gpu_load_op load, stencilLoad;
  gpu_save_op save, stencilSave;
} gpu_pass_depth_info;

typedef struct {
  gpu_pass_color_info color[4];
  gpu_pass_depth_info depth;
  uint32_t count;
  uint32_t views;
  uint32_t samples;
  bool resolve;
  const char* label;
} gpu_pass_info;

bool gpu_pass_init(gpu_pass* pass, gpu_pass_info* info);
void gpu_pass_destroy(gpu_pass* pass);

// Shader

typedef struct {
  const void* code;
  size_t size;
  const char* entry;
} gpu_shader_source;

typedef enum {
  GPU_SLOT_UNIFORM_BUFFER,
  GPU_SLOT_STORAGE_BUFFER,
  GPU_SLOT_UNIFORM_BUFFER_DYNAMIC,
  GPU_SLOT_STORAGE_BUFFER_DYNAMIC,
  GPU_SLOT_SAMPLED_TEXTURE,
  GPU_SLOT_STORAGE_TEXTURE
} gpu_slot_type;

enum {
  GPU_STAGE_ALL = 0,
  GPU_STAGE_VERTEX = (1 << 0),
  GPU_STAGE_FRAGMENT = (1 << 1),
  GPU_STAGE_COMPUTE = (1 << 2)
};

typedef struct {
  uint8_t id;
  uint8_t type;
  uint8_t stage;
  uint8_t count;
} gpu_slot;

typedef struct {
  gpu_shader_source stages[2];
  uint32_t slotCount[4];
  gpu_slot* slots[4];
  const char* label;
} gpu_shader_info;

bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info);
void gpu_shader_destroy(gpu_shader* shader);

// Bunch

typedef struct {
  uint32_t bundleCount;
  uint32_t bindingCounts[6];
} gpu_bunch_info;

bool gpu_bunch_init(gpu_bunch* bunch, gpu_bunch_info* info);
void gpu_bunch_destroy(gpu_bunch* bunch);

// Bundle

typedef struct {
  gpu_buffer* object;
  uint32_t offset;
  uint32_t extent;
} gpu_buffer_binding;

typedef struct {
  gpu_texture* object;
  gpu_sampler* sampler;
} gpu_texture_binding;

typedef union {
  gpu_buffer_binding buffer;
  gpu_texture_binding texture;
} gpu_binding;

typedef struct {
  gpu_shader* shader;
  uint32_t group;
  uint32_t slotCount;
  gpu_slot* slots;
  gpu_binding* bindings;
} gpu_bundle_info;

void gpu_bundle_init(gpu_bundle* bundle, gpu_bundle_info* info);

// Pipeline

typedef enum {
  GPU_DRAW_POINTS,
  GPU_DRAW_LINES,
  GPU_DRAW_TRIANGLES
} gpu_draw_mode;

typedef enum {
  GPU_FORMAT_I8,
  GPU_FORMAT_U8,
  GPU_FORMAT_I16,
  GPU_FORMAT_U16,
  GPU_FORMAT_I32,
  GPU_FORMAT_U32,
  GPU_FORMAT_F32,
  GPU_FORMAT_I8x2,
  GPU_FORMAT_U8x2,
  GPU_FORMAT_I8Nx2,
  GPU_FORMAT_U8Nx2,
  GPU_FORMAT_I16x2,
  GPU_FORMAT_U16x2,
  GPU_FORMAT_I16Nx2,
  GPU_FORMAT_U16Nx2,
  GPU_FORMAT_I32x2,
  GPU_FORMAT_U32x2,
  GPU_FORMAT_F32x2,
  GPU_FORMAT_U10Nx3,
  GPU_FORMAT_I32x3,
  GPU_FORMAT_U32x3,
  GPU_FORMAT_F32x3,
  GPU_FORMAT_I8x4,
  GPU_FORMAT_U8x4,
  GPU_FORMAT_I8Nx4,
  GPU_FORMAT_U8Nx4,
  GPU_FORMAT_I16x4,
  GPU_FORMAT_U16x4,
  GPU_FORMAT_I16Nx4,
  GPU_FORMAT_U16Nx4,
  GPU_FORMAT_I32x4,
  GPU_FORMAT_U32x4,
  GPU_FORMAT_F32x4
} gpu_attribute_format;

typedef struct {
  uint8_t location;
  uint8_t buffer;
  uint8_t format;
  uint8_t offset;
} gpu_vertex_attribute;

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
  gpu_compare_mode test;
  bool write;
} gpu_depth_state;

typedef enum {
  GPU_STENCIL_KEEP,
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
  uint8_t value;
  uint8_t readMask;
  uint8_t writeMask;
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
  gpu_draw_mode drawMode;
  uint16_t vertexBufferCount;
  uint16_t instancedBufferMask;
  uint16_t bufferStrides[16];
  uint16_t attributeCount;
  gpu_vertex_attribute attributes[16];
  gpu_rasterizer_state rasterizer;
  gpu_depth_state depth;
  gpu_stencil_state stencil;
  gpu_blend_state blend;
  uint8_t colorMask;
  bool alphaToCoverage;
  const char* label;
} gpu_pipeline_info;

bool gpu_pipeline_init_graphics(gpu_pipeline* pipelines, gpu_pipeline_info* infos, uint32_t count);
bool gpu_pipeline_init_compute(gpu_pipeline* pipeline, gpu_shader* shader, const char* label);
void gpu_pipeline_destroy(gpu_pipeline* pipeline);

// Stream

typedef enum {
  GPU_INDEX_U16,
  GPU_INDEX_U32
} gpu_index_type;

typedef struct {
  gpu_texture* texture;
  gpu_texture* resolve;
  float clear[4];
} gpu_color_attachment;

typedef struct {
  gpu_texture* texture;
  struct { float depth; uint8_t stencil; } clear;
} gpu_depth_attachment;

typedef struct {
  gpu_pass* pass;
  gpu_color_attachment color[4];
  gpu_depth_attachment depth;
  uint32_t size[2];
} gpu_canvas;

gpu_stream* gpu_stream_begin(void);
void gpu_stream_end(gpu_stream* stream);
void gpu_render_begin(gpu_stream* stream, gpu_canvas* canvas);
void gpu_render_end(gpu_stream* stream);
void gpu_compute_begin(gpu_stream* stream);
void gpu_compute_end(gpu_stream* stream);
void gpu_bind_pipeline(gpu_stream* stream, gpu_pipeline* pipeline, bool compute);
void gpu_bind_bundle(gpu_stream* stream, gpu_shader* shader, uint32_t group, gpu_bundle* bundle, uint32_t* offsets, uint32_t offsetCount);
void gpu_bind_vertex_buffers(gpu_stream* stream, gpu_buffer** buffers, uint32_t* offsets, uint32_t first, uint32_t count);
void gpu_bind_index_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, gpu_index_type type);
void gpu_draw(gpu_stream* stream, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex);
void gpu_draw_indexed(gpu_stream* stream, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex);
void gpu_draw_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount);
void gpu_draw_indirect_indexed(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount);
void gpu_compute(gpu_stream* stream, uint32_t x, uint32_t y, uint32_t z);
void gpu_compute_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset);
void gpu_copy_buffers(gpu_stream* stream, gpu_buffer* src, gpu_buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size);
void gpu_copy_textures(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t size[3]);
void gpu_copy_buffer_texture(gpu_stream* stream, gpu_buffer* src, gpu_texture* dst, uint32_t srcOffset, uint16_t dstOffset[4], uint16_t extent[3]);
void gpu_copy_texture_buffer(gpu_stream* stream, gpu_texture* src, gpu_buffer* dst, uint16_t srcOffset[4], uint32_t dstOffset, uint16_t extent[3]);
void gpu_clear_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t size, uint32_t value);
void gpu_clear_texture(gpu_stream* stream, gpu_texture* texture, uint16_t layer, uint16_t layerCount, uint16_t level, uint16_t levelCount, float color[4]);
void gpu_blit(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t srcExtent[3], uint16_t dstExtent[3], gpu_filter filter);
void gpu_sync(gpu_stream* stream);
void gpu_label_push(gpu_stream* stream, const char* label);
void gpu_label_pop(gpu_stream* stream);
void gpu_timer_write(gpu_stream* stream);
void gpu_timer_gather(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset);

// Entry

enum {
  GPU_FEATURE_SAMPLE       = (1 << 0),
  GPU_FEATURE_RENDER_COLOR = (1 << 1),
  GPU_FEATURE_RENDER_DEPTH = (1 << 2),
  GPU_FEATURE_BLEND        = (1 << 3),
  GPU_FEATURE_FILTER       = (1 << 4),
  GPU_FEATURE_STORAGE      = (1 << 5),
  GPU_FEATURE_ATOMIC       = (1 << 6),
  GPU_FEATURE_BLIT         = (1 << 7),
  GPU_FEATURE_RENDER       = GPU_FEATURE_RENDER_COLOR | GPU_FEATURE_RENDER_DEPTH
};

typedef struct {
  uint32_t vendorId;
  uint32_t deviceId;
  char deviceName[256];
  uint32_t driverMajor;
  uint32_t driverMinor;
  uint32_t driverPatch;
  bool discrete;
} gpu_hardware;

typedef struct {
  bool bptc;
  bool astc;
  bool pointSize;
  bool wireframe;
  bool multiblend;
  bool anisotropy;
  bool depthClamp;
  bool depthOffsetClamp;
  bool clipDistance;
  bool cullDistance;
  bool fullIndexBufferRange;
  bool indirectDrawFirstInstance;
  bool extraShaderInputs;
  bool dynamicIndexing;
  bool float64;
  bool int64;
  bool int16;
  uint8_t formats[GPU_FORMAT_COUNT];
} gpu_features;

typedef struct {
  uint32_t textureSize2D;
  uint32_t textureSize3D;
  uint32_t textureSizeCube;
  uint32_t textureLayers;
  uint32_t renderSize[2];
  uint32_t renderViews;
  uint32_t bundleCount;
  uint32_t uniformBufferRange;
  uint32_t storageBufferRange;
  uint32_t uniformBufferAlign;
  uint32_t storageBufferAlign;
  uint32_t vertexAttributes;
  uint32_t vertexAttributeOffset;
  uint32_t vertexBuffers;
  uint32_t vertexBufferStride;
  uint32_t vertexShaderOutputs;
  uint32_t computeCount[3];
  uint32_t computeGroupSize[3];
  uint32_t computeGroupVolume;
  uint32_t computeSharedMemory;
  uint32_t indirectDrawCount;
  uint32_t pointSize[2];
  float anisotropy;
} gpu_limits;

typedef struct {
  bool debug;
  gpu_hardware* hardware;
  gpu_features* features;
  gpu_limits* limits;
  void (*callback)(void* userdata, const char* message, int level);
  void* userdata;
  struct {
    bool surface;
    int vsync;
    bool (*getInstanceExtensions)(char* buffer, uint32_t size);
    bool (*getDeviceExtensions)(char* buffer, uint32_t size, uintptr_t physicalDevice);
    uint32_t (*createSurface)(void* instance, void** surface);
  } vk;
} gpu_config;

bool gpu_init(gpu_config* config);
void gpu_destroy(void);
uint32_t gpu_begin(void);
void gpu_submit(gpu_stream** streams, uint32_t count);
bool gpu_finished(uint32_t tick);
