#include "gpu.h"
#include "lib/webgpu/webgpu.h"
#include <string.h>

struct gpu_buffer {
  WGpuBuffer handle;
  void* memory;
};

struct gpu_texture {
  WGpuTexture handle;
  WGpuTextureView view;
  WGPU_TEXTURE_FORMAT format;
};

struct gpu_sampler {
  WGpuSampler handle;
};

struct gpu_layout {
  WGpuBindGroupLayout handle;
};

struct gpu_shader {
  WGpuShaderModule handles[2];
  WGpuPipelineLayout pipelineLayout;
};

struct gpu_bundle_pool {
  void* unused;
};

struct gpu_bundle {
  WGpuBindGroup handle;
  uint32_t dynamicBufferCount;
};

struct gpu_pipeline {
  WGpuObjectBase handle;
};

struct gpu_tally {
  WGpuQuerySet handle;
};

struct gpu_stream {
  WGpuCommandEncoder commands;
  WGpuObjectBase pass;
};

size_t gpu_sizeof_buffer(void) { return sizeof(gpu_buffer); }
size_t gpu_sizeof_texture(void) { return sizeof(gpu_texture); }
size_t gpu_sizeof_sampler(void) { return sizeof(gpu_sampler); }
size_t gpu_sizeof_layout(void) { return sizeof(gpu_layout); }
size_t gpu_sizeof_shader(void) { return sizeof(gpu_shader); }
size_t gpu_sizeof_bundle_pool(void) { return sizeof(gpu_bundle_pool); }
size_t gpu_sizeof_bundle(void) { return sizeof(gpu_bundle); }
size_t gpu_sizeof_pipeline(void) { return sizeof(gpu_pipeline); }
size_t gpu_sizeof_tally(void) { return sizeof(gpu_tally); }

// Internals

typedef struct {
  void* next;
  void* data;
  WGpuBuffer buffer;
  uint32_t offset;
  uint32_t extent;
} gpu_mapping;

// State

static struct {
  WGpuAdapter adapter;
  WGpuDevice device;
  WGpuQueue queue;
  WGpuCanvasContext context;
  gpu_texture backbuffer;
  gpu_mapping* mappings;
  gpu_stream streams[64];
  uint32_t streamCount;
  uint32_t tick;
  uint32_t lastTickFinished;
} state;

// Helpers

#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

static WGPU_TEXTURE_FORMAT convertFormat(gpu_texture_format format, bool srgb);
static uint32_t getRowSize(gpu_texture_format format, uint32_t width);

// Buffer

bool gpu_buffer_init(gpu_buffer* buffer, gpu_buffer_info* info) {
  static const WGPU_BUFFER_USAGE_FLAGS usages[] = {
    [GPU_BUFFER_STATIC] =
      WGPU_BUFFER_USAGE_VERTEX |
      WGPU_BUFFER_USAGE_INDEX |
      WGPU_BUFFER_USAGE_UNIFORM |
      WGPU_BUFFER_USAGE_STORAGE |
      WGPU_BUFFER_USAGE_INDIRECT |
      WGPU_BUFFER_USAGE_COPY_SRC |
      WGPU_BUFFER_USAGE_COPY_DST |
      WGPU_BUFFER_USAGE_QUERY_RESOLVE,
    [GPU_BUFFER_STREAM] =
      WGPU_BUFFER_USAGE_VERTEX |
      WGPU_BUFFER_USAGE_INDEX |
      WGPU_BUFFER_USAGE_UNIFORM |
      WGPU_BUFFER_USAGE_COPY_SRC |
      WGPU_BUFFER_USAGE_COPY_DST,
    [GPU_BUFFER_DOWNLOAD] =
      WGPU_BUFFER_USAGE_COPY_DST |
      WGPU_BUFFER_USAGE_STORAGE
  };

  buffer->handle = wgpu_device_create_buffer(state.device, &(WGpuBufferDescriptor) {
    .size = info->size,
    .usage = usages[info->type]
  });

  if (!buffer->handle) {
    return false;
  }

  wgpu_object_set_label(buffer->handle, info->label);

  buffer->memory = NULL;

  if (info->pointer) {
    if (info->type != GPU_BUFFER_STATIC) {
      buffer->memory = malloc(info->size);
    }

    *info->pointer = buffer->memory;
  }

  return true;
}

void gpu_buffer_destroy(gpu_buffer* buffer) {
  wgpu_object_destroy(buffer->handle);
  free(buffer->memory);
}

void gpu_buffer_flush(gpu_buffer* buffer, uint32_t offset, uint32_t extent) {
  if (extent > 0) {
    wgpu_queue_write_buffer(state.queue, buffer->handle, offset, buffer->memory, extent);
  }
}

// Texture

bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info) {
  static const WGPU_TEXTURE_DIMENSION dimensions[] = {
    [GPU_TEXTURE_2D] = WGPU_TEXTURE_DIMENSION_2D,
    [GPU_TEXTURE_3D] = WGPU_TEXTURE_DIMENSION_3D,
    [GPU_TEXTURE_CUBE] = WGPU_TEXTURE_DIMENSION_2D,
    [GPU_TEXTURE_ARRAY] = WGPU_TEXTURE_DIMENSION_2D
  };

  texture->handle = wgpu_device_create_texture(state.device, &(WGpuTextureDescriptor) {
    .usage =
      ((info->usage & GPU_TEXTURE_RENDER) ? WGPU_TEXTURE_USAGE_RENDER_ATTACHMENT : 0) |
      ((info->usage & GPU_TEXTURE_SAMPLE) ? WGPU_TEXTURE_USAGE_TEXTURE_BINDING : 0) |
      ((info->usage & GPU_TEXTURE_STORAGE) ? WGPU_TEXTURE_USAGE_STORAGE_BINDING : 0) |
      ((info->usage & GPU_TEXTURE_COPY_SRC) ? WGPU_TEXTURE_USAGE_COPY_SRC : 0) |
      ((info->usage & GPU_TEXTURE_COPY_DST) ? WGPU_TEXTURE_USAGE_COPY_DST : 0) |
      (info->upload.levelCount > 0 ? WGPU_TEXTURE_USAGE_COPY_DST : 0),
    .dimension = dimensions[info->type],
    .width = info->size[0],
    .height = info->size[1],
    .depthOrArrayLayers = info->size[2],
    .format = convertFormat(info->format, info->srgb),
    .mipLevelCount = info->mipmaps,
    .sampleCount = MAX(info->samples, 1)
  });

  if (!texture->handle) {
    return false;
  }

  wgpu_object_set_label(texture->handle, info->label);

  texture->format = wgpu_texture_format(texture->handle);
  texture->view = wgpu_texture_create_view_simple(texture->handle);

  if (!texture->view) {
    wgpu_object_destroy(texture->handle);
    return false;
  }

  if (info->upload.levelCount) {
    for (uint32_t i = 0; i < info->upload.levelCount; i++) {
      void* data = info->upload.levelData[i];
      uint32_t offset[4] = { 0, 0, 0, i };
      uint32_t extent[3] = { MAX(info->size[0] >> i, 1), MAX(info->size[1] >> i, 1), info->size[2] };
      gpu_write_texture(info->upload.stream, texture, data, offset, extent, 0, 0);
    }
  }

  // TODO mipgen

  return true;
}

bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info) {
  static const WGPU_TEXTURE_VIEW_DIMENSION types[] = {
    [GPU_TEXTURE_2D] = WGPU_TEXTURE_VIEW_DIMENSION_2D,
    [GPU_TEXTURE_3D] = WGPU_TEXTURE_VIEW_DIMENSION_3D,
    [GPU_TEXTURE_CUBE] = WGPU_TEXTURE_VIEW_DIMENSION_CUBE,
    [GPU_TEXTURE_ARRAY] = WGPU_TEXTURE_VIEW_DIMENSION_2D_ARRAY
  };

  texture->handle = 0;
  texture->format = info->source->format;

  return texture->view = wgpu_texture_create_view(info->source->handle, &(WGpuTextureViewDescriptor) {
    .format = texture->format,
    .dimension = types[info->type],
    .baseMipLevel = info->levelIndex,
    .mipLevelCount = info->levelCount,
    .baseArrayLayer = info->layerIndex,
    .arrayLayerCount = info->layerCount
  });
}

void gpu_texture_destroy(gpu_texture* texture) {
  wgpu_object_destroy(texture->view);
  wgpu_object_destroy(texture->handle);
}

// Surface

