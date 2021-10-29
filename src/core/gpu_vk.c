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
  bool layered;
};

struct gpu_sampler {
  VkSampler handle;
};

struct gpu_pass {
  VkRenderPass handle;
  uint32_t samples;
  uint32_t count;
};

struct gpu_layout {
  VkDescriptorSetLayout handle;
  uint8_t slotTypes[32];
  uint8_t slotLengths[32];
  uint8_t slotNumbers[32];
  uint32_t slotCount;
};

struct gpu_shader {
  VkShaderModule handles[2];
  VkPipelineLayout pipelineLayout;
};

struct gpu_bunch {
  VkDescriptorPool handle;
};

struct gpu_bundle {
  VkDescriptorSet handle;
};

struct gpu_pipeline {
  VkPipeline handle;
  VkPipelineLayout layout;
};

struct gpu_stream {
  VkCommandBuffer commands;
};

size_t gpu_sizeof_buffer() { return sizeof(gpu_buffer); }
size_t gpu_sizeof_texture() { return sizeof(gpu_texture); }
size_t gpu_sizeof_sampler() { return sizeof(gpu_sampler); }
size_t gpu_sizeof_layout() { return sizeof(gpu_layout); }
size_t gpu_sizeof_shader() { return sizeof(gpu_shader); }
size_t gpu_sizeof_bunch() { return sizeof(gpu_bunch); }
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

// State

static struct {
  void* library;
  gpu_config config;
  VkInstance instance;
  VkPhysicalDevice adapter;
  VkDevice device;
  VkQueue queue;
  uint32_t queueFamilyIndex;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  gpu_texture backbuffers[8];
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
  VkQueryPool queryPool;
  uint32_t queryCount;
} state;

// Helpers

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#define TRY(f, s) if (!try(f, s))
#define CHECK(c, s) if (!check(c, s))
#define TICK_MASK (COUNTOF(state.ticks) - 1)
#define MORGUE_MASK (COUNTOF(state.morgue.data) - 1)
#define HASH_SEED 2166136261
#define QUERY_CHUNK 64

enum { CPU, GPU };
enum { LINEAR, SRGB };

static void hash32(uint32_t* hash, void* data, uint32_t size);
static void condemn(void* handle, VkObjectType type);
static void expunge(void);
static VkImageLayout getNaturalLayout(uint32_t usage, VkImageAspectFlags aspect);
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
  X(vkCmdPushConstants)\
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

  bool mappable = state.memoryTypes[memoryType] & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

  if (info->mapping) {
    if (!mappable) {
      *info->mapping = NULL;
    } else if (!try(vkMapMemory(state.device, buffer->memory, 0, VK_WHOLE_SIZE, 0, info->mapping), "Could not map memory")) {
      vkDestroyBuffer(state.device, buffer->handle, NULL);
      vkFreeMemory(state.device, buffer->memory, NULL);
      return false;
    }
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

  texture->format = info->format == ~0u ? state.backbuffers[0].format : convertFormat(info->format, info->srgb);
  texture->layered = type == VK_IMAGE_TYPE_2D;

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
    .extent.depth = texture->layered ? 1 : info->size[2],
    .mipLevels = info->mipmaps,
    .arrayLayers = texture->layered ? info->size[2] : 1,
    .samples = info->samples,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage =
      (((info->usage & GPU_TEXTURE_RENDER) && !depth) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0) |
      (((info->usage & GPU_TEXTURE_RENDER) && depth) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0) |
      ((info->usage & GPU_TEXTURE_SAMPLE) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0) |
      ((info->usage & GPU_TEXTURE_STORAGE) ? VK_IMAGE_USAGE_STORAGE_BIT : 0) |
      ((info->usage & GPU_TEXTURE_COPY_SRC) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0) |
      ((info->usage & GPU_TEXTURE_COPY_DST) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0) |
      ((info->usage & GPU_TEXTURE_TRANSIENT) ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0) |
      (info->upload.levelCount > 0 ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0) |
      (info->upload.generateMipmaps ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0)
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

  texture->layout = getNaturalLayout(info->usage, texture->aspect);

  if (info->upload.stream) {
    VkImage image = texture->handle;
    VkBuffer buffer = info->upload.levelCount > 0 ? info->upload.buffer->handle : VK_NULL_HANDLE;
    VkCommandBuffer commands = info->upload.stream->commands;

    VkPipelineStageFlags prev, next;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageMemoryBarrier transition = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .image = image,
      .subresourceRange.aspectMask = texture->aspect,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS,
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS
    };

    if (info->upload.levelCount > 0) {
      VkBufferImageCopy regions[16];
      for (uint32_t i = 0; i < info->upload.levelCount; i++) {
        regions[i] = (VkBufferImageCopy) {
          .bufferOffset = info->upload.levelOffsets[i],
          .imageSubresource.aspectMask = texture->aspect,
          .imageSubresource.mipLevel = i,
          .imageSubresource.baseArrayLayer = 0,
          .imageSubresource.layerCount = texture->layered ? info->size[2] : 1,
          .imageExtent.width = MAX(info->size[0] >> i, 1),
          .imageExtent.height = MAX(info->size[1] >> i, 1),
          .imageExtent.depth = texture->layered ? 1 : MAX(info->size[2] >> i, 1)
        };
      }

      // Upload initial contents
      prev = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      next = VK_PIPELINE_STAGE_TRANSFER_BIT;
      transition.srcAccessMask = 0;
      transition.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      transition.oldLayout = layout;
      transition.newLayout = layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      vkCmdPipelineBarrier(commands, prev, next, 0, 0, NULL, 0, NULL, 1, &transition);
      vkCmdCopyBufferToImage(commands, buffer, image, layout, info->upload.levelCount, regions);

      // Generate mipmaps
      if (info->upload.generateMipmaps) {
        prev = VK_PIPELINE_STAGE_TRANSFER_BIT;
        next = VK_PIPELINE_STAGE_TRANSFER_BIT;
        transition.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        transition.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        transition.subresourceRange.baseMipLevel = 0;
        transition.subresourceRange.levelCount = info->upload.levelCount;
        transition.oldLayout = layout;
        transition.newLayout = layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        vkCmdPipelineBarrier(commands, prev, next, 0, 0, NULL, 0, NULL, 1, &transition);
        for (uint32_t i = info->upload.levelCount; i < info->mipmaps; i++) {
          VkImageBlit region = {
            .srcSubresource = {
              .aspectMask = texture->aspect,
              .mipLevel = i - 1,
              .layerCount = texture->layered ? info->size[2] : 1
            },
            .dstSubresource = {
              .aspectMask = texture->aspect,
              .mipLevel = i,
              .layerCount = texture->layered ? info->size[2] : 1
            },
            .srcOffsets[1] = { MAX(info->size[0] >> (i - 1), 1), MAX(info->size[1] >> (i - 1), 1), 1 },
            .dstOffsets[1] = { MAX(info->size[0] >> i, 1), MAX(info->size[1] >> i, 1), 1 }
          };
          vkCmdBlitImage(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);
          transition.subresourceRange.baseMipLevel = i;
          transition.subresourceRange.levelCount = 1;
          transition.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
          transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
          vkCmdPipelineBarrier(commands, prev, next, 0, 0, NULL, 0, NULL, 1, &transition);
        }
      }
    }

    // Transition to natural layout
    prev = info->upload.levelCount > 0 ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    next = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    transition.srcAccessMask = info->upload.levelCount > 0 ? VK_ACCESS_TRANSFER_WRITE_BIT : 0;
    transition.dstAccessMask = 0;
    transition.oldLayout = layout;
    transition.newLayout = texture->layout;
    transition.subresourceRange.baseMipLevel = 0;
    transition.subresourceRange.levelCount = info->mipmaps;
    vkCmdPipelineBarrier(commands, prev, next, 0, 0, NULL, 0, NULL, 1, &transition);
  }

  return true;
}

bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info) {
  if (texture != info->source) {
    texture->handle = VK_NULL_HANDLE;
    texture->memory = VK_NULL_HANDLE;
    texture->format = info->source->format;
    texture->aspect = info->source->aspect;
    texture->layered = info->type != GPU_TEXTURE_3D;
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
  gpu_tick* tick = &state.ticks[state.tick[CPU] & TICK_MASK];
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

// Layout

bool gpu_layout_init(gpu_layout* layout, gpu_layout_info* info) {
  VkDescriptorSetLayoutBinding bindings[32];

  static const VkDescriptorType types[] = {
    [GPU_SLOT_UNIFORM_BUFFER] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [GPU_SLOT_STORAGE_BUFFER] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    [GPU_SLOT_UNIFORM_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    [GPU_SLOT_STORAGE_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    [GPU_SLOT_SAMPLED_TEXTURE] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    [GPU_SLOT_STORAGE_TEXTURE] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    [GPU_SLOT_SAMPLER] = VK_DESCRIPTOR_TYPE_SAMPLER
  };

  for (uint32_t i = 0; i < info->count; i++) {
    layout->slotTypes[i] = info->slots[i].type;
    layout->slotLengths[i] = info->slots[i].count;
    layout->slotNumbers[i] = info->slots[i].number;
    bindings[i] = (VkDescriptorSetLayoutBinding) {
      .binding = info->slots[i].number,
      .descriptorType = types[info->slots[i].type],
      .descriptorCount = info->slots[i].count,
      .stageFlags = info->slots[i].stage == GPU_STAGE_ALL ? VK_SHADER_STAGE_ALL :
        (((info->slots[i].stage & GPU_STAGE_VERTEX) ? VK_SHADER_STAGE_VERTEX_BIT : 0) |
        ((info->slots[i].stage & GPU_STAGE_FRAGMENT) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0) |
        ((info->slots[i].stage & GPU_STAGE_COMPUTE) ? VK_SHADER_STAGE_COMPUTE_BIT : 0))
    };
  }

  VkDescriptorSetLayoutCreateInfo layoutInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = info->count,
    .pBindings = bindings
  };

  TRY(vkCreateDescriptorSetLayout(state.device, &layoutInfo, NULL, &layout->handle), "Failed to create layout") {
    return false;
  }

  layout->slotCount = info->count;
  return true;
}

void gpu_layout_destroy(gpu_layout* layout) {
  condemn(layout->handle, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
}

// Shader

bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info) {
  VkPipelineBindPoint type = info->stages[1].code ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE; // TODO

  for (uint32_t i = 0; i < COUNTOF(info->stages) && info->stages[i].code; i++) {
    VkShaderModuleCreateInfo moduleInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = info->stages[i].size,
      .pCode = info->stages[i].code
    };

    TRY(vkCreateShaderModule(state.device, &moduleInfo, NULL, &shader->handles[i]), "Failed to load shader") {
      return false;
    }
  }

  VkDescriptorSetLayout layouts[4];
  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pSetLayouts = layouts,
    .pushConstantRangeCount = type == VK_PIPELINE_BIND_POINT_GRAPHICS && info->pushConstantSize > 0,
    .pPushConstantRanges = &(VkPushConstantRange) {
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = info->pushConstantSize
    }
  };

  for (uint32_t i = 0; i < COUNTOF(info->layouts) && info->layouts[i]; i++) {
    layouts[i] = info->layouts[i]->handle;
    pipelineLayoutInfo.setLayoutCount++;
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
  condemn(shader->pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT);
}

// Bundle

bool gpu_bunch_init(gpu_bunch* bunch, gpu_bunch_info* info) {
  VkDescriptorPoolSize sizes[7] = {
    [GPU_SLOT_UNIFORM_BUFFER] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0 },
    [GPU_SLOT_STORAGE_BUFFER] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0 },
    [GPU_SLOT_UNIFORM_BUFFER_DYNAMIC] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0 },
    [GPU_SLOT_STORAGE_BUFFER_DYNAMIC] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 0 },
    [GPU_SLOT_SAMPLED_TEXTURE] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0 },
    [GPU_SLOT_STORAGE_TEXTURE] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0 },
    [GPU_SLOT_SAMPLER] = { VK_DESCRIPTOR_TYPE_SAMPLER, 0 }
  };

  if (info->layout) {
    for (uint32_t i = 0; i < info->layout->slotCount; i++) {
      sizes[info->layout->slotTypes[i]].descriptorCount += info->layout->slotLengths[i] * info->count;
    }
  } else {
    for (uint32_t i = 0; i < info->count; i++) {
      gpu_layout* layout = info->contents[i].layout;
      for (uint32_t j = 0; j < layout->slotCount; j++) {
        sizes[layout->slotTypes[j]].descriptorCount += layout->slotLengths[j];
      }
    }
  }

  // Descriptor counts of zero are forbidden, so swap any zero-sized sizes with the last entry
  uint32_t poolSizeCount = COUNTOF(sizes);
  for (uint32_t i = 0; i < poolSizeCount; i++) {
    if (sizes[i].descriptorCount == 0) {
      VkDescriptorPoolSize last = sizes[poolSizeCount - 1];
      sizes[poolSizeCount - 1] = sizes[i];
      sizes[i] = last;
      poolSizeCount--;
      i--;
    }
  }

  VkDescriptorPoolCreateInfo poolInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = info->count,
    .poolSizeCount = poolSizeCount,
    .pPoolSizes = sizes
  };

  TRY(vkCreateDescriptorPool(state.device, &poolInfo, NULL, &bunch->handle), "Could not create bunch") {
    return false;
  }

  VkDescriptorSetLayout layouts[1024];
  for (uint32_t i = 0; i < info->count; i+= COUNTOF(layouts)) {
    uint32_t chunk = MIN(info->count - i, COUNTOF(layouts));

    for (uint32_t j = 0; j < chunk; j++) {
      layouts[j] = info->layout ? info->layout->handle : info->contents[i + j].layout->handle;
    }

    VkDescriptorSetAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = bunch->handle,
      .descriptorSetCount = chunk,
      .pSetLayouts = layouts
    };

    TRY(vkAllocateDescriptorSets(state.device, &allocateInfo, &info->bundles[i].handle), "Could not allocate descriptor sets") {
      gpu_bunch_destroy(bunch);
      return false;
    }
  }

  return true;
}

