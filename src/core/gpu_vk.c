#include "gpu.h"
#include <string.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Objects

struct gpu_buffer {
  VkBuffer handle;
  uint32_t memory;
  uint32_t offset;
};

struct gpu_texture {
  VkImage handle;
  VkImageView view;
  VkFormat format;
  VkImageAspectFlagBits aspect;
  VkImageLayout layout;
  uint16_t memory;
  bool layered;
};

struct gpu_sampler {
  VkSampler handle;
};

struct gpu_stream {
  VkCommandBuffer commands;
};

struct gpu_layout {
  VkDescriptorSetLayout handle;
  uint32_t descriptorCounts[7];
};

size_t gpu_sizeof_buffer() { return sizeof(gpu_buffer); }
size_t gpu_sizeof_texture() { return sizeof(gpu_texture); }
size_t gpu_sizeof_sampler() { return sizeof(gpu_sampler); }
size_t gpu_sizeof_layout() { return sizeof(gpu_layout); }

// Internals

typedef struct {
  VkDeviceMemory handle;
  void* pointer;
  uint32_t refs;
} gpu_memory;

typedef enum {
  GPU_MEMORY_BUFFER_GPU,
  GPU_MEMORY_BUFFER_CPU_WRITE,
  GPU_MEMORY_BUFFER_CPU_READ,
  GPU_MEMORY_TEXTURE_COLOR,
  GPU_MEMORY_TEXTURE_D16,
  GPU_MEMORY_TEXTURE_D24S8,
  GPU_MEMORY_TEXTURE_D32F,
  GPU_MEMORY_TEXTURE_LAZY_COLOR,
  GPU_MEMORY_TEXTURE_LAZY_D16,
  GPU_MEMORY_TEXTURE_LAZY_D24S8,
  GPU_MEMORY_TEXTURE_LAZY_D32F,
  GPU_MEMORY_COUNT
} gpu_memory_type;

typedef struct {
  gpu_memory* block;
  uint32_t cursor;
  uint16_t memoryType;
  uint16_t memoryFlags;
} gpu_allocator;

typedef struct {
  void* handle;
  VkObjectType type;
  uint32_t tick;
} gpu_victim;

typedef struct {
  uint32_t head;
  uint32_t tail;
  gpu_victim data[256];
} gpu_morgue;

typedef struct {
  gpu_memory* memory;
  VkBuffer buffer;
  uint32_t cursor;
  uint32_t size;
  char* pointer;
} gpu_scratchpad;

typedef struct {
  VkCommandPool pool;
  gpu_stream streams[64];
  VkSemaphore semaphores[2];
  VkFence fence;
} gpu_tick;

// State

static struct {
  void* library;
  gpu_config config;
  VkInstance instance;
  VkPhysicalDevice adapter;
  VkDevice device;
  VkQueue queue;
  uint32_t queueFamilyIndex;
  VkDebugUtilsMessengerEXT messenger;
  gpu_allocator allocators[GPU_MEMORY_COUNT];
  uint8_t allocatorLookup[GPU_MEMORY_COUNT];
  gpu_scratchpad scratchpad[2];
  gpu_memory memory[256];
  uint32_t streamCount;
  uint32_t tick[2];
  gpu_tick ticks[4];
  gpu_morgue morgue;
} state;

// Helpers

enum { CPU, GPU };
enum { LINEAR, SRGB };

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#define ALIGN(p, n) (((uintptr_t) (p) + (n - 1)) & ~(n - 1))
#define VK(f, s) if (!vcheck(f, s))
#define CHECK(c, s) if (!check(c, s))
#define TICK_MASK (COUNTOF(state.ticks) - 1)
#define MORGUE_MASK (COUNTOF(state.morgue.data) - 1)

static gpu_memory* gpu_allocate(gpu_memory_type type, VkMemoryRequirements info, VkDeviceSize* offset);
static void gpu_release(gpu_memory* memory);
static void condemn(void* handle, VkObjectType type);
static void expunge(void);
static VkImageLayout getNaturalLayout(uint32_t usage, VkImageAspectFlags aspect);
static VkFormat convertFormat(gpu_texture_format format, int colorspace);
static VkBool32 relay(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userdata);
static void nickname(void* object, VkObjectType type, const char* name);
static bool vcheck(VkResult result, const char* message);
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
  X(vkCmdClearDepthStencilImage)\
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
    buffer->memory = ~0u;
    buffer->handle = (VkBuffer) info->handle;
    return true;
  }

  VkBufferCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = info->size,
    .usage =
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
      VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
      VK_BUFFER_USAGE_TRANSFER_DST_BIT
  };

  VK(vkCreateBuffer(state.device, &createInfo, NULL, &buffer->handle), "Could not create buffer") return false;
  nickname(buffer->handle, VK_OBJECT_TYPE_BUFFER, info->label);

  VkDeviceSize offset;
  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(state.device, buffer->handle, &requirements);
  gpu_memory* memory = gpu_allocate(GPU_MEMORY_BUFFER_GPU, requirements, &offset);

  VK(vkBindBufferMemory(state.device, buffer->handle, memory->handle, offset), "Could not bind buffer memory") {
    vkDestroyBuffer(state.device, buffer->handle, NULL);
    gpu_release(memory);
    return false;
  }

  if (info->pointer) {
    *info->pointer = memory->pointer ? (char*) memory->pointer + offset : NULL;
  }

  buffer->memory = memory - state.memory;
  return true;
}