bool gpu_surface_init(gpu_surface_info* info) {
  state.context = wgpu_canvas_get_webgpu_context("canvas");

  WGpuCanvasConfiguration config = {
    .device = state.device,
    .format = navigator_gpu_get_preferred_canvas_format(),
    .usage = WGPU_TEXTURE_USAGE_RENDER_ATTACHMENT,
    .colorSpace = HTML_PREDEFINED_COLOR_SPACE_SRGB,
    .toneMapping.mode = WGPU_CANVAS_TONE_MAPPING_MODE_STANDARD,
    .alphaMode = WGPU_CANVAS_ALPHA_MODE_OPAQUE
  };

  wgpu_canvas_context_configure(state.context, &config);

  return true;
}

gpu_texture_format gpu_surface_get_format(void) {
  switch (navigator_gpu_get_preferred_canvas_format()) {
    case WGPU_TEXTURE_FORMAT_RGBA8UNORM: return GPU_FORMAT_RGBA8;
    case WGPU_TEXTURE_FORMAT_BGRA8UNORM: return GPU_FORMAT_BGRA8;
    default: return ~0u;
  }
}

bool gpu_surface_is_hdr(void) {
  return false;
}

bool gpu_surface_resize(uint32_t width, uint32_t height) {
  return true;
}

bool gpu_surface_acquire(gpu_texture** texture, uint32_t* width, uint32_t* height) {
  state.backbuffer.handle = wgpu_canvas_context_get_current_texture(state.context);
  state.backbuffer.view = wgpu_texture_create_view_simple(state.backbuffer.handle);
  *texture = &state.backbuffer;
  *width = wgpu_texture_width(state.backbuffer.handle);
  *height = wgpu_texture_height(state.backbuffer.handle);
  return true;
}

bool gpu_surface_present(void) {
  return true;
}

// Sampler

bool gpu_sampler_init(gpu_sampler* sampler, gpu_sampler_info* info) {
  static const WGPU_FILTER_MODE filters[] = {
    [GPU_FILTER_NEAREST] = WGPU_FILTER_MODE_NEAREST,
    [GPU_FILTER_LINEAR] = WGPU_FILTER_MODE_LINEAR
  };

  static const WGPU_MIPMAP_FILTER_MODE mipFilters[] = {
    [GPU_FILTER_NEAREST] = WGPU_MIPMAP_FILTER_MODE_NEAREST,
    [GPU_FILTER_LINEAR] = WGPU_MIPMAP_FILTER_MODE_LINEAR
  };

  static const WGPU_ADDRESS_MODE wraps[] = {
    [GPU_WRAP_CLAMP] = WGPU_ADDRESS_MODE_CLAMP_TO_EDGE,
    [GPU_WRAP_REPEAT] = WGPU_ADDRESS_MODE_REPEAT,
    [GPU_WRAP_MIRROR] = WGPU_ADDRESS_MODE_MIRROR_REPEAT
  };

  static const WGPU_COMPARE_FUNCTION compares[] = {
    [GPU_COMPARE_NONE] = WGPU_COMPARE_FUNCTION_ALWAYS,
    [GPU_COMPARE_EQUAL] = WGPU_COMPARE_FUNCTION_EQUAL,
    [GPU_COMPARE_NEQUAL] = WGPU_COMPARE_FUNCTION_NOT_EQUAL,
    [GPU_COMPARE_LESS] = WGPU_COMPARE_FUNCTION_LESS,
    [GPU_COMPARE_LEQUAL] = WGPU_COMPARE_FUNCTION_LESS_EQUAL,
    [GPU_COMPARE_GREATER] = WGPU_COMPARE_FUNCTION_GREATER,
    [GPU_COMPARE_GEQUAL] = WGPU_COMPARE_FUNCTION_GREATER_EQUAL
  };

  return sampler->handle = wgpu_device_create_sampler(state.device, &(WGpuSamplerDescriptor) {
    .addressModeU = wraps[info->wrap[0]],
    .addressModeV = wraps[info->wrap[1]],
    .addressModeW = wraps[info->wrap[2]],
    .magFilter = filters[info->mag],
    .minFilter = filters[info->min],
    .mipmapFilter = mipFilters[info->mip],
    .lodMinClamp = info->lodClamp[0],
    .lodMaxClamp = info->lodClamp[1] < 0.f ? 32 : info->lodClamp[1],
    .compare = compares[info->compare],
    .maxAnisotropy = MAX(info->anisotropy, 1.f)
  });
}

void gpu_sampler_destroy(gpu_sampler* sampler) {
  wgpu_object_destroy(sampler->handle);
}

// Layout

bool gpu_layout_init(gpu_layout* layout, gpu_layout_info* info) {
  static const WGPU_BIND_GROUP_LAYOUT_TYPE bindingTypes[] = {
    [GPU_SLOT_UNIFORM_BUFFER] = WGPU_BIND_GROUP_LAYOUT_TYPE_BUFFER,
    [GPU_SLOT_STORAGE_BUFFER] = WGPU_BIND_GROUP_LAYOUT_TYPE_BUFFER,
    [GPU_SLOT_UNIFORM_BUFFER_DYNAMIC] = WGPU_BIND_GROUP_LAYOUT_TYPE_BUFFER,
    [GPU_SLOT_STORAGE_BUFFER_DYNAMIC] = WGPU_BIND_GROUP_LAYOUT_TYPE_BUFFER,
    [GPU_SLOT_TEXTURE_WITH_SAMPLER] = WGPU_BIND_GROUP_LAYOUT_TYPE_INVALID, // Not supported
    [GPU_SLOT_SAMPLED_TEXTURE] = WGPU_BIND_GROUP_LAYOUT_TYPE_TEXTURE,
    [GPU_SLOT_STORAGE_TEXTURE] = WGPU_BIND_GROUP_LAYOUT_TYPE_STORAGE_TEXTURE,
    [GPU_SLOT_SAMPLER] = WGPU_BIND_GROUP_LAYOUT_TYPE_SAMPLER
  };

  static const WGPU_BUFFER_BINDING_TYPE bufferTypes[] = {
    [GPU_SLOT_UNIFORM_BUFFER] = WGPU_BUFFER_BINDING_TYPE_UNIFORM,
    [GPU_SLOT_STORAGE_BUFFER] = WGPU_BUFFER_BINDING_TYPE_STORAGE,
    [GPU_SLOT_UNIFORM_BUFFER_DYNAMIC] = WGPU_BUFFER_BINDING_TYPE_UNIFORM,
    [GPU_SLOT_STORAGE_BUFFER_DYNAMIC] = WGPU_BUFFER_BINDING_TYPE_STORAGE
  };

  gpu_slot* slot = info->slots;
  WGpuBindGroupLayoutEntry entries[32];
  for (uint32_t i = 0; i < info->count; i++, slot++) {
    entries[i] = (WGpuBindGroupLayoutEntry) {
      .binding = slot->number,
      .visibility =
        (((slot->stages & GPU_STAGE_VERTEX) ? WGPU_SHADER_STAGE_VERTEX : 0) |
        ((slot->stages & GPU_STAGE_FRAGMENT) ? WGPU_SHADER_STAGE_FRAGMENT : 0) |
        ((slot->stages & GPU_STAGE_COMPUTE) ? WGPU_SHADER_STAGE_COMPUTE : 0)),
      .type = bindingTypes[info->slots[i].type]
    };

    switch (info->slots[i].type) {
      case GPU_SLOT_UNIFORM_BUFFER_DYNAMIC:
      case GPU_SLOT_STORAGE_BUFFER_DYNAMIC:
        entries[i].layout.buffer.hasDynamicOffset = true;
        /* fallthrough */
      case GPU_SLOT_UNIFORM_BUFFER:
      case GPU_SLOT_STORAGE_BUFFER:
        entries[i].layout.buffer.type = bufferTypes[slot->type];
        break;

      case GPU_SLOT_TEXTURE_WITH_SAMPLER:
        break;

      // FIXME need more metadata
      case GPU_SLOT_SAMPLED_TEXTURE:
        entries[i].layout.texture.sampleType = WGPU_TEXTURE_SAMPLE_TYPE_FLOAT;
        entries[i].layout.texture.viewDimension = WGPU_TEXTURE_VIEW_DIMENSION_2D;
        entries[i].layout.texture.multisampled = false;
        break;

      // FIXME need more metadata
      case GPU_SLOT_STORAGE_TEXTURE:
        entries[i].layout.storageTexture.access = WGPU_STORAGE_TEXTURE_ACCESS_WRITE_ONLY;
        entries[i].layout.storageTexture.format = WGPU_TEXTURE_FORMAT_INVALID;
        entries[i].layout.storageTexture.viewDimension = WGPU_TEXTURE_VIEW_DIMENSION_2D;
        break;

      // FIXME need more metadata?
      case GPU_SLOT_SAMPLER:
        entries[i].layout.sampler.type = WGPU_SAMPLER_BINDING_TYPE_FILTERING;
        break;
    }
  }

  return layout->handle = wgpu_device_create_bind_group_layout(state.device, entries, info->count);
}

