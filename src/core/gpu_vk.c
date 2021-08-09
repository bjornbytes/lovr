#include "gpu.h"
#include <string.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Objects

struct gpu_buffer {
  VkBuffer handle;
  VkDeviceMemory memory;
};

struct gpu_texture {
  VkImage handle;
  VkDeviceMemory memory;
  VkImageView view;
  VkFormat format;
  VkImageAspectFlagBits aspect;
  VkImageLayout layout;
  bool array;
};

struct gpu_sampler {
  VkSampler handle;
};

struct gpu_pass {
  VkRenderPass handle;
  uint32_t samples;
  uint32_t count;
};

struct gpu_shader {
  VkShaderModule handles[2];
  VkPipelineShaderStageCreateInfo pipelineInfo[2];
  VkDescriptorSetLayout layouts[4];
  VkPipelineLayout pipelineLayout;
  VkPipelineBindPoint type;
};

struct gpu_bundle {
  VkDescriptorSet handle;
};

struct gpu_pipeline {
  VkPipeline handle;
  VkPipelineBindPoint type;
};

struct gpu_stream {
  VkCommandBuffer commands;
};

size_t gpu_sizeof_buffer() { return sizeof(gpu_buffer); }
size_t gpu_sizeof_texture() { return sizeof(gpu_texture); }
size_t gpu_sizeof_sampler() { return sizeof(gpu_sampler); }
size_t gpu_sizeof_shader() { return sizeof(gpu_shader); }
size_t gpu_sizeof_bundle() { return sizeof(gpu_bundle); }
size_t gpu_sizeof_pass() { return sizeof(gpu_pass); }
size_t gpu_sizeof_pipeline() { return sizeof(gpu_pipeline); }

// Pools

typedef struct {
  VkCommandPool pool;
  gpu_stream streams[32];
  VkSemaphore semaphores[2];
  VkFence fence;
} gpu_tick;

typedef struct {
  void* handle;
  VkObjectType type;
  uint32_t tick;
} gpu_ref;

typedef struct {
  uint32_t head;
  uint32_t tail;
  gpu_ref data[256];
} gpu_morgue;

typedef struct {
  uint32_t hash;
  uint32_t tick;
  void* object;
} gpu_cache_entry;

typedef struct {
  VkDescriptorPool handle;
  uint32_t tick;
  uint32_t next;
} gpu_binding_puddle;

typedef struct {
  uint32_t count;
  uint32_t head;
  uint32_t tail;
  gpu_binding_puddle data[16];
} gpu_binding_pool;

// State

static struct {
  void* library;
  gpu_config config;
  VkInstance instance;
  VkPhysicalDevice adapter;
  VkDevice device;
  VkQueue queue;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  gpu_texture backbuffers[4];
  uint32_t currentBackbuffer;
  VkPipelineCache pipelineCache;
  VkDebugUtilsMessengerEXT messenger;
  uint32_t memoryTypeCount;
  uint32_t memoryTypes[32];
  uint32_t streamCount;
  uint32_t tick[2];
  gpu_tick ticks[4];
  gpu_morgue morgue;
  gpu_cache_entry framebuffers[16][4];
  gpu_binding_pool bindings;
  VkQueryPool queryPool;
  uint32_t queryCount;
} state;

// Helpers

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#define TRY(f, s) if (!try(f, s))
#define CHECK(c, s) if (!check(c, s))
#define SCRATCHPAD_SIZE (32 * 1024 * 1024)
#define HASH_SEED 2166136261
#define QUERY_CHUNK 64

enum { CPU, GPU };
enum { LINEAR, SRGB };

static void hash32(uint32_t* hash, void* data, uint32_t size);
static void ketchup(void);
static void condemn(void* handle, VkObjectType type);
static void expunge(void);
static VkFormat convertFormat(gpu_texture_format format, int colorspace);
static void nickname(void* object, VkObjectType type, const char* name);
static VkBool32 relay(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* context);
static bool try(VkResult result, const char* message);
static bool check(bool condition, const char* message);

// Loader

// Functions that don't require an instance
#define GPU_FOREACH_ANONYMOUS(X)\
  X(vkCreateInstance)

// Functions that require an instance but don't require a device
#define GPU_FOREACH_INSTANCE(X)\
  X(vkDestroyInstance)\
  X(vkCreateDebugUtilsMessengerEXT)\
  X(vkDestroyDebugUtilsMessengerEXT)\
  X(vkDestroySurfaceKHR)\
  X(vkEnumeratePhysicalDevices)\
  X(vkGetPhysicalDeviceProperties2)\
  X(vkGetPhysicalDeviceFeatures2)\
  X(vkGetPhysicalDeviceMemoryProperties)\
  X(vkGetPhysicalDeviceFormatProperties)\
  X(vkGetPhysicalDeviceQueueFamilyProperties)\
  X(vkGetPhysicalDeviceSurfaceSupportKHR)\
  X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)\
  X(vkGetPhysicalDeviceSurfaceFormatsKHR)\
  X(vkCreateDevice)\
  X(vkDestroyDevice)\
  X(vkGetDeviceQueue)\
  X(vkGetDeviceProcAddr)

// Functions that require a device
#define GPU_FOREACH_DEVICE(X)\
  X(vkSetDebugUtilsObjectNameEXT)\
  X(vkCmdBeginDebugUtilsLabelEXT)\
  X(vkCmdEndDebugUtilsLabelEXT)\
  X(vkDeviceWaitIdle)\
  X(vkQueueSubmit)\
  X(vkQueuePresentKHR)\
  X(vkCreateSwapchainKHR)\
  X(vkDestroySwapchainKHR)\
  X(vkGetSwapchainImagesKHR)\
  X(vkAcquireNextImageKHR)\
  X(vkCreateCommandPool)\
  X(vkDestroyCommandPool)\
  X(vkResetCommandPool)\
  X(vkAllocateCommandBuffers)\
  X(vkBeginCommandBuffer)\
  X(vkEndCommandBuffer)\
  X(vkCreateFence)\
  X(vkDestroyFence)\
  X(vkResetFences)\
  X(vkGetFenceStatus)\
  X(vkWaitForFences)\
  X(vkCreateSemaphore)\
  X(vkDestroySemaphore)\
  X(vkCmdPipelineBarrier)\
  X(vkCreateQueryPool)\
  X(vkDestroyQueryPool)\
  X(vkCmdResetQueryPool)\
  X(vkCmdWriteTimestamp)\
  X(vkCmdCopyQueryPoolResults)\
  X(vkCreateBuffer)\
  X(vkDestroyBuffer)\
  X(vkGetBufferMemoryRequirements)\
  X(vkBindBufferMemory)\
  X(vkCreateImage)\
  X(vkDestroyImage)\
  X(vkGetImageMemoryRequirements)\
  X(vkBindImageMemory)\
  X(vkCmdCopyBuffer)\
  X(vkCmdCopyImage)\
  X(vkCmdBlitImage)\
  X(vkCmdCopyBufferToImage)\
  X(vkCmdCopyImageToBuffer)\
  X(vkCmdFillBuffer)\
  X(vkCmdClearColorImage)\
  X(vkAllocateMemory)\
  X(vkFreeMemory)\
  X(vkMapMemory)\
  X(vkCreateSampler)\
  X(vkDestroySampler)\
  X(vkCreateRenderPass)\
  X(vkDestroyRenderPass)\
  X(vkCmdBeginRenderPass)\
  X(vkCmdEndRenderPass)\
  X(vkCreateImageView)\
  X(vkDestroyImageView)\
  X(vkCreateFramebuffer)\
  X(vkDestroyFramebuffer)\
  X(vkCreateShaderModule)\
  X(vkDestroyShaderModule)\
  X(vkCreateDescriptorSetLayout)\
  X(vkDestroyDescriptorSetLayout)\
  X(vkCreatePipelineLayout)\
  X(vkDestroyPipelineLayout)\
  X(vkCreateDescriptorPool)\
  X(vkDestroyDescriptorPool)\
  X(vkAllocateDescriptorSets)\
  X(vkResetDescriptorPool)\
  X(vkUpdateDescriptorSets)\
  X(vkCreatePipelineCache)\
  X(vkDestroyPipelineCache)\
  X(vkCreateGraphicsPipelines)\
  X(vkCreateComputePipelines)\
  X(vkDestroyPipeline)\
  X(vkCmdSetViewport)\
  X(vkCmdSetScissor)\
  X(vkCmdBindPipeline)\
  X(vkCmdBindDescriptorSets)\
  X(vkCmdBindVertexBuffers)\
  X(vkCmdBindIndexBuffer)\
  X(vkCmdDraw)\
  X(vkCmdDrawIndexed)\
  X(vkCmdDrawIndirect)\
  X(vkCmdDrawIndexedIndirect)\
  X(vkCmdDispatch)\
  X(vkCmdDispatchIndirect)

// Used to load/declare Vulkan functions without lots of clutter
#define GPU_LOAD_ANONYMOUS(fn) fn = (PFN_##fn) vkGetInstanceProcAddr(NULL, #fn);
#define GPU_LOAD_INSTANCE(fn) fn = (PFN_##fn) vkGetInstanceProcAddr(state.instance, #fn);
#define GPU_LOAD_DEVICE(fn) fn = (PFN_##fn) vkGetDeviceProcAddr(state.device, #fn);
#define GPU_DECLARE(fn) static PFN_##fn fn;

// Declare function pointers
GPU_FOREACH_ANONYMOUS(GPU_DECLARE)
GPU_FOREACH_INSTANCE(GPU_DECLARE)
GPU_FOREACH_DEVICE(GPU_DECLARE)

// Buffer

bool gpu_buffer_init(gpu_buffer* buffer, gpu_buffer_info* info) {
  if (info->handle) {
    buffer->handle = (VkBuffer) info->handle;
    return true;
  }

  VkBufferCreateInfo bufferInfo = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = info->size,
    .usage =
      ((info->usage & GPU_BUFFER_VERTEX) ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : 0) |
      ((info->usage & GPU_BUFFER_INDEX) ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT : 0) |
      ((info->usage & GPU_BUFFER_UNIFORM) ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0) |
      ((info->usage & GPU_BUFFER_STORAGE) ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0) |
      ((info->usage & GPU_BUFFER_INDIRECT) ? VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT : 0) |
      ((info->usage & GPU_BUFFER_COPY_SRC) ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0) |
      ((info->usage & GPU_BUFFER_COPY_DST) ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0)
  };

  TRY(vkCreateBuffer(state.device, &bufferInfo, NULL, &buffer->handle), "Could not create buffer") {
    return false;
  }

  nickname(buffer->handle, VK_OBJECT_TYPE_BUFFER, info->label);

  VkMemoryPropertyFlags wants[] = {
    [GPU_MEMORY_GPU] = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    [GPU_MEMORY_CPU_WRITE] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    [GPU_MEMORY_CPU_READ] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
  };

  VkMemoryPropertyFlags needs[] = {
    [GPU_MEMORY_GPU] = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    [GPU_MEMORY_CPU_WRITE] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    [GPU_MEMORY_CPU_READ] = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  };

  uint32_t fallback = ~0u;
  uint32_t memoryType = ~0u;
  uint32_t want = wants[info->memory];
  uint32_t need = needs[info->memory];
  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(state.device, buffer->handle, &requirements);
  for (uint32_t i = 0; i < state.memoryTypeCount; i++) {
    if (fallback == ~0u && (state.memoryTypes[i] & need) == need && requirements.memoryTypeBits & (1 << i)) fallback = i;
    if ((state.memoryTypes[i] & want) != want || ~requirements.memoryTypeBits & (1 << i)) continue;
    memoryType = i;
    break;
  }

  if (memoryType == ~0u) {
    memoryType = fallback;
  }

  VkMemoryAllocateInfo memoryInfo = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = requirements.size,
    .memoryTypeIndex = memoryType
  };

  if (memoryType == ~0u || !try(vkAllocateMemory(state.device, &memoryInfo, NULL, &buffer->memory), "Could not allocate buffer memory")) {
    vkDestroyBuffer(state.device, buffer->handle, NULL);
    return false;
  }

  TRY(vkBindBufferMemory(state.device, buffer->handle, buffer->memory, 0), "Could not bind buffer memory") {
    vkDestroyBuffer(state.device, buffer->handle, NULL);
    vkFreeMemory(state.device, buffer->memory, NULL);
    return false;
  }

  if (info->memory != GPU_MEMORY_GPU && info->data && !try(vkMapMemory(state.device, buffer->memory, 0, VK_WHOLE_SIZE, 0, info->data), "Could not map memory")) {
    vkDestroyBuffer(state.device, buffer->handle, NULL);
    vkFreeMemory(state.device, buffer->memory, NULL);
    return false;
  }

  return true;
}