void gpu_buffer_destroy(gpu_buffer* buffer) {
  if (buffer->memory == ~0u) return;
  condemn(buffer->handle, VK_OBJECT_TYPE_BUFFER);
  gpu_release(&state.memory[buffer->memory]);
}

void* gpu_map(gpu_buffer* buffer, uint32_t size, uint32_t align, gpu_map_mode mode) {
  gpu_scratchpad* pool = &state.scratchpad[mode];

  uint32_t zone = state.tick[CPU] & TICK_MASK;
  uint32_t cursor = ALIGN(pool->cursor, align);
  bool oversized = size > (1 << 26); // "Big" buffers don't pollute the scratchpad (heuristic)

  if (oversized || cursor + size > pool->size) {
    uint32_t bufferSize;

    if (oversized) {
      bufferSize = size;
    } else {
      while (pool->size < size) {
        pool->size = pool->size ? (pool->size << 1) : (1 << 22);
      }
      bufferSize = pool->size * COUNTOF(state.ticks);
    }

    VkBufferCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bufferSize,
      .usage = mode == GPU_MAP_WRITE ?
        (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT) :
        VK_BUFFER_USAGE_TRANSFER_DST_BIT
    };

    VkBuffer handle;
    VK(vkCreateBuffer(state.device, &info, NULL, &handle), "Could not create scratch buffer") return NULL;
    nickname(handle, VK_OBJECT_TYPE_BUFFER, "Scratchpad");

    VkDeviceSize offset;
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(state.device, handle, &requirements);
    gpu_memory* memory = gpu_allocate(GPU_MEMORY_BUFFER_CPU_WRITE + mode, requirements, &offset);

    VK(vkBindBufferMemory(state.device, handle, memory->handle, offset), "Could not bind scratchpad memory") {
      vkDestroyBuffer(state.device, handle, NULL);
      gpu_release(memory);
      return NULL;
    }

    if (oversized) {
      gpu_release(memory);
      condemn(handle, VK_OBJECT_TYPE_BUFFER);
      buffer->handle = handle;
      buffer->memory = ~0u;
      buffer->offset = 0;
      return memory->pointer;
    } else {
      gpu_release(pool->memory);
      condemn(pool->buffer, VK_OBJECT_TYPE_BUFFER);
      pool->memory = memory;
      pool->buffer = handle;
      pool->cursor = cursor = 0;
      pool->pointer = pool->memory->pointer;
    }
  }

  pool->cursor = cursor + size;
  buffer->handle = pool->buffer;
  buffer->memory = ~0u;
  buffer->offset = pool->size * zone + cursor;

  return pool->pointer + pool->size * zone + cursor;
}

// Texture

bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info) {
  VkImageType type;
  VkImageCreateFlags flags = 0;

  switch (info->type) {
    case GPU_TEXTURE_2D: type = VK_IMAGE_TYPE_2D; break;
    case GPU_TEXTURE_3D: type = VK_IMAGE_TYPE_3D, flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT; break;
    case GPU_TEXTURE_CUBE: type = VK_IMAGE_TYPE_2D, flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; break;
    case GPU_TEXTURE_ARRAY: type = VK_IMAGE_TYPE_2D; break;
    default: return false;
  }

  gpu_memory_type memoryType;

  switch (info->format) {
    case GPU_FORMAT_D16:
      texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
      memoryType = GPU_MEMORY_TEXTURE_D16;
      break;
    case GPU_FORMAT_D24S8:
      texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      memoryType = GPU_MEMORY_TEXTURE_D24S8;
      break;
    case GPU_FORMAT_D32F:
      texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
      memoryType = GPU_MEMORY_TEXTURE_D32F;
      break;
    default:
      texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
      memoryType = GPU_MEMORY_TEXTURE_COLOR;
      break;
  }

  texture->layout = getNaturalLayout(info->usage, texture->aspect);
  texture->format = convertFormat(info->format, info->srgb);
  texture->layered = type == VK_IMAGE_TYPE_2D;

  gpu_texture_view_info viewInfo = {
    .source = texture,
    .type = info->type
  };

  if (info->handle) {
    texture->memory = 0xffff;
    texture->handle = (VkImage) info->handle;
    return gpu_texture_init_view(texture, &viewInfo);
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

  VK(vkCreateImage(state.device, &imageInfo, NULL, &texture->handle), "Could not create texture") return false;
  nickname(texture->handle, VK_OBJECT_TYPE_IMAGE, info->label);

  VkDeviceSize offset;
  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(state.device, texture->handle, &requirements);
  gpu_memory* memory = gpu_allocate(memoryType, requirements, &offset);

  VK(vkBindImageMemory(state.device, texture->handle, memory->handle, 0), "Could not bind texture memory") {
    vkDestroyImage(state.device, texture->handle, NULL);
    gpu_release(memory);
    return false;
  }

  if (!gpu_texture_init_view(texture, &viewInfo)) {
    vkDestroyImage(state.device, texture->handle, NULL);
    gpu_release(memory);
    return false;
  }

  if (info->upload.stream) {
    VkImage image = texture->handle;
    VkCommandBuffer commands = info->upload.stream->commands;
    uint32_t levelCount = info->upload.levelCount;
    gpu_buffer* buffer = info->upload.buffer;

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

    if (levelCount > 0) {
      VkBufferImageCopy regions[16];
      for (uint32_t i = 0; i < levelCount; i++) {
        regions[i] = (VkBufferImageCopy) {
          .bufferOffset = buffer->offset + info->upload.levelOffsets[i],
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
      vkCmdCopyBufferToImage(commands, buffer->handle, image, layout, levelCount, regions);

      // Generate mipmaps
      if (info->upload.generateMipmaps) {
        prev = VK_PIPELINE_STAGE_TRANSFER_BIT;
        next = VK_PIPELINE_STAGE_TRANSFER_BIT;
        transition.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        transition.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        transition.subresourceRange.baseMipLevel = 0;
        transition.subresourceRange.levelCount = levelCount;
        transition.oldLayout = layout;
        transition.newLayout = layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        vkCmdPipelineBarrier(commands, prev, next, 0, 0, NULL, 0, NULL, 1, &transition);
        for (uint32_t i = levelCount; i < info->mipmaps; i++) {
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
    prev = levelCount > 0 ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    next = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    transition.srcAccessMask = levelCount > 0 ? VK_ACCESS_TRANSFER_WRITE_BIT : 0;
    transition.dstAccessMask = 0;
    transition.oldLayout = layout;
    transition.newLayout = texture->layout;
    transition.subresourceRange.baseMipLevel = 0;
    transition.subresourceRange.levelCount = info->mipmaps;
    vkCmdPipelineBarrier(commands, prev, next, 0, 0, NULL, 0, NULL, 1, &transition);
  }

  texture->memory = memory - state.memory;

  return true;
}

bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info) {
  if (texture != info->source) {
    texture->handle = VK_NULL_HANDLE;
    texture->memory = 0xffff;
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

  VK(vkCreateImageView(state.device, &createInfo, NULL, &texture->view), "Could not create texture view") {
    return false;
  }

  return true;
}

void gpu_texture_destroy(gpu_texture* texture) {
  condemn(texture->view, VK_OBJECT_TYPE_IMAGE_VIEW);
  if (texture->memory == 0xffff) return;
  condemn(texture->handle, VK_OBJECT_TYPE_IMAGE);
  gpu_release(state.memory + texture->memory);
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

  VK(vkCreateSampler(state.device, &samplerInfo, NULL, &sampler->handle), "Could not create sampler") {
    return false;
  }

  return true;
}

void gpu_sampler_destroy(gpu_sampler* sampler) {
  condemn(sampler->handle, VK_OBJECT_TYPE_SAMPLER);
}

// Layout

bool gpu_layout_init(gpu_layout* layout, gpu_layout_info* info) {
  static const VkDescriptorType types[] = {
    [GPU_SLOT_UNIFORM_BUFFER] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [GPU_SLOT_STORAGE_BUFFER] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    [GPU_SLOT_UNIFORM_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    [GPU_SLOT_STORAGE_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    [GPU_SLOT_SAMPLED_TEXTURE] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    [GPU_SLOT_STORAGE_TEXTURE] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    [GPU_SLOT_SAMPLER] = VK_DESCRIPTOR_TYPE_SAMPLER
  };

  VkDescriptorSetLayoutBinding bindings[32];
  for (uint32_t i = 0; i < info->count; i++) {
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

  VK(vkCreateDescriptorSetLayout(state.device, &layoutInfo, NULL, &layout->handle), "Failed to create layout") {
    return false;
  }

  memset(layout->descriptorCounts, 0, sizeof(layout->descriptorCounts));

  for (uint32_t i = 0; i < info->count; i++) {
    layout->descriptorCounts[info->slots[i].type] += MAX(info->slots[i].count, 1);
  }

  return true;
}

void gpu_layout_destroy(gpu_layout* layout) {
  condemn(layout->handle, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
}

// Stream

gpu_stream* gpu_stream_begin(const char* label) {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & TICK_MASK];
  CHECK(state.streamCount < COUNTOF(tick->streams), "Too many streams") return NULL;
  gpu_stream* stream = &tick->streams[state.streamCount];
  nickname(stream->commands, VK_OBJECT_TYPE_COMMAND_BUFFER, label);

  VkCommandBufferBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };

  VK(vkBeginCommandBuffer(stream->commands, &beginfo), "Failed to begin stream") return NULL;
  state.streamCount++;
  return stream;
}

void gpu_stream_end(gpu_stream* stream) {
  VK(vkEndCommandBuffer(stream->commands), "Failed to end stream") return;
}

void gpu_copy_buffers(gpu_stream* stream, gpu_buffer* src, gpu_buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size) {
  vkCmdCopyBuffer(stream->commands, src->handle, dst->handle, 1, &(VkBufferCopy) {
    .srcOffset = src->offset + srcOffset,
    .dstOffset = dst->offset + dstOffset,
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
    .bufferOffset = src->offset + srcOffset,
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
    .bufferOffset = dst->offset + dstOffset,
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
  vkCmdFillBuffer(stream->commands, buffer->handle, buffer->offset + offset, size, 0);
}

void gpu_clear_texture(gpu_stream* stream, gpu_texture* texture, uint16_t layer, uint16_t layerCount, uint16_t level, uint16_t levelCount, float value[4]) {
  VkImageSubresourceRange range = {
    .aspectMask = texture->aspect,
    .baseMipLevel = level,
    .levelCount = levelCount,
    .baseArrayLayer = layer,
    .layerCount = layerCount
  };

  if (texture->aspect == VK_IMAGE_ASPECT_COLOR_BIT) {
    VkClearColorValue clear;
    memcpy(&clear.float32, value, sizeof(clear.float32));
    vkCmdClearColorImage(stream->commands, texture->handle, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
  } else {
    VkClearDepthStencilValue clear;
    clear.depth = value[0];
    clear.stencil = (uint8_t) value[1];
    vkCmdClearDepthStencilImage(stream->commands, texture->handle, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
  }
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

// Entry

bool gpu_init(gpu_config* config) {
  state.config = *config;

  // Load
#ifdef _WIN32
  state.library = LoadLibraryA("vulkan-1.dll");
  CHECK(state.library, "Failed to load vulkan library") return gpu_destroy(), false;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) GetProcAddress(state.library, "vkGetInstanceProcAddr");
#elif __APPLE__
  state.library = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
  CHECK(state.library, "Failed to load vulkan library") return gpu_destroy(), false;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym(state.library, "vkGetInstanceProcAddr");
#else
  state.library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  CHECK(state.library, "Failed to load vulkan library") return gpu_destroy(), false;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym(state.library, "vkGetInstanceProcAddr");
#endif
  GPU_FOREACH_ANONYMOUS(GPU_LOAD_ANONYMOUS);

  { // Instance
    VkInstanceCreateInfo instanceInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &(VkApplicationInfo) {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pEngineName = config->engineName,
        .engineVersion = VK_MAKE_VERSION(config->engineVersion[0], config->engineVersion[1], config->engineVersion[3]),
        .apiVersion = VK_MAKE_VERSION(1, 1, 0)
      },
      .enabledLayerCount = state.config.debug ? 1 : 0,
      .ppEnabledLayerNames = (const char*[]) { "VK_LAYER_KHRONOS_validation" },
      .enabledExtensionCount = state.config.debug ? 1 : 0,
      .ppEnabledExtensionNames = (const char*[]) { "VK_EXT_debug_utils" }
    };

    VK(vkCreateInstance(&instanceInfo, NULL, &state.instance), "Instance creation failed") return gpu_destroy(), false;

    GPU_FOREACH_INSTANCE(GPU_LOAD_INSTANCE);

    if (state.config.debug) {
      VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = relay
      };

      VK(vkCreateDebugUtilsMessengerEXT(state.instance, &messengerInfo, NULL, &state.messenger), "Debug hook setup failed") return gpu_destroy(), false;
    }
  }

  { // Device
    uint32_t deviceCount = 1;
    VK(vkEnumeratePhysicalDevices(state.instance, &deviceCount, &state.adapter), "Physical device enumeration failed") return gpu_destroy(), false;

    VkPhysicalDeviceMultiviewProperties multiviewProperties = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES };
    VkPhysicalDeviceSubgroupProperties subgroupProperties = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES, .pNext = &multiviewProperties };
    VkPhysicalDeviceProperties2 properties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &subgroupProperties };
    vkGetPhysicalDeviceProperties2(state.adapter, &properties2);

    if (config->device) {
      VkPhysicalDeviceProperties* properties = &properties2.properties;
      config->device->deviceId = properties->deviceID;
      config->device->vendorId = properties->vendorID;
      memcpy(config->device->deviceName, properties->deviceName, MIN(sizeof(config->device->deviceName), sizeof(properties->deviceName)));
      config->device->renderer = "Vulkan";
      config->device->subgroupSize = subgroupProperties.subgroupSize;
      config->device->discrete = properties->deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    }

    if (config->limits) {
      VkPhysicalDeviceLimits* limits = &properties2.properties.limits;
      config->limits->textureSize2D = MIN(limits->maxImageDimension2D, UINT16_MAX);
      config->limits->textureSize3D = MIN(limits->maxImageDimension3D, UINT16_MAX);
      config->limits->textureSizeCube = MIN(limits->maxImageDimensionCube, UINT16_MAX);
      config->limits->textureLayers = MIN(limits->maxImageArrayLayers, UINT16_MAX);
      config->limits->renderSize[0] = limits->maxFramebufferWidth;
      config->limits->renderSize[1] = limits->maxFramebufferHeight;
      config->limits->renderSize[2] = multiviewProperties.maxMultiviewViewCount;
      config->limits->uniformBufferRange = limits->maxUniformBufferRange;
      config->limits->storageBufferRange = limits->maxStorageBufferRange;
      config->limits->uniformBufferAlign = limits->minUniformBufferOffsetAlignment;
      config->limits->storageBufferAlign = limits->minStorageBufferOffsetAlignment;
      config->limits->vertexAttributes = limits->maxVertexInputAttributes;
      config->limits->vertexBuffers = limits->maxVertexInputBindings;
      config->limits->vertexBufferStride = MIN(limits->maxVertexInputBindingStride, UINT16_MAX);
      config->limits->vertexShaderOutputs = limits->maxVertexOutputComponents;
      config->limits->clipDistances = limits->maxClipDistances;
      config->limits->cullDistances = limits->maxCullDistances;
      config->limits->clipAndCullDistances = limits->maxCombinedClipAndCullDistances;
      config->limits->computeDispatchCount[0] = limits->maxComputeWorkGroupCount[0];
      config->limits->computeDispatchCount[1] = limits->maxComputeWorkGroupCount[1];
      config->limits->computeDispatchCount[2] = limits->maxComputeWorkGroupCount[2];
      config->limits->computeWorkgroupSize[0] = limits->maxComputeWorkGroupSize[0];
      config->limits->computeWorkgroupSize[1] = limits->maxComputeWorkGroupSize[1];
      config->limits->computeWorkgroupSize[2] = limits->maxComputeWorkGroupSize[2];
      config->limits->computeWorkgroupVolume = limits->maxComputeWorkGroupInvocations;
      config->limits->computeSharedMemory = limits->maxComputeSharedMemorySize;
      config->limits->pushConstantSize = limits->maxPushConstantsSize;
      config->limits->indirectDrawCount = limits->maxDrawIndirectCount;
      config->limits->instances = multiviewProperties.maxMultiviewInstanceIndex;
      config->limits->anisotropy = limits->maxSamplerAnisotropy;
      config->limits->pointSize = limits->pointSizeRange[1];
    }

    VkPhysicalDeviceShaderDrawParameterFeatures shaderDrawParameterFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES
    };

    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
      .pNext = &shaderDrawParameterFeatures
    };

    VkPhysicalDeviceFeatures2 enabledFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &multiviewFeatures
    };

    if (config->features) {
      VkPhysicalDeviceFeatures2 features2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
      VkPhysicalDeviceFeatures* enable = &enabledFeatures.features;
      VkPhysicalDeviceFeatures* supports = &features2.features;
      vkGetPhysicalDeviceFeatures2(state.adapter, &features2);

      // Required features
      enable->fullDrawIndexUint32 = true;
      multiviewFeatures.multiview = true;
      shaderDrawParameterFeatures.shaderDrawParameters = true;

      // Internal features (exposed as limits)
      enable->samplerAnisotropy = supports->samplerAnisotropy;
      enable->multiDrawIndirect = supports->multiDrawIndirect;
      enable->shaderClipDistance = supports->shaderClipDistance;
      enable->shaderCullDistance = supports->shaderCullDistance;
      enable->largePoints = supports->largePoints;

      // Optional features (currently always enabled when supported)
      config->features->textureBC = enable->textureCompressionBC = supports->textureCompressionBC;
      config->features->textureASTC = enable->textureCompressionASTC_LDR = supports->textureCompressionASTC_LDR;
      config->features->wireframe = enable->fillModeNonSolid = supports->fillModeNonSolid;
      config->features->depthClamp = enable->depthClamp = supports->depthClamp;
      config->features->indirectDrawFirstInstance = enable->drawIndirectFirstInstance = supports->drawIndirectFirstInstance;
      config->features->float64 = enable->shaderFloat64 = supports->shaderFloat64;
      config->features->int64 = enable->shaderInt64 = supports->shaderInt64;
      config->features->int16 = enable->shaderInt16 = supports->shaderInt16;

      // Formats
      for (uint32_t i = 0; i < GPU_FORMAT_COUNT; i++) {
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(state.adapter, convertFormat(i, LINEAR), &formatProperties);
        uint32_t renderMask = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        uint32_t flags = formatProperties.optimalTilingFeatures;
        config->features->formats[i] =
          ((flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ? GPU_FEATURE_SAMPLE : 0) |
          ((flags & renderMask) ? GPU_FEATURE_RENDER : 0) |
          ((flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) ? GPU_FEATURE_BLEND : 0) |
          ((flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ? GPU_FEATURE_FILTER : 0) |
          ((flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) ? GPU_FEATURE_STORAGE : 0) |
          ((flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT) ? GPU_FEATURE_ATOMIC : 0) |
          ((flags & VK_FORMAT_FEATURE_BLIT_SRC_BIT) ? GPU_FEATURE_BLIT_SRC : 0) |
          ((flags & VK_FORMAT_FEATURE_BLIT_DST_BIT) ? GPU_FEATURE_BLIT_DST : 0);
      }
    }

    state.queueFamilyIndex = ~0u;
    VkQueueFamilyProperties queueFamilies[8];
    uint32_t queueFamilyCount = COUNTOF(queueFamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(state.adapter, &queueFamilyCount, queueFamilies);
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
      uint32_t mask = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
      if ((queueFamilies[i].queueFlags & mask) == mask) {
        state.queueFamilyIndex = i;
        break;
      }
    }
    CHECK(state.queueFamilyIndex != ~0u, "Queue selection failed") return gpu_destroy(), false;

    VkDeviceCreateInfo deviceInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = config->features ? &enabledFeatures : NULL,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &(VkDeviceQueueCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = state.queueFamilyIndex,
        .pQueuePriorities = &(float) { 1.f },
        .queueCount = 1
      }
    };

    VK(vkCreateDevice(state.adapter, &deviceInfo, NULL, &state.device), "Device creation failed") return gpu_destroy(), false;
    vkGetDeviceQueue(state.device, state.queueFamilyIndex, 0, &state.queue);
    GPU_FOREACH_DEVICE(GPU_LOAD_DEVICE);
  }

  { // Allocators (without VK_KHR_maintenance4, need to create objects to get memory requirements)
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(state.adapter, &memoryProperties);
    VkMemoryType* memoryTypes = memoryProperties.memoryTypes;

    VkMemoryPropertyFlags hostVisible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // Buffers

    struct { VkBufferUsageFlags usage; VkMemoryPropertyFlags flags; } bufferFlags[] = {
      [GPU_MEMORY_BUFFER_GPU] = {
        .usage =
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
      },
      [GPU_MEMORY_BUFFER_CPU_WRITE] = {
        .usage =
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .flags = hostVisible | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
      },
      [GPU_MEMORY_BUFFER_CPU_READ] = {
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .flags = hostVisible | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
      }
    };

    for (uint32_t i = 0; i < COUNTOF(bufferFlags); i++) {
      gpu_allocator* allocator = &state.allocators[i];
      state.allocatorLookup[i] = i;

      VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = bufferFlags[i].usage,
        .size = 4
      };

      VkBuffer buffer;
      VkMemoryRequirements requirements;
      vkCreateBuffer(state.device, &info, NULL, &buffer);
      vkGetBufferMemoryRequirements(state.device, buffer, &requirements);
      vkDestroyBuffer(state.device, buffer, NULL);

      VkMemoryPropertyFlags fallback = i == GPU_MEMORY_BUFFER_GPU ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : hostVisible;

      for (uint32_t j = 0; j < memoryProperties.memoryTypeCount; j++) {
        if (~requirements.memoryTypeBits & (1 << j)) {
          continue;
        }

        if ((memoryTypes[j].propertyFlags & fallback) == fallback) {
          allocator->memoryFlags = memoryTypes[j].propertyFlags;
          allocator->memoryType = j;
          continue;
        }

        if ((memoryTypes[j].propertyFlags & bufferFlags[i].flags) == bufferFlags[i].flags) {
          allocator->memoryFlags = memoryTypes[j].propertyFlags;
          allocator->memoryType = j;
          break;
        }
      }
    }

    // Textures

    VkImageUsageFlags transient = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

    struct { VkFormat format; VkImageUsageFlags usage; } imageFlags[] = {
      [GPU_MEMORY_TEXTURE_COLOR] = { VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_D16] = { VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_D24S8] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_D32F] = { VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_LAZY_COLOR] = { VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | transient },
      [GPU_MEMORY_TEXTURE_LAZY_D16] = { VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient },
      [GPU_MEMORY_TEXTURE_LAZY_D24S8] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient },
      [GPU_MEMORY_TEXTURE_LAZY_D32F] = { VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient }
    };

    uint32_t allocatorCount = GPU_MEMORY_TEXTURE_COLOR;

    for (uint32_t i = GPU_MEMORY_TEXTURE_COLOR; i < COUNTOF(imageFlags); i++) {
      VkImageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = imageFlags[i].format,
        .extent = { 1, 1, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage = imageFlags[i].usage
      };

      VkImage image;
      VkMemoryRequirements requirements;
      vkCreateImage(state.device, &info, NULL, &image);
      vkGetImageMemoryRequirements(state.device, image, &requirements);
      vkDestroyImage(state.device, image, NULL);

      uint16_t memoryType, memoryFlags;
      for (uint32_t j = 0; j < memoryProperties.memoryTypeCount; j++) {
        if ((requirements.memoryTypeBits & (1 << j)) && (memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
          memoryFlags = memoryTypes[j].propertyFlags;
          memoryType = j;
          break;
        }
      }

      // Unlike buffers, we try to merge our texture allocators since all the textures have similar
      // lifetime characteristics, and using less allocators greatly reduces memory usage due to the
      // huge block size for textures.  Basically, only append an allocator if needed.

      bool merged = false;
      for (uint32_t j = GPU_MEMORY_TEXTURE_COLOR; j < allocatorCount; j++) {
        if (memoryType == state.allocators[j].memoryType) {
          state.allocatorLookup[i] = j;
          merged = true;
          break;
        }
      }

      if (!merged) {
        uint32_t index = allocatorCount++;
        state.allocators[index].memoryFlags = memoryFlags;
        state.allocators[index].memoryType = memoryType;
        state.allocatorLookup[i] = index;
      }
    }
  }

  // Ticks
  for (uint32_t i = 0; i < COUNTOF(state.ticks); i++) {
    VkCommandPoolCreateInfo poolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = state.queueFamilyIndex
    };

    VK(vkCreateCommandPool(state.device, &poolInfo, NULL, &state.ticks[i].pool), "Command pool creation failed") return gpu_destroy(), false;

    VkCommandBufferAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = state.ticks[i].pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = COUNTOF(state.ticks[i].streams)
    };

    VkCommandBuffer* commandBuffers = &state.ticks[i].streams[0].commands;
    VK(vkAllocateCommandBuffers(state.device, &allocateInfo, commandBuffers), "Commmand buffer allocation failed") return gpu_destroy(), false;

    VkSemaphoreCreateInfo semaphoreInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VK(vkCreateSemaphore(state.device, &semaphoreInfo, NULL, &state.ticks[i].semaphores[0]), "Semaphore creation failed") return gpu_destroy(), false;
    VK(vkCreateSemaphore(state.device, &semaphoreInfo, NULL, &state.ticks[i].semaphores[1]), "Semaphore creation failed") return gpu_destroy(), false;

    VkFenceCreateInfo fenceInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VK(vkCreateFence(state.device, &fenceInfo, NULL, &state.ticks[i].fence), "Fence creation failed") return gpu_destroy(), false;
  }

  state.tick[CPU] = COUNTOF(state.ticks) - 1;
  return true;
}

void gpu_destroy(void) {
  if (state.device) vkDeviceWaitIdle(state.device);
  state.tick[GPU] = state.tick[CPU];
  expunge();
  if (state.scratchpad[0].buffer) vkDestroyBuffer(state.device, state.scratchpad[0].buffer, NULL);
  if (state.scratchpad[1].buffer) vkDestroyBuffer(state.device, state.scratchpad[0].buffer, NULL);
  for (uint32_t i = 0; i < COUNTOF(state.ticks); i++) {
    gpu_tick* tick = &state.ticks[i];
    if (tick->pool) vkDestroyCommandPool(state.device, tick->pool, NULL);
    if (tick->semaphores[0]) vkDestroySemaphore(state.device, tick->semaphores[0], NULL);
    if (tick->semaphores[1]) vkDestroySemaphore(state.device, tick->semaphores[1], NULL);
    if (tick->fence) vkDestroyFence(state.device, tick->fence, NULL);
  }
  for (uint32_t i = 0; i < COUNTOF(state.memory); i++) {
    if (state.memory[i].handle) vkFreeMemory(state.device, state.memory[i].handle, NULL);
  }
  if (state.device) vkDestroyDevice(state.device, NULL);
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
  VK(vkWaitForFences(state.device, 1, &tick->fence, VK_FALSE, ~0ull), "Fence wait failed") return 0;
  VK(vkResetFences(state.device, 1, &tick->fence), "Fence reset failed") return 0;
  VK(vkResetCommandPool(state.device, tick->pool, 0), "Command pool reset failed") return 0;
  state.tick[GPU] = MAX(state.tick[GPU], state.tick[CPU] - COUNTOF(state.ticks));
  state.streamCount = 0;
  expunge();
  return state.tick[CPU];
}

void gpu_submit(gpu_stream** streams, uint32_t count) {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & TICK_MASK];

  VkCommandBuffer commands[COUNTOF(tick->streams)];
  for (uint32_t i = 0; i < count; i++) {
    commands[i] = streams[i]->commands;
  }

  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = count,
    .pCommandBuffers = commands
  };

  VK(vkQueueSubmit(state.queue, 1, &submit, tick->fence), "Queue submit failed") return;
}

