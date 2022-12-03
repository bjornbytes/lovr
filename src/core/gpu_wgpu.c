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

size_t gpu_sizeof_buffer(void) { return sizeof(gpu_buffer); }
size_t gpu_sizeof_texture(void) { return sizeof(gpu_texture); }
size_t gpu_sizeof_sampler(void) { return sizeof(gpu_sampler); }
size_t gpu_sizeof_layout(void) { return sizeof(gpu_layout); }
size_t gpu_sizeof_shader(void) { return sizeof(gpu_shader); }

// State

static struct {
  WGPUDevice device;
} state;

// Helpers

#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))

static WGPUTextureFormat convertFormat(gpu_texture_format format, bool srgb);

// Buffer

bool gpu_buffer_init(gpu_buffer* buffer, gpu_buffer_info* info) {
  return buffer->handle = wgpuDeviceCreateBuffer(state.device, &(WGPUBufferDescriptor) {
    .label = info->label,
    .usage =
      WGPUBufferUsage_Vertex |
      WGPUBufferUsage_Index |
      WGPUBufferUsage_Uniform |
      WGPUBufferUsage_Storage |
      WGPUBufferUsage_Indirect |
      WGPUBufferUsage_CopySrc |
      WGPUBufferUsage_CopyDst |
      WGPUBufferUsage_QueryResolve,
    .size = info->size,
    .mappedAtCreation = !!info->pointer
  });
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

// Sampler

bool gpu_sampler_init(gpu_sampler* sampler, gpu_sampler_info* info) {
  static const WGPUFilterMode filters[] = {
    [GPU_FILTER_NEAREST] = WGPUFilterMode_Nearest,
    [GPU_FILTER_LINEAR] = WGPUFilterMode_Linear
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
    .mipmapFilter = filters[info->mip],
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
  for (uint32_t i = 0; i < COUNTOF(info->stages) && info->stages[i].code; i++) {
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

// Entry

bool gpu_init(gpu_config* config) {
  state.device = emscripten_webgpu_get_device();
  return !!state.device;
}

void gpu_destroy(void) {
  if (state.device) wgpuDeviceDestroy(state.device);
  memset(&state, 0, sizeof(state));
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