void gpu_buffer_destroy(gpu_buffer* buffer) {
  if (!buffer->memory) return;
  condemn(buffer->handle, VK_OBJECT_TYPE_BUFFER);
  condemn(buffer->memory, VK_OBJECT_TYPE_DEVICE_MEMORY);
}

// Texture

bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info) {
  VkImageType type;
  VkImageCreateFlags flags = 0;
  switch (info->type) {
    case GPU_TEXTURE_2D: type = VK_IMAGE_TYPE_2D; break;
    case GPU_TEXTURE_3D: type = VK_IMAGE_TYPE_3D; flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT; break;
    case GPU_TEXTURE_CUBE: type = VK_IMAGE_TYPE_2D; flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; break;
    case GPU_TEXTURE_ARRAY: type = VK_IMAGE_TYPE_2D; break;
    default: return false;
  }

  texture->format = convertFormat(info->format, info->srgb);
  texture->array = info->type == GPU_TEXTURE_ARRAY;
  texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;

  if (info->format == GPU_FORMAT_D24S8) {
    texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  } else if (info->format == GPU_FORMAT_D16 || info->format == GPU_FORMAT_D32F) {
    texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else {
    texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  if (info->handle) {
    texture->handle = (VkImage) info->handle;
    return gpu_texture_init_view(texture, &(gpu_texture_view_info) {
      .source = texture,
      .type = info->type
    });
  }

  bool depth = texture->aspect & VK_IMAGE_ASPECT_DEPTH_BIT;
  VkImageCreateInfo imageInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = flags,
    .imageType = type,
    .format = texture->format,
    .extent.width = info->size[0],
    .extent.height = info->size[1],
    .extent.depth = texture->array ? 1 : info->size[2],
    .mipLevels = info->mipmaps,
    .arrayLayers = texture->array ? info->size[2] : 1,
    .samples = info->samples,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage =
      (((info->usage & GPU_TEXTURE_RENDER) && !depth) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0) |
      (((info->usage & GPU_TEXTURE_RENDER) && depth) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0) |
      ((info->usage & GPU_TEXTURE_SAMPLE) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0) |
      ((info->usage & GPU_TEXTURE_STORAGE) ? VK_IMAGE_USAGE_STORAGE_BIT : 0) |
      ((info->usage & GPU_TEXTURE_COPY_SRC) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0) |
      ((info->usage & GPU_TEXTURE_COPY_DST) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0) |
      ((info->usage & GPU_TEXTURE_TRANSIENT) ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0)
  };

  TRY(vkCreateImage(state.device, &imageInfo, NULL, &texture->handle), "Could not create texture") {
    return false;
  }

  nickname(texture->handle, VK_OBJECT_TYPE_IMAGE, info->label);

  uint32_t memoryType = ~0u;
  uint32_t mask = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(state.device, texture->handle, &requirements);
  for (uint32_t i = 0; i < state.memoryTypeCount; i++) {
    if ((state.memoryTypes[i] & mask) != mask || ~requirements.memoryTypeBits & (1 << i)) continue;
    memoryType = i;
    break;
  }

  VkMemoryAllocateInfo memoryInfo = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = requirements.size,
    .memoryTypeIndex = memoryType
  };

  if (memoryType == ~0u || !try(vkAllocateMemory(state.device, &memoryInfo, NULL, &texture->memory), "Could not allocate texture memory")) {
    vkDestroyImage(state.device, texture->handle, NULL);
    return false;
  }

  TRY(vkBindImageMemory(state.device, texture->handle, texture->memory, 0), "Could not bind texture memory") {
    vkDestroyImage(state.device, texture->handle, NULL);
    vkFreeMemory(state.device, texture->memory, NULL);
    return false;
  }

  if (!gpu_texture_init_view(texture, &(gpu_texture_view_info) { .source = texture, .type = info->type })) {
    vkDestroyImage(state.device, texture->handle, NULL);
    vkFreeMemory(state.device, texture->memory, NULL);
    return false;
  }

  return true;
}

bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info) {
  if (texture != info->source) {
    texture->handle = VK_NULL_HANDLE;
    texture->memory = VK_NULL_HANDLE;
    texture->format = info->source->format;
    texture->aspect = info->source->aspect;
    texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;
    texture->array = info->type == GPU_TEXTURE_ARRAY;
  }

  static const VkImageViewType types[] = {
    [GPU_TEXTURE_2D] = VK_IMAGE_VIEW_TYPE_2D,
    [GPU_TEXTURE_3D] = VK_IMAGE_VIEW_TYPE_3D,
    [GPU_TEXTURE_CUBE] = VK_IMAGE_VIEW_TYPE_CUBE,
    [GPU_TEXTURE_ARRAY] = VK_IMAGE_VIEW_TYPE_2D_ARRAY
  };

  VkImageViewCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = info->source->handle,
    .viewType = types[info->type],
    .format = texture->format,
    .subresourceRange = {
      .aspectMask = texture->aspect,
      .baseMipLevel = info ? info->levelIndex : 0,
      .levelCount = (info && info->levelCount) ? info->levelCount : VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = info ? info->layerIndex : 0,
      .layerCount = (info && info->layerCount) ? info->layerCount : VK_REMAINING_ARRAY_LAYERS
    }
  };

  TRY(vkCreateImageView(state.device, &createInfo, NULL, &texture->view), "Could not create texture view") {
    return false;
  }

  return true;
}

void gpu_texture_destroy(gpu_texture* texture) {
  if (texture->memory) condemn(texture->handle, VK_OBJECT_TYPE_IMAGE);
  condemn(texture->memory, VK_OBJECT_TYPE_DEVICE_MEMORY);
  condemn(texture->view, VK_OBJECT_TYPE_IMAGE_VIEW);
}

gpu_texture* gpu_surface_acquire(void) {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0x3];
  TRY(vkAcquireNextImageKHR(state.device, state.swapchain, UINT64_MAX, tick->semaphores[0], VK_NULL_HANDLE, &state.currentBackbuffer), "Surface image acquisition failed") return NULL;
  return &state.backbuffers[state.currentBackbuffer];
}

// Sampler

bool gpu_sampler_init(gpu_sampler* sampler, gpu_sampler_info* info) {
  static const VkFilter filters[] = {
    [GPU_FILTER_NEAREST] = VK_FILTER_NEAREST,
    [GPU_FILTER_LINEAR] = VK_FILTER_LINEAR
  };

  static const VkSamplerMipmapMode mipFilters[] = {
    [GPU_FILTER_NEAREST] = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    [GPU_FILTER_LINEAR] = VK_SAMPLER_MIPMAP_MODE_LINEAR
  };

  static const VkSamplerAddressMode wraps[] = {
    [GPU_WRAP_CLAMP] = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    [GPU_WRAP_REPEAT] = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    [GPU_WRAP_MIRROR] = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
  };

  static const VkCompareOp compareOps[] = {
    [GPU_COMPARE_NONE] = VK_COMPARE_OP_ALWAYS,
    [GPU_COMPARE_EQUAL] = VK_COMPARE_OP_EQUAL,
    [GPU_COMPARE_NEQUAL] = VK_COMPARE_OP_NOT_EQUAL,
    [GPU_COMPARE_LESS] = VK_COMPARE_OP_LESS,
    [GPU_COMPARE_LEQUAL] = VK_COMPARE_OP_LESS_OR_EQUAL,
    [GPU_COMPARE_GREATER] = VK_COMPARE_OP_GREATER,
    [GPU_COMPARE_GEQUAL] = VK_COMPARE_OP_GREATER_OR_EQUAL
  };

  VkSamplerCreateInfo samplerInfo = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = filters[info->mag],
    .minFilter = filters[info->min],
    .mipmapMode = mipFilters[info->mip],
    .addressModeU = wraps[info->wrap[0]],
    .addressModeV = wraps[info->wrap[1]],
    .addressModeW = wraps[info->wrap[2]],
    .anisotropyEnable = info->anisotropy >= 1.f,
    .maxAnisotropy = info->anisotropy,
    .compareEnable = info->compare != GPU_COMPARE_NONE,
    .compareOp = compareOps[info->compare],
    .minLod = info->lodClamp[0],
    .maxLod = info->lodClamp[1] < 0.f ? VK_LOD_CLAMP_NONE : info->lodClamp[1]
  };

  TRY(vkCreateSampler(state.device, &samplerInfo, NULL, &sampler->handle), "Could not create sampler") {
    return false;
  }

  return true;
}

void gpu_sampler_destroy(gpu_sampler* sampler) {
  condemn(sampler->handle, VK_OBJECT_TYPE_SAMPLER);
}

// Shader

bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info) {
  shader->type = info->stages[1].code ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE; // TODO

  VkShaderStageFlags stageFlags[][2] = {
    [VK_PIPELINE_BIND_POINT_GRAPHICS] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT },
    [VK_PIPELINE_BIND_POINT_COMPUTE] = { VK_SHADER_STAGE_COMPUTE_BIT },
  };

  for (uint32_t i = 0; i < COUNTOF(info->stages) && info->stages[i].code; i++) {
    VkShaderModuleCreateInfo moduleInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = info->stages[i].size,
      .pCode = info->stages[i].code
    };

    TRY(vkCreateShaderModule(state.device, &moduleInfo, NULL, &shader->handles[i]), "Failed to load shader") {
      return false;
    }

    shader->pipelineInfo[i] = (VkPipelineShaderStageCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = stageFlags[shader->type][i],
      .module = shader->handles[i],
      .pName = info->stages[i].entry ? info->stages[i].entry : "main"
    };
  }

  static const VkDescriptorType descriptorTypes[] = {
    [GPU_SLOT_UNIFORM_BUFFER] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [GPU_SLOT_STORAGE_BUFFER] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    [GPU_SLOT_UNIFORM_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    [GPU_SLOT_STORAGE_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    [GPU_SLOT_SAMPLED_TEXTURE] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [GPU_SLOT_STORAGE_TEXTURE] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
  };

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pSetLayouts = shader->layouts
  };

  for (uint32_t i = 0; i < COUNTOF(shader->layouts); i++) {
    VkDescriptorSetLayoutBinding bindings[16];
    for (uint32_t j = 0; j < info->slotCount[i]; j++) {
      gpu_slot slot = info->slots[i][j];
      bindings[j] = (VkDescriptorSetLayoutBinding) {
        .binding = slot.id,
        .descriptorType = descriptorTypes[slot.type],
        .descriptorCount = slot.count,
        .stageFlags = slot.stage == GPU_STAGE_ALL ? VK_SHADER_STAGE_ALL :
          (((slot.stage & GPU_STAGE_VERTEX) ? VK_SHADER_STAGE_VERTEX_BIT : 0) |
          ((slot.stage & GPU_STAGE_FRAGMENT) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0) |
          ((slot.stage & GPU_STAGE_COMPUTE) ? VK_SHADER_STAGE_COMPUTE_BIT : 0))
      };
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = info->slotCount[i],
      .pBindings = bindings
    };

    TRY(vkCreateDescriptorSetLayout(state.device, &layoutInfo, NULL, &shader->layouts[i]), "Failed to create descriptor set layout") {
      gpu_shader_destroy(shader);
      return false;
    }

    if (info->slotCount[i] > 0) {
      pipelineLayoutInfo.setLayoutCount = i + 1;
    }
  }

  TRY(vkCreatePipelineLayout(state.device, &pipelineLayoutInfo, NULL, &shader->pipelineLayout), "Failed to create pipeline layout") {
    gpu_shader_destroy(shader);
    return false;
  }

  return true;
}

void gpu_shader_destroy(gpu_shader* shader) {
  // The spec says it's safe to destroy shaders while still in use
  if (shader->handles[0]) vkDestroyShaderModule(state.device, shader->handles[0], NULL);
  if (shader->handles[1]) vkDestroyShaderModule(state.device, shader->handles[1], NULL);
  if (shader->layouts[0]) vkDestroyDescriptorSetLayout(state.device, shader->layouts[0], NULL);
  if (shader->layouts[1]) vkDestroyDescriptorSetLayout(state.device, shader->layouts[1], NULL);
  if (shader->layouts[2]) vkDestroyDescriptorSetLayout(state.device, shader->layouts[2], NULL);
  if (shader->layouts[3]) vkDestroyDescriptorSetLayout(state.device, shader->layouts[3], NULL);
  if (shader->pipelineLayout) vkDestroyPipelineLayout(state.device, shader->pipelineLayout, NULL);
}

// Bundle

