#include "gpu.h"
#include <webgpu/webgpu.h>
#include <emscripten/html5_webgpu.h>
#include <string.h>

struct gpu_buffer {
  WGPUBuffer handle;
};

struct gpu_texture {
  WGPUTexture handle;
  WGPUTextureView view;
};

struct gpu_sampler {
  WGPUSampler handle;
};

struct gpu_layout {
  WGPUBindGroupLayout handle;
};

struct gpu_shader {
  WGPUShaderModule handles[2];
  WGPUPipelineLayout pipelineLayout;
};

struct gpu_bundle_pool {
  void* unused;
};

struct gpu_bundle {
  WGPUBindGroup handle;
};

struct gpu_pass {
  gpu_pass_info info;
};

struct gpu_pipeline {
  WGPURenderPipeline render;
  WGPUComputePipeline compute;
};

struct gpu_tally {
  WGPUQuerySet handle;
};

struct gpu_stream {
  WGPUCommandEncoder commands;
  union {
    WGPURenderPassEncoder render;
    WGPUComputePassEncoder compute;
  };
};

size_t gpu_sizeof_buffer(void) { return sizeof(gpu_buffer); }
size_t gpu_sizeof_texture(void) { return sizeof(gpu_texture); }
size_t gpu_sizeof_sampler(void) { return sizeof(gpu_sampler); }
size_t gpu_sizeof_layout(void) { return sizeof(gpu_layout); }
size_t gpu_sizeof_shader(void) { return sizeof(gpu_shader); }
size_t gpu_sizeof_bundle_pool(void) { return sizeof(gpu_bundle_pool); }
size_t gpu_sizeof_bundle(void) { return sizeof(gpu_bundle); }
size_t gpu_sizeof_pass(void) { return sizeof(gpu_pass); }
size_t gpu_sizeof_pipeline(void) { return sizeof(gpu_pipeline); }
size_t gpu_sizeof_tally(void) { return sizeof(gpu_tally); }

// State

static struct {
  WGPUDevice device;
  WGPUQueue queue;
  gpu_stream streams[64];
  uint32_t streamCount;
  uint32_t tick;
  uint32_t lastTickFinished;
} state;

// Helpers

#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

static WGPUTextureFormat convertFormat(gpu_texture_format format, bool srgb);

// Buffer

bool gpu_buffer_init(gpu_buffer* buffer, gpu_buffer_info* info) {
  static const WGPUBufferUsage usages[] = {
    [GPU_BUFFER_STATIC] =
      WGPUBufferUsage_Vertex |
      WGPUBufferUsage_Index |
      WGPUBufferUsage_Uniform |
      WGPUBufferUsage_Storage |
      WGPUBufferUsage_Indirect |
      WGPUBufferUsage_CopySrc |
      WGPUBufferUsage_CopyDst |
      WGPUBufferUsage_QueryResolve,
    [GPU_BUFFER_STREAM] =
      WGPUBufferUsage_Vertex |
      WGPUBufferUsage_Index |
      WGPUBufferUsage_Uniform |
      WGPUBufferUsage_CopySrc |
      WGPUBufferUsage_MapWrite,
    [GPU_BUFFER_UPLOAD] =
      WGPUBufferUsage_CopySrc |
      WGPUBufferUsage_MapWrite,
    [GPU_BUFFER_DOWNLOAD] =
      WGPUBufferUsage_CopyDst |
      WGPUBufferUsage_Storage |
      WGPUBufferUsage_MapRead,
  };

  buffer->handle = wgpuDeviceCreateBuffer(state.device, &(WGPUBufferDescriptor) {
    .label = info->label,
    .usage = usages[info->type],
    .size = info->size,
    .mappedAtCreation = !!info->pointer
  });

  if (info->pointer) {
    *info->pointer = wgpuBufferGetMappedRange(buffer->handle, 0, info->size);
  }

  return !!buffer->handle;
}

void gpu_buffer_destroy(gpu_buffer* buffer) {
  wgpuBufferDestroy(buffer->handle);
}

// Texture

bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info) {
  static const WGPUTextureDimension dimensions[] = {
    [GPU_TEXTURE_2D] = WGPUTextureDimension_2D,
    [GPU_TEXTURE_3D] = WGPUTextureDimension_3D,
    [GPU_TEXTURE_CUBE] = WGPUTextureDimension_2D,
    [GPU_TEXTURE_ARRAY] = WGPUTextureDimension_2D
  };

  static const WGPUTextureViewDimension types[] = {
    [GPU_TEXTURE_2D] = WGPUTextureViewDimension_2D,
    [GPU_TEXTURE_3D] = WGPUTextureViewDimension_3D,
    [GPU_TEXTURE_CUBE] = WGPUTextureViewDimension_Cube,
    [GPU_TEXTURE_ARRAY] = WGPUTextureViewDimension_2DArray
  };

  texture->handle = wgpuDeviceCreateTexture(state.device, &(WGPUTextureDescriptor) {
    .label = info->label,
    .usage =
      ((info->usage & GPU_TEXTURE_RENDER) ? WGPUTextureUsage_RenderAttachment : 0) |
      ((info->usage & GPU_TEXTURE_SAMPLE) ? WGPUTextureUsage_TextureBinding : 0) |
      ((info->usage & GPU_TEXTURE_STORAGE) ? WGPUTextureUsage_StorageBinding : 0) |
      ((info->usage & GPU_TEXTURE_COPY_SRC) ? WGPUTextureUsage_CopySrc : 0) |
      ((info->usage & GPU_TEXTURE_COPY_DST) ? WGPUTextureUsage_CopyDst : 0),
    .dimension = dimensions[info->type],
    .size = { info->size[0], info->size[1], info->size[2] },
    .format = convertFormat(info->format, info->srgb),
    .mipLevelCount = info->mipmaps,
    .sampleCount = info->samples
  });

  texture->view = wgpuTextureCreateView(texture->handle, &(WGPUTextureViewDescriptor) {
    .format = wgpuTextureGetFormat(texture->handle),
    .dimension = types[info->type],
    .mipLevelCount = info->mipmaps,
    .arrayLayerCount = info->type == GPU_TEXTURE_ARRAY ? info->size[2] : 1
  });

  // TODO upload, mipgen

  return true;
}

bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info) {
  static const WGPUTextureViewDimension types[] = {
    [GPU_TEXTURE_2D] = WGPUTextureViewDimension_2D,
    [GPU_TEXTURE_3D] = WGPUTextureViewDimension_3D,
    [GPU_TEXTURE_CUBE] = WGPUTextureViewDimension_Cube,
    [GPU_TEXTURE_ARRAY] = WGPUTextureViewDimension_2DArray
  };

  texture->handle = NULL;

  return texture->view = wgpuTextureCreateView(info->source->handle, &(WGPUTextureViewDescriptor) {
    .format = wgpuTextureGetFormat(info->source->handle),
    .dimension = types[info->type],
    .baseMipLevel = info->levelIndex,
    .mipLevelCount = info->levelCount,
    .baseArrayLayer = info->layerIndex,
    .arrayLayerCount = info->layerCount
  });
}

void gpu_texture_destroy(gpu_texture* texture) {
  wgpuTextureViewRelease(texture->view);
  wgpuTextureDestroy(texture->handle);
}

// Surface

bool gpu_surface_init(gpu_surface_info* info) {
  return false; // TODO
}

void gpu_surface_resize(uint32_t width, uint32_t height) {
  // TODO
}

gpu_texture* gpu_surface_acquire(void) {
  return NULL; // TODO
}

void gpu_surface_present(void) {
  // TODO
}

// Sampler

bool gpu_sampler_init(gpu_sampler* sampler, gpu_sampler_info* info) {
  static const WGPUFilterMode filters[] = {
    [GPU_FILTER_NEAREST] = WGPUFilterMode_Nearest,
    [GPU_FILTER_LINEAR] = WGPUFilterMode_Linear
  };

  static const WGPUMipmapFilterMode mipFilters[] = {
    [GPU_FILTER_NEAREST] = WGPUMipmapFilterMode_Nearest,
    [GPU_FILTER_LINEAR] = WGPUMipmapFilterMode_Linear
  };

  static const WGPUAddressMode wraps[] = {
    [GPU_WRAP_CLAMP] = WGPUAddressMode_ClampToEdge,
    [GPU_WRAP_REPEAT] = WGPUAddressMode_Repeat,
    [GPU_WRAP_MIRROR] = WGPUAddressMode_MirrorRepeat
  };

  static const WGPUCompareFunction compares[] = {
    [GPU_COMPARE_NONE] = WGPUCompareFunction_Always,
    [GPU_COMPARE_EQUAL] = WGPUCompareFunction_Equal,
    [GPU_COMPARE_NEQUAL] = WGPUCompareFunction_NotEqual,
    [GPU_COMPARE_LESS] = WGPUCompareFunction_Less,
    [GPU_COMPARE_LEQUAL] = WGPUCompareFunction_LessEqual,
    [GPU_COMPARE_GREATER] = WGPUCompareFunction_Greater,
    [GPU_COMPARE_GEQUAL] = WGPUCompareFunction_GreaterEqual
  };

  return sampler->handle = wgpuDeviceCreateSampler(state.device, &(WGPUSamplerDescriptor) {
    .addressModeU = wraps[info->wrap[0]],
    .addressModeV = wraps[info->wrap[1]],
    .addressModeW = wraps[info->wrap[2]],
    .magFilter = filters[info->mag],
    .minFilter = filters[info->min],
    .mipmapFilter = mipFilters[info->mip],
    .lodMinClamp = info->lodClamp[0],
    .lodMaxClamp = info->lodClamp[1],
    .compare = compares[info->compare],
    .maxAnisotropy = info->anisotropy
  });
}

void gpu_sampler_destroy(gpu_sampler* sampler) {
  wgpuSamplerRelease(sampler->handle);
}

// Layout

bool gpu_layout_init(gpu_layout* layout, gpu_layout_info* info) {
  static const WGPUBufferBindingType bufferTypes[] = {
    [GPU_SLOT_UNIFORM_BUFFER] = WGPUBufferBindingType_Uniform,
    [GPU_SLOT_STORAGE_BUFFER] = WGPUBufferBindingType_Storage,
    [GPU_SLOT_UNIFORM_BUFFER_DYNAMIC] = WGPUBufferBindingType_Uniform,
    [GPU_SLOT_STORAGE_BUFFER_DYNAMIC] = WGPUBufferBindingType_Storage
  };

  gpu_slot* slot = info->slots;
  WGPUBindGroupLayoutEntry entries[32];
  for (uint32_t i = 0; i < info->count; i++, slot++) {
    entries[i] = (WGPUBindGroupLayoutEntry) {
      .binding = slot->number,
      .visibility =
        (((slot->stages & GPU_STAGE_VERTEX) ? WGPUShaderStage_Vertex : 0) |
        ((slot->stages & GPU_STAGE_FRAGMENT) ? WGPUShaderStage_Fragment : 0) |
        ((slot->stages & GPU_STAGE_COMPUTE) ? WGPUShaderStage_Compute : 0))
    };

    switch (info->slots[i].type) {
      case GPU_SLOT_UNIFORM_BUFFER_DYNAMIC:
      case GPU_SLOT_STORAGE_BUFFER_DYNAMIC:
        entries[i].buffer.hasDynamicOffset = true;
        /* fallthrough */
      case GPU_SLOT_UNIFORM_BUFFER:
      case GPU_SLOT_STORAGE_BUFFER:
        entries[i].buffer.type = bufferTypes[slot->type];
        break;

      case GPU_SLOT_TEXTURE_WITH_SAMPLER:
        return false; // Unsupported

      // FIXME need more metadata
      case GPU_SLOT_SAMPLED_TEXTURE:
        entries[i].texture.sampleType = WGPUTextureSampleType_Float;
        entries[i].texture.viewDimension = WGPUTextureViewDimension_2D;
        entries[i].texture.multisampled = false;
        break;

      // FIXME need more metadata
      case GPU_SLOT_STORAGE_TEXTURE:
        entries[i].storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
        entries[i].storageTexture.format = WGPUTextureFormat_Undefined;
        entries[i].storageTexture.viewDimension = WGPUTextureViewDimension_2D;
        break;

      // FIXME need more metadata?
      case GPU_SLOT_SAMPLER:
        entries[i].sampler.type = WGPUSamplerBindingType_Filtering;
        break;
    }
  }

  return layout->handle = wgpuDeviceCreateBindGroupLayout(state.device, &(WGPUBindGroupLayoutDescriptor) {
    .entryCount = info->count,
    .entries = entries
  });
}

void gpu_layout_destroy(gpu_layout* layout) {
  wgpuBindGroupLayoutRelease(layout->handle);
}

