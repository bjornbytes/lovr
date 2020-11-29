#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

typedef struct gpu_buffer gpu_buffer;
typedef struct gpu_texture gpu_texture;
typedef struct gpu_sampler gpu_sampler;
typedef struct gpu_canvas gpu_canvas;
typedef struct gpu_shader gpu_shader;
typedef struct gpu_bundle gpu_bundle;
typedef struct gpu_pipeline gpu_pipeline;
typedef struct gpu_batch gpu_batch;

typedef void gpu_read_fn(void* data, uint64_t size, void* userdata);

// Buffer

typedef enum {
  GPU_BUFFER_USAGE_VERTEX  = (1 << 0),
  GPU_BUFFER_USAGE_INDEX   = (1 << 1),
  GPU_BUFFER_USAGE_UNIFORM = (1 << 2),
  GPU_BUFFER_USAGE_STORAGE = (1 << 3),
  GPU_BUFFER_USAGE_COPY    = (1 << 4),
  GPU_BUFFER_USAGE_PASTE   = (1 << 5)
} gpu_buffer_usage;

typedef struct {
  uint64_t size;
  uint32_t usage;
  void* data;
  const char* label;
} gpu_buffer_info;

size_t gpu_sizeof_buffer(void);
bool gpu_buffer_init(gpu_buffer* buffer, gpu_buffer_info* info);
void gpu_buffer_destroy(gpu_buffer* buffer);
void* gpu_buffer_map(gpu_buffer* buffer, uint64_t offset, uint64_t size);
void gpu_buffer_read(gpu_buffer* buffer, uint64_t offset, uint64_t size, gpu_read_fn* fn, void* userdata);
void gpu_buffer_copy(gpu_buffer* src, gpu_buffer* dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size);

// Texture

typedef enum {
  GPU_TEXTURE_USAGE_SAMPLE  = (1 << 0),
  GPU_TEXTURE_USAGE_CANVAS  = (1 << 1),
  GPU_TEXTURE_USAGE_STORAGE = (1 << 2),
  GPU_TEXTURE_USAGE_COPY    = (1 << 3),
  GPU_TEXTURE_USAGE_PASTE   = (1 << 4)
} gpu_texture_usage;

typedef enum {
  GPU_TEXTURE_TYPE_2D,
  GPU_TEXTURE_TYPE_3D,
  GPU_TEXTURE_TYPE_CUBE,
  GPU_TEXTURE_TYPE_ARRAY
} gpu_texture_type;

typedef enum {
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
  GPU_TEXTURE_FORMAT_ASTC_12x12
} gpu_texture_format;

typedef struct {
  gpu_texture* source;
  gpu_texture_type type;
  gpu_texture_format format;
  uint32_t baseMipmap;
  uint32_t mipmapCount;
  uint32_t baseLayer;
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

// Canvas

typedef enum {
  GPU_LOAD_OP_LOAD,
  GPU_LOAD_OP_CLEAR,
  GPU_LOAD_OP_DISCARD
} gpu_load_op;

typedef enum {
  GPU_STORE_OP_STORE,
  GPU_STORE_OP_DISCARD
} gpu_store_op;

typedef struct {
  gpu_texture* texture;
  gpu_texture* resolve;
  gpu_load_op load;
  gpu_store_op store;
  float clear[4];
} gpu_color_attachment;

typedef struct {
  gpu_texture* texture;
  gpu_load_op load;
  gpu_store_op store;
  float clear;
  struct {
    gpu_load_op load;
    gpu_store_op store;
    uint8_t clear;
  } stencil;
} gpu_depth_attachment;

typedef struct {
  gpu_color_attachment color[4];
  gpu_depth_attachment depth;
  uint32_t size[2];
  uint32_t views;
  const char* label;
} gpu_canvas_info;

size_t gpu_sizeof_canvas(void);
bool gpu_canvas_init(gpu_canvas* canvas, gpu_canvas_info* info);
void gpu_canvas_destroy(gpu_canvas* canvas);

// Bundle

typedef enum {
  GPU_BINDING_UNIFORM_BUFFER,
  GPU_BINDING_STORAGE_BUFFER,
  GPU_BINDING_SAMPLED_TEXTURE,
  GPU_BINDING_STORAGE_TEXTURE
} gpu_binding_type;

typedef enum {
  GPU_BINDING_VERTEX   = (1 << 0),
  GPU_BINDING_FRAGMENT = (1 << 1),
  GPU_BINDING_COMPUTE  = (1 << 2),
  GPU_BINDING_DYNAMIC  = (1 << 3)
} gpu_binding_usage;

typedef struct {
  gpu_buffer* buffer;
  uint64_t offset;
  uint32_t size;
} gpu_buffer_binding;

typedef struct {
  gpu_texture* texture;
  gpu_sampler* sampler;
} gpu_texture_binding;

typedef struct {
  uint16_t slot;
  uint16_t count;
  gpu_buffer_binding* buffers;
  gpu_texture_binding* textures;
} gpu_binding;

typedef struct {
  uint16_t type;
  uint16_t index;
  uint16_t count;
  uint16_t usage;
} gpu_slot;

typedef struct {
  gpu_slot slots[32];
  uint32_t count;
  uintptr_t secret;
} gpu_bundle_layout;

typedef struct {
  gpu_bundle_layout layout;
  gpu_binding bindings[32];
  uint32_t count;
  bool immutable;
} gpu_bundle_info;

size_t gpu_sizeof_bundle(void);
bool gpu_bundle_init(gpu_bundle* bundle, gpu_bundle_info* info);
void gpu_bundle_destroy(gpu_bundle* bundle);
void gpu_bundle_update(gpu_bundle* bundle, gpu_binding bindings[32], uint32_t count);

// Shader

typedef struct {
  const void* code;
  size_t size;
  const char* entry;
} gpu_shader_source;

typedef struct {
  uint16_t group;
  uint16_t slot;
} gpu_shader_var;

typedef struct {
  gpu_shader_source vertex;
  gpu_shader_source fragment;
  gpu_shader_source compute;
  gpu_shader_var* dynamicBuffers;
  uint32_t dynamicBufferCount;
  const char* label;
} gpu_shader_info;

size_t gpu_sizeof_shader(void);
bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info);
void gpu_shader_destroy(gpu_shader* shader);
bool gpu_shader_get_layout(gpu_shader* shader, uint32_t group, gpu_bundle_layout* layout);