bool gpu_bundle_init(gpu_bundle* bundle, gpu_bundle_info* info) {
  bundle->handle = VK_NULL_HANDLE;

  VkDescriptorSetAllocateInfo allocateInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = VK_NULL_HANDLE,
    .descriptorSetCount = 1,
    .pSetLayouts = &info->shader->layouts[info->group]
  };

  // Try to allocate bindings from the current pool
  gpu_binding_pool* pool = &state.bindings;
  gpu_binding_puddle* puddle = NULL;

  if (pool->head != ~0u) {
    puddle = &pool->data[pool->head];
    allocateInfo.descriptorPool = puddle->handle;
    VkResult result = vkAllocateDescriptorSets(state.device, &allocateInfo, &bundle->handle);
    if (result != VK_ERROR_OUT_OF_POOL_MEMORY && !try(result, "Could not allocate bundle")) {
      return false;
    }
  }

  // If that didn't work, there either wasn't a puddle available or the current one overflowed.
  if (!bundle->handle) {
    uint32_t index = ~0u;

    // Make sure we have an up to date GPU tick before trying to reuse a puddle
    ketchup();

    // If there's a crusty old puddle laying around that isn't even being used by anyone, use it
    if (pool->tail != ~0u && state.tick[GPU] >= pool->data[pool->tail].tick) {
      index = pool->tail;
      puddle = &pool->data[index];
      pool->tail = puddle->next;
      vkResetDescriptorPool(state.device, puddle->handle, 0);
    } else {
      CHECK(pool->count < COUNTOF(pool->data), "GPU binding pool overflow") return false;
      index = pool->count++;
      puddle = &pool->data[index];

      VkDescriptorPoolSize sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 * 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 * 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 * 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2 * 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 2 * 1024 }
      };

      VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1024,
        .poolSizeCount = COUNTOF(sizes),
        .pPoolSizes = sizes
      };

      TRY(vkCreateDescriptorPool(state.device, &descriptorPoolInfo, NULL, &puddle->handle), "Could not create descriptor pool") {
        return false;
      }
    }

    // If there's no tail, this pool is the tail now
    if (pool->tail == ~0u) {
      pool->tail = index;
    }

    // This puddle is the new head, adjust old head
    if (pool->head != ~0u) {
      pool->data[pool->head].next = index;
    }

    pool->head = index;
    puddle->next = ~0u;

    allocateInfo.descriptorPool = puddle->handle;
    TRY(vkAllocateDescriptorSets(state.device, &allocateInfo, &bundle->handle), "Could not allocate bundle") {
      return false;
    }
  }

  puddle->tick = state.tick[CPU];

  static const VkDescriptorType descriptorTypes[] = {
    [GPU_SLOT_UNIFORM_BUFFER] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [GPU_SLOT_STORAGE_BUFFER] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    [GPU_SLOT_UNIFORM_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    [GPU_SLOT_STORAGE_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    [GPU_SLOT_SAMPLED_TEXTURE] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [GPU_SLOT_STORAGE_TEXTURE] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
  };

  gpu_binding* binding = info->bindings;
  VkDescriptorBufferInfo buffers[64];
  VkDescriptorImageInfo textures[64];
  VkWriteDescriptorSet writes[16];
  uint32_t bufferCount = 0;
  uint32_t textureCount = 0;
  uint32_t writeCount = 0;

  for (uint32_t i = 0; i < info->slotCount; i++) {
    gpu_slot slot = info->slots[i];
    bool texture = slot.type == GPU_SLOT_SAMPLED_TEXTURE || slot.type == GPU_SLOT_STORAGE_TEXTURE;
    uint32_t cursor = 0;

    while (cursor < slot.count) {
      if (texture ? textureCount >= COUNTOF(textures) : bufferCount >= COUNTOF(buffers)) {
        vkUpdateDescriptorSets(state.device, writeCount, writes, 0, NULL);
        bufferCount = textureCount = writeCount = 0;
      }

      uint32_t available = texture ? COUNTOF(textures) - textureCount : COUNTOF(buffers) - bufferCount;
      uint32_t remaining = slot.count - cursor;
      uint32_t chunk = MIN(available, remaining);

      writes[writeCount++] = (VkWriteDescriptorSet) {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = bundle->handle,
        .dstBinding = slot.id,
        .dstArrayElement = cursor,
        .descriptorCount = chunk,
        .descriptorType = descriptorTypes[slot.type],
        .pBufferInfo = &buffers[bufferCount],
        .pImageInfo = &textures[textureCount]
      };

      if (texture) {
        for (uint32_t j = 0; j < chunk; j++, binding++) {
          textures[textureCount++] = (VkDescriptorImageInfo) {
            .sampler = binding->texture.sampler->handle,
            .imageView = binding->texture.object->view,
            .imageLayout = binding->texture.object->layout
          };
        }
      } else {
        for (uint32_t j = 0; j < chunk; j++, binding++) {
          buffers[bufferCount++] = (VkDescriptorBufferInfo) {
            .buffer = binding->buffer.object->handle,
            .offset = binding->buffer.offset,
            .range = binding->buffer.extent
          };
        }
      }

      cursor += chunk;
    }
  }

  if (writeCount > 0) {
    vkUpdateDescriptorSets(state.device, writeCount, writes, 0, NULL);
  }

  return true;
}

void gpu_bundle_destroy(gpu_bundle* bundle) {
  //
}

// Pass

bool gpu_pass_init(gpu_pass* pass, gpu_pass_info* info) {
  static const VkAttachmentLoadOp loadOps[] = {
    [GPU_LOAD_OP_LOAD] = VK_ATTACHMENT_LOAD_OP_LOAD,
    [GPU_LOAD_OP_CLEAR] = VK_ATTACHMENT_LOAD_OP_CLEAR,
    [GPU_LOAD_OP_DISCARD] = VK_ATTACHMENT_LOAD_OP_DONT_CARE
  };

  static const VkAttachmentStoreOp storeOps[] = {
    [GPU_SAVE_OP_SAVE] = VK_ATTACHMENT_STORE_OP_STORE,
    [GPU_SAVE_OP_DISCARD] = VK_ATTACHMENT_STORE_OP_DONT_CARE
  };

  VkAttachmentDescription attachments[9];
  VkAttachmentReference references[9];

  for (uint32_t i = 0; i < info->count; i++) {
    references[i].attachment = i;
    references[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[i] = (VkAttachmentDescription) {
      .format = info->color[i].format == ~0u ? state.backbuffers[0].format : convertFormat(info->color[i].format, info->color[i].srgb),
      .samples = info->samples,
      .loadOp = loadOps[info->color[i].load],
      .storeOp = info->resolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : storeOps[info->color[i].save],
      .initialLayout = info->color[i].load == GPU_LOAD_OP_LOAD ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = info->color[i].format == ~0u ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
  }

  if (info->resolve) {
    for (uint32_t i = 0; i < info->count; i++) {
      uint32_t index = info->count + i;
      references[index].attachment = index;
      references[index].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      attachments[index] = (VkAttachmentDescription) {
        .format = info->color[i].format == ~0u ? state.backbuffers[0].format : convertFormat(info->color[i].format, info->color[i].srgb),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = storeOps[info->color[i].save],
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = info->color[i].format == ~0u ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
      };
    }
  }

  if (info->depth.format) { // You'll never catch me!
    uint32_t index = info->count << info->resolve;
    references[index].attachment = index;
    references[index].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[index] = (VkAttachmentDescription) {
      .format = convertFormat(info->depth.format, LINEAR),
      .samples = info->samples,
      .loadOp = loadOps[info->depth.load],
      .storeOp = storeOps[info->depth.save],
      .stencilLoadOp = loadOps[info->depth.stencilLoad],
      .stencilStoreOp = storeOps[info->depth.stencilSave],
      .initialLayout = info->depth.load == GPU_LOAD_OP_LOAD ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };
  }

  VkSubpassDescription subpass = {
    .colorAttachmentCount = info->count,
    .pColorAttachments = references + 0,
    .pResolveAttachments = info->resolve ? references + info->count : NULL,
    .pDepthStencilAttachment = info->depth.format ? references + (info->count << info->resolve) : NULL
  };

  VkRenderPassMultiviewCreateInfo multiview = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
    .subpassCount = 1,
    .pViewMasks = (uint32_t[1]) { (1 << info->views) - 1 }
  };

  VkRenderPassCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .pNext = info->views > 0 ? &multiview : NULL,
    .attachmentCount = (info->count << info->resolve) + !!info->depth.format,
    .pAttachments = attachments,
    .subpassCount = 1,
    .pSubpasses = &subpass
  };

  TRY(vkCreateRenderPass(state.device, &createInfo, NULL, &pass->handle), "Could not create render pass") {
    return false;
  }

  nickname(pass->handle, VK_OBJECT_TYPE_RENDER_PASS, info->label);
  pass->samples = info->samples;
  pass->count = info->count;
  return true;
}

void gpu_pass_destroy(gpu_pass* pass) {
  condemn(pass->handle, VK_OBJECT_TYPE_RENDER_PASS);
}

// Pipeline

bool gpu_pipeline_init_graphics(gpu_pipeline* pipeline, gpu_pipeline_info* info) {
  static const VkPrimitiveTopology topologies[] = {
    [GPU_DRAW_POINTS] = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    [GPU_DRAW_LINES] = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    [GPU_DRAW_TRIANGLES] = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
  };

  static const VkFormat vertexFormats[] = {
    [GPU_FORMAT_I8] = VK_FORMAT_R8_SINT,
    [GPU_FORMAT_U8] = VK_FORMAT_R8_UINT,
    [GPU_FORMAT_I16] = VK_FORMAT_R16_SINT,
    [GPU_FORMAT_U16] = VK_FORMAT_R16_UINT,
    [GPU_FORMAT_I32] = VK_FORMAT_R32_SINT,
    [GPU_FORMAT_U32] = VK_FORMAT_R32_UINT,
    [GPU_FORMAT_F32] = VK_FORMAT_R32_SFLOAT,
    [GPU_FORMAT_I8x2] = VK_FORMAT_R8G8_SINT,
    [GPU_FORMAT_U8x2] = VK_FORMAT_R8G8_UINT,
    [GPU_FORMAT_I8Nx2] = VK_FORMAT_R8G8_SNORM,
    [GPU_FORMAT_U8Nx2] = VK_FORMAT_R8G8_UNORM,
    [GPU_FORMAT_I16x2] = VK_FORMAT_R16G16_SINT,
    [GPU_FORMAT_U16x2] = VK_FORMAT_R16G16_UINT,
    [GPU_FORMAT_I16Nx2] = VK_FORMAT_R16G16_SNORM,
    [GPU_FORMAT_U16Nx2] = VK_FORMAT_R16G16_UNORM,
    [GPU_FORMAT_I32x2] = VK_FORMAT_R32G32_SINT,
    [GPU_FORMAT_U32x2] = VK_FORMAT_R32G32_UINT,
    [GPU_FORMAT_F32x2] = VK_FORMAT_R32G32_SFLOAT,
    [GPU_FORMAT_I32x3] = VK_FORMAT_R32G32B32_SINT,
    [GPU_FORMAT_U32x3] = VK_FORMAT_R32G32B32_UINT,
    [GPU_FORMAT_F32x3] = VK_FORMAT_R32G32B32_SFLOAT,
    [GPU_FORMAT_I8x4] = VK_FORMAT_R8G8B8A8_SINT,
    [GPU_FORMAT_U8x4] = VK_FORMAT_R8G8B8A8_UINT,
    [GPU_FORMAT_I8Nx4] = VK_FORMAT_R8G8B8A8_SNORM,
    [GPU_FORMAT_U8Nx4] = VK_FORMAT_R8G8B8A8_UNORM,
    [GPU_FORMAT_I16x4] = VK_FORMAT_R16G16B16A16_SINT,
    [GPU_FORMAT_U16x4] = VK_FORMAT_R16G16B16A16_UINT,
    [GPU_FORMAT_I16Nx4] = VK_FORMAT_R16G16B16A16_SNORM,
    [GPU_FORMAT_U16Nx4] = VK_FORMAT_R16G16B16A16_UNORM,
    [GPU_FORMAT_I32x4] = VK_FORMAT_R32G32B32A32_SINT,
    [GPU_FORMAT_U32x4] = VK_FORMAT_R32G32B32A32_UINT,
    [GPU_FORMAT_F32x4] = VK_FORMAT_R32G32B32A32_SFLOAT
  };

  static const VkCullModeFlagBits cullModes[] = {
    [GPU_CULL_NONE] = VK_CULL_MODE_NONE,
    [GPU_CULL_FRONT] = VK_CULL_MODE_FRONT_BIT,
    [GPU_CULL_BACK] = VK_CULL_MODE_BACK_BIT
  };

  static const VkFrontFace frontFaces[] = {
    [GPU_WINDING_CCW] = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    [GPU_WINDING_CW] = VK_FRONT_FACE_CLOCKWISE
  };

  static const VkCompareOp compareOps[] = {
    [GPU_COMPARE_NONE] = VK_COMPARE_OP_ALWAYS,
    [GPU_COMPARE_EQUAL] = VK_COMPARE_OP_EQUAL,
    [GPU_COMPARE_NEQUAL] = VK_COMPARE_OP_NOT_EQUAL,
    [GPU_COMPARE_LESS] = VK_COMPARE_OP_LESS,
    [GPU_COMPARE_LEQUAL] = VK_COMPARE_OP_LESS_OR_EQUAL,
    [GPU_COMPARE_GREATER] = VK_COMPARE_OP_GREATER,
    [GPU_COMPARE_GEQUAL] = VK_COMPARE_OP_GREATER_OR_EQUAL
  };

  static const VkStencilOp stencilOps[] = {
    [GPU_STENCIL_KEEP] = VK_STENCIL_OP_KEEP,
    [GPU_STENCIL_REPLACE] = VK_STENCIL_OP_REPLACE,
    [GPU_STENCIL_INCREMENT] = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
    [GPU_STENCIL_DECREMENT] = VK_STENCIL_OP_DECREMENT_AND_CLAMP,
    [GPU_STENCIL_INCREMENT_WRAP] = VK_STENCIL_OP_INCREMENT_AND_WRAP,
    [GPU_STENCIL_DECREMENT_WRAP] = VK_STENCIL_OP_DECREMENT_AND_WRAP,
    [GPU_STENCIL_INVERT] = VK_STENCIL_OP_INVERT
  };

  static const VkBlendFactor blendFactors[] = {
    [GPU_BLEND_ZERO] = VK_BLEND_FACTOR_ZERO,
    [GPU_BLEND_ONE] = VK_BLEND_FACTOR_ONE,
    [GPU_BLEND_SRC_COLOR] = VK_BLEND_FACTOR_SRC_COLOR,
    [GPU_BLEND_ONE_MINUS_SRC_COLOR] = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    [GPU_BLEND_SRC_ALPHA] = VK_BLEND_FACTOR_SRC_ALPHA,
    [GPU_BLEND_ONE_MINUS_SRC_ALPHA] = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    [GPU_BLEND_DST_COLOR] = VK_BLEND_FACTOR_DST_COLOR,
    [GPU_BLEND_ONE_MINUS_DST_COLOR] = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    [GPU_BLEND_DST_ALPHA] = VK_BLEND_FACTOR_DST_ALPHA,
    [GPU_BLEND_ONE_MINUS_DST_ALPHA] = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA
  };

  static const VkBlendOp blendOps[] = {
    [GPU_BLEND_ADD] = VK_BLEND_OP_ADD,
    [GPU_BLEND_SUB] = VK_BLEND_OP_SUBTRACT,
    [GPU_BLEND_RSUB] = VK_BLEND_OP_REVERSE_SUBTRACT,
    [GPU_BLEND_MIN] = VK_BLEND_OP_MIN,
    [GPU_BLEND_MAX] = VK_BLEND_OP_MAX
  };

  VkVertexInputBindingDescription vertexBuffers[16];
  for (uint32_t i = 0; i < info->vertexBufferCount; i++) {
    vertexBuffers[i] = (VkVertexInputBindingDescription) {
      .binding = i,
      .stride = info->bufferStrides[i],
      .inputRate = (info->instancedBufferMask & (1 << i)) ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX
    };
  }

  VkVertexInputAttributeDescription vertexAttributes[COUNTOF(info->attributes)];
  for (uint32_t i = 0; i < info->attributeCount; i++) {
    vertexAttributes[i] = (VkVertexInputAttributeDescription) {
      .location = info->attributes[i].location,
      .binding = info->attributes[i].buffer,
      .format = vertexFormats[info->attributes[i].format],
      .offset = info->attributes[i].offset
    };
  }

  VkPipelineVertexInputStateCreateInfo vertexInput = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = info->vertexBufferCount,
    .pVertexBindingDescriptions = vertexBuffers,
    .vertexAttributeDescriptionCount = info->attributeCount,
    .pVertexAttributeDescriptions = vertexAttributes
  };

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = topologies[info->drawMode]
  };

  VkPipelineViewportStateCreateInfo viewport = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1
  };

  VkPipelineRasterizationStateCreateInfo rasterization = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode = info->rasterizer.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
    .cullMode = cullModes[info->rasterizer.cullMode],
    .frontFace = frontFaces[info->rasterizer.winding],
    .depthBiasEnable = info->rasterizer.depthOffset != 0.f || info->rasterizer.depthOffsetSloped != 0.f,
    .depthBiasConstantFactor = info->rasterizer.depthOffset,
    .depthBiasSlopeFactor = info->rasterizer.depthOffsetSloped,
    .depthBiasClamp = info->rasterizer.depthOffsetClamp,
    .lineWidth = 1.f
  };

  VkPipelineMultisampleStateCreateInfo multisample = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = info->pass->samples,
    .alphaToCoverageEnable = info->alphaToCoverage
  };

  VkStencilOpState stencil = {
    .failOp = stencilOps[info->stencil.failOp],
    .passOp = stencilOps[info->stencil.passOp],
    .depthFailOp = stencilOps[info->stencil.depthFailOp],
    .compareOp = compareOps[info->stencil.test],
    .compareMask = info->stencil.readMask,
    .writeMask = info->stencil.writeMask,
    .reference = info->stencil.value
  };

  VkPipelineDepthStencilStateCreateInfo depthStencil = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = info->depth.test != GPU_COMPARE_NONE,
    .depthWriteEnable = info->depth.write,
    .depthCompareOp = compareOps[info->depth.test],
    .stencilTestEnable = info->stencil.test != GPU_COMPARE_NONE,
    .front = stencil,
    .back = stencil
  };

  VkPipelineColorBlendAttachmentState colorAttachments[4];
  for (uint32_t i = 0; i < info->pass->count; i++) {
    colorAttachments[i] = (VkPipelineColorBlendAttachmentState) {
      .blendEnable = info->blend[i].enabled,
      .srcColorBlendFactor = blendFactors[info->blend[i].color.src],
      .dstColorBlendFactor = blendFactors[info->blend[i].color.dst],
      .colorBlendOp = blendOps[info->blend[i].color.op],
      .srcAlphaBlendFactor = blendFactors[info->blend[i].alpha.src],
      .dstAlphaBlendFactor = blendFactors[info->blend[i].alpha.dst],
      .alphaBlendOp = blendOps[info->blend[i].alpha.op],
      .colorWriteMask = info->colorMask[i]
    };
  }

  VkPipelineColorBlendStateCreateInfo colorBlend = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = info->pass->count,
    .pAttachments = colorAttachments
  };

  VkDynamicState dynamicStates[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dynamicState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = COUNTOF(dynamicStates),
    .pDynamicStates = dynamicStates
  };

  VkGraphicsPipelineCreateInfo pipelineInfo = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2,
    .pStages = info->shader->pipelineInfo,
    .pVertexInputState = &vertexInput,
    .pInputAssemblyState = &inputAssembly,
    .pViewportState = &viewport,
    .pRasterizationState = &rasterization,
    .pMultisampleState = &multisample,
    .pDepthStencilState = &depthStencil,
    .pColorBlendState = &colorBlend,
    .pDynamicState = &dynamicState,
    .layout = info->shader->pipelineLayout,
    .renderPass = info->pass->handle
  };

  TRY(vkCreateGraphicsPipelines(state.device, state.pipelineCache, 1, &pipelineInfo, NULL, &pipeline->handle), "Could not create pipeline") {
    return false;
  }

  nickname(pipeline->handle, VK_OBJECT_TYPE_PIPELINE, info->label);
  pipeline->type = VK_PIPELINE_BIND_POINT_GRAPHICS;
  return true;
}