// Shader

bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info) {
  for (uint32_t i = 0; i < info->stageCount; i++) {
    WGPUShaderModuleSPIRVDescriptor spirv = {
      .chain.sType = WGPUSType_ShaderModuleSPIRVDescriptor,
      .codeSize = info->stages[i].length,
      .code = info->stages[i].code
    };

    shader->handles[i] = wgpuDeviceCreateShaderModule(state.device, &(WGPUShaderModuleDescriptor) {
      .nextInChain = &spirv.chain,
      .label = info->label
    });
  }

  uint32_t layoutCount = 0;
  WGPUBindGroupLayout layouts[4];
  for (uint32_t i = 0; i < COUNTOF(info->layouts) && info->layouts[i]; i++) {
    layouts[layoutCount++] = info->layouts[i]->handle;
  }

  return shader->pipelineLayout = wgpuDeviceCreatePipelineLayout(state.device, &(WGPUPipelineLayoutDescriptor) {
    .bindGroupLayoutCount = layoutCount,
    .bindGroupLayouts = layouts
  });
}

void gpu_shader_destroy(gpu_shader* shader) {
  wgpuShaderModuleRelease(shader->handles[0]);
  wgpuShaderModuleRelease(shader->handles[1]);
  wgpuPipelineLayoutRelease(shader->pipelineLayout);
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
  WGPUBindGroupEntry entries[32];

  for (uint32_t i = 0; i < count; i++) {
    gpu_bundle_info* info = &infos[i];
    WGPUBindGroupEntry* entry = entries;
    gpu_binding* binding = info->bindings;

    // TODO: error if binding array is given
    // TODO: error if binding count is bigger than 32

    for (uint32_t j = 0; j < info->count; j++, entry++, binding++) {
      memset(entry, 0, sizeof(*entry));
      entry->binding = binding->number;

      switch (binding->type) {
        case GPU_SLOT_UNIFORM_BUFFER:
        case GPU_SLOT_STORAGE_BUFFER:
        case GPU_SLOT_UNIFORM_BUFFER_DYNAMIC:
        case GPU_SLOT_STORAGE_BUFFER_DYNAMIC:
          entry->buffer = binding->buffer.object->handle;
          entry->offset = binding->buffer.offset;
          entry->size = binding->buffer.extent;
          break;
        case GPU_SLOT_TEXTURE_WITH_SAMPLER:
          break; // Unsupported
        case GPU_SLOT_SAMPLED_TEXTURE:
        case GPU_SLOT_STORAGE_TEXTURE:
          entry->textureView = binding->texture.object->view;
          break;
        case GPU_SLOT_SAMPLER:
          entry->sampler = binding->texture.sampler->handle;
          break;
      }
    }

    WGPUBindGroupDescriptor descriptor = {
      .layout = info->layout->handle,
      .entryCount = info->count,
      .entries = entries
    };

    bundles[i]->handle = wgpuDeviceCreateBindGroup(state.device, &descriptor);
  }
}

// Pass

bool gpu_pass_init(gpu_pass* pass, gpu_pass_info* info) {
  pass->info = *info;
  return true;
}

void gpu_pass_destroy(gpu_pass* pass) {
  //
}

// Pipeline