void gpu_layout_destroy(gpu_layout* layout) {
  wgpu_object_destroy(layout->handle);
}

// Shader

bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info) {
  for (uint32_t i = 0; i < info->stageCount; i++) {
    WGpuShaderModuleDescriptor descriptor = { 0 }; // TODO
    shader->handles[i] = wgpu_device_create_shader_module(state.device, &descriptor);
    wgpu_object_set_label(shader->handles[i], info->label);
  }

  uint32_t layoutCount = 0;
  WGpuBindGroupLayout layouts[4];
  for (uint32_t i = 0; i < COUNTOF(info->layouts) && info->layouts[i]; i++) {
    layouts[layoutCount++] = info->layouts[i]->handle;
  }

  return shader->pipelineLayout = wgpu_device_create_pipeline_layout(state.device, layouts, layoutCount);
}

void gpu_shader_destroy(gpu_shader* shader) {
  wgpu_object_destroy(shader->handles[0]);
  wgpu_object_destroy(shader->handles[1]);
  wgpu_object_destroy(shader->pipelineLayout);
}

// Bundles

bool gpu_bundle_pool_init(gpu_bundle_pool* pool, gpu_bundle_pool_info* info) {
  pool->unused = NULL;
  return true;
}

void gpu_bundle_pool_destroy(gpu_bundle_pool* pool) {
  //
}

void gpu_bundle_write(gpu_bundle** bundles, gpu_bundle_info* infos, uint32_t count) {
  WGpuBindGroupEntry entries[32];

  for (uint32_t i = 0; i < count; i++) {
    gpu_bundle_info* info = &infos[i];
    WGpuBindGroupEntry* entry = entries;
    gpu_binding* binding = info->bindings;
    uint32_t dynamicBufferCount = 0;

    // TODO: error if binding array is given
    // TODO: error if binding count is bigger than 32

    for (uint32_t j = 0; j < info->count; j++, entry++, binding++) {
      memset(entry, 0, sizeof(*entry));
      entry->binding = binding->number;

      switch (binding->type) {
        case GPU_SLOT_UNIFORM_BUFFER_DYNAMIC:
        case GPU_SLOT_STORAGE_BUFFER_DYNAMIC:
          dynamicBufferCount++;
          /* fallthrough */
        case GPU_SLOT_UNIFORM_BUFFER:
        case GPU_SLOT_STORAGE_BUFFER:
          entry->resource = binding->buffer.object->handle;
          entry->bufferBindOffset = binding->buffer.offset;
          entry->bufferBindSize = binding->buffer.extent;
          break;
        case GPU_SLOT_TEXTURE_WITH_SAMPLER:
          break; // Unsupported
        case GPU_SLOT_SAMPLED_TEXTURE:
        case GPU_SLOT_STORAGE_TEXTURE:
          entry->resource = binding->texture.object->view;
          break;
        case GPU_SLOT_SAMPLER:
          entry->resource = binding->texture.sampler->handle;
          break;
      }
    }

    bundles[i]->handle = wgpu_device_create_bind_group(state.device, info->layout->handle, entries, info->count);
    bundles[i]->dynamicBufferCount = dynamicBufferCount;
  }
}

// Pipeline