bool gpu_pipeline_init_compute(gpu_pipeline* pipeline, gpu_shader* shader, const char* label) {
  VkComputePipelineCreateInfo pipelineInfo = {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage = shader->pipelineInfo[0],
    .layout = shader->pipelineLayout
  };

  TRY(vkCreateComputePipelines(state.device, state.pipelineCache, 1, &pipelineInfo, NULL, &pipeline->handle), "Could not create compute pipeline") {
    return false;
  }

  nickname(pipeline->handle, VK_OBJECT_TYPE_PIPELINE, label);
  pipeline->type = VK_PIPELINE_BIND_POINT_COMPUTE;
  return true;
}

void gpu_pipeline_destroy(gpu_pipeline* pipeline) {
  condemn(pipeline->handle, VK_OBJECT_TYPE_PIPELINE);
}

// Stream

gpu_stream* gpu_stream_begin() {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0x3];
  CHECK(state.streamCount < COUNTOF(tick->streams), "Too many streams") return NULL;
  gpu_stream* stream = &tick->streams[state.streamCount];

  VkCommandBufferBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };

  TRY(vkBeginCommandBuffer(stream->commands, &beginfo), "Failed to begin stream") return NULL;
  state.streamCount++;
  return stream;
}

void gpu_stream_end(gpu_stream* stream) {
  TRY(vkEndCommandBuffer(stream->commands), "Failed to end stream") return;
}

void gpu_render_begin(gpu_stream* stream, gpu_canvas* canvas) {
  bool resolve = canvas->pass->samples > 1 && canvas->color[0].resolve;

  // Framebuffer
  VkClearValue clears[9];
  VkImageView textures[9];
  uint32_t count = 0;

  for (uint32_t i = 0; i < COUNTOF(canvas->color) && canvas->color[i].texture; i++, count++) {
    for (uint32_t j = 0; j < 4; j++) {
      clears[i].color.float32[j] = canvas->color[i].clear[j];
    }
    textures[i] = canvas->color[i].texture->view;
  }

  if (resolve) {
    for (uint32_t i = 0; i < count; i++) {
      textures[count + i] = canvas->color[i].resolve->view;
    }
    count <<= 1;
  }

  if (canvas->depth.texture) {
    uint32_t index = count++;
    clears[index].depthStencil.depth = canvas->depth.clear.depth;
    clears[index].depthStencil.stencil = canvas->depth.clear.stencil;
    textures[index] = canvas->depth.texture->view;
  }

  uint32_t hash = HASH_SEED;
  hash32(&hash, textures, count);
  hash32(&hash, &canvas->pass->handle, sizeof(canvas->pass->handle));
  hash32(&hash, canvas->size, 2 * sizeof(uint32_t));

  // Search for matching framebuffer in cache table
  VkFramebuffer framebuffer;
  uint32_t rows = COUNTOF(state.framebuffers);
  uint32_t cols = COUNTOF(state.framebuffers[0]);
  gpu_cache_entry* row = state.framebuffers[0] + (hash & (rows - 1)) * cols;
  gpu_cache_entry* entry = NULL;
  for (uint32_t i = 0; i < cols; i++) {
    if (row[i].hash == hash) {
      entry = &row[i];
      break;
    }
  }

  // If no match was found, create new framebuffer, add to an empty slot, evicting oldest if needed
  if (!entry) {
    VkFramebufferCreateInfo framebufferInfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = canvas->pass->handle,
      .attachmentCount = count,
      .pAttachments = textures,
      .width = canvas->size[0],
      .height = canvas->size[1],
      .layers = 1
    };

    entry = &row[0];
    for (uint32_t i = 1; i < cols; i++) {
      if (!row[i].object || row[i].tick < entry->tick) {
        entry = &row[i];
        break;
      }
    }

    TRY(vkCreateFramebuffer(state.device, &framebufferInfo, NULL, &framebuffer), "Failed to create framebuffer") return;
    condemn(entry->object, VK_OBJECT_TYPE_FRAMEBUFFER);
    entry->object = framebuffer;
    entry->hash = hash;
  }

  framebuffer = entry->object;
  entry->tick = state.tick[CPU];

  VkRenderPassBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = canvas->pass->handle,
    .framebuffer = framebuffer,
    .renderArea = { { 0, 0 }, { canvas->size[0], canvas->size[1] } },
    .clearValueCount = count,
    .pClearValues = clears
  };

  vkCmdBeginRenderPass(stream->commands, &beginfo, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdSetViewport(stream->commands, 0, 1, (VkViewport[]) { { 0.f, 0.f, canvas->size[0], canvas->size[1], 0.f, 1.f } });
  vkCmdSetScissor(stream->commands, 0, 1, &beginfo.renderArea);
}

void gpu_render_end(gpu_stream* stream) {
  vkCmdEndRenderPass(stream->commands);
}

void gpu_compute_begin(gpu_stream* stream) {
  //
}

void gpu_compute_end(gpu_stream* stream) {
  //
}

void gpu_bind_pipeline(gpu_stream* stream, gpu_pipeline* pipeline) {
  vkCmdBindPipeline(stream->commands, pipeline->type, pipeline->handle);
}

void gpu_bind_bundle(gpu_stream* stream, gpu_shader* shader, uint32_t group, gpu_bundle* bundle, uint32_t* offsets, uint32_t offsetCount) {
  vkCmdBindDescriptorSets(stream->commands, shader->type, shader->pipelineLayout, group, 1, &bundle->handle, offsetCount, offsets);
}

void gpu_bind_vertex_buffers(gpu_stream* stream, gpu_buffer** buffers, uint32_t* offsets, uint32_t first, uint32_t count) {
  VkBuffer handles[COUNTOF(((gpu_pipeline_info*) NULL)->bufferStrides)];
  uint64_t offsets64[COUNTOF(handles)];
  for (uint32_t i = 0; i < count; i++) {
    handles[i] = buffers[i]->handle;
    offsets64[i] = offsets[i];
  }
  vkCmdBindVertexBuffers(stream->commands, first, count, handles, offsets64);
}

void gpu_bind_index_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, gpu_index_type type) {
  vkCmdBindIndexBuffer(stream->commands, buffer->handle, offset, (VkIndexType) type);
}

void gpu_draw(gpu_stream* stream, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) {
  vkCmdDraw(stream->commands, vertexCount, instanceCount, firstVertex, 0);
}

void gpu_draw_indexed(gpu_stream* stream, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex) {
  vkCmdDrawIndexed(stream->commands, indexCount, instanceCount, firstIndex, baseVertex, 0);
}

void gpu_draw_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount) {
  vkCmdDrawIndirect(stream->commands, buffer->handle, offset, drawCount, 16);
}

void gpu_draw_indirect_indexed(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount) {
  vkCmdDrawIndexedIndirect(stream->commands, buffer->handle, offset, drawCount, 20);
}

void gpu_compute(gpu_stream* stream, uint32_t x, uint32_t y, uint32_t z) {
  vkCmdDispatch(stream->commands, x, y, z);
}

void gpu_compute_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset) {
  vkCmdDispatchIndirect(stream->commands, buffer->handle, offset);
}

void gpu_copy_buffers(gpu_stream* stream, gpu_buffer* src, gpu_buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size) {
  vkCmdCopyBuffer(stream->commands, src->handle, dst->handle, 1, &(VkBufferCopy) {
    .srcOffset = srcOffset,
    .dstOffset = dstOffset,
    .size = size
  });
}

void gpu_copy_textures(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t size[3]) {
  vkCmdCopyImage(stream->commands, src->handle, src->layout, dst->handle, dst->layout, 1, &(VkImageCopy) {
    .srcSubresource = {
      .aspectMask = src->aspect,
      .mipLevel = srcOffset[3],
      .baseArrayLayer = src->array ? srcOffset[2] : 0,
      .layerCount = src->array ? size[2] : 1
    },
    .dstSubresource = {
      .aspectMask = dst->aspect,
      .mipLevel = dstOffset[3],
      .baseArrayLayer = dst->array ? dstOffset[2] : 0,
      .layerCount = dst->array ? size[2] : 1
    },
    .srcOffset = { srcOffset[0], srcOffset[1], src->array ? 0 : srcOffset[2] },
    .dstOffset = { dstOffset[0], dstOffset[1], dst->array ? 0 : dstOffset[2] },
    .extent = { size[0], size[1], size[2] }
  });
}

void gpu_copy_buffer_texture(gpu_stream* stream, gpu_buffer* src, gpu_texture* dst, uint32_t srcOffset, uint16_t dstOffset[4], uint16_t extent[3]) {
  VkBufferImageCopy region = {
    .bufferOffset = srcOffset,
    .imageSubresource.aspectMask = dst->aspect,
    .imageSubresource.mipLevel = dstOffset[3],
    .imageSubresource.baseArrayLayer = dst->array ? dstOffset[2] : 0,
    .imageSubresource.layerCount = dst->array ? extent[2] : 1,
    .imageOffset = { dstOffset[0], dstOffset[1], dst->array ? 0 : dstOffset[2] },
    .imageExtent = { extent[0], extent[1], dst->array ? 1 : extent[2] }
  };

  vkCmdCopyBufferToImage(stream->commands, src->handle, dst->handle, dst->layout, 1, &region);
}

void gpu_copy_texture_buffer(gpu_stream* stream, gpu_texture* src, gpu_buffer* dst, uint16_t srcOffset[4], uint32_t dstOffset, uint16_t extent[3]) {
  VkBufferImageCopy region = {
    .bufferOffset = dstOffset,
    .imageSubresource.aspectMask = src->aspect,
    .imageSubresource.mipLevel = srcOffset[3],
    .imageSubresource.baseArrayLayer = src->array ? srcOffset[2] : 0,
    .imageSubresource.layerCount = src->array ? extent[2] : 1,
    .imageOffset = { srcOffset[0], srcOffset[1], src->array ? 0 : srcOffset[2] },
    .imageExtent = { extent[0], extent[1], src->array ? 1 : extent[2] }
  };

  vkCmdCopyImageToBuffer(stream->commands, src->handle, src->layout, dst->handle, 1, &region);
}

void gpu_clear_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t size, uint32_t value) {
  vkCmdFillBuffer(stream->commands, buffer->handle, offset, size, value);
}

void gpu_clear_texture(gpu_stream* stream, gpu_texture* texture, uint16_t layer, uint16_t layerCount, uint16_t level, uint16_t levelCount, float color[4]) {
  VkClearColorValue clear = { .float32 = { color[0], color[1], color[2], color[3] } };

  VkImageSubresourceRange range = {
    .aspectMask = texture->aspect,
    .baseMipLevel = level,
    .levelCount = levelCount,
    .baseArrayLayer = layer,
    .layerCount = layerCount
  };

  vkCmdClearColorImage(stream->commands, texture->handle, texture->layout, &clear, 1, &range);
}