bool gpu_pipeline_init_graphics(gpu_pipeline* pipeline, gpu_pipeline_info* info) {
  static const WGPUPrimitiveTopology topologies[] = {
    [GPU_DRAW_POINTS] = WGPUPrimitiveTopology_PointList,
    [GPU_DRAW_LINES] = WGPUPrimitiveTopology_LineList,
    [GPU_DRAW_TRIANGLES] = WGPUPrimitiveTopology_TriangleList
  };

  static const WGPUVertexFormat attributeTypes[] = {
    [GPU_TYPE_I8x4] = WGPUVertexFormat_Sint8x4,
    [GPU_TYPE_U8x4] = WGPUVertexFormat_Uint8x4,
    [GPU_TYPE_SN8x4] = WGPUVertexFormat_Snorm8x4,
    [GPU_TYPE_UN8x4] = WGPUVertexFormat_Unorm8x4,
    [GPU_TYPE_SN10x3] = WGPUVertexFormat_Undefined,
    [GPU_TYPE_UN10x3] = WGPUVertexFormat_Undefined,
    [GPU_TYPE_I16] = WGPUVertexFormat_Undefined,
    [GPU_TYPE_I16x2] = WGPUVertexFormat_Sint16x2,
    [GPU_TYPE_I16x4] = WGPUVertexFormat_Sint16x4,
    [GPU_TYPE_U16] = WGPUVertexFormat_Undefined,
    [GPU_TYPE_U16x2] = WGPUVertexFormat_Uint16x2,
    [GPU_TYPE_U16x4] = WGPUVertexFormat_Uint16x4,
    [GPU_TYPE_SN16x2] = WGPUVertexFormat_Snorm16x2,
    [GPU_TYPE_SN16x4] = WGPUVertexFormat_Snorm16x4,
    [GPU_TYPE_UN16x2] = WGPUVertexFormat_Unorm16x2,
    [GPU_TYPE_UN16x4] = WGPUVertexFormat_Unorm16x4,
    [GPU_TYPE_I32] = WGPUVertexFormat_Sint32,
    [GPU_TYPE_I32x2] = WGPUVertexFormat_Sint32x2,
    [GPU_TYPE_I32x3] = WGPUVertexFormat_Sint32x3,
    [GPU_TYPE_I32x4] = WGPUVertexFormat_Sint32x4,
    [GPU_TYPE_U32] = WGPUVertexFormat_Uint32,
    [GPU_TYPE_U32x2] = WGPUVertexFormat_Uint32x2,
    [GPU_TYPE_U32x3] = WGPUVertexFormat_Uint32x3,
    [GPU_TYPE_U32x4] = WGPUVertexFormat_Uint32x4,
    [GPU_TYPE_F16x2] = WGPUVertexFormat_Float16x2,
    [GPU_TYPE_F16x4] = WGPUVertexFormat_Float16x4,
    [GPU_TYPE_F32] = WGPUVertexFormat_Float32,
    [GPU_TYPE_F32x2] = WGPUVertexFormat_Float32x2,
    [GPU_TYPE_F32x3] = WGPUVertexFormat_Float32x3,
    [GPU_TYPE_F32x4] = WGPUVertexFormat_Float32x4
  };

  static const WGPUFrontFace frontFaces[] = {
    [GPU_WINDING_CCW] = WGPUFrontFace_CCW,
    [GPU_WINDING_CW] = WGPUFrontFace_CW
  };

  static const WGPUCullMode cullModes[] = {
    [GPU_CULL_NONE] = WGPUCullMode_None,
    [GPU_CULL_FRONT] = WGPUCullMode_Front,
    [GPU_CULL_BACK] = WGPUCullMode_Back
  };

  static const WGPUCompareFunction compares[] = {
    [GPU_COMPARE_NONE] = WGPUCompareFunction_Always,
    [GPU_COMPARE_EQUAL] = WGPUCompareFunction_Equal,
    [GPU_COMPARE_NEQUAL] = WGPUCompareFunction_NotEqual,
    [GPU_COMPARE_LESS] = WGPUCompareFunction_Less,
    [GPU_COMPARE_LEQUAL] = WGPUCompareFunction_LessEqual,
    [GPU_COMPARE_GREATER] = WGPUCompareFunction_Greater,
    [GPU_COMPARE_GEQUAL] = WGPUCompareFunction_GreaterEqual
  };

  static const WGPUStencilOperation stencilOps[] = {
    [GPU_STENCIL_KEEP] = WGPUStencilOperation_Keep,
    [GPU_STENCIL_ZERO] = WGPUStencilOperation_Zero,
    [GPU_STENCIL_REPLACE] = WGPUStencilOperation_Replace,
    [GPU_STENCIL_INCREMENT] = WGPUStencilOperation_IncrementClamp,
    [GPU_STENCIL_DECREMENT] = WGPUStencilOperation_DecrementClamp,
    [GPU_STENCIL_INCREMENT_WRAP] = WGPUStencilOperation_IncrementWrap,
    [GPU_STENCIL_DECREMENT_WRAP] = WGPUStencilOperation_DecrementWrap,
    [GPU_STENCIL_INVERT] = WGPUStencilOperation_Invert
  };

  static const WGPUBlendFactor blendFactors[] = {
    [GPU_BLEND_ZERO] = WGPUBlendFactor_Zero,
    [GPU_BLEND_ONE] = WGPUBlendFactor_One,
    [GPU_BLEND_SRC_COLOR] = WGPUBlendFactor_Src,
    [GPU_BLEND_ONE_MINUS_SRC_COLOR] = WGPUBlendFactor_OneMinusSrc,
    [GPU_BLEND_SRC_ALPHA] = WGPUBlendFactor_SrcAlpha,
    [GPU_BLEND_ONE_MINUS_SRC_ALPHA] = WGPUBlendFactor_OneMinusSrcAlpha,
    [GPU_BLEND_DST_COLOR] = WGPUBlendFactor_Dst,
    [GPU_BLEND_ONE_MINUS_DST_COLOR] = WGPUBlendFactor_OneMinusDst,
    [GPU_BLEND_DST_ALPHA] = WGPUBlendFactor_DstAlpha,
    [GPU_BLEND_ONE_MINUS_DST_ALPHA] = WGPUBlendFactor_OneMinusDstAlpha
  };

  static const WGPUBlendOperation blendOps[] = {
    [GPU_BLEND_ADD] = WGPUBlendOperation_Add,
    [GPU_BLEND_SUB] = WGPUBlendOperation_Subtract,
    [GPU_BLEND_RSUB] = WGPUBlendOperation_ReverseSubtract,
    [GPU_BLEND_MIN] = WGPUBlendOperation_Min,
    [GPU_BLEND_MAX] = WGPUBlendOperation_Max
  };

  uint32_t totalAttributeCount = 0;
  WGPUVertexAttribute attributes[16];
  WGPUVertexBufferLayout vertexBuffers[16];
  for (uint32_t i = 0; i < info->vertex.bufferCount; i++) {
    vertexBuffers[i] = (WGPUVertexBufferLayout) {
      .arrayStride = info->vertex.bufferStrides[i],
      .stepMode = (info->vertex.instancedBuffers & (1 << i)) ? WGPUVertexStepMode_Instance : WGPUVertexStepMode_Vertex,
      .attributeCount = 0,
      .attributes = &attributes[totalAttributeCount]
    };

    for (uint32_t j = 0; j < info->vertex.attributeCount; j++) {
      if (info->vertex.attributes[j].buffer == i) {
        attributes[totalAttributeCount++] = (WGPUVertexAttribute) {
          .format = attributeTypes[info->vertex.attributes[j].type],
          .offset = info->vertex.attributes[j].offset,
          .shaderLocation = info->vertex.attributes[j].location
        };
      }
    }

    totalAttributeCount += vertexBuffers[i].attributeCount;
  }

  WGPUVertexState vertex = {
    .module = info->shader->handles[0],
    .entryPoint = "main",
    .bufferCount = info->vertex.bufferCount,
    .buffers = vertexBuffers
  };

  WGPUPrimitiveState primitive = {
    .topology = topologies[info->drawMode],
    .frontFace = frontFaces[info->rasterizer.winding],
    .cullMode = cullModes[info->rasterizer.cullMode],
  };

  WGPUStencilFaceState stencil = {
    .compare = compares[info->stencil.test],
    .failOp = stencilOps[info->stencil.failOp],
    .depthFailOp = stencilOps[info->stencil.depthFailOp],
    .passOp = stencilOps[info->stencil.passOp]
  };

  WGPUDepthStencilState depth = {
    .format = convertFormat(info->pass->info.depth.format, false),
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

  WGPUMultisampleState multisample = {
    .count = info->multisample.count,
    .alphaToCoverageEnabled = info->multisample.alphaToCoverage
  };

  WGPUBlendState blends[4];
  WGPUColorTargetState targets[4];
  for (uint32_t i = 0; i < info->pass->info.colorCount; i++) {
    targets[i] = (WGPUColorTargetState) {
      .format = convertFormat(info->pass->info.color[i].format, info->pass->info.color[i].srgb),
      .blend = info->blend[i].enabled ? &blends[i] : NULL,
      .writeMask = info->colorMask[i]
    };

    if (info->blend[i].enabled) {
      blends[i] = (WGPUBlendState) {
        .color.operation = blendOps[info->blend[i].color.op],
        .color.srcFactor = blendFactors[info->blend[i].color.src],
        .color.dstFactor = blendFactors[info->blend[i].color.dst],
        .alpha.operation = blendOps[info->blend[i].alpha.op],
        .alpha.srcFactor = blendFactors[info->blend[i].alpha.src],
        .alpha.dstFactor = blendFactors[info->blend[i].alpha.dst]
      };
    }
  }

  WGPUFragmentState fragment = {
    .module = info->shader->handles[1],
    .entryPoint = "main",
    .targetCount = info->pass->info.colorCount,
    .targets = targets
  };

  WGPURenderPipelineDescriptor pipelineInfo = {
    .label = info->label,
    .layout = info->shader->pipelineLayout,
    .vertex = vertex,
    .primitive = primitive,
    .depthStencil = info->pass->info.depth.format ? &depth : NULL,
    .multisample = multisample,
    .fragment = &fragment
  };

  return pipeline->render = wgpuDeviceCreateRenderPipeline(state.device, &pipelineInfo);
}

bool gpu_pipeline_init_compute(gpu_pipeline* pipeline, gpu_compute_pipeline_info* info) {
  WGPUComputePipelineDescriptor pipelineInfo = {
    .layout = info->shader->pipelineLayout,
    .compute = {
      .module = info->shader->handles[0],
      .entryPoint = "main"
    }
  };

  return pipeline->compute = wgpuDeviceCreateComputePipeline(state.device, &pipelineInfo);
}

void gpu_pipeline_destroy(gpu_pipeline* pipeline) {
  if (pipeline->render) wgpuRenderPipelineRelease(pipeline->render);
  if (pipeline->compute) wgpuComputePipelineRelease(pipeline->compute);
}

void gpu_pipeline_get_cache(void* data, size_t* size) {
  *size = 0;
}

// Tally

bool gpu_tally_init(gpu_tally* tally, gpu_tally_info* info) {
  static const WGPUQueryType types[] = {
    [GPU_TALLY_TIME] = WGPUQueryType_Timestamp,
    [GPU_TALLY_PIXEL] = WGPUQueryType_Occlusion
  };

  return tally->handle = wgpuDeviceCreateQuerySet(state.device, &(WGPUQuerySetDescriptor) {
    .type = types[info->type],
    .count = info->count
  });
}

void gpu_tally_destroy(gpu_tally* tally) {
  wgpuQuerySetRelease(tally->handle);
}

// Stream

gpu_stream* gpu_stream_begin(const char* label) {
  if (state.streamCount >= COUNTOF(state.streams)) return NULL;
  gpu_stream* stream = &state.streams[state.streamCount++];

  stream->commands = wgpuDeviceCreateCommandEncoder(state.device, &(WGPUCommandEncoderDescriptor) {
    .label = label
  });

  return stream;
}

void gpu_stream_end(gpu_stream* stream) {
  //
}

void gpu_render_begin(gpu_stream* stream, gpu_canvas* canvas) {
  static const WGPULoadOp loadOps[] = {
    [GPU_LOAD_OP_CLEAR] = WGPULoadOp_Clear,
    [GPU_LOAD_OP_DISCARD] = WGPULoadOp_Clear,
    [GPU_LOAD_OP_KEEP] = WGPULoadOp_Load
  };

  static const WGPUStoreOp storeOps[] = {
    [GPU_SAVE_OP_KEEP] = WGPUStoreOp_Store,
    [GPU_LOAD_OP_DISCARD] = WGPUStoreOp_Discard
  };

  WGPURenderPassColorAttachment colorAttachments[COUNTOF(canvas->color)];

  for (uint32_t i = 0; i < canvas->pass->info.colorCount; i++) {
    colorAttachments[i] = (WGPURenderPassColorAttachment) {
      .view = canvas->color[i].texture->view,
      .resolveTarget = canvas->color[i].resolve->view,
      .loadOp = loadOps[canvas->pass->info.color[i].load],
      .storeOp = storeOps[canvas->pass->info.color[i].save],
      .clearValue.r = canvas->color[i].clear[0],
      .clearValue.g = canvas->color[i].clear[1],
      .clearValue.b = canvas->color[i].clear[2],
      .clearValue.a = canvas->color[i].clear[3]
    };
  }

  WGPURenderPassDepthStencilAttachment depth = {
    .view = canvas->depth.texture->view,
    .depthLoadOp = loadOps[canvas->pass->info.depth.load],
    .depthStoreOp = storeOps[canvas->pass->info.depth.save],
    .depthClearValue = canvas->depth.clear,
    .depthReadOnly = false,
    .stencilLoadOp = loadOps[canvas->pass->info.depth.stencilLoad],
    .stencilStoreOp = storeOps[canvas->pass->info.depth.stencilSave],
    .stencilClearValue = 0,
    .stencilReadOnly = false
  };

  WGPURenderPassDescriptor info = {
    .colorAttachmentCount = canvas->pass->info.colorCount,
    .colorAttachments = colorAttachments,
    .depthStencilAttachment = canvas->depth.texture ? &depth : NULL
  };

  stream->render = wgpuCommandEncoderBeginRenderPass(stream->commands, &info);
}

void gpu_render_end(gpu_stream* stream, gpu_canvas* canvas) {
  wgpuRenderPassEncoderEnd(stream->render);
  stream->render = NULL;
}

void gpu_compute_begin(gpu_stream* stream) {
  stream->compute = wgpuCommandEncoderBeginComputePass(stream->commands, NULL);
}

void gpu_compute_end(gpu_stream* stream) {
  wgpuComputePassEncoderEnd(stream->compute);
  stream->compute = NULL;
}

void gpu_set_viewport(gpu_stream* stream, float view[4], float depth[2]) {
  wgpuRenderPassEncoderSetViewport(stream->render, view[0], view[1], view[2], view[3], depth[0], depth[1]);
}

void gpu_set_scissor(gpu_stream* stream, uint32_t scissor[4]) {
  wgpuRenderPassEncoderSetScissorRect(stream->render, scissor[0], scissor[1], scissor[2], scissor[3]);
}

void gpu_push_constants(gpu_stream* stream, gpu_shader* shader, void* data, uint32_t size) {
  // Unsupported
}

void gpu_bind_pipeline(gpu_stream* stream, gpu_pipeline* pipeline, gpu_pipeline_type type) {
  if (type == GPU_PIPELINE_COMPUTE) {
    wgpuComputePassEncoderSetPipeline(stream->compute, pipeline->compute);
  } else {
    wgpuRenderPassEncoderSetPipeline(stream->render, pipeline->render);
  }
}

void gpu_bind_bundles(gpu_stream* stream, gpu_shader* shader, gpu_bundle** bundles, uint32_t first, uint32_t count, uint32_t* dynamicOffsets, uint32_t dynamicOffsetCount) {
  if (stream->compute) {
    for (uint32_t i = 0; i < count; i++) {
      wgpuComputePassEncoderSetBindGroup(stream->compute, first + i, bundles[i]->handle, 0, NULL); // TODO dynamic offsets buh
    }
  } else {
    for (uint32_t i = 0; i < count; i++) {
      wgpuRenderPassEncoderSetBindGroup(stream->render, first + i, bundles[i]->handle, 0, NULL); // TODO dynamic offsets buh
    }
  }
}

void gpu_bind_vertex_buffers(gpu_stream* stream, gpu_buffer** buffers, uint32_t* offsets, uint32_t first, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    uint64_t size = wgpuBufferGetSize(buffers[i]->handle) - offsets[i];
    wgpuRenderPassEncoderSetVertexBuffer(stream->render, first + i, buffers[i]->handle, offsets[i], size);
  }
}