bool gpu_pipeline_init_graphics(gpu_pipeline* pipeline, gpu_pipeline_info* info, bool* slow) {
  static const WGPU_PRIMITIVE_TOPOLOGY topologies[] = {
    [GPU_DRAW_POINTS] = WGPU_PRIMITIVE_TOPOLOGY_POINT_LIST,
    [GPU_DRAW_LINES] = WGPU_PRIMITIVE_TOPOLOGY_LINE_LIST,
    [GPU_DRAW_TRIANGLES] = WGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
  };

  static const WGPU_VERTEX_FORMAT attributeTypes[] = {
    [GPU_TYPE_I8x4] = WGPU_VERTEX_FORMAT_SINT8X4,
    [GPU_TYPE_U8x4] = WGPU_VERTEX_FORMAT_UINT8X4,
    [GPU_TYPE_SN8x4] = WGPU_VERTEX_FORMAT_SNORM8X4,
    [GPU_TYPE_UN8x4] = WGPU_VERTEX_FORMAT_UNORM8X4,
    [GPU_TYPE_SN10x3] = WGPU_VERTEX_FORMAT_INVALID,
    [GPU_TYPE_UN10x3] = WGPU_VERTEX_FORMAT_INVALID,
    [GPU_TYPE_I16] = WGPU_VERTEX_FORMAT_INVALID,
    [GPU_TYPE_I16x2] = WGPU_VERTEX_FORMAT_SINT16X2,
    [GPU_TYPE_I16x4] = WGPU_VERTEX_FORMAT_SINT16X4,
    [GPU_TYPE_U16] = WGPU_VERTEX_FORMAT_INVALID,
    [GPU_TYPE_U16x2] = WGPU_VERTEX_FORMAT_UINT16X2,
    [GPU_TYPE_U16x4] = WGPU_VERTEX_FORMAT_UINT16X4,
    [GPU_TYPE_SN16x2] = WGPU_VERTEX_FORMAT_SNORM16X2,
    [GPU_TYPE_SN16x4] = WGPU_VERTEX_FORMAT_SNORM16X4,
    [GPU_TYPE_UN16x2] = WGPU_VERTEX_FORMAT_UNORM16X2,
    [GPU_TYPE_UN16x4] = WGPU_VERTEX_FORMAT_UNORM16X4,
    [GPU_TYPE_I32] = WGPU_VERTEX_FORMAT_SINT32,
    [GPU_TYPE_I32x2] = WGPU_VERTEX_FORMAT_SINT32X2,
    [GPU_TYPE_I32x3] = WGPU_VERTEX_FORMAT_SINT32X3,
    [GPU_TYPE_I32x4] = WGPU_VERTEX_FORMAT_SINT32X4,
    [GPU_TYPE_U32] = WGPU_VERTEX_FORMAT_UINT32,
    [GPU_TYPE_U32x2] = WGPU_VERTEX_FORMAT_UINT32X2,
    [GPU_TYPE_U32x3] = WGPU_VERTEX_FORMAT_UINT32X3,
    [GPU_TYPE_U32x4] = WGPU_VERTEX_FORMAT_UINT32X4,
    [GPU_TYPE_F16x2] = WGPU_VERTEX_FORMAT_FLOAT16X2,
    [GPU_TYPE_F16x4] = WGPU_VERTEX_FORMAT_FLOAT16X4,
    [GPU_TYPE_F32] = WGPU_VERTEX_FORMAT_FLOAT32,
    [GPU_TYPE_F32x2] = WGPU_VERTEX_FORMAT_FLOAT32X2,
    [GPU_TYPE_F32x3] = WGPU_VERTEX_FORMAT_FLOAT32X3,
    [GPU_TYPE_F32x4] = WGPU_VERTEX_FORMAT_FLOAT32X4
  };

  static const WGPU_FRONT_FACE frontFaces[] = {
    [GPU_WINDING_CCW] = WGPU_FRONT_FACE_CCW,
    [GPU_WINDING_CW] = WGPU_FRONT_FACE_CW
  };

  static const WGPU_CULL_MODE cullModes[] = {
    [GPU_CULL_NONE] = WGPU_CULL_MODE_NONE,
    [GPU_CULL_FRONT] = WGPU_CULL_MODE_FRONT,
    [GPU_CULL_BACK] = WGPU_CULL_MODE_BACK
  };

  static const WGPU_COMPARE_FUNCTION compares[] = {
    [GPU_COMPARE_NONE] = WGPU_COMPARE_FUNCTION_ALWAYS,
    [GPU_COMPARE_EQUAL] = WGPU_COMPARE_FUNCTION_EQUAL,
    [GPU_COMPARE_NEQUAL] = WGPU_COMPARE_FUNCTION_NOT_EQUAL,
    [GPU_COMPARE_LESS] = WGPU_COMPARE_FUNCTION_LESS,
    [GPU_COMPARE_LEQUAL] = WGPU_COMPARE_FUNCTION_LESS_EQUAL,
    [GPU_COMPARE_GREATER] = WGPU_COMPARE_FUNCTION_GREATER,
    [GPU_COMPARE_GEQUAL] = WGPU_COMPARE_FUNCTION_GREATER_EQUAL
  };

  static const WGPU_STENCIL_OPERATION stencilOps[] = {
    [GPU_STENCIL_KEEP] = WGPU_STENCIL_OPERATION_KEEP,
    [GPU_STENCIL_ZERO] = WGPU_STENCIL_OPERATION_ZERO,
    [GPU_STENCIL_REPLACE] = WGPU_STENCIL_OPERATION_REPLACE,
    [GPU_STENCIL_INCREMENT] = WGPU_STENCIL_OPERATION_INCREMENT_CLAMP,
    [GPU_STENCIL_DECREMENT] = WGPU_STENCIL_OPERATION_DECREMENT_CLAMP,
    [GPU_STENCIL_INCREMENT_WRAP] = WGPU_STENCIL_OPERATION_INCREMENT_WRAP,
    [GPU_STENCIL_DECREMENT_WRAP] = WGPU_STENCIL_OPERATION_DECREMENT_WRAP,
    [GPU_STENCIL_INVERT] = WGPU_STENCIL_OPERATION_INVERT
  };

  static const WGPU_BLEND_FACTOR blendFactors[] = {
    [GPU_BLEND_ZERO] = WGPU_BLEND_FACTOR_ZERO,
    [GPU_BLEND_ONE] = WGPU_BLEND_FACTOR_ONE,
    [GPU_BLEND_SRC_COLOR] = WGPU_BLEND_FACTOR_SRC,
    [GPU_BLEND_ONE_MINUS_SRC_COLOR] = WGPU_BLEND_FACTOR_ONE_MINUS_SRC,
    [GPU_BLEND_SRC_ALPHA] = WGPU_BLEND_FACTOR_SRC_ALPHA,
    [GPU_BLEND_ONE_MINUS_SRC_ALPHA] = WGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    [GPU_BLEND_DST_COLOR] = WGPU_BLEND_FACTOR_DST,
    [GPU_BLEND_ONE_MINUS_DST_COLOR] = WGPU_BLEND_FACTOR_ONE_MINUS_DST,
    [GPU_BLEND_DST_ALPHA] = WGPU_BLEND_FACTOR_DST_ALPHA,
    [GPU_BLEND_ONE_MINUS_DST_ALPHA] = WGPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA
  };

  static const WGPU_BLEND_OPERATION blendOps[] = {
    [GPU_BLEND_ADD] = WGPU_BLEND_OPERATION_ADD,
    [GPU_BLEND_SUB] = WGPU_BLEND_OPERATION_SUBTRACT,
    [GPU_BLEND_RSUB] = WGPU_BLEND_OPERATION_REVERSE_SUBTRACT,
    [GPU_BLEND_MIN] = WGPU_BLEND_OPERATION_MIN,
    [GPU_BLEND_MAX] = WGPU_BLEND_OPERATION_MAX
  };

  uint32_t totalAttributeCount = 0;
  WGpuVertexAttribute attributes[16];
  WGpuVertexBufferLayout vertexBuffers[16];
  for (uint32_t i = 0; i < info->vertex.bufferCount; i++) {
    vertexBuffers[i] = (WGpuVertexBufferLayout) {
      .arrayStride = info->vertex.bufferStrides[i],
      .stepMode = (info->vertex.instancedBuffers & (1 << i)) ? WGPU_VERTEX_STEP_MODE_INSTANCE : WGPU_VERTEX_STEP_MODE_VERTEX,
      .numAttributes = 0,
      .attributes = &attributes[totalAttributeCount]
    };

    for (uint32_t j = 0; j < info->vertex.attributeCount; j++) {
      if (info->vertex.attributes[j].buffer == i) {
        attributes[totalAttributeCount++] = (WGpuVertexAttribute) {
          .format = attributeTypes[info->vertex.attributes[j].type],
          .offset = info->vertex.attributes[j].offset,
          .shaderLocation = info->vertex.attributes[j].location
        };
      }
    }

    totalAttributeCount += vertexBuffers[i].numAttributes;
  }

  WGpuVertexState vertex = {
    .module = info->shader->handles[0],
    .entryPoint = "main",
    .numBuffers = info->vertex.bufferCount,
    .buffers = vertexBuffers
  };

  WGpuPrimitiveState primitive = {
    .topology = topologies[info->drawMode],
    .frontFace = frontFaces[info->rasterizer.winding],
    .cullMode = cullModes[info->rasterizer.cullMode],
  };

  WGpuStencilFaceState stencil = {
    .compare = compares[info->stencil.test],
    .failOp = stencilOps[info->stencil.failOp],
    .depthFailOp = stencilOps[info->stencil.depthFailOp],
    .passOp = stencilOps[info->stencil.passOp]
  };

  WGpuDepthStencilState depth = {
    .format = info->depth.format ? convertFormat(info->depth.format, false) : WGPU_TEXTURE_FORMAT_INVALID,
    .depthWriteEnabled = info->depth.write,
    .depthCompare = compares[info->depth.test],
    .stencilFront = stencil,
    .stencilBack = stencil,
    .stencilReadMask = info->stencil.testMask,
    .stencilWriteMask = info->stencil.writeMask,
    .depthBias = info->rasterizer.depthOffset,
    .depthBiasSlopeScale = info->rasterizer.depthOffsetSloped,
    .depthBiasClamp = info->rasterizer.depthOffsetClamp
  };

  WGpuMultisampleState multisample = {
    .count = info->multisample.count,
    .alphaToCoverageEnabled = info->multisample.alphaToCoverage
  };

  WGpuColorTargetState targets[4];
  for (uint32_t i = 0; i < info->attachmentCount; i++) {
    targets[i] = (WGpuColorTargetState) {
      .format = convertFormat(info->color[i].format, info->color[i].srgb),
      .writeMask = info->color[i].mask
    };

    if (info->color[i].blend.enabled) {
      targets[i].blend = (WGpuBlendState) {
        .color.operation = blendOps[info->color[i].blend.color.op],
        .color.srcFactor = blendFactors[info->color[i].blend.color.src],
        .color.dstFactor = blendFactors[info->color[i].blend.color.dst],
        .alpha.operation = blendOps[info->color[i].blend.alpha.op],
        .alpha.srcFactor = blendFactors[info->color[i].blend.alpha.src],
        .alpha.dstFactor = blendFactors[info->color[i].blend.alpha.dst]
      };
    } else {
      targets[i].blend = (WGpuBlendState) {
        .color.operation = WGPU_BLEND_OPERATION_DISABLED,
        .alpha.operation = WGPU_BLEND_OPERATION_DISABLED
      };
    }
  }

  WGpuFragmentState fragment = {
    .module = info->shader->handles[1],
    .entryPoint = "main",
    .numTargets = info->attachmentCount,
    .targets = targets
  };

  WGpuRenderPipelineDescriptor pipelineInfo = {
    .layout = info->shader->pipelineLayout,
    .vertex = vertex,
    .primitive = primitive,
    .depthStencil = depth,
    .multisample = multisample,
    .fragment = fragment
  };

  if (slow) {
    *slow = false;
  }

  pipeline->handle = wgpu_device_create_render_pipeline(state.device, &pipelineInfo);

  if (!pipeline->handle) {
    return false;
  }

  wgpu_object_set_label(pipeline->handle, info->label);

  return true;
}

bool gpu_pipeline_init_compute(gpu_pipeline* pipeline, gpu_compute_pipeline_info* info) {
  WGpuShaderModule shader = info->shader->handles[0];
  const char* entry = "main";
  WGpuPipelineLayout layout = info->shader->pipelineLayout;
  return pipeline->handle = wgpu_device_create_compute_pipeline(state.device, shader, entry, layout, NULL, 0);
}

void gpu_pipeline_destroy(gpu_pipeline* pipeline) {
  wgpu_object_destroy(pipeline->handle);
}

void gpu_pipeline_get_cache(void* data, size_t* size) {
  *size = 0;
}

// Tally

bool gpu_tally_init(gpu_tally* tally, gpu_tally_info* info) {
  static const WGPU_QUERY_TYPE types[] = {
    [GPU_TALLY_TIME] = WGPU_QUERY_TYPE_TIMESTAMP,
    [GPU_TALLY_PIXEL] = WGPU_QUERY_TYPE_OCCLUSION
  };

  return tally->handle = wgpu_device_create_query_set(state.device, &(WGpuQuerySetDescriptor) {
    .type = types[info->type],
    .count = info->count
  });
}

void gpu_tally_destroy(gpu_tally* tally) {
  wgpu_object_destroy(tally->handle);
}