void gpu_blit(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t srcExtent[3], uint16_t dstExtent[3], gpu_filter filter) {
  VkImageBlit region = {
    .srcSubresource = {
      .aspectMask = src->aspect,
      .mipLevel = srcOffset[3],
      .baseArrayLayer = src->array ? srcOffset[2] : 0,
      .layerCount = src->array ? srcExtent[2] : 1
    },
    .dstSubresource = {
      .aspectMask = dst->aspect,
      .mipLevel = dstOffset[3],
      .baseArrayLayer = dst->array ? dstOffset[2] : 0,
      .layerCount = dst->array ? dstExtent[2] : 1
    },
    .srcOffsets[0] = { srcOffset[0], srcOffset[1], src->array ? 0 : srcOffset[2] },
    .dstOffsets[0] = { dstOffset[0], dstOffset[1], dst->array ? 0 : dstOffset[2] },
    .srcOffsets[1] = { srcOffset[0] + srcExtent[0], srcOffset[1] + srcExtent[1], src->array ? 1 : srcOffset[2] + srcExtent[2] },
    .dstOffsets[1] = { dstOffset[0] + dstExtent[0], dstOffset[1] + dstExtent[1], dst->array ? 1 : dstOffset[2] + dstExtent[2] }
  };

  static const VkFilter filters[] = {
    [GPU_FILTER_NEAREST] = VK_FILTER_NEAREST,
    [GPU_FILTER_LINEAR] = VK_FILTER_LINEAR
  };

  vkCmdBlitImage(stream->commands, src->handle, src->layout, dst->handle, dst->layout, 1, &region, filters[filter]);
}

void gpu_sync(gpu_stream* stream) {
  //
}

void gpu_label_push(gpu_stream* stream, const char* label) {
  if (state.config.debug) {
    vkCmdBeginDebugUtilsLabelEXT(stream->commands, &(VkDebugUtilsLabelEXT) {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pLabelName = label
    });
  }
}

void gpu_label_pop(gpu_stream* stream) {
  if (state.config.debug) {
    vkCmdEndDebugUtilsLabelEXT(stream->commands);
  }
}

void gpu_timer_write(gpu_stream* stream) {
  if (state.config.debug) {
    if (state.queryCount == 0) {
      uint32_t queryIndex = (state.tick[CPU] & 0x3) * QUERY_CHUNK;
      vkCmdResetQueryPool(stream->commands, state.queryPool, queryIndex, QUERY_CHUNK);
    }

    vkCmdWriteTimestamp(stream->commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, state.queryPool, state.queryCount++);
  }
}

void gpu_timer_gather(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset) {
  if (!state.config.debug || state.queryCount == 0) {
    return;
  }

  uint32_t queryIndex = (state.tick[CPU] & 0x3) * QUERY_CHUNK;
  VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT;
  vkCmdCopyQueryPoolResults(stream->commands, state.queryPool, queryIndex, state.queryCount, buffer->handle, offset, sizeof(uint64_t), flags);
}

// Entry