void gpu_bind_index_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, gpu_index_type type) {
  WGPUIndexFormat indexTypes[] = {
    [GPU_INDEX_U16] = WGPUIndexFormat_Uint16,
    [GPU_INDEX_U32] = WGPUIndexFormat_Uint32
  };
  uint64_t size = wgpuBufferGetSize(buffer->handle) - offset;
  wgpuRenderPassEncoderSetIndexBuffer(stream->render, buffer->handle, indexTypes[type], offset, size);
}

void gpu_draw(gpu_stream* stream, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t baseInstance) {
  wgpuRenderPassEncoderDraw(stream->render, vertexCount, instanceCount, firstVertex, baseInstance);
}

void gpu_draw_indexed(gpu_stream* stream, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex, uint32_t baseInstance) {
  wgpuRenderPassEncoderDrawIndexed(stream->render, indexCount, instanceCount, firstIndex, baseVertex, baseInstance);
}

void gpu_draw_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount, uint32_t stride) {
  stride = stride ? stride : 16;
  for (uint32_t i = 0; i < drawCount; i++) {
    wgpuRenderPassEncoderDrawIndirect(stream->render, buffer->handle, offset + stride * i);
  }
}

void gpu_draw_indirect_indexed(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount, uint32_t stride) {
  stride = stride ? stride : 20;
  for (uint32_t i = 0; i < drawCount; i++) {
    wgpuRenderPassEncoderDrawIndexedIndirect(stream->render, buffer->handle, offset + stride * i);
  }
}