bool gpu_finished(uint32_t tick) {
  return state.tick[GPU] >= tick;
}

void gpu_wait() {
  vkDeviceWaitIdle(state.device);
}

// Helpers

static gpu_memory* gpu_allocate(gpu_memory_type type, VkMemoryRequirements info, VkDeviceSize* offset) {
  gpu_allocator* allocator = &state.allocators[state.allocatorLookup[type]];

  static const uint32_t blockSizes[] = {
    [GPU_MEMORY_BUFFER_GPU] = 1 << 26,
    [GPU_MEMORY_BUFFER_CPU_WRITE] = 0,
    [GPU_MEMORY_BUFFER_CPU_READ] = 0,
    [GPU_MEMORY_TEXTURE_COLOR] = 1 << 28,
    [GPU_MEMORY_TEXTURE_D16] = 1 << 28,
    [GPU_MEMORY_TEXTURE_D24S8] = 1 << 28,
    [GPU_MEMORY_TEXTURE_D32F] = 1 << 28,
    [GPU_MEMORY_TEXTURE_LAZY_COLOR] = 1 << 28,
    [GPU_MEMORY_TEXTURE_LAZY_D16] = 1 << 28,
    [GPU_MEMORY_TEXTURE_LAZY_D24S8] = 1 << 28,
    [GPU_MEMORY_TEXTURE_LAZY_D32F] = 1 << 28
  };

  uint32_t blockSize = blockSizes[type];
  uint32_t cursor = ALIGN(allocator->cursor, info.alignment);

  if (allocator->block && cursor + info.size <= blockSize) {
    allocator->cursor = cursor + info.size;
    allocator->block->refs++;
    *offset = cursor;
    return allocator->block;
  }

  // If there wasn't an active block or it overflowed, find an empty block to allocate
  for (uint32_t i = 0; i < COUNTOF(state.memory); i++) {
    if (!state.memory[i].handle) {
      gpu_memory* memory = &state.memory[i];

      VkMemoryAllocateInfo memoryInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = MAX(blockSize, info.size),
        .memoryTypeIndex = allocator->memoryType
      };

      VK(vkAllocateMemory(state.device, &memoryInfo, NULL, &memory->handle), "Failed to allocate GPU memory") {
        allocator->block = NULL;
        return NULL;
      }

      if (allocator->memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        VK(vkMapMemory(state.device, memory->handle, 0, VK_WHOLE_SIZE, 0, &memory->pointer), "Failed to map memory") {
          vkFreeMemory(state.device, memory->handle, NULL);
          memory->handle = NULL;
          return NULL;
        }
      }

      allocator->block = memory;
      allocator->cursor = info.size;
      *offset = 0;
      return memory;
    }
  }

  check(false, "Out of space for memory blocks");
  return NULL;
}