bool gpu_init(gpu_config* config) {
  state.config = *config;
#define DIE() return gpu_destroy(), false

  // Load
#ifdef _WIN32
  state.library = LoadLibraryA("vulkan-1.dll");
  CHECK(state.library, "Failed to load vulkan library") DIE();
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) GetProcAddress(state.library, "vkGetInstanceProcAddr");
#else
  state.library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  CHECK(state.library, "Failed to load vulkan library") DIE();
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym(state.library, "vkGetInstanceProcAddr");
#endif
  GPU_FOREACH_ANONYMOUS(GPU_LOAD_ANONYMOUS);

  { // Instance
    const char* layers[] = {
      "VK_LAYER_KHRONOS_validation"
    };

    char buffer[1024];
    const char* extensions[16];
    uint32_t extensionCount = 0;
    if (state.config.vk.getInstanceExtensions) {
      CHECK(state.config.vk.getInstanceExtensions(buffer, sizeof(buffer)), "Failed to get instance extensions") DIE();

      char* cursor = buffer;
      char* extension = buffer;
      while (*cursor++) {
        if (*cursor == ' ' || *cursor == '\0') {
          CHECK(extensionCount < COUNTOF(extensions), "Too many instance extensions") DIE();
          extensions[extensionCount++] = extension;
          extension = cursor + 1;
          if (*cursor == ' ') {
            *cursor = '\0';
            cursor++;
          }
        }
      }
    }

    if (state.config.debug) {
      CHECK(extensionCount < COUNTOF(extensions), "Too many instance extensions") DIE();
      extensions[extensionCount++] = "VK_EXT_debug_utils";
    }

    VkInstanceCreateInfo instanceInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &(VkApplicationInfo) {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_MAKE_VERSION(1, 1, 0)
      },
      .enabledLayerCount = state.config.debug ? COUNTOF(layers) : 0,
      .ppEnabledLayerNames = layers,
      .enabledExtensionCount = extensionCount,
      .ppEnabledExtensionNames = extensions
    };

    TRY(vkCreateInstance(&instanceInfo, NULL, &state.instance), "Instance creation failed") DIE();

    GPU_FOREACH_INSTANCE(GPU_LOAD_INSTANCE);

    if (state.config.debug) {
      VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = relay
      };

      TRY(vkCreateDebugUtilsMessengerEXT(state.instance, &messengerInfo, NULL, &state.messenger), "Debug hook setup failed") DIE();
    }
  }

  // Surface
  if (state.config.vk.createSurface) {
    TRY(state.config.vk.createSurface(state.instance, (void**) &state.surface), "Surface creation failed") DIE();
  }

  uint32_t queueFamilyIndex = ~0u;

  { // Device
    uint32_t deviceCount = 1;
    TRY(vkEnumeratePhysicalDevices(state.instance, &deviceCount, &state.adapter), "Physical device enumeration failed") DIE();

    VkQueueFamilyProperties queueFamilies[4];
    uint32_t queueFamilyCount = COUNTOF(queueFamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(state.adapter, &queueFamilyCount, queueFamilies);

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
      VkBool32 presentable = VK_TRUE;

      if (state.surface) {
        vkGetPhysicalDeviceSurfaceSupportKHR(state.adapter, i, state.surface, &presentable);
      }

      uint32_t flags = queueFamilies[i].queueFlags;
      if ((flags & VK_QUEUE_GRAPHICS_BIT) && (flags & VK_QUEUE_COMPUTE_BIT) && presentable) {
        queueFamilyIndex = i;
        break;
      }
    }

    CHECK(queueFamilyIndex != ~0u, "Queue selection failed") DIE();

    VkDeviceQueueCreateInfo queueInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = queueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = &(float) { 1.f }
    };

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(state.adapter, &memoryProperties);
    state.memoryTypeCount = memoryProperties.memoryTypeCount;
    for (uint32_t i = 0; i < state.memoryTypeCount; i++) {
      state.memoryTypes[i] = memoryProperties.memoryTypes[i].propertyFlags;
    }

    VkPhysicalDeviceMultiviewProperties multiviewLimits = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES };
    VkPhysicalDeviceProperties2 properties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &multiviewLimits };
    VkPhysicalDeviceLimits* deviceLimits = &properties2.properties.limits;

    if (config->limits || config->hardware) {
      vkGetPhysicalDeviceProperties2(state.adapter, &properties2);
    }

    if (config->hardware) {
      VkPhysicalDeviceProperties* properties = &properties2.properties;
      config->hardware->vendorId = properties->vendorID;
      config->hardware->deviceId = properties->deviceID;
      memcpy(config->hardware->deviceName, properties->deviceName, MIN(sizeof(config->hardware->deviceName), sizeof(properties->deviceName)));
      config->hardware->driverMajor = VK_VERSION_MAJOR(properties->driverVersion);
      config->hardware->driverMinor = VK_VERSION_MINOR(properties->driverVersion);
      config->hardware->driverPatch = VK_VERSION_PATCH(properties->driverVersion);
      config->hardware->discrete = properties->deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    }

    if (config->limits) {
      config->limits->textureSize2D = MIN(deviceLimits->maxImageDimension2D, UINT16_MAX);
      config->limits->textureSize3D = MIN(deviceLimits->maxImageDimension3D, UINT16_MAX);
      config->limits->textureSizeCube = MIN(deviceLimits->maxImageDimensionCube, UINT16_MAX);
      config->limits->textureLayers = MIN(deviceLimits->maxImageArrayLayers, UINT16_MAX);
      config->limits->renderSize[0] = deviceLimits->maxFramebufferWidth;
      config->limits->renderSize[1] = deviceLimits->maxFramebufferHeight;
      config->limits->renderViews = multiviewLimits.maxMultiviewViewCount;
      config->limits->bundleCount = MIN(deviceLimits->maxBoundDescriptorSets, COUNTOF(((gpu_shader*) NULL)->layouts));
      config->limits->uniformBufferRange = deviceLimits->maxUniformBufferRange;
      config->limits->storageBufferRange = deviceLimits->maxStorageBufferRange;
      config->limits->uniformBufferAlign = deviceLimits->minUniformBufferOffsetAlignment;
      config->limits->storageBufferAlign = deviceLimits->minStorageBufferOffsetAlignment;
      config->limits->vertexAttributes = MIN(deviceLimits->maxVertexInputAttributes, COUNTOF(((gpu_pipeline_info*) NULL)->attributes));
      config->limits->vertexAttributeOffset = MIN(deviceLimits->maxVertexInputAttributeOffset, UINT8_MAX);
      config->limits->vertexBuffers = MIN(deviceLimits->maxVertexInputBindings, COUNTOF(((gpu_pipeline_info*) NULL)->bufferStrides));
      config->limits->vertexBufferStride = MIN(deviceLimits->maxVertexInputBindingStride, UINT16_MAX);
      config->limits->vertexShaderOutputs = deviceLimits->maxVertexOutputComponents;
      config->limits->computeCount[0] = deviceLimits->maxComputeWorkGroupCount[0];
      config->limits->computeCount[1] = deviceLimits->maxComputeWorkGroupCount[1];
      config->limits->computeCount[2] = deviceLimits->maxComputeWorkGroupCount[2];
      config->limits->computeGroupSize[0] = deviceLimits->maxComputeWorkGroupSize[0];
      config->limits->computeGroupSize[1] = deviceLimits->maxComputeWorkGroupSize[1];
      config->limits->computeGroupSize[2] = deviceLimits->maxComputeWorkGroupSize[2];
      config->limits->computeGroupVolume = deviceLimits->maxComputeWorkGroupInvocations;
      config->limits->computeSharedMemory = deviceLimits->maxComputeSharedMemorySize;
      config->limits->indirectDrawCount = deviceLimits->maxDrawIndirectCount;
      config->limits->pointSize[0] = deviceLimits->pointSizeRange[0];
      config->limits->pointSize[1] = deviceLimits->pointSizeRange[1];
      config->limits->anisotropy = deviceLimits->maxSamplerAnisotropy;
    }

    VkPhysicalDeviceMultiviewFeatures enableMultiview = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
      .multiview = true
    };

    VkPhysicalDeviceShaderDrawParameterFeatures enableShaderDrawParameter = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES,
      .pNext = &enableMultiview
    };

    VkPhysicalDeviceFeatures2 enabledFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &enableShaderDrawParameter
    };

    if (config->features) {
      VkPhysicalDeviceShaderDrawParameterFeatures supportsShaderDrawParameter = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES };
      VkPhysicalDeviceFeatures2 root = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &supportsShaderDrawParameter };
      vkGetPhysicalDeviceFeatures2(state.adapter, &root);

      VkPhysicalDeviceFeatures* enable = &enabledFeatures.features;
      VkPhysicalDeviceFeatures* supports = &root.features;

      config->features->astc = enable->textureCompressionASTC_LDR = supports->textureCompressionASTC_LDR;
      config->features->bptc = enable->textureCompressionBC = supports->textureCompressionBC;
      config->features->pointSize = enable->largePoints = supports->largePoints;
      config->features->wireframe = enable->fillModeNonSolid = supports->fillModeNonSolid;
      config->features->multiblend = enable->independentBlend = supports->independentBlend;
      config->features->anisotropy = enable->samplerAnisotropy = supports->samplerAnisotropy;
      config->features->depthClamp = enable->depthClamp = supports->depthClamp;
      config->features->depthOffsetClamp = enable->depthBiasClamp = supports->depthBiasClamp;
      config->features->clipDistance = enable->shaderClipDistance = supports->shaderClipDistance;
      config->features->cullDistance = enable->shaderCullDistance = supports->shaderCullDistance;
      config->features->fullIndexBufferRange = enable->fullDrawIndexUint32 = supports->fullDrawIndexUint32;
      config->features->indirectDrawFirstInstance = enable->drawIndirectFirstInstance = supports->drawIndirectFirstInstance;
      config->features->extraShaderInputs = enableShaderDrawParameter.shaderDrawParameters = supportsShaderDrawParameter.shaderDrawParameters;
      config->features->float64 = enable->shaderFloat64 = supports->shaderFloat64;
      config->features->int64 = enable->shaderInt64 = supports->shaderInt64;
      config->features->int16 = enable->shaderInt16 = supports->shaderInt16;
      enable->multiDrawIndirect = supports->multiDrawIndirect;

      if (supports->shaderUniformBufferArrayDynamicIndexing && supports->shaderSampledImageArrayDynamicIndexing && supports->shaderStorageBufferArrayDynamicIndexing && supports->shaderStorageImageArrayDynamicIndexing) {
        enable->shaderUniformBufferArrayDynamicIndexing = true;
        enable->shaderSampledImageArrayDynamicIndexing = true;
        enable->shaderStorageBufferArrayDynamicIndexing = true;
        enable->shaderStorageImageArrayDynamicIndexing = true;
        config->features->dynamicIndexing = true;
      }

      VkFormatProperties formatProperties;
      for (uint32_t i = 0; i < GPU_FORMAT_COUNT; i++) {
        vkGetPhysicalDeviceFormatProperties(state.adapter, convertFormat(i, LINEAR), &formatProperties);
        uint32_t blitMask = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
        uint32_t flags = formatProperties.optimalTilingFeatures;
        config->features->formats[i] =
          ((flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ? GPU_FEATURE_SAMPLE : 0) |
          ((flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? GPU_FEATURE_RENDER_COLOR : 0) |
          ((flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ? GPU_FEATURE_RENDER_DEPTH : 0) |
          ((flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) ? GPU_FEATURE_BLEND : 0) |
          ((flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ? GPU_FEATURE_FILTER : 0) |
          ((flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) ? GPU_FEATURE_STORAGE : 0) |
          ((flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT) ? GPU_FEATURE_ATOMIC : 0) |
          ((flags & blitMask) == blitMask ? GPU_FEATURE_BLIT : 0);
      }
    }

    char buffer[1024];
    const char* extensions[64];
    uint32_t extensionCount = 0;
    if (state.config.vk.getDeviceExtensions) {
      CHECK(state.config.vk.getDeviceExtensions(buffer, sizeof(buffer), (uintptr_t) state.adapter), "Failed to get device extensions") DIE();

      char* cursor = buffer;
      char* extension = buffer;
      while (*cursor++) {
        if (*cursor == ' ' || *cursor == '\0') {
          CHECK(extensionCount < COUNTOF(extensions), "Too many device extensions") DIE();
          extensions[extensionCount++] = extension;
          extension= cursor + 1;
          if (*cursor == ' ') {
            *cursor = '\0';
            cursor++;
          }
        }
      }
    }

    if (state.surface) {
      CHECK(extensionCount < COUNTOF(extensions), "Too many device extensions") DIE();
      extensions[extensionCount++] = "VK_KHR_swapchain";
    }

    VkDeviceCreateInfo deviceInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = config->features ? &enabledFeatures : NULL,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queueInfo,
      .enabledExtensionCount = extensionCount,
      .ppEnabledExtensionNames = extensions
    };

    TRY(vkCreateDevice(state.adapter, &deviceInfo, NULL, &state.device), "Device creation failed") DIE();
    vkGetDeviceQueue(state.device, queueFamilyIndex, 0, &state.queue);
    GPU_FOREACH_DEVICE(GPU_LOAD_DEVICE);
  }

  // Swapchain
  if (state.surface) {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.adapter, state.surface, &surfaceCapabilities);

    VkSurfaceFormatKHR formats[16];
    uint32_t formatCount = COUNTOF(formats);
    VkSurfaceFormatKHR surfaceFormat = { .format = VK_FORMAT_UNDEFINED };
    vkGetPhysicalDeviceSurfaceFormatsKHR(state.adapter, state.surface, &formatCount, formats);

    for (uint32_t i = 0; i < formatCount; i++) {
      if (formats[i].format == VK_FORMAT_R8G8B8A8_SRGB || formats[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
        surfaceFormat = formats[i];
        break;
      }
    }

    TRY(surfaceFormat.format == VK_FORMAT_UNDEFINED ? VK_ERROR_FORMAT_NOT_SUPPORTED : VK_SUCCESS, "No supported surface formats") DIE();

    VkSwapchainCreateInfoKHR swapchainInfo = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = state.surface,
      .minImageCount = surfaceCapabilities.minImageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = surfaceCapabilities.currentExtent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      // TODO IMMEDIATE may not be supported, only FIFO is guaranteed
      .presentMode = state.config.vk.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR,
      .clipped = VK_TRUE
    };

    TRY(vkCreateSwapchainKHR(state.device, &swapchainInfo, NULL, &state.swapchain), "Swapchain creation failed") DIE();

    VkImage images[4];
    uint32_t imageCount;
    TRY(vkGetSwapchainImagesKHR(state.device, state.swapchain, &imageCount, NULL), "Failed to get swapchain images") DIE();
    TRY(imageCount > COUNTOF(images) ? VK_ERROR_TOO_MANY_OBJECTS : VK_SUCCESS, "Failed to get swapchain images") DIE();
    TRY(vkGetSwapchainImagesKHR(state.device, state.swapchain, &imageCount, images), "Failed to get swapchain images") DIE();

    for (uint32_t i = 0; i < imageCount; i++) {
      gpu_texture* texture = &state.backbuffers[i];

      texture->handle = images[i];
      texture->format = surfaceFormat.format;
      texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;

      gpu_texture_view_info view = {
        .source = texture,
        .type = GPU_TEXTURE_2D
      };

      CHECK(gpu_texture_init_view(texture, &view), "Swapchain texture view creation failed") DIE();
    }
  }

  // Tick resources
  for (uint32_t i = 0; i < COUNTOF(state.ticks); i++) {
    VkCommandPoolCreateInfo poolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = queueFamilyIndex
    };

    TRY(vkCreateCommandPool(state.device, &poolInfo, NULL, &state.ticks[i].pool), "Command pool creation failed") DIE();

    VkCommandBufferAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = state.ticks[i].pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = COUNTOF(state.ticks[i].streams)
    };

    VkSemaphoreCreateInfo semaphoreInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    TRY(vkAllocateCommandBuffers(state.device, &allocateInfo, &state.ticks[i].streams[0].commands), "Commmand buffer allocation failed") DIE();
    TRY(vkCreateSemaphore(state.device, &semaphoreInfo, NULL, &state.ticks[i].semaphores[0]), "Semaphore creation failed") DIE();
    TRY(vkCreateSemaphore(state.device, &semaphoreInfo, NULL, &state.ticks[i].semaphores[1]), "Semaphore creation failed") DIE();
    TRY(vkCreateFence(state.device, &fenceInfo, NULL, &state.ticks[i].fence), "Fence creation failed") DIE();
  }

  // Pipeline cache
  VkPipelineCacheCreateInfo cacheInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
  TRY(vkCreatePipelineCache(state.device, &cacheInfo, NULL, &state.pipelineCache), "Pipeline cache creation failed") DIE();

  // Query pool
  if (state.config.debug) {
    VkQueryPoolCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType = VK_QUERY_TYPE_TIMESTAMP,
      .queryCount = COUNTOF(state.ticks) * QUERY_CHUNK
    };
    TRY(vkCreateQueryPool(state.device, &info, NULL, &state.queryPool), "Query pool creation failed") DIE();
  }

  state.currentBackbuffer = ~0u;
  state.bindings.head = state.bindings.tail = ~0u;
  state.tick[CPU] = COUNTOF(state.ticks) - 1;
  return true;
}

void gpu_destroy(void) {
  if (state.device) vkDeviceWaitIdle(state.device);
  state.tick[GPU] = state.tick[CPU];
  expunge();
  for (uint32_t i = 0; i < COUNTOF(state.framebuffers); i++) {
    for (uint32_t j = 0; j < COUNTOF(state.framebuffers[0]); j++) {
      if (state.framebuffers[i][j].object) vkDestroyFramebuffer(state.device, state.framebuffers[i][j].object, NULL);
    }
  }
  for (uint32_t i = 0; i < COUNTOF(state.ticks); i++) {
    gpu_tick* tick = &state.ticks[i];
    if (tick->pool) vkDestroyCommandPool(state.device, tick->pool, NULL);
    if (tick->semaphores[0]) vkDestroySemaphore(state.device, tick->semaphores[0], NULL);
    if (tick->semaphores[1]) vkDestroySemaphore(state.device, tick->semaphores[1], NULL);
    if (tick->fence) vkDestroyFence(state.device, tick->fence, NULL);
  }
  for (uint32_t i = 0; i < COUNTOF(state.backbuffers); i++) {
    if (state.backbuffers[i].view) vkDestroyImageView(state.device, state.backbuffers[i].view, NULL);
  }
  if (state.queryPool) vkDestroyQueryPool(state.device, state.queryPool, NULL);
  if (state.pipelineCache) vkDestroyPipelineCache(state.device, state.pipelineCache, NULL);
  if (state.swapchain) vkDestroySwapchainKHR(state.device, state.swapchain, NULL);
  if (state.device) vkDestroyDevice(state.device, NULL);
  if (state.surface) vkDestroySurfaceKHR(state.instance, state.surface, NULL);
  if (state.messenger) vkDestroyDebugUtilsMessengerEXT(state.instance, state.messenger, NULL);
  if (state.instance) vkDestroyInstance(state.instance, NULL);
#ifdef _WIN32
  if (state.library) FreeLibrary(state.library);
#else
  if (state.library) dlclose(state.library);
#endif
  memset(&state, 0, sizeof(state));
}

