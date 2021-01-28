#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

typedef struct gpu_buffer gpu_buffer;
typedef struct gpu_texture gpu_texture;
typedef struct gpu_sampler gpu_sampler;
typedef struct gpu_pass gpu_pass;
typedef struct gpu_shader gpu_shader;
typedef struct gpu_bundle gpu_bundle;
typedef struct gpu_pipeline gpu_pipeline;
typedef struct gpu_batch gpu_batch;

typedef void gpu_read_fn(void* data, uint64_t size, void* userdata);

// Buffer

enum {
  GPU_BUFFER_USAGE_VERTEX   = (1 << 0),
  GPU_BUFFER_USAGE_INDEX    = (1 << 1),
  GPU_BUFFER_USAGE_UNIFORM  = (1 << 2),
  GPU_BUFFER_USAGE_STORAGE  = (1 << 3),
  GPU_BUFFER_USAGE_INDIRECT = (1 << 4),
  GPU_BUFFER_USAGE_UPLOAD   = (1 << 5),
  GPU_BUFFER_USAGE_DOWNLOAD = (1 << 6)
};

typedef struct {
  uint64_t size;
  uint32_t usage;
  const char* label;
} gpu_buffer_info;

size_t gpu_sizeof_buffer(void);
bool gpu_buffer_init(gpu_buffer* buffer, gpu_buffer_info* info);
void gpu_buffer_destroy(gpu_buffer* buffer);
void* gpu_buffer_map(gpu_buffer* buffer, uint64_t offset, uint64_t size);
void gpu_buffer_read(gpu_buffer* buffer, uint64_t offset, uint64_t size, gpu_read_fn* fn, void* userdata);
void gpu_buffer_copy(gpu_buffer* src, gpu_buffer* dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size);

// Texture

enum {
  GPU_TEXTURE_USAGE_SAMPLE   = (1 << 0),
  GPU_TEXTURE_USAGE_RENDER   = (1 << 1),
  GPU_TEXTURE_USAGE_STORAGE  = (1 << 2),
  GPU_TEXTURE_USAGE_UPLOAD   = (1 << 3),
  GPU_TEXTURE_USAGE_DOWNLOAD = (1 << 4)
};

typedef enum {
  GPU_TEXTURE_TYPE_2D,
  GPU_TEXTURE_TYPE_3D,
  GPU_TEXTURE_TYPE_CUBE,
  GPU_TEXTURE_TYPE_ARRAY
} gpu_texture_type;

typedef enum {
  GPU_TEXTURE_FORMAT_NONE,
  GPU_TEXTURE_FORMAT_R8,
  GPU_TEXTURE_FORMAT_RG8,
  GPU_TEXTURE_FORMAT_RGBA8,
  GPU_TEXTURE_FORMAT_R16,
  GPU_TEXTURE_FORMAT_RG16,
  GPU_TEXTURE_FORMAT_RGBA16,
  GPU_TEXTURE_FORMAT_R16F,
  GPU_TEXTURE_FORMAT_RG16F,
  GPU_TEXTURE_FORMAT_RGBA16F,
  GPU_TEXTURE_FORMAT_R32F,
  GPU_TEXTURE_FORMAT_RG32F,
  GPU_TEXTURE_FORMAT_RGBA32F,
  GPU_TEXTURE_FORMAT_RGB565,
  GPU_TEXTURE_FORMAT_RGB5A1,
  GPU_TEXTURE_FORMAT_RGB10A2,
  GPU_TEXTURE_FORMAT_RG11B10F,
  GPU_TEXTURE_FORMAT_D16,
  GPU_TEXTURE_FORMAT_D24S8,
  GPU_TEXTURE_FORMAT_D32F,
  GPU_TEXTURE_FORMAT_BC6,
  GPU_TEXTURE_FORMAT_BC7,
  GPU_TEXTURE_FORMAT_ASTC_4x4,
  GPU_TEXTURE_FORMAT_ASTC_5x4,
  GPU_TEXTURE_FORMAT_ASTC_5x5,
  GPU_TEXTURE_FORMAT_ASTC_6x5,
  GPU_TEXTURE_FORMAT_ASTC_6x6,
  GPU_TEXTURE_FORMAT_ASTC_8x5,
  GPU_TEXTURE_FORMAT_ASTC_8x6,
  GPU_TEXTURE_FORMAT_ASTC_8x8,
  GPU_TEXTURE_FORMAT_ASTC_10x5,
  GPU_TEXTURE_FORMAT_ASTC_10x6,
  GPU_TEXTURE_FORMAT_ASTC_10x8,
  GPU_TEXTURE_FORMAT_ASTC_10x10,
  GPU_TEXTURE_FORMAT_ASTC_12x10,
  GPU_TEXTURE_FORMAT_ASTC_12x12,
  GPU_TEXTURE_FORMAT_COUNT
} gpu_texture_format;