// Pipeline

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
  GPU_DRAW_POINTS,
  GPU_DRAW_LINES,
  GPU_DRAW_LINE_STRIP,
  GPU_DRAW_TRIANGLES,
  GPU_DRAW_TRIANGLE_STRIP
} gpu_draw_mode;

typedef enum {
  GPU_CULL_NONE,
  GPU_CULL_FRONT,
  GPU_CULL_BACK
} gpu_cull_mode;

typedef enum {
  GPU_WINDING_CCW,
  GPU_WINDING_CW
} gpu_winding;

typedef enum {
  GPU_STENCIL_KEEP,
  GPU_STENCIL_ZERO,
  GPU_STENCIL_REPLACE,
  GPU_STENCIL_INCREMENT,
  GPU_STENCIL_DECREMENT,
  GPU_STENCIL_INCREMENT_WRAP,
  GPU_STENCIL_DECREMENT_WRAP,
  GPU_STENCIL_INVERT
} gpu_stencil_action;

typedef struct {
  gpu_stencil_action fail;
  gpu_stencil_action depthFail;
  gpu_stencil_action pass;
  gpu_compare_mode test;
  uint8_t readMask;
  uint8_t writeMask;
  uint8_t reference;
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
  gpu_shader* shader;
  gpu_canvas* canvas;
  gpu_buffer_layout buffers[16];
  gpu_attribute attributes[16];
  gpu_draw_mode drawMode;
  gpu_cull_mode cullMode;
  gpu_winding winding;
  float depthOffset;
  float depthOffsetSloped;
  bool depthWrite;
  gpu_compare_mode depthTest;
  gpu_stencil_state stencilFront;
  gpu_stencil_state stencilBack;
  bool alphaToCoverage;
  uint8_t colorMask;
  gpu_blend_state blend;
  const char* label;
} gpu_pipeline_info;

size_t gpu_sizeof_pipeline(void);
bool gpu_pipeline_init_graphics(gpu_pipeline* pipeline, gpu_pipeline_info* info);
bool gpu_pipeline_init_compute(gpu_pipeline* pipeline, gpu_pipeline_info* info);
void gpu_pipeline_destroy(gpu_pipeline* pipeline);

// Batch

typedef enum {
  GPU_INDEX_U16,
  GPU_INDEX_U32
} gpu_index_type;

gpu_batch* gpu_batch_begin(gpu_canvas* canvas);
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
  GPU_FORMAT_FEATURE_CANVAS_COLOR = (1 << 1),
  GPU_FORMAT_FEATURE_CANVAS_DEPTH = (1 << 2),
  GPU_FORMAT_FEATURE_BLEND        = (1 << 3),
  GPU_FORMAT_FEATURE_FILTER       = (1 << 4),
  GPU_FORMAT_FEATURE_STORAGE      = (1 << 5),
  GPU_FORMAT_FEATURE_ATOMIC       = (1 << 6),
  GPU_FORMAT_FEATURE_BLIT         = (1 << 7)
};

typedef struct {
  bool astc;
  bool bptc;
  uint8_t formats[32];
} gpu_features;

typedef struct {
  uint32_t textureSize2D;
  uint32_t textureSize3D;
  uint32_t textureSizeCube;
  uint32_t textureLayers;
  uint32_t canvasSize[2];
  uint32_t canvasViews;
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
  bool surface;
  void (*callback)(void* userdata, const char* message, int level);
  void* userdata;
  struct {
    const char** (*getExtraInstanceExtensions)(uint32_t* count);
    uint32_t (*createSurface)(void* instance, void** surface);
  } vk;
} gpu_config;

bool gpu_init(gpu_config* config);
void gpu_destroy(void);
void gpu_thread_attach(void);
void gpu_thread_detach(void);
void gpu_get_features(gpu_features* features);
void gpu_get_limits(gpu_limits* limits);
void gpu_begin(void);
void gpu_flush(void);
void gpu_render(gpu_canvas* canvas, gpu_batch** batches, uint32_t count);
void gpu_compute(gpu_batch** batches, uint32_t count);
void gpu_debug_push(const char* label);
void gpu_debug_pop(void);
void gpu_time_write(void);
void gpu_time_query(gpu_read_fn* fn, void* userdata);