uint32_t gpu_begin() {
  gpu_tick* tick = &state.ticks[++state.tick[CPU] & 0x3];
  TRY(vkWaitForFences(state.device, 1, &tick->fence, VK_FALSE, ~0ull), "Fence wait failed") return 0;
  TRY(vkResetFences(state.device, 1, &tick->fence), "Fence reset failed") return 0;
  TRY(vkResetCommandPool(state.device, tick->pool, 0), "Command pool reset failed") return 0;
  state.tick[GPU] = MAX(state.tick[GPU], state.tick[CPU] - COUNTOF(state.ticks));
  state.streamCount = 0;
  state.queryCount = 0;
  expunge();
  return state.tick[CPU];
}

void gpu_submit(gpu_stream** streams, uint32_t count) {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0x3];

  VkCommandBuffer commands[32];
  for (uint32_t i = 0; i < count; i++) {
    commands[i] = streams[i]->commands;
  }

  VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  bool present = state.currentBackbuffer != ~0u;

  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = present,
    .pWaitSemaphores = &tick->semaphores[0],
    .pWaitDstStageMask = &waitMask,
    .commandBufferCount = count,
    .pCommandBuffers = commands,
    .signalSemaphoreCount = present,
    .pSignalSemaphores = &tick->semaphores[1]
  };

  TRY(vkQueueSubmit(state.queue, 1, &submit, tick->fence), "Queue submit failed") return;

  if (present) {
    VkPresentInfoKHR presentInfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &tick->semaphores[1],
      .swapchainCount = 1,
      .pSwapchains = &state.swapchain,
      .pImageIndices = &state.currentBackbuffer
    };

    TRY(vkQueuePresentKHR(state.queue, &presentInfo), "Queue present failed") {}
    state.currentBackbuffer = ~0u;
  }
}

bool gpu_finished(uint32_t tick) {
  return state.tick[GPU] >= tick;
}

uintptr_t gpu_vk_get_instance() {
  return (uintptr_t) state.instance;
}

uintptr_t gpu_vk_get_physical_device() {
  return (uintptr_t) state.adapter;
}

uintptr_t gpu_vk_get_device() {
  return (uintptr_t) state.device;
}

uintptr_t gpu_vk_get_queue() {
  return (uintptr_t) state.queue;
}

// Helpers

static void hash32(uint32_t* hash, void* data, uint32_t size) {
  const uint8_t* bytes = data;
  for (uint32_t i = 0; i < size; i++) {
    *hash = (*hash ^ bytes[i]) * 16777619;
  }
}

static void ketchup() {
  while (state.tick[GPU] + 1 < state.tick[CPU]) {
    gpu_tick* next = &state.ticks[(state.tick[GPU] + 1) & 0x3];
    VkResult result = vkGetFenceStatus(state.device, next->fence);
    switch (result) {
      case VK_SUCCESS: state.tick[GPU]++; continue;
      case VK_NOT_READY: return;
      default: try(result, "Failed to ketchup"); return;
    }
  }
}

static void condemn(void* handle, VkObjectType type) {
  if (!handle) return;
  gpu_morgue* morgue = &state.morgue;
  CHECK(morgue->head - morgue->tail != COUNTOF(morgue->data), "GPU morgue overflow") return; // TODO ketchup and expunge if morgue is full
  morgue->data[morgue->head++ & 0xff] = (gpu_ref) { handle, type, state.tick[CPU] };
}

static void expunge() {
  gpu_morgue* morgue = &state.morgue;
  while (morgue->tail != morgue->head && state.tick[GPU] >= morgue->data[morgue->tail & 0xff].tick) {
    gpu_ref* victim = &morgue->data[morgue->tail++ & 0xff];
    switch (victim->type) {
      case VK_OBJECT_TYPE_BUFFER: vkDestroyBuffer(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_IMAGE: vkDestroyImage(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_DEVICE_MEMORY: vkFreeMemory(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_IMAGE_VIEW: vkDestroyImageView(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_SAMPLER: vkDestroySampler(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_RENDER_PASS: vkDestroyRenderPass(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_FRAMEBUFFER: vkDestroyFramebuffer(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_PIPELINE: vkDestroyPipeline(state.device, victim->handle, NULL); break;
      default: check(false, "Unreachable"); break;
    }
  }
}

static VkFormat convertFormat(gpu_texture_format format, int colorspace) {
  static const VkFormat formats[][2] = {
    [GPU_FORMAT_R8] = { VK_FORMAT_R8_UNORM, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_RG8] = { VK_FORMAT_R8G8_UNORM, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_RGBA8] = { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB },
    [GPU_FORMAT_R16] = { VK_FORMAT_R16_UNORM, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_RG16] = { VK_FORMAT_R16G16_UNORM, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_RGBA16] = { VK_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_R16F] = { VK_FORMAT_R16_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_RG16F] = { VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_RGBA16F] = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_R32F] = { VK_FORMAT_R32_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_RG32F] = { VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_RGBA32F] = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_RGB565] = { VK_FORMAT_R5G6B5_UNORM_PACK16, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_RGB5A1] = { VK_FORMAT_R5G5B5A1_UNORM_PACK16, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_RGB10A2] = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_RG11B10F] = { VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_D16] = { VK_FORMAT_D16_UNORM, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_D24S8] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_D32F] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_BC6] = { VK_FORMAT_BC6H_SFLOAT_BLOCK, VK_FORMAT_UNDEFINED },
    [GPU_FORMAT_BC7] = { VK_FORMAT_BC7_UNORM_BLOCK, VK_FORMAT_BC7_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_4x4] = { VK_FORMAT_ASTC_4x4_UNORM_BLOCK, VK_FORMAT_ASTC_4x4_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_5x4] = { VK_FORMAT_ASTC_5x4_UNORM_BLOCK, VK_FORMAT_ASTC_5x4_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_5x5] = { VK_FORMAT_ASTC_5x5_UNORM_BLOCK, VK_FORMAT_ASTC_5x5_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_6x5] = { VK_FORMAT_ASTC_6x5_UNORM_BLOCK, VK_FORMAT_ASTC_6x5_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_6x6] = { VK_FORMAT_ASTC_6x6_UNORM_BLOCK, VK_FORMAT_ASTC_6x6_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_8x5] = { VK_FORMAT_ASTC_8x5_UNORM_BLOCK, VK_FORMAT_ASTC_8x5_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_8x6] = { VK_FORMAT_ASTC_8x6_UNORM_BLOCK, VK_FORMAT_ASTC_8x6_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_8x8] = { VK_FORMAT_ASTC_8x8_UNORM_BLOCK, VK_FORMAT_ASTC_8x8_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_10x5] = { VK_FORMAT_ASTC_10x5_UNORM_BLOCK, VK_FORMAT_ASTC_10x5_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_10x6] = { VK_FORMAT_ASTC_10x6_UNORM_BLOCK, VK_FORMAT_ASTC_10x6_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_10x8] = { VK_FORMAT_ASTC_10x8_UNORM_BLOCK, VK_FORMAT_ASTC_10x8_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_10x10] = { VK_FORMAT_ASTC_10x10_UNORM_BLOCK, VK_FORMAT_ASTC_10x10_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_12x10] = { VK_FORMAT_ASTC_12x10_UNORM_BLOCK, VK_FORMAT_ASTC_12x10_SRGB_BLOCK },
    [GPU_FORMAT_ASTC_12x12] = { VK_FORMAT_ASTC_12x12_UNORM_BLOCK, VK_FORMAT_ASTC_12x12_SRGB_BLOCK }
  };
  return formats[format][colorspace];
}

static void nickname(void* handle, VkObjectType type, const char* name) {
  if (name && state.config.debug) {
    union { uint64_t u64; void* p; } pointer = { .p = handle };

    VkDebugUtilsObjectNameInfoEXT info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = type,
      .objectHandle = pointer.u64,
      .pObjectName = name
    };

    TRY(vkSetDebugUtilsObjectNameEXT(state.device, &info), "Nickname failed") return;
  }
}

static VkBool32 relay(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userdata) {
  if (state.config.callback) {
    bool severe = severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    state.config.callback(state.config.userdata, data->pMessage, severe);
  }
  return VK_FALSE;
}

static bool try(VkResult result, const char* message) {
  if (result >= 0) return true;
  if (!state.config.callback) return false;
#define CASE(x) case x: state.config.callback(state.config.userdata, "Vulkan error: " #x, false); break;
  switch (result) {
    CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
    CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CASE(VK_ERROR_INITIALIZATION_FAILED);
    CASE(VK_ERROR_DEVICE_LOST);
    CASE(VK_ERROR_MEMORY_MAP_FAILED);
    CASE(VK_ERROR_LAYER_NOT_PRESENT);
    CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
    CASE(VK_ERROR_FEATURE_NOT_PRESENT);
    CASE(VK_ERROR_TOO_MANY_OBJECTS);
    CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
    CASE(VK_ERROR_FRAGMENTED_POOL);
    CASE(VK_ERROR_OUT_OF_POOL_MEMORY);
    default: break;
  }
#undef CASE
  state.config.callback(state.config.userdata, message, true);
  return false;
}

static bool check(bool condition, const char* message) {
  if (!condition && state.config.callback) {
    state.config.callback(state.config.userdata, message, true);
  }
  return condition;
}

/*
Comments
## General
- Vulkan 1.1 is targeted.
- The first physical device is used.
- There's almost no error handling.  The intent is to use validation layers or handle the errors at
  a higher level if needed, so they don't need to be duplicated for each backend.
- There is no dynamic memory allocation in this layer.  All state is in ~9kb of static bss data.
- Writing to storage resources from vertex/fragment shaders is currently not allowed.
- Texel buffers are not supported.
- ~0u aka -1 is commonly used as a "nil" value.
## Threads
- Currently, the API is not generally thread safe.
## Ticks
- There is no concept of a frame.  There are ticks.  Every sequence of commands between gpu_begin
  and gpu_submit is a tick.  We keep track of the current CPU tick (the tick being recorded) and the
  current GPU tick (the most recent tick the GPU has completed).  These are monotonically increasing
  counters.
- Each tick has a set of resources (see gpu_tick).  There is a ring buffer of ticks.  The size of
  the ring buffer determines how many ticks can be in flight (currently 4).  A tick index can be
  masked off to find the element of the ring buffer it corresponds to.  The tick's fence is used to
  make sure that the GPU is done with a tick before recording a new one in the same slot.
- In a lot of places (command buffers, descriptor pools, morgue, scratchpad) a tick is tracked to
  know when the GPU is done with a resource so it can be recycled or destroyed.
- The ketchup helper function checks the oldest fences to try to increment the GPU tick as much as
  possible, allowing more resources to be recycled and avoid potential resource exhaustion.
- I don't know what will happen if the tick counter wraps around or reaches UINT32_MAX.
- In the future I'd like to use timeline semaphores instead of fences, but those are in Vulkan 1.2
  and currently aren't widely supported.
## Command buffers
- Each tick has a cap on the number of primary command buffers it uses.
- Each render or compute pass uses one primary command buffer.  This is purely for synchronization
  reasons.  When starting a command buffer for a pass, the previous command buffer is left "open"
  until the pass completes.  Once the pass completes, we know what resources were used during the
  pass, which is used to figure out which barriers are required.  The barriers are recorded at the
  end of the previous command buffer, after which it's closed.
- If you run out of command buffers, the current recommendation is to split the workload into
  multiple queue submissions.  In the future it may be possible to support an unbounded number of
  command buffers using dynamic memory allocation.
*/