void gpu_compute(gpu_stream* stream, uint32_t x, uint32_t y, uint32_t z) {
  wgpuComputePassEncoderDispatchWorkgroups(stream->compute, x, y, z);
}

void gpu_compute_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset) {
  wgpuComputePassEncoderDispatchWorkgroupsIndirect(stream->compute, buffer->handle, offset);
}

void gpu_copy_buffers(gpu_stream* stream, gpu_buffer* src, gpu_buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t extent) {
  wgpuCommandEncoderCopyBufferToBuffer(stream->commands, src->handle, srcOffset, dst->handle, dstOffset, extent);
}

void gpu_copy_textures(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t extent[3]) {
  WGPUImageCopyTexture srcRegion = {
    .texture = src->handle,
    .mipLevel = srcOffset[3],
    .origin = { srcOffset[0], srcOffset[1], srcOffset[2] }
  };

  WGPUImageCopyTexture dstRegion = {
    .texture = dst->handle,
    .mipLevel = dstOffset[3],
    .origin = { dstOffset[0], dstOffset[1], dstOffset[2] }
  };

  WGPUExtent3D size = { extent[0], extent[1], extent[2] };

  wgpuCommandEncoderCopyTextureToTexture(stream->commands, &srcRegion, &dstRegion, &size);
}

void gpu_copy_buffer_texture(gpu_stream* stream, gpu_buffer* src, gpu_texture* dst, uint32_t srcOffset, uint32_t dstOffset[4], uint32_t extent[3]) {
  WGPUImageCopyBuffer srcRegion = {
    .layout.offset = srcOffset,
    .layout.bytesPerRow = 0, // FIXME
    .layout.rowsPerImage = 0, // FIXME
    .buffer = src->handle
  };

  WGPUImageCopyTexture dstRegion = {
    .texture = dst->handle,
    .mipLevel = dstOffset[3],
    .origin = { dstOffset[0], dstOffset[1], dstOffset[2] }
  };

  WGPUExtent3D size = { extent[0], extent[1], extent[2] };

  wgpuCommandEncoderCopyBufferToTexture(stream->commands, &srcRegion, &dstRegion, &size);
}

void gpu_copy_texture_buffer(gpu_stream* stream, gpu_texture* src, gpu_buffer* dst, uint32_t srcOffset[4], uint32_t dstOffset, uint32_t extent[3]) {
  WGPUImageCopyTexture srcRegion = {
    .texture = src->handle,
    .mipLevel = srcOffset[3],
    .origin = { srcOffset[0], srcOffset[1], srcOffset[2] }
  };

  WGPUImageCopyBuffer dstRegion = {
    .layout.offset = dstOffset,
    .layout.bytesPerRow = 0, // FIXME
    .layout.rowsPerImage = 0, // FIXME
    .buffer = dst->handle
  };

  WGPUExtent3D size = { extent[0], extent[1], extent[2] };

  wgpuCommandEncoderCopyTextureToBuffer(stream->commands, &srcRegion, &dstRegion, &size);
}

void gpu_copy_tally_buffer(gpu_stream* stream, gpu_tally* src, gpu_buffer* dst, uint32_t srcIndex, uint32_t dstOffset, uint32_t count) {
  wgpuCommandEncoderResolveQuerySet(stream->commands, src->handle, srcIndex, count, dst->handle, dstOffset);
}

void gpu_clear_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t size, uint32_t value) {
  wgpuCommandEncoderClearBuffer(stream->commands, buffer->handle, offset, size);
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
  wgpuRenderPassEncoderBeginOcclusionQuery(stream->render, index);
}

void gpu_tally_finish(gpu_stream* stream, gpu_tally* tally, uint32_t index) {
  wgpuRenderPassEncoderEndOcclusionQuery(stream->render);
}

void gpu_tally_mark(gpu_stream* stream, gpu_tally* tally, uint32_t index) {
  wgpuCommandEncoderWriteTimestamp(stream->commands, tally->handle, index);
}