// Stream

gpu_stream* gpu_stream_begin(const char* label) {
  if (state.streamCount >= COUNTOF(state.streams)) return NULL;
  gpu_stream* stream = &state.streams[state.streamCount++];

  stream->commands = wgpu_device_create_command_encoder_simple(state.device);

  return stream;
}

bool gpu_stream_end(gpu_stream* stream) {
  return true;
}

void gpu_render_begin(gpu_stream* stream, gpu_canvas* canvas) {
  static const WGPU_LOAD_OP loadOps[] = {
    [GPU_LOAD_OP_CLEAR] = WGPU_LOAD_OP_CLEAR,
    [GPU_LOAD_OP_DISCARD] = WGPU_LOAD_OP_CLEAR,
    [GPU_LOAD_OP_KEEP] = WGPU_LOAD_OP_LOAD
  };

  static const WGPU_STORE_OP storeOps[] = {
    [GPU_SAVE_OP_KEEP] = WGPU_STORE_OP_STORE,
    [GPU_LOAD_OP_DISCARD] = WGPU_STORE_OP_DISCARD
  };

  uint32_t colorAttachmentCount = 0;
  WGpuRenderPassColorAttachment colorAttachments[COUNTOF(canvas->color)];

  for (uint32_t i = 0; i < 4 && canvas->color[i].texture; i++, colorAttachmentCount++) {
    colorAttachments[i] = (WGpuRenderPassColorAttachment) {
      .view = canvas->color[i].texture->view,
      .depthSlice = -1,
      .resolveTarget = canvas->color[i].resolve->view,
      .loadOp = loadOps[canvas->color[i].load],
      .storeOp = storeOps[canvas->color[i].save],
      .clearValue.r = canvas->color[i].clear[0],
      .clearValue.g = canvas->color[i].clear[1],
      .clearValue.b = canvas->color[i].clear[2],
      .clearValue.a = canvas->color[i].clear[3]
    };
  }

  WGPU_TEXTURE_FORMAT depthFormat = canvas->depth.texture ? canvas->depth.texture->format : WGPU_TEXTURE_FORMAT_INVALID;
  bool stencil = depthFormat == WGPU_TEXTURE_FORMAT_DEPTH24PLUS_STENCIL8 || depthFormat == WGPU_TEXTURE_FORMAT_DEPTH32FLOAT_STENCIL8;

  WGpuRenderPassDepthStencilAttachment depth = {
    .view = canvas->depth.texture ? canvas->depth.texture->view : 0,
    .depthLoadOp = canvas->depth.texture ? loadOps[canvas->depth.load] : WGPU_LOAD_OP_UNDEFINED,
    .depthStoreOp = canvas->depth.texture ? storeOps[canvas->depth.save] : WGPU_STORE_OP_UNDEFINED,
    .depthClearValue = canvas->depth.clear,
    .depthReadOnly = false,
    .stencilLoadOp = stencil ? loadOps[canvas->depth.stencilLoad] : WGPU_LOAD_OP_UNDEFINED,
    .stencilStoreOp = stencil ? storeOps[canvas->depth.stencilSave] : WGPU_STORE_OP_UNDEFINED,
    .stencilClearValue = 0,
    .stencilReadOnly = false
  };

  WGpuRenderPassDescriptor info = {
    .numColorAttachments = colorAttachmentCount,
    .colorAttachments = colorAttachments,
    .depthStencilAttachment = depth
  };

  stream->pass = wgpu_command_encoder_begin_render_pass(stream->commands, &info);
}

void gpu_render_end(gpu_stream* stream, gpu_canvas* canvas) {
  wgpu_render_pass_encoder_end(stream->pass);
  stream->pass = 0;
}

void gpu_compute_begin(gpu_stream* stream) {
  stream->pass = wgpu_command_encoder_begin_compute_pass(stream->commands, NULL);
}

void gpu_compute_end(gpu_stream* stream) {
  wgpu_compute_pass_encoder_end(stream->pass);
  stream->pass = 0;
}

void gpu_set_viewport(gpu_stream* stream, float view[4], float depth[2]) {
  wgpu_render_pass_encoder_set_viewport(stream->pass, view[0], view[1], view[2], view[3], depth[0], depth[1]);
}

void gpu_set_scissor(gpu_stream* stream, uint32_t scissor[4]) {
wgpu_render_pass_encoder_set_scissor_rect(stream->pass, scissor[0], scissor[1], scissor[2], scissor[3]);
}

void gpu_push_constants(gpu_stream* stream, gpu_shader* shader, void* data, uint32_t size) {
  // Unsupported
}

void gpu_bind_pipeline(gpu_stream* stream, gpu_pipeline* pipeline, gpu_pipeline_type type) {
  wgpu_encoder_set_pipeline(stream->pass, pipeline->handle);
}

void gpu_bind_bundles(gpu_stream* stream, gpu_shader* shader, gpu_bundle** bundles, uint32_t first, uint32_t count, uint32_t* dynamicOffsets, uint32_t dynamicOffsetCount) {
  for (uint32_t i = 0; i < count; i++) {
    uint32_t offsetCount = bundles[i]->dynamicBufferCount;
    uint32_t* offsets = offsetCount > 0 ? dynamicOffsets : NULL;
    wgpu_encoder_set_bind_group(stream->pass, first + i, bundles[i]->handle, offsets, offsetCount);
    dynamicOffsets += offsetCount;
  }
}

void gpu_bind_vertex_buffers(gpu_stream* stream, gpu_buffer** buffers, uint32_t* offsets, uint32_t first, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    uint64_t size = wgpu_buffer_size(buffers[i]->handle) - offsets[i];
    wgpu_render_pass_encoder_set_vertex_buffer(stream->pass, first + i, buffers[i]->handle, offsets[i], size);
  }
}

void gpu_bind_index_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, gpu_index_type type) {
  WGPU_INDEX_FORMAT indexTypes[] = {
    [GPU_INDEX_U16] = WGPU_INDEX_FORMAT_UINT16,
    [GPU_INDEX_U32] = WGPU_INDEX_FORMAT_UINT32
  };
  uint64_t size = wgpu_buffer_size(buffer->handle) - offset;
  wgpu_render_pass_encoder_set_index_buffer(stream->pass, buffer->handle, indexTypes[type], offset, size);
}

void gpu_draw(gpu_stream* stream, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t baseInstance) {
  wgpu_render_pass_encoder_draw(stream->pass, vertexCount, instanceCount, firstVertex, baseInstance);
}

void gpu_draw_indexed(gpu_stream* stream, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex, uint32_t baseInstance) {
  wgpu_render_pass_encoder_draw_indexed(stream->pass, indexCount, instanceCount, firstIndex, baseVertex, baseInstance);
}

void gpu_draw_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount, uint32_t stride) {
  stride = stride ? stride : 16;
  for (uint32_t i = 0; i < drawCount; i++) {
    wgpu_render_pass_encoder_draw_indirect(stream->pass, buffer->handle, offset + stride * i);
  }
}

void gpu_draw_indirect_indexed(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount, uint32_t stride) {
  stride = stride ? stride : 20;
  for (uint32_t i = 0; i < drawCount; i++) {
    wgpu_render_pass_encoder_draw_indexed_indirect(stream->pass, buffer->handle, offset + stride * i);
  }
}

void gpu_compute(gpu_stream* stream, uint32_t x, uint32_t y, uint32_t z) {
  wgpu_compute_pass_encoder_dispatch_workgroups(stream->pass, x, y, z);
}

void gpu_compute_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset) {
  wgpu_compute_pass_encoder_dispatch_workgroups_indirect(stream->pass, buffer->handle, offset);
}

void* gpu_map(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t extent) {
  gpu_mapping* mapping = malloc(sizeof(gpu_mapping));
  mapping->next = state.mappings;
  mapping->data = malloc(extent);
  mapping->buffer = buffer->handle;
  mapping->offset = offset;
  mapping->extent = extent;
  state.mappings = mapping;
  return mapping->data;
}

bool gpu_write_buffer(gpu_stream* stream, gpu_buffer* buffer, const void* data, uint32_t offset, uint32_t extent) {
  wgpu_queue_write_buffer(state.queue, buffer->handle, offset, data, extent);
  return true;
}