void gpu_bunch_destroy(gpu_bunch* bunch) {
  condemn(bunch->handle, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
}

void gpu_bundle_write(gpu_bundle** bundles, gpu_bundle_info* infos, uint32_t count) {
  VkDescriptorBufferInfo buffers[256];
  VkDescriptorImageInfo images[256];
  VkWriteDescriptorSet writes[64];
  uint32_t bufferCount = 0;
  uint32_t imageCount = 0;
  uint32_t writeCount = 0;

  static const VkDescriptorType types[] = {
    [GPU_SLOT_UNIFORM_BUFFER] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [GPU_SLOT_STORAGE_BUFFER] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    [GPU_SLOT_UNIFORM_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    [GPU_SLOT_STORAGE_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    [GPU_SLOT_SAMPLED_TEXTURE] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    [GPU_SLOT_STORAGE_TEXTURE] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    [GPU_SLOT_SAMPLER] = VK_DESCRIPTOR_TYPE_SAMPLER
  };

  for (uint32_t i = 0; i < count; i++) {
    gpu_layout* layout = infos[i].layout;
    gpu_binding* binding = infos[i].bindings;
    for (uint32_t j = 0; j < layout->slotCount; j++) {
      VkDescriptorType type = types[layout->slotTypes[j]];
      bool texture = type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      bool sampler = type == VK_DESCRIPTOR_TYPE_SAMPLER;
      bool image = texture || sampler;
      uint32_t length = layout->slotLengths[j];
      uint32_t number = layout->slotNumbers[j];
      uint32_t cursor = 0;

      while (cursor < length) {
        if (image ? imageCount >= COUNTOF(images) : bufferCount >= COUNTOF(buffers)) {
          vkUpdateDescriptorSets(state.device, writeCount, writes, 0, NULL);
          bufferCount = imageCount = writeCount = 0;
        }

        uint32_t available = image ? COUNTOF(images) - imageCount : COUNTOF(buffers) - bufferCount;
        uint32_t remaining = length - cursor;
        uint32_t chunk = MIN(available, remaining);

        writes[writeCount++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = bundles[i]->handle,
          .dstBinding = number,
          .dstArrayElement = cursor,
          .descriptorCount = chunk,
          .descriptorType = type,
          .pBufferInfo = &buffers[bufferCount],
          .pImageInfo = &images[imageCount]
        };

        if (sampler) {
          for (uint32_t k = 0; k < chunk; k++, binding++) {
            images[imageCount++] = (VkDescriptorImageInfo) {
              .sampler = binding->sampler->handle
            };
          }
        } else if (texture) {
          for (uint32_t k = 0; k < chunk; k++, binding++) {
            images[imageCount++] = (VkDescriptorImageInfo) {
              .imageView = binding->texture->view,
              .imageLayout = binding->texture->layout
            };
          }
        } else {
          for (uint32_t k = 0; k < chunk; k++, binding++) {
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
  }

  if (writeCount > 0) {
    vkUpdateDescriptorSets(state.device, writeCount, writes, 0, NULL);
  }
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
    VkImageLayout naturalLayout = getNaturalLayout(info->color[i].usage, VK_IMAGE_ASPECT_COLOR_BIT);
    attachments[i] = (VkAttachmentDescription) {
      .format = info->color[i].format == ~0u ? state.backbuffers[0].format : convertFormat(info->color[i].format, info->color[i].srgb),
      .samples = info->samples,
      .loadOp = loadOps[info->color[i].load],
      .storeOp = info->resolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : storeOps[info->color[i].save],
      .initialLayout = info->color[i].format == ~0u ? VK_IMAGE_LAYOUT_UNDEFINED : naturalLayout,
      .finalLayout = info->color[i].format == ~0u && !info->resolve ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : naturalLayout
    };
  }

  if (info->resolve) {
    for (uint32_t i = 0; i < info->count; i++) {
      uint32_t index = info->count + i;
      references[index].attachment = index;
      references[index].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      VkImageLayout naturalLayout = getNaturalLayout(info->color[i].usage, VK_IMAGE_ASPECT_COLOR_BIT);
      attachments[index] = (VkAttachmentDescription) {
        .format = info->color[i].format == ~0u ? state.backbuffers[0].format : convertFormat(info->color[i].format, info->color[i].srgb),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = storeOps[info->color[i].save],
        .initialLayout = info->color[i].format == ~0u ? VK_IMAGE_LAYOUT_UNDEFINED : naturalLayout,
        .finalLayout = info->color[i].format == ~0u ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : naturalLayout
      };
    }
  }

  if (info->depth.format) { // You'll never catch me!
    uint32_t index = info->count << info->resolve;
    references[index].attachment = index;
    references[index].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkImageLayout naturalLayout = getNaturalLayout(info->depth.usage, VK_IMAGE_ASPECT_DEPTH_BIT);
    attachments[index] = (VkAttachmentDescription) {
      .format = convertFormat(info->depth.format, LINEAR),
      .samples = info->samples,
      .loadOp = loadOps[info->depth.load],
      .storeOp = storeOps[info->depth.save],
      .stencilLoadOp = loadOps[info->depth.stencilLoad],
      .stencilStoreOp = storeOps[info->depth.stencilSave],
      .initialLayout = info->depth.load == GPU_LOAD_OP_LOAD ? naturalLayout : VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = naturalLayout
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

bool gpu_pipeline_init_graphics(gpu_pipeline* pipelines, gpu_pipeline_info* infos, uint32_t count) {
  static const VkPrimitiveTopology topologies[] = {
    [GPU_DRAW_POINTS] = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    [GPU_DRAW_LINES] = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    [GPU_DRAW_TRIANGLES] = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
  };

  static const VkFormat attributeTypes[] = {
    [GPU_TYPE_I8] = VK_FORMAT_R8_SINT,
    [GPU_TYPE_U8] = VK_FORMAT_R8_UINT,
    [GPU_TYPE_I16] = VK_FORMAT_R16_SINT,
    [GPU_TYPE_U16] = VK_FORMAT_R16_UINT,
    [GPU_TYPE_I32] = VK_FORMAT_R32_SINT,
    [GPU_TYPE_U32] = VK_FORMAT_R32_UINT,
    [GPU_TYPE_F32] = VK_FORMAT_R32_SFLOAT,
    [GPU_TYPE_I8x2] = VK_FORMAT_R8G8_SINT,
    [GPU_TYPE_U8x2] = VK_FORMAT_R8G8_UINT,
    [GPU_TYPE_I8Nx2] = VK_FORMAT_R8G8_SNORM,
    [GPU_TYPE_U8Nx2] = VK_FORMAT_R8G8_UNORM,
    [GPU_TYPE_I16x2] = VK_FORMAT_R16G16_SINT,
    [GPU_TYPE_U16x2] = VK_FORMAT_R16G16_UINT,
    [GPU_TYPE_I16Nx2] = VK_FORMAT_R16G16_SNORM,
    [GPU_TYPE_U16Nx2] = VK_FORMAT_R16G16_UNORM,
    [GPU_TYPE_I32x2] = VK_FORMAT_R32G32_SINT,
    [GPU_TYPE_U32x2] = VK_FORMAT_R32G32_UINT,
    [GPU_TYPE_F32x2] = VK_FORMAT_R32G32_SFLOAT,
    [GPU_TYPE_U10Nx3] = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
    [GPU_TYPE_I32x3] = VK_FORMAT_R32G32B32_SINT,
    [GPU_TYPE_U32x3] = VK_FORMAT_R32G32B32_UINT,
    [GPU_TYPE_F32x3] = VK_FORMAT_R32G32B32_SFLOAT,
    [GPU_TYPE_I8x4] = VK_FORMAT_R8G8B8A8_SINT,
    [GPU_TYPE_U8x4] = VK_FORMAT_R8G8B8A8_UINT,
    [GPU_TYPE_I8Nx4] = VK_FORMAT_R8G8B8A8_SNORM,
    [GPU_TYPE_U8Nx4] = VK_FORMAT_R8G8B8A8_UNORM,
    [GPU_TYPE_I16x4] = VK_FORMAT_R16G16B16A16_SINT,
    [GPU_TYPE_U16x4] = VK_FORMAT_R16G16B16A16_UINT,
    [GPU_TYPE_I16Nx4] = VK_FORMAT_R16G16B16A16_SNORM,
    [GPU_TYPE_U16Nx4] = VK_FORMAT_R16G16B16A16_UNORM,
    [GPU_TYPE_I32x4] = VK_FORMAT_R32G32B32A32_SINT,
    [GPU_TYPE_U32x4] = VK_FORMAT_R32G32B32A32_UINT,
    [GPU_TYPE_F32x4] = VK_FORMAT_R32G32B32A32_SFLOAT
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
    [GPU_STENCIL_ZERO] = VK_STENCIL_OP_ZERO,
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

  VkGraphicsPipelineCreateInfo pipelineInfos[16];

  do {
    uint32_t chunk = MIN(count, COUNTOF(pipelineInfos));

    for (uint32_t i = 0; i < chunk; i++) {
      VkVertexInputBindingDescription vertexBuffers[16];
      for (uint32_t j = 0; j < infos[i].vertex.bufferCount; j++) {
        vertexBuffers[j] = (VkVertexInputBindingDescription) {
          .binding = j,
          .stride = infos[i].vertex.bufferStrides[j],
          .inputRate = (infos[i].vertex.instancedBuffers & (1 << j)) ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX
        };
      }

      VkVertexInputAttributeDescription vertexAttributes[COUNTOF(infos[i].vertex.attributes)];
      for (uint32_t j = 0; j < infos[i].vertex.attributeCount; j++) {
        vertexAttributes[j] = (VkVertexInputAttributeDescription) {
          .location = infos[i].vertex.attributes[j].location,
          .binding = infos[i].vertex.attributes[j].buffer,
          .format = attributeTypes[infos[i].vertex.attributes[j].type],
          .offset = infos[i].vertex.attributes[j].offset
        };
      }

      VkPipelineVertexInputStateCreateInfo vertexInput = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = infos[i].vertex.bufferCount,
        .pVertexBindingDescriptions = vertexBuffers,
        .vertexAttributeDescriptionCount = infos[i].vertex.attributeCount,
        .pVertexAttributeDescriptions = vertexAttributes
      };

      VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = topologies[infos[i].drawMode]
      };

      VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
      };

      VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = infos[i].rasterizer.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
        .cullMode = cullModes[infos[i].rasterizer.cullMode],
        .frontFace = frontFaces[infos[i].rasterizer.winding],
        .depthBiasEnable = infos[i].rasterizer.depthOffset != 0.f || infos[i].rasterizer.depthOffsetSloped != 0.f,
        .depthBiasConstantFactor = infos[i].rasterizer.depthOffset,
        .depthBiasSlopeFactor = infos[i].rasterizer.depthOffsetSloped,
        .lineWidth = 1.f
      };

      VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = infos[i].pass->samples,
        .alphaToCoverageEnable = infos[i].alphaToCoverage
      };

      VkStencilOpState stencil = {
        .failOp = stencilOps[infos[i].stencil.failOp],
        .passOp = stencilOps[infos[i].stencil.passOp],
        .depthFailOp = stencilOps[infos[i].stencil.depthFailOp],
        .compareOp = compareOps[infos[i].stencil.test],
        .compareMask = infos[i].stencil.testMask,
        .writeMask = infos[i].stencil.writeMask,
        .reference = infos[i].stencil.value
      };

      VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = infos[i].depth.test != GPU_COMPARE_NONE,
        .depthWriteEnable = infos[i].depth.write,
        .depthCompareOp = compareOps[infos[i].depth.test],
        .stencilTestEnable = infos[i].stencil.test != GPU_COMPARE_NONE,
        .front = stencil,
        .back = stencil
      };

      VkPipelineColorBlendAttachmentState colorAttachments[4];
      for (uint32_t j = 0; j < infos[i].pass->count; j++) {
        colorAttachments[j] = (VkPipelineColorBlendAttachmentState) {
          .blendEnable = infos[i].blend.enabled,
          .srcColorBlendFactor = blendFactors[infos[i].blend.color.src],
          .dstColorBlendFactor = blendFactors[infos[i].blend.color.dst],
          .colorBlendOp = blendOps[infos[i].blend.color.op],
          .srcAlphaBlendFactor = blendFactors[infos[i].blend.alpha.src],
          .dstAlphaBlendFactor = blendFactors[infos[i].blend.alpha.dst],
          .alphaBlendOp = blendOps[infos[i].blend.alpha.op],
          .colorWriteMask = infos[i].colorMask
        };
      }

      VkPipelineColorBlendStateCreateInfo colorBlend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = infos[i].pass->count,
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

      uint32_t constants[32];
      VkSpecializationMapEntry entries[32];
      CHECK(infos[i].flagCount <= COUNTOF(constants), "Too many specialization constants") return false;
      for (uint32_t j = 0; j < infos[i].flagCount; j++) {
        gpu_shader_flag* flag = &infos[i].flags[j];
        switch (flag->type) {
          case GPU_FLAG_B32: constants[j] = flag->value == 0. ? VK_FALSE : VK_TRUE; break;
          case GPU_FLAG_I32: constants[j] = (uint32_t) flag->value; break;
          case GPU_FLAG_U32: constants[j] = (uint32_t) flag->value; break;
          case GPU_FLAG_F32: memcpy(&constants[j], &(float) { flag->value }, sizeof(float)); break;
          default: flag->value = 0;
        }
        entries[j] = (VkSpecializationMapEntry) {
          .constantID = infos[i].flags[j].id,
          .offset = j * sizeof(uint32_t),
          .size = sizeof(uint32_t)
        };
      }

      VkSpecializationInfo specialization = {
        .mapEntryCount = infos[i].flagCount,
        .pMapEntries = entries,
        .dataSize = sizeof(constants),
        .pData = (const void*) constants
      };

      VkPipelineShaderStageCreateInfo shaders[2] = {
        [0] = {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = infos[i].shader->handles[0],
          .pName = "main",
          .pSpecializationInfo = &specialization
        },
        [1] = {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = infos[i].shader->handles[1],
          .pName = "main",
          .pSpecializationInfo = &specialization
        }
      };

      pipelineInfos[i] = (VkGraphicsPipelineCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shaders,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamicState,
        .layout = infos[i].shader->pipelineLayout,
        .renderPass = infos[i].pass->handle
      };
    }

    TRY(vkCreateGraphicsPipelines(state.device, state.pipelineCache, chunk, pipelineInfos, NULL, &pipelines[0].handle), "Could not create pipeline") {
      return false;
    }

    for (uint32_t i = 0; i < chunk; i++) {
      pipelines[i].layout = infos[i].shader->pipelineLayout;
    }

    pipelines += chunk;
    infos += chunk;
    count -= chunk;
  } while (count > 0);

  return true;
}

bool gpu_pipeline_init_compute(gpu_pipeline* pipeline, gpu_compute_pipeline_info* info) {
  uint32_t constants[32];
  VkSpecializationMapEntry entries[32];
  CHECK(info->flagCount <= COUNTOF(constants), "Too many specialization constants") return false;
  for (uint32_t i = 0; i < info->flagCount; i++) {
    gpu_shader_flag* flag = &info->flags[i];
    switch (flag->type) {
      case GPU_FLAG_B32: default: constants[i] = flag->value == 0. ? VK_FALSE : VK_TRUE; break;
      case GPU_FLAG_I32: constants[i] = (uint32_t) flag->value; break;
      case GPU_FLAG_U32: constants[i] = (uint32_t) flag->value; break;
      case GPU_FLAG_F32: constants[i] = (float) flag->value; break;
    }
    entries[i] = (VkSpecializationMapEntry) {
      .constantID = info->flags[i].id,
      .offset = i * sizeof(uint32_t),
      .size = sizeof(uint32_t)
    };
  }

  VkSpecializationInfo specialization = {
    .mapEntryCount = info->flagCount,
    .pMapEntries = entries,
    .dataSize = sizeof(constants),
    .pData = (const void*) constants
  };

  VkPipelineShaderStageCreateInfo shader = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
    .module = info->shader->handles[0],
    .pName = "main",
    .pSpecializationInfo = &specialization
  };

  VkComputePipelineCreateInfo pipelineInfo = {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage = shader,
    .layout = info->shader->pipelineLayout
  };

  TRY(vkCreateComputePipelines(state.device, state.pipelineCache, 1, &pipelineInfo, NULL, &pipeline->handle), "Could not create compute pipeline") {
    return false;
  }

  nickname(pipeline->handle, VK_OBJECT_TYPE_PIPELINE, info->label);
  return true;
}

void gpu_pipeline_destroy(gpu_pipeline* pipeline) {
  condemn(pipeline->handle, VK_OBJECT_TYPE_PIPELINE);
}

// Stream

gpu_stream* gpu_stream_begin() {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & TICK_MASK];
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
  hash32(&hash, textures, count * sizeof(textures[0]));
  hash32(&hash, &canvas->pass->handle, sizeof(canvas->pass->handle));
  hash32(&hash, canvas->size, 2 * sizeof(uint32_t));

  // Search for matching framebuffer in cache table
  VkFramebuffer framebuffer;
  uint32_t rows = COUNTOF(state.framebuffers);
  uint32_t cols = COUNTOF(state.framebuffers[0]);
  gpu_cache_entry* row = state.framebuffers[hash & (rows - 1)];
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

void gpu_set_viewport(gpu_stream* stream, float view[4], float depthRange[2]) {
  VkViewport viewport = { view[0], view[1], view[2], view[3], depthRange[0], depthRange[1] };
  vkCmdSetViewport(stream->commands, 0, 1, &viewport);
}

void gpu_set_scissor(gpu_stream* stream, uint32_t scissor[4]) {
  VkRect2D rect = { { scissor[0], scissor[1] }, { scissor[2], scissor[3] } };
  vkCmdSetScissor(stream->commands, 0, 1, &rect);
}

void gpu_push_constants(gpu_stream* stream, gpu_pipeline* pipeline, void* data, uint32_t size) {
  VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  vkCmdPushConstants(stream->commands, pipeline->layout, stages, 0, size, data);
}

void gpu_bind_pipeline_graphics(gpu_stream* stream, gpu_pipeline* pipeline) {
  vkCmdBindPipeline(stream->commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);
}

void gpu_bind_pipeline_compute(gpu_stream* stream, gpu_pipeline* pipeline) {
  vkCmdBindPipeline(stream->commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->handle);
}

void gpu_bind_bundle(gpu_stream* stream, gpu_pipeline* pipeline, bool compute, uint32_t group, gpu_bundle* bundle, uint32_t* offsets, uint32_t offsetCount) {
  VkPipelineBindPoint bindPoint = compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkCmdBindDescriptorSets(stream->commands, bindPoint, pipeline->layout, group, 1, &bundle->handle, offsetCount, offsets);
}

void gpu_bind_vertex_buffers(gpu_stream* stream, gpu_buffer** buffers, uint32_t* offsets, uint32_t first, uint32_t count) {
  VkBuffer handles[COUNTOF(((gpu_pipeline_info*) NULL)->vertex.bufferStrides)];
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

void gpu_draw(gpu_stream* stream, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t baseInstance) {
  vkCmdDraw(stream->commands, vertexCount, instanceCount, firstVertex, baseInstance);
}

void gpu_draw_indexed(gpu_stream* stream, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex, uint32_t baseInstance) {
  vkCmdDrawIndexed(stream->commands, indexCount, instanceCount, firstIndex, baseVertex, baseInstance);
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
  vkCmdCopyImage(stream->commands, src->handle, VK_IMAGE_LAYOUT_GENERAL, dst->handle, VK_IMAGE_LAYOUT_GENERAL, 1, &(VkImageCopy) {
    .srcSubresource = {
      .aspectMask = src->aspect,
      .mipLevel = srcOffset[3],
      .baseArrayLayer = src->layered ? srcOffset[2] : 0,
      .layerCount = src->layered ? size[2] : 1
    },
    .dstSubresource = {
      .aspectMask = dst->aspect,
      .mipLevel = dstOffset[3],
      .baseArrayLayer = dst->layered ? dstOffset[2] : 0,
      .layerCount = dst->layered ? size[2] : 1
    },
    .srcOffset = { srcOffset[0], srcOffset[1], src->layered ? 0 : srcOffset[2] },
    .dstOffset = { dstOffset[0], dstOffset[1], dst->layered ? 0 : dstOffset[2] },
    .extent = { size[0], size[1], size[2] }
  });
}

void gpu_copy_buffer_texture(gpu_stream* stream, gpu_buffer* src, gpu_texture* dst, uint32_t srcOffset, uint16_t dstOffset[4], uint16_t extent[3]) {
  VkBufferImageCopy region = {
    .bufferOffset = srcOffset,
    .imageSubresource.aspectMask = dst->aspect,
    .imageSubresource.mipLevel = dstOffset[3],
    .imageSubresource.baseArrayLayer = dst->layered ? dstOffset[2] : 0,
    .imageSubresource.layerCount = dst->layered ? extent[2] : 1,
    .imageOffset = { dstOffset[0], dstOffset[1], dst->layered ? 0 : dstOffset[2] },
    .imageExtent = { extent[0], extent[1], dst->layered ? 1 : extent[2] }
  };

  vkCmdCopyBufferToImage(stream->commands, src->handle, dst->handle, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
}

void gpu_copy_texture_buffer(gpu_stream* stream, gpu_texture* src, gpu_buffer* dst, uint16_t srcOffset[4], uint32_t dstOffset, uint16_t extent[3]) {
  VkBufferImageCopy region = {
    .bufferOffset = dstOffset,
    .imageSubresource.aspectMask = src->aspect,
    .imageSubresource.mipLevel = srcOffset[3],
    .imageSubresource.baseArrayLayer = src->layered ? srcOffset[2] : 0,
    .imageSubresource.layerCount = src->layered ? extent[2] : 1,
    .imageOffset = { srcOffset[0], srcOffset[1], src->layered ? 0 : srcOffset[2] },
    .imageExtent = { extent[0], extent[1], src->layered ? 1 : extent[2] }
  };

  vkCmdCopyImageToBuffer(stream->commands, src->handle, VK_IMAGE_LAYOUT_GENERAL, dst->handle, 1, &region);
}

void gpu_clear_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t size) {
  vkCmdFillBuffer(stream->commands, buffer->handle, offset, size, 0);
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

  vkCmdClearColorImage(stream->commands, texture->handle, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
}

void gpu_blit(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t srcExtent[3], uint16_t dstExtent[3], gpu_filter filter) {
  VkImageBlit region = {
    .srcSubresource = {
      .aspectMask = src->aspect,
      .mipLevel = srcOffset[3],
      .baseArrayLayer = src->layered ? srcOffset[2] : 0,
      .layerCount = src->layered ? srcExtent[2] : 1
    },
    .dstSubresource = {
      .aspectMask = dst->aspect,
      .mipLevel = dstOffset[3],
      .baseArrayLayer = dst->layered ? dstOffset[2] : 0,
      .layerCount = dst->layered ? dstExtent[2] : 1
    },
    .srcOffsets[0] = { srcOffset[0], srcOffset[1], src->layered ? 0 : srcOffset[2] },
    .dstOffsets[0] = { dstOffset[0], dstOffset[1], dst->layered ? 0 : dstOffset[2] },
    .srcOffsets[1] = { srcOffset[0] + srcExtent[0], srcOffset[1] + srcExtent[1], src->layered ? 1 : srcOffset[2] + srcExtent[2] },
    .dstOffsets[1] = { dstOffset[0] + dstExtent[0], dstOffset[1] + dstExtent[1], dst->layered ? 1 : dstOffset[2] + dstExtent[2] }
  };

  static const VkFilter filters[] = {
    [GPU_FILTER_NEAREST] = VK_FILTER_NEAREST,
    [GPU_FILTER_LINEAR] = VK_FILTER_LINEAR
  };

  vkCmdBlitImage(stream->commands, src->handle, VK_IMAGE_LAYOUT_GENERAL, dst->handle, VK_IMAGE_LAYOUT_GENERAL, 1, &region, filters[filter]);
}

static VkPipelineStageFlags convertPhase(gpu_phase phase) {
  VkPipelineStageFlags flags = 0;
  if (phase & GPU_PHASE_INDIRECT) flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
  if (phase & GPU_PHASE_INPUT_INDEX) flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
  if (phase & GPU_PHASE_INPUT_VERTEX) flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
  if (phase & GPU_PHASE_SHADER_VERTEX) flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
  if (phase & GPU_PHASE_SHADER_FRAGMENT) flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  if (phase & GPU_PHASE_SHADER_COMPUTE) flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  if (phase & GPU_PHASE_DEPTH_EARLY) flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  if (phase & GPU_PHASE_DEPTH_LATE) flags |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  if (phase & GPU_PHASE_BLEND) flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  if (phase & GPU_PHASE_COPY) flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
  if (phase & GPU_PHASE_BLIT) flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
  if (phase & GPU_PHASE_CLEAR) flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
  return flags;
}

static VkAccessFlags convertCache(gpu_cache cache) {
  VkAccessFlags flags = 0;
  if (cache & GPU_CACHE_INDIRECT) flags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
  if (cache & GPU_CACHE_INDEX) flags |= VK_ACCESS_INDEX_READ_BIT;
  if (cache & GPU_CACHE_VERTEX) flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
  if (cache & GPU_CACHE_UNIFORM) flags |= VK_ACCESS_UNIFORM_READ_BIT;
  if (cache & GPU_CACHE_TEXTURE) flags |= VK_ACCESS_SHADER_READ_BIT;
  if (cache & GPU_CACHE_STORAGE_READ) flags |= VK_ACCESS_SHADER_READ_BIT;
  if (cache & GPU_CACHE_STORAGE_WRITE) flags |= VK_ACCESS_SHADER_WRITE_BIT;
  if (cache & GPU_CACHE_DEPTH_READ) flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  if (cache & GPU_CACHE_DEPTH_WRITE) flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  if (cache & GPU_CACHE_BLEND_READ) flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
  if (cache & GPU_CACHE_BLEND_WRITE) flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  if (cache & GPU_CACHE_TRANSFER_READ) flags |= VK_ACCESS_TRANSFER_READ_BIT;
  if (cache & GPU_CACHE_TRANSFER_WRITE) flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
  return flags;
}

void gpu_sync(gpu_stream* stream, gpu_barrier* barriers, uint32_t count) {
  VkMemoryBarrier memoryBarrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER };
  VkPipelineStageFlags src = 0;
  VkPipelineStageFlags dst = 0;

  for (uint32_t i = 0; i < count; i++) {
    gpu_barrier* barrier = &barriers[i];
    src |= convertPhase(barrier->prev);
    dst |= convertPhase(barrier->next);
    memoryBarrier.srcAccessMask |= convertCache(barrier->flush);
    memoryBarrier.dstAccessMask |= convertCache(barrier->invalidate);
  }

  if (src && dst) {
    vkCmdPipelineBarrier(stream->commands, src, dst, 0, 1, &memoryBarrier, 0, NULL, 0, NULL);
  }
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
      uint32_t queryIndex = (state.tick[CPU] & TICK_MASK) * QUERY_CHUNK;
      vkCmdResetQueryPool(stream->commands, state.queryPool, queryIndex, QUERY_CHUNK);
    }

    vkCmdWriteTimestamp(stream->commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, state.queryPool, state.queryCount++);
  }
}

void gpu_timer_gather(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset) {
  if (!state.config.debug || state.queryCount == 0) {
    return;
  }

  uint32_t queryIndex = (state.tick[CPU] & TICK_MASK) * QUERY_CHUNK;
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
#elif __APPLE__
  state.library = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
  CHECK(state.library, "Failed to load vulkan library") DIE();
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym(state.library, "vkGetInstanceProcAddr");
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

    if (state.config.vk.createInstance) {
      TRY(state.config.vk.createInstance(&instanceInfo, NULL, (uintptr_t) &state.instance, (void*) vkGetInstanceProcAddr), "Instance creation failed") DIE();
    } else {
      TRY(vkCreateInstance(&instanceInfo, NULL, &state.instance), "Instance creation failed") DIE();
    }

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

  state.queueFamilyIndex = ~0u;

  { // Device
    if (state.config.vk.getPhysicalDevice) {
      state.config.vk.getPhysicalDevice(state.instance, (uintptr_t) &state.adapter);
    } else {
      uint32_t deviceCount = 1;
      TRY(vkEnumeratePhysicalDevices(state.instance, &deviceCount, &state.adapter), "Physical device enumeration failed") DIE();
    }

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
        state.queueFamilyIndex = i;
        break;
      }
    }

    CHECK(state.queueFamilyIndex != ~0u, "Queue selection failed") DIE();

    VkDeviceQueueCreateInfo queueInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = state.queueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = &(float) { 1.f }
    };

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(state.adapter, &memoryProperties);
    state.memoryTypeCount = memoryProperties.memoryTypeCount;
    for (uint32_t i = 0; i < state.memoryTypeCount; i++) {
      state.memoryTypes[i] = memoryProperties.memoryTypes[i].propertyFlags;
    }

    VkPhysicalDeviceMultiviewProperties multiviewProperties = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES };
    VkPhysicalDeviceSubgroupProperties subgroupProperties = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES, .pNext = &multiviewProperties };
    VkPhysicalDeviceProperties2 properties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &subgroupProperties };
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
      config->hardware->subgroupSize = subgroupProperties.subgroupSize;
      config->hardware->discrete = properties->deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    }

    if (config->limits) {
      config->limits->textureSize2D = MIN(deviceLimits->maxImageDimension2D, UINT16_MAX);
      config->limits->textureSize3D = MIN(deviceLimits->maxImageDimension3D, UINT16_MAX);
      config->limits->textureSizeCube = MIN(deviceLimits->maxImageDimensionCube, UINT16_MAX);
      config->limits->textureLayers = MIN(deviceLimits->maxImageArrayLayers, UINT16_MAX);
      config->limits->renderSize[0] = deviceLimits->maxFramebufferWidth;
      config->limits->renderSize[1] = deviceLimits->maxFramebufferHeight;
      config->limits->renderSize[2] = multiviewProperties.maxMultiviewViewCount;
      config->limits->uniformBufferRange = deviceLimits->maxUniformBufferRange;
      config->limits->storageBufferRange = deviceLimits->maxStorageBufferRange;
      config->limits->uniformBufferAlign = deviceLimits->minUniformBufferOffsetAlignment;
      config->limits->storageBufferAlign = deviceLimits->minStorageBufferOffsetAlignment;
      config->limits->vertexAttributes = MIN(deviceLimits->maxVertexInputAttributes, COUNTOF(((gpu_pipeline_info*) NULL)->vertex.attributes));
      config->limits->vertexBuffers = MIN(deviceLimits->maxVertexInputBindings, COUNTOF(((gpu_pipeline_info*) NULL)->vertex.bufferStrides));
      config->limits->vertexBufferStride = MIN(deviceLimits->maxVertexInputBindingStride, UINT16_MAX);
      config->limits->vertexShaderOutputs = deviceLimits->maxVertexOutputComponents;
      config->limits->computeDispatchCount[0] = deviceLimits->maxComputeWorkGroupCount[0];
      config->limits->computeDispatchCount[1] = deviceLimits->maxComputeWorkGroupCount[1];
      config->limits->computeDispatchCount[2] = deviceLimits->maxComputeWorkGroupCount[2];
      config->limits->computeWorkgroupSize[0] = deviceLimits->maxComputeWorkGroupSize[0];
      config->limits->computeWorkgroupSize[1] = deviceLimits->maxComputeWorkGroupSize[1];
      config->limits->computeWorkgroupSize[2] = deviceLimits->maxComputeWorkGroupSize[2];
      config->limits->computeWorkgroupVolume = deviceLimits->maxComputeWorkGroupInvocations;
      config->limits->computeSharedMemory = deviceLimits->maxComputeSharedMemorySize;
      config->limits->pushConstantSize = deviceLimits->maxPushConstantsSize;
      config->limits->indirectDrawCount = deviceLimits->maxDrawIndirectCount;
      config->limits->instances = multiviewProperties.maxMultiviewInstanceIndex;
      config->limits->anisotropy = deviceLimits->maxSamplerAnisotropy;
      config->limits->pointSize = deviceLimits->pointSizeRange[1];
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

      // Required features
      enableShaderDrawParameter.shaderDrawParameters = true;

      // Internal features (they are exposed as limits)
      enable->samplerAnisotropy = supports->samplerAnisotropy;
      enable->multiDrawIndirect = supports->multiDrawIndirect;
      enable->largePoints = supports->largePoints;

      // Optional features (currently always enabled when supported)
      config->features->astc = enable->textureCompressionASTC_LDR = supports->textureCompressionASTC_LDR;
      config->features->bptc = enable->textureCompressionBC = supports->textureCompressionBC;
      config->features->wireframe = enable->fillModeNonSolid = supports->fillModeNonSolid;
      config->features->depthClamp = enable->depthClamp = supports->depthClamp;
      config->features->clipDistance = enable->shaderClipDistance = supports->shaderClipDistance;
      config->features->cullDistance = enable->shaderCullDistance = supports->shaderCullDistance;
      config->features->fullIndexBufferRange = enable->fullDrawIndexUint32 = supports->fullDrawIndexUint32;
      config->features->indirectDrawFirstInstance = enable->drawIndirectFirstInstance = supports->drawIndirectFirstInstance;
      config->features->float64 = enable->shaderFloat64 = supports->shaderFloat64;
      config->features->int64 = enable->shaderInt64 = supports->shaderInt64;
      config->features->int16 = enable->shaderInt16 = supports->shaderInt16;

      if (supports->shaderUniformBufferArrayDynamicIndexing && supports->shaderSampledImageArrayDynamicIndexing && supports->shaderStorageBufferArrayDynamicIndexing && supports->shaderStorageImageArrayDynamicIndexing) {
        enable->shaderUniformBufferArrayDynamicIndexing = true;
        enable->shaderSampledImageArrayDynamicIndexing = true;
        enable->shaderStorageBufferArrayDynamicIndexing = true;
        enable->shaderStorageImageArrayDynamicIndexing = true;
        config->features->dynamicIndexing = true;
      }

      // Format features
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

    if (state.config.vk.createDevice) {
      TRY(state.config.vk.createDevice(state.instance, &deviceInfo, NULL, (uintptr_t) &state.device, (void*) vkGetInstanceProcAddr), "Device creation failed") DIE();
    } else {
      TRY(vkCreateDevice(state.adapter, &deviceInfo, NULL, &state.device), "Device creation failed") DIE();
    }
    vkGetDeviceQueue(state.device, state.queueFamilyIndex, 0, &state.queue);
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
      .presentMode = state.config.vk.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR,
      .clipped = VK_TRUE
    };

    TRY(vkCreateSwapchainKHR(state.device, &swapchainInfo, NULL, &state.swapchain), "Swapchain creation failed") DIE();

    VkImage images[COUNTOF(state.backbuffers)];
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
      .queueFamilyIndex = state.queueFamilyIndex
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
  gpu_tick* tick = &state.ticks[++state.tick[CPU] & TICK_MASK];
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
  gpu_tick* tick = &state.ticks[state.tick[CPU] & TICK_MASK];

  VkCommandBuffer commands[COUNTOF(tick->streams)];
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