void gpu_xr_acquire(gpu_stream* stream, gpu_texture* texture) {
  //
}

void gpu_xr_release(gpu_stream* stream, gpu_texture* texture) {
  //
}

// Entry

bool gpu_init(gpu_config* config) {
  state.device = emscripten_webgpu_get_device();
  state.queue = wgpuDeviceGetQueue(state.device);

  if (config->features) {
    config->features->textureBC = wgpuDeviceHasFeature(state.device, WGPUFeatureName_TextureCompressionBC);
    config->features->textureASTC = wgpuDeviceHasFeature(state.device, WGPUFeatureName_TextureCompressionASTC);
    config->features->wireframe = false;
    config->features->depthClamp = wgpuDeviceHasFeature(state.device, WGPUFeatureName_DepthClipControl);
    config->features->depthResolve = false;
    config->features->indirectDrawFirstInstance = wgpuDeviceHasFeature(state.device, WGPUFeatureName_IndirectFirstInstance);
    config->features->shaderDebug = false;
    config->features->float64 = false;
    config->features->int64 = false;
    config->features->int16 = false;

    config->features->formats[GPU_FORMAT_R8][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_RG8][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER;
    config->features->formats[GPU_FORMAT_RGBA8][0] = GPU_FEATURE_SAMPLE | GPU_FEATURE_RENDER | GPU_FEATURE_STORAGE;
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
  }

  if (config->limits) {
    WGPUSupportedLimits supported;
    wgpuDeviceGetLimits(state.device, &supported);
    config->limits->textureSize2D = supported.limits.maxTextureDimension2D;
    config->limits->textureSize3D = supported.limits.maxTextureDimension3D;
    config->limits->textureSizeCube = supported.limits.maxTextureDimension2D;
    config->limits->textureLayers = supported.limits.maxTextureArrayLayers;
    config->limits->renderSize[0] = supported.limits.maxTextureDimension2D;
    config->limits->renderSize[1] = supported.limits.maxTextureDimension2D;
    config->limits->renderSize[2] = 1;
    config->limits->uniformBuffersPerStage = supported.limits.maxUniformBuffersPerShaderStage;
    config->limits->storageBuffersPerStage = supported.limits.maxStorageBuffersPerShaderStage;
    config->limits->sampledTexturesPerStage = supported.limits.maxSampledTexturesPerShaderStage;
    config->limits->storageTexturesPerStage = supported.limits.maxStorageTexturesPerShaderStage;
    config->limits->samplersPerStage = supported.limits.maxSamplersPerShaderStage;
    config->limits->uniformBufferRange = supported.limits.maxUniformBufferBindingSize;
    config->limits->storageBufferRange = supported.limits.maxStorageBufferBindingSize;
    config->limits->uniformBufferAlign = supported.limits.minUniformBufferOffsetAlignment;
    config->limits->storageBufferAlign = supported.limits.minStorageBufferOffsetAlignment;
    config->limits->vertexAttributes = supported.limits.maxVertexAttributes;
    config->limits->vertexBuffers = supported.limits.maxVertexBuffers;
    config->limits->vertexBufferStride = supported.limits.maxVertexBufferArrayStride;
    config->limits->vertexShaderOutputs = supported.limits.maxInterStageShaderComponents;
    config->limits->clipDistances = 0; // TODO
    config->limits->cullDistances = 0;
    config->limits->clipAndCullDistances = 0; // TODO
    config->limits->workgroupCount[0] = supported.limits.maxComputeWorkgroupsPerDimension;
    config->limits->workgroupCount[1] = supported.limits.maxComputeWorkgroupsPerDimension;
    config->limits->workgroupCount[2] = supported.limits.maxComputeWorkgroupsPerDimension;
    config->limits->workgroupSize[0] = supported.limits.maxComputeWorkgroupSizeX;
    config->limits->workgroupSize[1] = supported.limits.maxComputeWorkgroupSizeY;
    config->limits->workgroupSize[2] = supported.limits.maxComputeWorkgroupSizeZ;
    config->limits->totalWorkgroupSize = supported.limits.maxComputeInvocationsPerWorkgroup;
    config->limits->computeSharedMemory = supported.limits.maxComputeWorkgroupStorageSize;
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
  if (state.device) wgpuDeviceDestroy(state.device);
  memset(&state, 0, sizeof(state));
}

uint32_t gpu_begin(void) {
  return state.tick++;
}

static void onSubmittedWorkDone(WGPUQueueWorkDoneStatus status, void* userdata) {
  state.lastTickFinished = (uint32_t) (uintptr_t) userdata;
}

void gpu_submit(gpu_stream** streams, uint32_t count) {
  WGPUCommandBuffer commandBuffers[64];
  count = MIN(count, COUNTOF(commandBuffers));

  for (uint32_t i = 0; i < count; i++) {
    commandBuffers[i] = wgpuCommandEncoderFinish(streams[i]->commands, NULL);
  }

  wgpuQueueSubmit(state.queue, count, commandBuffers);
  wgpuQueueOnSubmittedWorkDone(state.queue, onSubmittedWorkDone, (void*) (uintptr_t) state.tick);

  for (uint32_t i = 0; i < state.streamCount; i++) {
    wgpuCommandEncoderRelease(state.streams[i].commands);
  }
}

bool gpu_is_complete(uint32_t tick) {
  return state.lastTickFinished >= tick;
}

bool gpu_wait_tick(uint32_t tick) {
  return true; // TODO Unsupported
}

void gpu_wait_idle(void) {
  // TODO Unsupported
}

// Helpers

static WGPUTextureFormat convertFormat(gpu_texture_format format, bool srgb) {
  static const WGPUTextureFormat formats[][2] = {
    [GPU_FORMAT_R8] = { WGPUTextureFormat_R8Unorm, WGPUTextureFormat_R8Unorm },
    [GPU_FORMAT_RG8] = { WGPUTextureFormat_RG8Unorm, WGPUTextureFormat_RG8Unorm },
    [GPU_FORMAT_RGBA8] = { WGPUTextureFormat_RGBA8Unorm, WGPUTextureFormat_RGBA8UnormSrgb },
    [GPU_FORMAT_R16] = { WGPUTextureFormat_Undefined, WGPUTextureFormat_Undefined },
    [GPU_FORMAT_RG16] = { WGPUTextureFormat_Undefined, WGPUTextureFormat_Undefined },
    [GPU_FORMAT_RGBA16] = { WGPUTextureFormat_Undefined, WGPUTextureFormat_Undefined },
    [GPU_FORMAT_R16F] = { WGPUTextureFormat_R16Float, WGPUTextureFormat_R16Float },
    [GPU_FORMAT_RG16F] = { WGPUTextureFormat_RG16Float, WGPUTextureFormat_RG16Float },
    [GPU_FORMAT_RGBA16F] = { WGPUTextureFormat_RGBA16Float, WGPUTextureFormat_RGBA16Float },
    [GPU_FORMAT_R32F] = { WGPUTextureFormat_R32Float, WGPUTextureFormat_R32Float },
    [GPU_FORMAT_RG32F] = { WGPUTextureFormat_RG32Float, WGPUTextureFormat_RG32Float },
    [GPU_FORMAT_RGBA32F] = { WGPUTextureFormat_RGBA32Float, WGPUTextureFormat_RGBA32Float },
    [GPU_FORMAT_RGB565] = { WGPUTextureFormat_Undefined, WGPUTextureFormat_Undefined },
    [GPU_FORMAT_RGB5A1] = { WGPUTextureFormat_Undefined, WGPUTextureFormat_Undefined },
    [GPU_FORMAT_RGB10A2] = {WGPUTextureFormat_RGB10A2Unorm, WGPUTextureFormat_RGB10A2Unorm },
    [GPU_FORMAT_RG11B10F] = { WGPUTextureFormat_RG11B10Ufloat, WGPUTextureFormat_RG11B10Ufloat },
    [GPU_FORMAT_D16] = { WGPUTextureFormat_Depth16Unorm, WGPUTextureFormat_Depth16Unorm },
    [GPU_FORMAT_D24] = { WGPUTextureFormat_Depth24Plus, WGPUTextureFormat_Depth24Plus },
    [GPU_FORMAT_D32F] = { WGPUTextureFormat_Depth32Float, WGPUTextureFormat_Depth32Float },
    [GPU_FORMAT_D24S8] = { WGPUTextureFormat_Depth24PlusStencil8, WGPUTextureFormat_Depth24PlusStencil8 },
    [GPU_FORMAT_D32FS8] = { WGPUTextureFormat_Depth32FloatStencil8, WGPUTextureFormat_Depth32FloatStencil8 },
    [GPU_FORMAT_BC1] = { WGPUTextureFormat_BC1RGBAUnorm, WGPUTextureFormat_BC1RGBAUnormSrgb },
    [GPU_FORMAT_BC2] = { WGPUTextureFormat_BC2RGBAUnorm, WGPUTextureFormat_BC2RGBAUnormSrgb },
    [GPU_FORMAT_BC3] = { WGPUTextureFormat_BC3RGBAUnorm, WGPUTextureFormat_BC3RGBAUnormSrgb },
    [GPU_FORMAT_BC4U] = { WGPUTextureFormat_BC4RUnorm, WGPUTextureFormat_BC4RUnorm },
    [GPU_FORMAT_BC4S] = { WGPUTextureFormat_BC4RSnorm, WGPUTextureFormat_BC4RSnorm },
    [GPU_FORMAT_BC5U] = { WGPUTextureFormat_BC5RGUnorm, WGPUTextureFormat_BC5RGUnorm },
    [GPU_FORMAT_BC5S] = { WGPUTextureFormat_BC5RGSnorm, WGPUTextureFormat_BC5RGSnorm },
    [GPU_FORMAT_BC6UF] = { WGPUTextureFormat_BC6HRGBUfloat, WGPUTextureFormat_BC6HRGBUfloat },
    [GPU_FORMAT_BC6SF] = { WGPUTextureFormat_BC6HRGBFloat, WGPUTextureFormat_BC6HRGBFloat },
    [GPU_FORMAT_BC7] = { WGPUTextureFormat_BC7RGBAUnorm, WGPUTextureFormat_BC7RGBAUnormSrgb },
    [GPU_FORMAT_ASTC_4x4] = { WGPUTextureFormat_ASTC4x4Unorm, WGPUTextureFormat_ASTC4x4UnormSrgb },
    [GPU_FORMAT_ASTC_5x4] = { WGPUTextureFormat_ASTC5x4Unorm, WGPUTextureFormat_ASTC5x4UnormSrgb },
    [GPU_FORMAT_ASTC_5x5] = { WGPUTextureFormat_ASTC5x5Unorm, WGPUTextureFormat_ASTC5x5UnormSrgb },
    [GPU_FORMAT_ASTC_6x5] = { WGPUTextureFormat_ASTC6x5Unorm, WGPUTextureFormat_ASTC6x5UnormSrgb },
    [GPU_FORMAT_ASTC_6x6] = { WGPUTextureFormat_ASTC6x6Unorm, WGPUTextureFormat_ASTC6x6UnormSrgb },
    [GPU_FORMAT_ASTC_8x5] = { WGPUTextureFormat_ASTC8x5Unorm, WGPUTextureFormat_ASTC8x5UnormSrgb },
    [GPU_FORMAT_ASTC_8x6] = { WGPUTextureFormat_ASTC8x6Unorm, WGPUTextureFormat_ASTC8x6UnormSrgb },
    [GPU_FORMAT_ASTC_8x8] = { WGPUTextureFormat_ASTC8x8Unorm, WGPUTextureFormat_ASTC8x8UnormSrgb },
    [GPU_FORMAT_ASTC_10x5] = { WGPUTextureFormat_ASTC10x5Unorm, WGPUTextureFormat_ASTC10x5UnormSrgb },
    [GPU_FORMAT_ASTC_10x6] = { WGPUTextureFormat_ASTC10x6Unorm, WGPUTextureFormat_ASTC10x6UnormSrgb },
    [GPU_FORMAT_ASTC_10x8] = { WGPUTextureFormat_ASTC10x8Unorm, WGPUTextureFormat_ASTC10x8UnormSrgb },
    [GPU_FORMAT_ASTC_10x10] = { WGPUTextureFormat_ASTC10x10Unorm, WGPUTextureFormat_ASTC10x10UnormSrgb },
    [GPU_FORMAT_ASTC_12x10] = { WGPUTextureFormat_ASTC12x10Unorm, WGPUTextureFormat_ASTC12x10UnormSrgb },
    [GPU_FORMAT_ASTC_12x12] = { WGPUTextureFormat_ASTC12x12Unorm, WGPUTextureFormat_ASTC12x12UnormSrgb }
  };

  return formats[format][srgb];
}