bool gpu_write_texture(gpu_stream* stream, gpu_texture* texture, void* data, uint32_t offset[4], uint32_t extent[3], uint32_t rowSize, uint32_t rowsPerImage) {
  if (rowSize == 0) {
    rowSize = getRowSize(texture->format, extent[0]);
  }

  if (rowsPerImage == 0) {
    rowsPerImage = extent[1];
  }

  WGpuImageCopyTexture dst = {
    .texture = texture->handle,
    .mipLevel = offset[3],
    .origin = { offset[0], offset[1], offset[2] }
  };

  wgpu_queue_write_texture(state.queue, &dst, data, rowSize, rowsPerImage, extent[0], extent[1], extent[2]);
  return true;
}

void gpu_copy_buffers(gpu_stream* stream, gpu_buffer* src, gpu_buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t extent) {
  wgpu_command_encoder_copy_buffer_to_buffer(stream->commands, src->handle, srcOffset, dst->handle, dstOffset, extent);
}

void gpu_copy_textures(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t extent[3]) {
  WGpuImageCopyTexture srcRegion = {
    .texture = src->handle,
    .mipLevel = srcOffset[3],
    .origin = { srcOffset[0], srcOffset[1], srcOffset[2] }
  };

  WGpuImageCopyTexture dstRegion = {
    .texture = dst->handle,
    .mipLevel = dstOffset[3],
    .origin = { dstOffset[0], dstOffset[1], dstOffset[2] }
  };

  WGpuExtent3D size = { extent[0], extent[1], extent[2] };

  wgpu_command_encoder_copy_texture_to_texture(stream->commands, &srcRegion, &dstRegion, extent[0], extent[1], extent[2]);
}

void gpu_copy_buffer_texture(gpu_stream* stream, gpu_buffer* src, gpu_texture* dst, uint32_t srcOffset, uint32_t dstOffset[4], uint32_t extent[3]) {
  WGpuImageCopyBuffer srcRegion = {
    .offset = srcOffset,
    .bytesPerRow = getRowSize(dst->format, extent[0]),
    .rowsPerImage = extent[1],
    .buffer = src->handle
  };

  WGpuImageCopyTexture dstRegion = {
    .texture = dst->handle,
    .mipLevel = dstOffset[3],
    .origin = { dstOffset[0], dstOffset[1], dstOffset[2] }
  };

  wgpu_command_encoder_copy_buffer_to_texture(stream->commands, &srcRegion, &dstRegion, extent[0], extent[1], extent[2]);
}

void gpu_copy_texture_buffer(gpu_stream* stream, gpu_texture* src, gpu_buffer* dst, uint32_t srcOffset[4], uint32_t dstOffset, uint32_t extent[3]) {
  WGpuImageCopyTexture srcRegion = {
    .texture = src->handle,
    .mipLevel = srcOffset[3],
    .origin = { srcOffset[0], srcOffset[1], srcOffset[2] }
  };

  WGpuImageCopyBuffer dstRegion = {
    .offset = dstOffset,
    .bytesPerRow = getRowSize(src->format, extent[0]),
    .rowsPerImage = extent[1],
    .buffer = dst->handle
  };

  wgpu_command_encoder_copy_texture_to_buffer(stream->commands, &srcRegion, &dstRegion, extent[0], extent[1], extent[2]);
}

void gpu_copy_tally_buffer(gpu_stream* stream, gpu_tally* src, gpu_buffer* dst, uint32_t srcIndex, uint32_t dstOffset, uint32_t count) {
  wgpu_command_encoder_resolve_query_set(stream->commands, src->handle, srcIndex, count, dst->handle, dstOffset);
}

void gpu_clear_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t size, uint32_t value) {
  wgpu_command_encoder_clear_buffer(stream->commands, buffer->handle, offset, size);
}

void gpu_clear_texture(gpu_stream* stream, gpu_texture* texture, float value[4], uint32_t layer, uint32_t layerCount, uint32_t level, uint32_t levelCount) {
  // TODO Unsupported, probably need compute shader
}

void gpu_clear_tally(gpu_stream* stream, gpu_tally* tally, uint32_t index, uint32_t count) {
  // TODO
}

void gpu_blit(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t srcExtent[3], uint32_t dstExtent[3], gpu_filter filter) {
  // TODO Unsupported
}

void gpu_sync(gpu_stream* stream, gpu_barrier* barriers, uint32_t count) {
  //
}

void gpu_tally_begin(gpu_stream* stream, gpu_tally* tally, uint32_t index) {
  wgpu_render_pass_encoder_begin_occlusion_query(stream->pass, index);
}

void gpu_tally_finish(gpu_stream* stream, gpu_tally* tally, uint32_t index) {
  wgpu_render_pass_encoder_end_occlusion_query(stream->pass);
}

void gpu_tally_mark(gpu_stream* stream, gpu_tally* tally, uint32_t index) {
  // TODO unsupported, should change API
}

void gpu_xr_acquire(gpu_stream* stream, gpu_texture* texture) {
  //
}

void gpu_xr_release(gpu_stream* stream, gpu_texture* texture) {
  //
}

// Entry