static void gpu_release(gpu_memory* memory) {
  if (memory && --memory->refs == 0) {
    condemn(memory->handle, VK_OBJECT_TYPE_DEVICE_MEMORY);
    memory->handle = NULL;
  }
}

static void condemn(void* handle, VkObjectType type) {
  if (!handle) return;
  gpu_morgue* morgue = &state.morgue;

  // If the morgue is full, perform an emergency expunge
  if (morgue->head - morgue->tail >= COUNTOF(morgue->data)) {
    vkDeviceWaitIdle(state.device);
    state.tick[GPU] = state.tick[CPU];
    expunge();
  }

  morgue->data[morgue->head++ & MORGUE_MASK] = (gpu_victim) { handle, type, state.tick[CPU] };
}

static void expunge() {
  gpu_morgue* morgue = &state.morgue;
  while (morgue->tail != morgue->head && state.tick[GPU] >= morgue->data[morgue->tail & MORGUE_MASK].tick) {
    gpu_victim* victim = &morgue->data[morgue->tail++ & MORGUE_MASK];
    switch (victim->type) {
      case VK_OBJECT_TYPE_BUFFER: vkDestroyBuffer(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_IMAGE: vkDestroyImage(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_IMAGE_VIEW: vkDestroyImageView(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_SAMPLER: vkDestroySampler(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT: vkDestroyDescriptorSetLayout(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_DEVICE_MEMORY: vkFreeMemory(state.device, victim->handle, NULL); break;
      default: break;
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
  return VK_IMAGE_LAYOUT_UNDEFINED;
}

static VkFormat convertFormat(gpu_texture_format format, int colorspace) {
  static const VkFormat formats[][2] = {
    [GPU_FORMAT_R8] = { VK_FORMAT_R8_UNORM, VK_FORMAT_R8_SRGB },
    [GPU_FORMAT_RG8] = { VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8_SRGB },
    [GPU_FORMAT_RGBA8] = { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB },
    [GPU_FORMAT_R16] = { VK_FORMAT_R16_UNORM, VK_FORMAT_R16_UNORM },
    [GPU_FORMAT_RG16] = { VK_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16_UNORM },
    [GPU_FORMAT_RGBA16] = { VK_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_R16G16B16A16_UNORM },
    [GPU_FORMAT_R16F] = { VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16_SFLOAT },
    [GPU_FORMAT_RG16F] = { VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R16G16_SFLOAT },
    [GPU_FORMAT_RGBA16F] = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT },
    [GPU_FORMAT_R32F] = { VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32_SFLOAT },
    [GPU_FORMAT_RG32F] = { VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32_SFLOAT },
    [GPU_FORMAT_RGBA32F] = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT },
    [GPU_FORMAT_RGB565] = { VK_FORMAT_R5G6B5_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16 },
    [GPU_FORMAT_RGB5A1] = { VK_FORMAT_R5G5B5A1_UNORM_PACK16, VK_FORMAT_R5G5B5A1_UNORM_PACK16 },
    [GPU_FORMAT_RGB10A2] = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2B10G10R10_UNORM_PACK32 },
    [GPU_FORMAT_RG11B10F] = { VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_B10G11R11_UFLOAT_PACK32 },
    [GPU_FORMAT_D16] = { VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM },
    [GPU_FORMAT_D24S8] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
    [GPU_FORMAT_D32F] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT },
    [GPU_FORMAT_BC1] = { VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_BC1_RGB_SRGB_BLOCK },
    [GPU_FORMAT_BC2] = { VK_FORMAT_BC2_UNORM_BLOCK, VK_FORMAT_BC2_SRGB_BLOCK },
    [GPU_FORMAT_BC3] = { VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_BC3_SRGB_BLOCK },
    [GPU_FORMAT_BC4U] = { VK_FORMAT_BC4_UNORM_BLOCK, VK_FORMAT_BC4_UNORM_BLOCK },
    [GPU_FORMAT_BC4S] = { VK_FORMAT_BC4_SNORM_BLOCK, VK_FORMAT_BC4_SNORM_BLOCK },
    [GPU_FORMAT_BC5U] = { VK_FORMAT_BC4_UNORM_BLOCK, VK_FORMAT_BC5_UNORM_BLOCK },
    [GPU_FORMAT_BC5S] = { VK_FORMAT_BC4_SNORM_BLOCK, VK_FORMAT_BC5_SNORM_BLOCK },
    [GPU_FORMAT_BC6UF] = { VK_FORMAT_BC6H_UFLOAT_BLOCK, VK_FORMAT_BC6H_UFLOAT_BLOCK },
    [GPU_FORMAT_BC6SF] = { VK_FORMAT_BC6H_SFLOAT_BLOCK, VK_FORMAT_BC6H_SFLOAT_BLOCK },
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

static VkBool32 relay(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userdata) {
  if (state.config.callback) {
    bool severe = severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    state.config.callback(state.config.userdata, data->pMessage, severe);
  }
  return VK_FALSE;
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

    VK(vkSetDebugUtilsObjectNameEXT(state.device, &info), "Nickname failed") {}
  }
}

static bool vcheck(VkResult result, const char* message) {
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