typedef struct {
  gpu_texture* source;
  gpu_texture_type type;
  uint32_t mipmapIndex;
  uint32_t mipmapCount;
  uint32_t layerIndex;
  uint32_t layerCount;
} gpu_texture_view_info;

typedef struct {
  gpu_texture_type type;
  gpu_texture_format format;
  uint32_t size[3];
  uint32_t mipmaps;
  uint32_t samples;
  uint32_t usage;
  bool srgb;
  const char* label;
} gpu_texture_info;

size_t gpu_sizeof_texture(void);
bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info);
bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info);
void gpu_texture_destroy(gpu_texture* texture);
void* gpu_texture_map(gpu_texture* texture, uint16_t offset[4], uint16_t extent[3]);
void gpu_texture_read(gpu_texture* texture, uint16_t offset[4], uint16_t extent[3], gpu_read_fn* fn, void* userdata);
void gpu_texture_copy(gpu_texture* src, gpu_texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t size[3]);

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

size_t gpu_sizeof_sampler(void);
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
  uint32_t samples;
  uint32_t views;
  const char* label;
} gpu_pass_info;

size_t gpu_sizeof_pass(void);
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
  gpu_shader_source vertex;
  gpu_shader_source fragment;
  gpu_shader_source compute;
  uint32_t slotCount[4];
  gpu_slot* slots[4];
  const char* label;
} gpu_shader_info;

size_t gpu_sizeof_shader(void);
bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info);
void gpu_shader_destroy(gpu_shader* shader);

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

size_t gpu_sizeof_bundle(void);
bool gpu_bundle_init(gpu_bundle* bundle, gpu_bundle_info* info);
void gpu_bundle_destroy(gpu_bundle* bundle);

// Pipeline

typedef enum {
  GPU_DRAW_POINTS,
  GPU_DRAW_LINES,
  GPU_DRAW_LINE_STRIP,
  GPU_DRAW_TRIANGLES,
  GPU_DRAW_TRIANGLE_STRIP
} gpu_draw_mode;

typedef enum {
  GPU_FLOAT_F32,
  GPU_VEC2_F32,
  GPU_VEC2_F16,
  GPU_VEC2_U16N,
  GPU_VEC2_I16N,
  GPU_VEC3_F32,
  GPU_VEC4_F32,
  GPU_VEC4_F16,
  GPU_VEC4_U16N,
  GPU_VEC4_I16N,
  GPU_VEC4_U8N,
  GPU_VEC4_I8N,
  GPU_UINT_U32,
  GPU_UVEC2_U32,
  GPU_UVEC3_U32,
  GPU_UVEC4_U32,
  GPU_INT_I32,
  GPU_IVEC2_I32,
  GPU_IVEC3_I32,
  GPU_IVEC4_I32
} gpu_attribute_format;

typedef struct {
  uint8_t location;
  uint8_t buffer;
  uint8_t format;
  uint8_t offset;
} gpu_attribute;

typedef struct {
  uint16_t stride;
  uint16_t divisor;
} gpu_buffer_layout;

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

typedef enum {
  GPU_COLOR_MASK_RGBA = 0,
  GPU_COLOR_MASK_NONE = 0xff,
  GPU_COLOR_MASK_R = (1 << 0),
  GPU_COLOR_MASK_G = (1 << 1),
  GPU_COLOR_MASK_B = (1 << 2),
  GPU_COLOR_MASK_A = (1 << 3)
} gpu_color_mask;

typedef struct {
  gpu_pass* pass;
  gpu_shader* shader;
  gpu_draw_mode drawMode;
  gpu_buffer_layout buffers[8];
  gpu_attribute attributes[8];
  gpu_rasterizer_state rasterizer;
  gpu_depth_state depth;
  gpu_stencil_state stencil;
  gpu_blend_state blend[4];
  uint8_t colorMask[4];
  bool alphaToCoverage;
  const char* label;
} gpu_pipeline_info;

size_t gpu_sizeof_pipeline(void);
bool gpu_pipeline_init_graphics(gpu_pipeline* pipeline, gpu_pipeline_info* info);
bool gpu_pipeline_init_compute(gpu_pipeline* pipeline, gpu_shader* shader, const char* label);
void gpu_pipeline_destroy(gpu_pipeline* pipeline);

// Batch

typedef struct {
  gpu_texture* texture;
  gpu_texture* resolve;
  float clear[4];
} gpu_color_target;