bool gpu_init(gpu_config* config) {
  if (!navigator_gpu_available()) {
    return false;
  }

  state.adapter = navigator_gpu_request_adapter_sync_simple();
  state.device = wgpu_adapter_request_device_sync_simple(state.adapter);
  state.queue = wgpu_device_get_queue(state.device);

  if (config->features) {
    config->features->textureBC = wgpu_device_supports_feature(state.device, WGPU_FEATURE_TEXTURE_COMPRESSION_BC);
    config->features->textureASTC = wgpu_device_supports_feature(state.device, WGPU_FEATURE_TEXTURE_COMPRESSION_ASTC);
    config->features->wireframe = false;
    config->features->depthClamp = wgpu_device_supports_feature(state.device, WGPU_FEATURE_DEPTH_CLIP_CONTROL);
    config->features->depthResolve = false;
    config->features->indirectDrawFirstInstance = wgpu_device_supports_feature(state.device, WGPU_FEATURE_INDIRECT_FIRST_INSTANCE);
    config->features->shaderDebug = false;
    config->features->float64 = false;
    config->features->int64 = false;
    config->features->int16 = false;

    config->features->formats[GPU_FORMAT_R8][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_RG8][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_RGBA8][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER | GPU_FEATURE_STORAGE;
    config->features->formats[GPU_FORMAT_BGRA8][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_R16][0] = 0;
    config->features->formats[GPU_FORMAT_RG16][0] = 0;
    config->features->formats[GPU_FORMAT_RGBA16][0] = 0;
    config->features->formats[GPU_FORMAT_R16F][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_RG16F][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_RGBA16F][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER | GPU_FEATURE_STORAGE;
    config->features->formats[GPU_FORMAT_R32F][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_STORAGE; // not blendable
    config->features->formats[GPU_FORMAT_RG32F][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_STORAGE; // not blendable
    config->features->formats[GPU_FORMAT_RGBA32F][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_STORAGE; // not blendable
    config->features->formats[GPU_FORMAT_RGB565][0] = 0;
    config->features->formats[GPU_FORMAT_RGB5A1][0] = 0;
    config->features->formats[GPU_FORMAT_RGB10A2][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_RG11B10F][0] = GPU_FEATURE_SAMPLE; // need rg11b10ufloat-renderable feature for RENDER
    config->features->formats[GPU_FORMAT_D16][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_D24][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_D32F][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_D24S8][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_D32FS8][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_BC1][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_BC2][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_BC3][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_BC4U][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_BC4S][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_BC5U][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_BC5S][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_BC6UF][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_BC6SF][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_BC7][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_4x4][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_5x4][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_5x5][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_6x5][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_6x6][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_8x5][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_8x6][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_8x8][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_10x5][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_10x6][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_10x8][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_10x10][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_12x10][0] = GPU_FEATURE_SAMPLE;
    config->features->formats[GPU_FORMAT_ASTC_12x12][0] = GPU_FEATURE_SAMPLE;

    for (uint32_t i = 0; i < GPU_FORMAT_COUNT; i++) {
      config->features->formats[i][1] = config->features->formats[i][0];
    }

    config->features->formats[GPU_FORMAT_RGBA8][1] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_BGRA8][1] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;

    config->features->sampleCounts = 1 | 4;
  }

  if (config->limits) {
    WGpuSupportedLimits supported;
    wgpu_device_get_limits(state.device, &supported);
    config->limits->textureSize2D = supported.maxTextureDimension2D;
    config->limits->textureSize3D = supported.maxTextureDimension3D;
    config->limits->textureSizeCube = supported.maxTextureDimension2D;
    config->limits->textureLayers = supported.maxTextureArrayLayers;
    config->limits->renderSize[0] = supported.maxTextureDimension2D;
    config->limits->renderSize[1] = supported.maxTextureDimension2D;
    config->limits->renderSize[2] = 6; // TODO actually support this
    config->limits->uniformBuffersPerStage = supported.maxUniformBuffersPerShaderStage;
    config->limits->storageBuffersPerStage = supported.maxStorageBuffersPerShaderStage;
    config->limits->sampledTexturesPerStage = supported.maxSampledTexturesPerShaderStage;
    config->limits->storageTexturesPerStage = supported.maxStorageTexturesPerShaderStage;
    config->limits->samplersPerStage = supported.maxSamplersPerShaderStage;
    config->limits->uniformBufferRange = supported.maxUniformBufferBindingSize;
    config->limits->storageBufferRange = supported.maxStorageBufferBindingSize;
    config->limits->uniformBufferAlign = supported.minUniformBufferOffsetAlignment;
    config->limits->storageBufferAlign = supported.minStorageBufferOffsetAlignment;
    config->limits->vertexAttributes = supported.maxVertexAttributes;
    config->limits->vertexBuffers = supported.maxVertexBuffers;
    config->limits->vertexBufferStride = supported.maxVertexBufferArrayStride;
    config->limits->vertexShaderOutputs = supported.maxInterStageShaderVariables;
    config->limits->clipDistances = 0; // TODO
    config->limits->cullDistances = 0;
    config->limits->clipAndCullDistances = 0; // TODO
    config->limits->workgroupCount[0] = 0; // TODO supported.maxComputeWorkgroupsPerDimension;
    config->limits->workgroupCount[1] = 0; // TODO supported.maxComputeWorkgroupsPerDimension;
    config->limits->workgroupCount[2] = 0; // TODO supported.maxComputeWorkgroupsPerDimension;
    config->limits->workgroupSize[0] = supported.maxComputeWorkgroupSizeX;
    config->limits->workgroupSize[1] = supported.maxComputeWorkgroupSizeY;
    config->limits->workgroupSize[2] = supported.maxComputeWorkgroupSizeZ;
    config->limits->totalWorkgroupSize = supported.maxComputeInvocationsPerWorkgroup;
    config->limits->computeSharedMemory = supported.maxComputeWorkgroupStorageSize;
    config->limits->pushConstantSize = 0;
    config->limits->indirectDrawCount = 1;
    config->limits->instances = ~0u;
    config->limits->timestampPeriod = 1.f;
    config->limits->anisotropy = 16.f;
    config->limits->pointSize = 1.f;
  }

  return !!state.device;
}

void gpu_destroy(void) {
  wgpu_object_destroy(state.device);
  memset(&state, 0, sizeof(state));
}

const char* gpu_get_error(void) {
  return NULL; // TODO
}

bool gpu_begin(uint32_t* tick) {
  *tick = state.tick++;
  return true;
}

static void onSubmittedWorkDone(WGpuQueue queue, void* userdata) {
  state.lastTickFinished = (uint32_t) (uintptr_t) userdata;
}

bool gpu_submit(gpu_stream** streams, uint32_t count) {
  while (state.mappings) {
    gpu_mapping* mapping = state.mappings;
    wgpu_queue_write_buffer(state.queue, mapping->buffer, mapping->offset, mapping->data, mapping->extent);
    state.mappings = mapping->next;
    free(mapping->data);
    free(mapping);
  }

  WGpuCommandBuffer commandBuffers[64];
  count = MIN(count, COUNTOF(commandBuffers));

  for (uint32_t i = 0; i < count; i++) {
    commandBuffers[i] = wgpu_command_encoder_finish(streams[i]->commands);
  }

  wgpu_queue_submit_multiple_and_destroy(state.queue, commandBuffers, count);
  wgpu_queue_set_on_submitted_work_done_callback(state.queue, onSubmittedWorkDone, (void*) (uintptr_t) state.tick);
  return true;
}

bool gpu_is_complete(uint32_t tick) {
  return state.lastTickFinished >= tick;
}

bool gpu_wait_tick(uint32_t tick, bool* waited) {
  return *waited = false, true; // TODO unsupported?
}

bool gpu_wait_idle(void) {
  return true; // TODO unsupported?
}

// Helpers

static WGPU_TEXTURE_FORMAT convertFormat(gpu_texture_format format, bool srgb) {
  static const WGPU_TEXTURE_FORMAT formats[][2] = {
    [GPU_FORMAT_R8] = { WGPU_TEXTURE_FORMAT_R8UNORM, WGPU_TEXTURE_FORMAT_R8UNORM },
    [GPU_FORMAT_RG8] = { WGPU_TEXTURE_FORMAT_RG8UNORM, WGPU_TEXTURE_FORMAT_RG8UNORM },
    [GPU_FORMAT_RGBA8] = { WGPU_TEXTURE_FORMAT_RGBA8UNORM, WGPU_TEXTURE_FORMAT_RGBA8UNORM_SRGB },
    [GPU_FORMAT_BGRA8] = { WGPU_TEXTURE_FORMAT_BGRA8UNORM, WGPU_TEXTURE_FORMAT_BGRA8UNORM_SRGB },
    [GPU_FORMAT_R16] = { WGPU_TEXTURE_FORMAT_INVALID, WGPU_TEXTURE_FORMAT_INVALID },
    [GPU_FORMAT_RG16] = { WGPU_TEXTURE_FORMAT_INVALID, WGPU_TEXTURE_FORMAT_INVALID },
    [GPU_FORMAT_RGBA16] = { WGPU_TEXTURE_FORMAT_INVALID, WGPU_TEXTURE_FORMAT_INVALID },
    [GPU_FORMAT_R16F] = { WGPU_TEXTURE_FORMAT_R16FLOAT, WGPU_TEXTURE_FORMAT_R16FLOAT },
    [GPU_FORMAT_RG16F] = { WGPU_TEXTURE_FORMAT_RG16FLOAT, WGPU_TEXTURE_FORMAT_RG16FLOAT },
    [GPU_FORMAT_RGBA16F] = { WGPU_TEXTURE_FORMAT_RGBA16FLOAT, WGPU_TEXTURE_FORMAT_RGBA16FLOAT },
    [GPU_FORMAT_R32F] = { WGPU_TEXTURE_FORMAT_R32FLOAT, WGPU_TEXTURE_FORMAT_R32FLOAT },
    [GPU_FORMAT_RG32F] = { WGPU_TEXTURE_FORMAT_RG32FLOAT, WGPU_TEXTURE_FORMAT_RG32FLOAT },
    [GPU_FORMAT_RGBA32F] = { WGPU_TEXTURE_FORMAT_RGBA32FLOAT, WGPU_TEXTURE_FORMAT_RGBA32FLOAT },
    [GPU_FORMAT_RGB565] = { WGPU_TEXTURE_FORMAT_INVALID, WGPU_TEXTURE_FORMAT_INVALID },
    [GPU_FORMAT_RGB5A1] = { WGPU_TEXTURE_FORMAT_INVALID, WGPU_TEXTURE_FORMAT_INVALID },
    [GPU_FORMAT_RGB10A2] = {WGPU_TEXTURE_FORMAT_RGB10A2UNORM, WGPU_TEXTURE_FORMAT_RGB10A2UNORM },
    [GPU_FORMAT_RG11B10F] = { WGPU_TEXTURE_FORMAT_RG11B10UFLOAT, WGPU_TEXTURE_FORMAT_RG11B10UFLOAT },
    [GPU_FORMAT_D16] = { WGPU_TEXTURE_FORMAT_DEPTH16UNORM, WGPU_TEXTURE_FORMAT_DEPTH16UNORM },
    [GPU_FORMAT_D24] = { WGPU_TEXTURE_FORMAT_DEPTH24PLUS, WGPU_TEXTURE_FORMAT_DEPTH24PLUS },
    [GPU_FORMAT_D32F] = { WGPU_TEXTURE_FORMAT_DEPTH32FLOAT, WGPU_TEXTURE_FORMAT_DEPTH32FLOAT },
    [GPU_FORMAT_D24S8] = { WGPU_TEXTURE_FORMAT_DEPTH24PLUS_STENCIL8, WGPU_TEXTURE_FORMAT_DEPTH24PLUS_STENCIL8 },
    [GPU_FORMAT_D32FS8] = { WGPU_TEXTURE_FORMAT_DEPTH32FLOAT_STENCIL8, WGPU_TEXTURE_FORMAT_DEPTH32FLOAT_STENCIL8 },
    [GPU_FORMAT_BC1] = { WGPU_TEXTURE_FORMAT_BC1_RGBA_UNORM, WGPU_TEXTURE_FORMAT_BC1_RGBA_UNORM_SRGB },
    [GPU_FORMAT_BC2] = { WGPU_TEXTURE_FORMAT_BC2_RGBA_UNORM, WGPU_TEXTURE_FORMAT_BC2_RGBA_UNORM_SRGB },
    [GPU_FORMAT_BC3] = { WGPU_TEXTURE_FORMAT_BC3_RGBA_UNORM, WGPU_TEXTURE_FORMAT_BC3_RGBA_UNORM_SRGB },
    [GPU_FORMAT_BC4U] = { WGPU_TEXTURE_FORMAT_BC4_R_UNORM, WGPU_TEXTURE_FORMAT_BC4_R_UNORM },
    [GPU_FORMAT_BC4S] = { WGPU_TEXTURE_FORMAT_BC4_R_SNORM, WGPU_TEXTURE_FORMAT_BC4_R_SNORM },
    [GPU_FORMAT_BC5U] = { WGPU_TEXTURE_FORMAT_BC5_RG_UNORM, WGPU_TEXTURE_FORMAT_BC5_RG_UNORM },
    [GPU_FORMAT_BC5S] = { WGPU_TEXTURE_FORMAT_BC5_RG_SNORM, WGPU_TEXTURE_FORMAT_BC5_RG_SNORM },
    [GPU_FORMAT_BC6UF] = { WGPU_TEXTURE_FORMAT_BC6H_RGB_UFLOAT, WGPU_TEXTURE_FORMAT_BC6H_RGB_UFLOAT },
    [GPU_FORMAT_BC6SF] = { WGPU_TEXTURE_FORMAT_BC6H_RGB_FLOAT, WGPU_TEXTURE_FORMAT_BC6H_RGB_FLOAT },
    [GPU_FORMAT_BC7] = { WGPU_TEXTURE_FORMAT_BC7_RGBA_UNORM, WGPU_TEXTURE_FORMAT_BC7_RGBA_UNORM_SRGB },
    [GPU_FORMAT_ASTC_4x4] = { WGPU_TEXTURE_FORMAT_ASTC_4X4_UNORM, WGPU_TEXTURE_FORMAT_ASTC_4X4_UNORM_SRGB },
    [GPU_FORMAT_ASTC_5x4] = { WGPU_TEXTURE_FORMAT_ASTC_5X4_UNORM, WGPU_TEXTURE_FORMAT_ASTC_5X4_UNORM_SRGB },
    [GPU_FORMAT_ASTC_5x5] = { WGPU_TEXTURE_FORMAT_ASTC_5X5_UNORM, WGPU_TEXTURE_FORMAT_ASTC_5X5_UNORM_SRGB },
    [GPU_FORMAT_ASTC_6x5] = { WGPU_TEXTURE_FORMAT_ASTC_6X5_UNORM, WGPU_TEXTURE_FORMAT_ASTC_6X5_UNORM_SRGB },
    [GPU_FORMAT_ASTC_6x6] = { WGPU_TEXTURE_FORMAT_ASTC_6X6_UNORM, WGPU_TEXTURE_FORMAT_ASTC_6X6_UNORM_SRGB },
    [GPU_FORMAT_ASTC_8x5] = { WGPU_TEXTURE_FORMAT_ASTC_8X5_UNORM, WGPU_TEXTURE_FORMAT_ASTC_8X5_UNORM_SRGB },
    [GPU_FORMAT_ASTC_8x6] = { WGPU_TEXTURE_FORMAT_ASTC_8X6_UNORM, WGPU_TEXTURE_FORMAT_ASTC_8X6_UNORM_SRGB },
    [GPU_FORMAT_ASTC_8x8] = { WGPU_TEXTURE_FORMAT_ASTC_8X8_UNORM, WGPU_TEXTURE_FORMAT_ASTC_8X8_UNORM_SRGB },
    [GPU_FORMAT_ASTC_10x5] = { WGPU_TEXTURE_FORMAT_ASTC_10X5_UNORM, WGPU_TEXTURE_FORMAT_ASTC_10X5_UNORM_SRGB },
    [GPU_FORMAT_ASTC_10x6] = { WGPU_TEXTURE_FORMAT_ASTC_10X6_UNORM, WGPU_TEXTURE_FORMAT_ASTC_10X6_UNORM_SRGB },
    [GPU_FORMAT_ASTC_10x8] = { WGPU_TEXTURE_FORMAT_ASTC_10X8_UNORM, WGPU_TEXTURE_FORMAT_ASTC_10X8_UNORM_SRGB },
    [GPU_FORMAT_ASTC_10x10] = { WGPU_TEXTURE_FORMAT_ASTC_10X10_UNORM, WGPU_TEXTURE_FORMAT_ASTC_10X10_UNORM_SRGB },
    [GPU_FORMAT_ASTC_12x10] = { WGPU_TEXTURE_FORMAT_ASTC_12X10_UNORM, WGPU_TEXTURE_FORMAT_ASTC_12X10_UNORM_SRGB },
    [GPU_FORMAT_ASTC_12x12] = { WGPU_TEXTURE_FORMAT_ASTC_12X12_UNORM, WGPU_TEXTURE_FORMAT_ASTC_12X12_UNORM_SRGB }
  };

  return formats[format][srgb];
}

static uint32_t getRowSize(gpu_texture_format format, uint32_t width) {
  switch (format) {
    case GPU_FORMAT_R8: return width; break;
    case GPU_FORMAT_RG8:
    case GPU_FORMAT_R16:
    case GPU_FORMAT_R16F:
    case GPU_FORMAT_RGB565:
    case GPU_FORMAT_RGB5A1:
    case GPU_FORMAT_D16: return width * 2; break;
    case GPU_FORMAT_RGBA8:
    case GPU_FORMAT_BGRA8:
    case GPU_FORMAT_RG16:
    case GPU_FORMAT_RG16F:
    case GPU_FORMAT_R32F:
    case GPU_FORMAT_RG11B10F:
    case GPU_FORMAT_RGB10A2:
    case GPU_FORMAT_D24:
    case GPU_FORMAT_D24S8:
    case GPU_FORMAT_D32F: return width * 4; break;
    case GPU_FORMAT_D32FS8: return width * 5; break;
    case GPU_FORMAT_RGBA16:
    case GPU_FORMAT_RGBA16F:
    case GPU_FORMAT_RG32F: return width * 8; break;
    case GPU_FORMAT_RGBA32F: return width * 16; break;
    case GPU_FORMAT_BC1: return ((width + 3) / 4) * 8; break;
    case GPU_FORMAT_BC2: return ((width + 3) / 4) * 16; break;
    case GPU_FORMAT_BC3: return ((width + 3) / 4) * 16; break;
    case GPU_FORMAT_BC4U: return ((width + 3) / 4) * 8; break;
    case GPU_FORMAT_BC4S: return ((width + 3) / 4) * 8; break;
    case GPU_FORMAT_BC5U: return ((width + 3) / 4) * 16; break;
    case GPU_FORMAT_BC5S: return ((width + 3) / 4) * 16; break;
    case GPU_FORMAT_BC6UF: return ((width + 3) / 4) * 16; break;
    case GPU_FORMAT_BC6SF: return ((width + 3) / 4) * 16; break;
    case GPU_FORMAT_BC7: return ((width + 3) / 4) * 16; break;
    case GPU_FORMAT_ASTC_4x4: return ((width + 3) / 4) * 16; break;
    case GPU_FORMAT_ASTC_5x4: return ((width + 4) / 5) * 16; break;
    case GPU_FORMAT_ASTC_5x5: return ((width + 4) / 5) * 16; break;
    case GPU_FORMAT_ASTC_6x5: return ((width + 5) / 6) * 16; break;
    case GPU_FORMAT_ASTC_6x6: return ((width + 5) / 6) * 16; break;
    case GPU_FORMAT_ASTC_8x5: return ((width + 7) / 8) * 16; break;
    case GPU_FORMAT_ASTC_8x6: return ((width + 7) / 8) * 16; break;
    case GPU_FORMAT_ASTC_8x8: return ((width + 7) / 8) * 16; break;
    case GPU_FORMAT_ASTC_10x5: return ((width + 9) / 10) * 16; break;
    case GPU_FORMAT_ASTC_10x6: return ((width + 9) / 10) * 16; break;
    case GPU_FORMAT_ASTC_10x8: return ((width + 9) / 10) * 16; break;
    case GPU_FORMAT_ASTC_10x10: return ((width + 9) / 10) * 16; break;
    case GPU_FORMAT_ASTC_12x10: return ((width + 11) / 12) * 16; break;
    case GPU_FORMAT_ASTC_12x12: return ((width + 11) / 12) * 16; break;
    default: return 0;
  }
}