void gpu_wait() {
  vkDeviceWaitIdle(state.device);
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

uintptr_t gpu_vk_get_queue(uint32_t* queueFamilyIndex, uint32_t* queueIndex) {
  return *queueFamilyIndex = state.queueFamilyIndex, *queueIndex = 0, (uintptr_t) state.queue;
}

// Helpers

static void hash32(uint32_t* hash, void* data, uint32_t size) {
  const uint8_t* bytes = data;
  for (uint32_t i = 0; i < size; i++) {
    *hash = (*hash ^ bytes[i]) * 16777619;
  }
}

static void condemn(void* handle, VkObjectType type) {
  if (!handle) return;
  gpu_morgue* morgue = &state.morgue;
  CHECK(morgue->head - morgue->tail != COUNTOF(morgue->data), "GPU morgue overflow") return; // TODO stall and expunge if morgue is full
  morgue->data[morgue->head++ & MORGUE_MASK] = (gpu_ref) { handle, type, state.tick[CPU] };
}

static void expunge() {
  gpu_morgue* morgue = &state.morgue;
  while (morgue->tail != morgue->head && state.tick[GPU] >= morgue->data[morgue->tail & MORGUE_MASK].tick) {
    gpu_ref* victim = &morgue->data[morgue->tail++ & MORGUE_MASK];
    switch (victim->type) {
      case VK_OBJECT_TYPE_BUFFER: vkDestroyBuffer(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_IMAGE: vkDestroyImage(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_DEVICE_MEMORY: vkFreeMemory(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_IMAGE_VIEW: vkDestroyImageView(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_SAMPLER: vkDestroySampler(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_RENDER_PASS: vkDestroyRenderPass(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_FRAMEBUFFER: vkDestroyFramebuffer(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT: vkDestroyDescriptorSetLayout(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_DESCRIPTOR_POOL: vkDestroyDescriptorPool(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_PIPELINE_LAYOUT: vkDestroyPipelineLayout(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_PIPELINE: vkDestroyPipeline(state.device, victim->handle, NULL); break;
      default: check(false, "Unreachable"); break;
    }
  }
}

static VkImageLayout getNaturalLayout(uint32_t usage, VkImageAspectFlags aspect) {
  if (usage & (GPU_TEXTURE_STORAGE | GPU_TEXTURE_COPY_SRC | GPU_TEXTURE_COPY_DST)) {
    return VK_IMAGE_LAYOUT_GENERAL;
  } else if (usage & GPU_TEXTURE_SAMPLE) {
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  } else {
    if (aspect == VK_IMAGE_ASPECT_COLOR_BIT) {
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    } else {
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
  }
  return VK_IMAGE_LAYOUT_UNDEFINED; // Explode, hopefully
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