typedef struct {
  gpu_texture* texture;
  float clear;
  uint32_t stencilClear;
} gpu_depth_target;

typedef struct {
  gpu_color_target color[4];
  gpu_depth_target depth;
  uint32_t size[2];
} gpu_render_target;

typedef enum {
  GPU_INDEX_U16,
  GPU_INDEX_U32
} gpu_index_type;

gpu_batch* gpu_batch_init_render(gpu_pass* pass, gpu_render_target* target, gpu_batch** recordings, uint32_t recordingCount);
gpu_batch* gpu_batch_init_record(gpu_pass* destination, uint32_t renderSize[2]);
gpu_batch* gpu_batch_init_compute(void);
void gpu_batch_end(gpu_batch* batch);
void gpu_batch_bind_pipeline(gpu_batch* batch, gpu_pipeline* pipeline);
void gpu_batch_bind_bundle(gpu_batch* batch, gpu_shader* shader, uint32_t group, gpu_bundle* bundle, uint32_t* offsets, uint32_t offsetCount);
void gpu_batch_bind_vertex_buffers(gpu_batch* batch, gpu_buffer** buffers, uint64_t* offsets, uint32_t count);
void gpu_batch_bind_index_buffer(gpu_batch* batch, gpu_buffer* buffer, uint64_t offset, gpu_index_type type);
void gpu_batch_draw(gpu_batch* batch, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex);
void gpu_batch_draw_indexed(gpu_batch* batch, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex);
void gpu_batch_draw_indirect(gpu_batch* batch, gpu_buffer* buffer, uint64_t offset, uint32_t drawCount);
void gpu_batch_draw_indirect_indexed(gpu_batch* batch, gpu_buffer* buffer, uint64_t offset, uint32_t drawCount);
void gpu_batch_compute(gpu_batch* batch, gpu_shader* shader, uint32_t x, uint32_t y, uint32_t z);
void gpu_batch_compute_indirect(gpu_batch* batch, gpu_shader* shader, gpu_buffer* buffer, uint64_t offset);

// Surface

gpu_texture* gpu_surface_acquire(void);
void gpu_surface_present(void);

// Entry

enum {
  GPU_FORMAT_FEATURE_SAMPLE       = (1 << 0),
  GPU_FORMAT_FEATURE_RENDER_COLOR = (1 << 1),
  GPU_FORMAT_FEATURE_RENDER_DEPTH = (1 << 2),
  GPU_FORMAT_FEATURE_BLEND        = (1 << 3),
  GPU_FORMAT_FEATURE_FILTER       = (1 << 4),
  GPU_FORMAT_FEATURE_STORAGE      = (1 << 5),
  GPU_FORMAT_FEATURE_ATOMIC       = (1 << 6),
  GPU_FORMAT_FEATURE_BLIT         = (1 << 7)
};

typedef struct {
  bool bptc;
  bool astc;
  bool pointSize;
  bool wireframe;
  bool multiview;
  bool multiblend;
  bool anisotropy;
  bool depthClamp;
  bool depthOffsetClamp;
  bool clipDistance;
  bool cullDistance;
  bool fullIndexBufferRange;
  bool indirectDrawCount;
  bool indirectDrawFirstInstance;
  bool extraShaderInputs;
  bool dynamicIndexing;
  bool float64;
  bool int64;
  bool int16;
  uint8_t formats[GPU_TEXTURE_FORMAT_COUNT];
} gpu_features;

typedef struct {
  uint32_t textureSize2D;
  uint32_t textureSize3D;
  uint32_t textureSizeCube;
  uint32_t textureLayers;
  uint32_t renderSize[2];
  uint32_t renderViews;
  uint32_t bundleCount;
  uint32_t bundleSlots;
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
  uint64_t allocationSize;
  uint32_t pointSize[2];
  float anisotropy;
} gpu_limits;

typedef struct {
  bool debug;
  gpu_features* features;
  gpu_limits* limits;
  void (*callback)(void* userdata, const char* message, int level);
  void* userdata;
  struct {
    bool surface;
    int vsync;
    const char** (*getExtraInstanceExtensions)(uint32_t* count);
    uint32_t (*createSurface)(void* instance, void** surface);
  } vk;
} gpu_config;

bool gpu_init(gpu_config* config);
void gpu_destroy(void);
void gpu_thread_attach(void);
void gpu_thread_detach(void);
void gpu_begin(void);
void gpu_flush(void);
void gpu_debug_push(const char* label);
void gpu_debug_pop(void);
void gpu_time_write(void);
void gpu_time_query(gpu_read_fn* fn, void* userdata);
