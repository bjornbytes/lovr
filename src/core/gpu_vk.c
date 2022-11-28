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
  VkImageAspectFlagBits aspect;
  VkImageLayout layout;
  uint32_t memory;
  uint32_t samples;
  uint32_t layers;
  uint8_t format;
  bool srgb;
};

struct gpu_sampler {
  VkSampler handle;
};

struct gpu_layout {
  VkDescriptorSetLayout handle;
  uint32_t descriptorCounts[7];
};

struct gpu_shader {
  VkShaderModule handles[2];
  VkPipelineLayout pipelineLayout;
};

struct gpu_bundle_pool {
  VkDescriptorPool handle;
};

struct gpu_bundle {
  VkDescriptorSet handle;
};

struct gpu_pipeline {
  VkPipeline handle;
};

struct gpu_tally {
  VkQueryPool handle;
};

struct gpu_stream {
  VkCommandBuffer commands;
};

size_t gpu_sizeof_buffer() { return sizeof(gpu_buffer); }
size_t gpu_sizeof_texture() { return sizeof(gpu_texture); }
size_t gpu_sizeof_sampler() { return sizeof(gpu_sampler); }
size_t gpu_sizeof_layout() { return sizeof(gpu_layout); }
size_t gpu_sizeof_shader() { return sizeof(gpu_shader); }
size_t gpu_sizeof_bundle_pool() { return sizeof(gpu_bundle_pool); }
size_t gpu_sizeof_bundle() { return sizeof(gpu_bundle); }
size_t gpu_sizeof_pipeline() { return sizeof(gpu_pipeline); }
size_t gpu_sizeof_tally() { return sizeof(gpu_tally); }

// Internals

typedef struct {
  VkDeviceMemory handle;
  void* pointer;
  uint32_t refs;
} gpu_memory;

typedef enum {
  GPU_MEMORY_BUFFER_GPU,
  GPU_MEMORY_BUFFER_MAP_STREAM,
  GPU_MEMORY_BUFFER_MAP_STAGING,
  GPU_MEMORY_BUFFER_MAP_READBACK,
  GPU_MEMORY_TEXTURE_COLOR,
  GPU_MEMORY_TEXTURE_D16,
  GPU_MEMORY_TEXTURE_D32F,
  GPU_MEMORY_TEXTURE_D24S8,
  GPU_MEMORY_TEXTURE_D32FS8,
  GPU_MEMORY_TEXTURE_LAZY_COLOR,
  GPU_MEMORY_TEXTURE_LAZY_D16,
  GPU_MEMORY_TEXTURE_LAZY_D32F,
  GPU_MEMORY_TEXTURE_LAZY_D24S8,
  GPU_MEMORY_TEXTURE_LAZY_D32FS8,
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
  gpu_victim data[1024];
} gpu_morgue;

typedef struct {
  uint32_t count;
  uint32_t views;
  uint32_t samples;
  bool resolve;
  struct {
    VkFormat format;
    VkImageLayout layout;
    VkImageLayout resolveLayout;
    gpu_load_op load;
    gpu_save_op save;
  } color[4];
  struct {
    VkFormat format;
    VkImageLayout layout;
    gpu_load_op load;
    gpu_save_op save;
  } depth;
} gpu_pass_info;

typedef struct {
  void* object;
  uint64_t hash;
} gpu_cache_entry;

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
  VkSurfaceKHR surface;
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  VkSurfaceFormatKHR surfaceFormat;
  bool swapchainValid;
  VkSwapchainKHR swapchain;
  VkSemaphore swapchainSemaphore;
  uint32_t currentSwapchainTexture;
  gpu_texture swapchainTextures[8];
  VkPipelineCache pipelineCache;
  VkDebugUtilsMessengerEXT messenger;
  gpu_cache_entry renderpasses[16][4];
  gpu_cache_entry framebuffers[16][4];
  gpu_allocator allocators[GPU_MEMORY_COUNT];
  uint8_t allocatorLookup[GPU_MEMORY_COUNT];
  gpu_scratchpad scratchpad[3];
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
#define HASH_SEED 2166136261

static uint32_t hash32(uint32_t initial, void* data, uint32_t size);
static gpu_memory* gpu_allocate(gpu_memory_type type, VkMemoryRequirements info, VkDeviceSize* offset);
static void gpu_release(gpu_memory* memory);
static void condemn(void* handle, VkObjectType type);
static void expunge(void);
static bool hasLayer(VkLayerProperties* layers, uint32_t count, const char* layer);
static bool hasExtension(VkExtensionProperties* extensions, uint32_t count, const char* extension);
static void createSwapchain(uint32_t width, uint32_t height);
static VkRenderPass getCachedRenderPass(gpu_pass_info* pass, bool exact);
static VkFramebuffer getCachedFramebuffer(VkRenderPass pass, VkImageView images[9], uint32_t imageCount, uint32_t size[2]);
static VkImageLayout getNaturalLayout(uint32_t usage, VkImageAspectFlags aspect);
static VkFormat convertFormat(gpu_texture_format format, int colorspace);
static VkPipelineStageFlags convertPhase(gpu_phase phase, bool dst);
static VkAccessFlags convertCache(gpu_cache cache);
static VkBool32 relay(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userdata);
static void nickname(void* object, VkObjectType type, const char* name);
static bool vcheck(VkResult result, const char* message);
static bool check(bool condition, const char* message);

// Loader

// Functions that don't require an instance
#define GPU_FOREACH_ANONYMOUS(X)\
  X(vkEnumerateInstanceLayerProperties)\
  X(vkEnumerateInstanceExtensionProperties)\
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
  X(vkEnumerateDeviceExtensionProperties)\
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
  X(vkCmdBeginQuery)\
  X(vkCmdEndQuery)\
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
  X(vkGetPipelineCacheData)\
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
    buffer->memory = ~0u;
    buffer->offset = 0;
    nickname(buffer->handle, VK_OBJECT_TYPE_BUFFER, info->label);
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
  buffer->offset = 0;
  return true;
}

void gpu_buffer_destroy(gpu_buffer* buffer) {
  if (buffer->memory == ~0u) return;
  condemn(buffer->handle, VK_OBJECT_TYPE_BUFFER);
  gpu_release(&state.memory[buffer->memory]);
}

// There are 3 mapping modes, which use different strategies/memory types:
// - MAP_STREAM: Used to "stream" data to the GPU, to be read by shaders.  This tries to use the
//   special 256MB memory type present on discrete GPUs because it's both device local and host-
//   visible and that supposedly makes it fast.  A single buffer is allocated with a "zone" for each
//   tick.  If one of the zones fills up, a new bigger buffer is allocated.  It's important to have
//   one buffer and keep it alive since streaming is expected to happen very frequently.
// - MAP_STAGING: Used to stage data to upload to buffers/textures.  Can only be used for transfers.
//   Uses uncached host-visible memory so as to not pollute the CPU cache.  This uses a slightly
//   different allocation strategy where blocks of memory are allocated, linearly allocated from,
//   and condemned once they fill up.  This is because uploads are much less frequent than
//   streaming and are usually too big to fit in the 256MB memory.
// - MAP_READBACK: Used for readbacks.  Uses cached memory when available since reading from
//   uncached memory on the CPU is super duper slow.  Uses the same "zone" system as STREAM, since
//   we want to be able to handle per-frame readbacks without thrashing.
void* gpu_map(gpu_buffer* buffer, uint32_t size, uint32_t align, gpu_map_mode mode) {
  gpu_scratchpad* pool = &state.scratchpad[mode];
  uint32_t cursor = ALIGN(pool->cursor, align);
  uint32_t zone = mode == GPU_MAP_STAGING ? 0 : (state.tick[CPU] & TICK_MASK);

  // If the scratchpad buffer fills up, condemn it and allocate a new/bigger one to use.
  if (cursor + size > pool->size) {
    VkBufferUsageFlags usages[] = {
      [GPU_MAP_STREAM] =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      [GPU_MAP_STAGING] = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      [GPU_MAP_READBACK] = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    };

    VkBufferCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .usage = usages[mode]
    };

    // Staging buffers use 4MB block sizes, stream/download start out at 4MB and double after that
    if (pool->size == 0) {
      pool->size = 1 << 22;
    }

    if (mode == GPU_MAP_STAGING) {
      info.size = MAX(pool->size, size);
    } else {
      while (pool->size < size) {
        pool->size <<= 1;
      }
      info.size = pool->size * COUNTOF(state.ticks);
    }

    VkBuffer handle;
    VK(vkCreateBuffer(state.device, &info, NULL, &handle), "Could not create scratch buffer") return NULL;
    nickname(handle, VK_OBJECT_TYPE_BUFFER, "Scratchpad");

    VkDeviceSize offset;
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(state.device, handle, &requirements);
    gpu_memory* memory = gpu_allocate(GPU_MEMORY_BUFFER_MAP_STREAM + mode, requirements, &offset);

    VK(vkBindBufferMemory(state.device, handle, memory->handle, offset), "Could not bind scratchpad memory") {
      vkDestroyBuffer(state.device, handle, NULL);
      gpu_release(memory);
      return NULL;
    }

    // If this was an oversized allocation, condemn it immediately, don't touch the pool
    if (size > pool->size) {
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
      memoryType = (info->usage & GPU_TEXTURE_TRANSIENT) ? GPU_MEMORY_TEXTURE_LAZY_D16 : GPU_MEMORY_TEXTURE_D16;
      break;
    case GPU_FORMAT_D32F:
      texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
      memoryType = (info->usage & GPU_TEXTURE_TRANSIENT) ? GPU_MEMORY_TEXTURE_LAZY_D32F : GPU_MEMORY_TEXTURE_D32F;
      break;
    case GPU_FORMAT_D24S8:
      texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      memoryType = (info->usage & GPU_TEXTURE_TRANSIENT) ? GPU_MEMORY_TEXTURE_LAZY_D24S8 : GPU_MEMORY_TEXTURE_D24S8;
      break;
    case GPU_FORMAT_D32FS8:
      texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      memoryType = (info->usage & GPU_TEXTURE_TRANSIENT) ? GPU_MEMORY_TEXTURE_LAZY_D32FS8 : GPU_MEMORY_TEXTURE_D32FS8;
      break;
    default:
      texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
      memoryType = (info->usage & GPU_TEXTURE_TRANSIENT) ? GPU_MEMORY_TEXTURE_LAZY_COLOR : GPU_MEMORY_TEXTURE_COLOR;
      break;
  }

  texture->layout = getNaturalLayout(info->usage, texture->aspect);
  texture->layers = type == VK_IMAGE_TYPE_2D ? info->size[2] : 0;
  texture->samples = info->samples;
  texture->format = info->format;
  texture->srgb = info->srgb;

  gpu_texture_view_info viewInfo = {
    .source = texture,
    .type = info->type
  };

  if (info->handle) {
    texture->memory = ~0u;
    texture->handle = (VkImage) info->handle;
    nickname(texture->handle, VK_OBJECT_TYPE_IMAGE, info->label);
    return gpu_texture_init_view(texture, &viewInfo);
  }

  bool depth = texture->aspect & VK_IMAGE_ASPECT_DEPTH_BIT;

  VkImageCreateInfo imageInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = flags,
    .imageType = type,
    .format = convertFormat(texture->format, texture->srgb),
    .extent.width = info->size[0],
    .extent.height = info->size[1],
    .extent.depth = texture->layers ? 1 : info->size[2],
    .mipLevels = info->mipmaps,
    .arrayLayers = texture->layers ? texture->layers : 1,
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

  VK(vkBindImageMemory(state.device, texture->handle, memory->handle, offset), "Could not bind texture memory") {
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
          .imageSubresource.layerCount = texture->layers ? info->size[2] : 1,
          .imageExtent.width = MAX(info->size[0] >> i, 1),
          .imageExtent.height = MAX(info->size[1] >> i, 1),
          .imageExtent.depth = texture->layers ? 1 : MAX(info->size[2] >> i, 1)
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
              .layerCount = texture->layers ? info->size[2] : 1
            },
            .dstSubresource = {
              .aspectMask = texture->aspect,
              .mipLevel = i,
              .layerCount = texture->layers ? info->size[2] : 1
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
    texture->memory = ~0u;
    texture->aspect = info->source->aspect;
    texture->layout = info->source->layout;
    texture->samples = info->source->samples;
    texture->layers = info->layerCount ? info->layerCount : (info->source->layers - info->layerIndex);
    texture->format = info->source->format;
    texture->srgb = info->source->srgb;
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
    .format = convertFormat(texture->format, texture->srgb),
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
  if (texture->memory == ~0u) return;
  condemn(texture->handle, VK_OBJECT_TYPE_IMAGE);
  gpu_release(state.memory + texture->memory);
}

gpu_texture* gpu_surface_acquire() {
  if (!state.swapchainValid) {
    return NULL;
  }

  gpu_tick* tick = &state.ticks[state.tick[CPU] & TICK_MASK];
  VkResult result = vkAcquireNextImageKHR(state.device, state.swapchain, UINT64_MAX, tick->semaphores[0], VK_NULL_HANDLE, &state.currentSwapchainTexture);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    state.currentSwapchainTexture = ~0u;
    state.swapchainValid = false;
    return NULL;
  } else {
    vcheck(result, "Failed to acquire swapchain");
  }

  state.swapchainSemaphore = tick->semaphores[0];
  return &state.swapchainTextures[state.currentSwapchainTexture];
}

void gpu_surface_resize(uint32_t width, uint32_t height) {
  createSwapchain(width, height);
}

// The barriers here are a bit lazy (oversynchronized) and can be improved
void gpu_xr_acquire(gpu_stream* stream, gpu_texture* texture) {
  VkImageLayout attachmentLayout = texture->aspect == VK_IMAGE_ASPECT_COLOR_BIT ?
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // If the texture only has the RENDER usage, its natural layout matches the layout that OpenXR
  // gives us the texture in, so no layout transition is needed.
  if (texture->layout == attachmentLayout) {
    return;
  }

  VkImageMemoryBarrier transition = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
    .oldLayout = attachmentLayout,
    .newLayout = texture->layout,
    .image = texture->handle,
    .subresourceRange.aspectMask = texture->aspect,
    .subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS,
    .subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS
  };

  VkPipelineStageFlags prev = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags next = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

  vkCmdPipelineBarrier(stream->commands, prev, next, 0, 0, NULL, 0, NULL, 1, &transition);
}

// The barriers here are a bit lazy (oversynchronized) and can be improved
void gpu_xr_release(gpu_stream* stream, gpu_texture* texture) {
  VkImageLayout attachmentLayout = texture->aspect == VK_IMAGE_ASPECT_COLOR_BIT ?
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // If the texture only has the RENDER usage, its natural layout matches the layout that OpenXR
  // expects the texture to be in, so no layout transition is needed.
  if (texture->layout == attachmentLayout) {
    return;
  }

  VkImageMemoryBarrier transition = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
    .dstAccessMask = 0,
    .oldLayout = texture->layout,
    .newLayout = attachmentLayout,
    .image = texture->handle,
    .subresourceRange.aspectMask = texture->aspect,
    .subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS,
    .subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS
  };

  VkPipelineStageFlags prev = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  VkPipelineStageFlags next = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

  vkCmdPipelineBarrier(stream->commands, prev, next, 0, 0, NULL, 0, NULL, 1, &transition);
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
      .descriptorCount = 1,
      .stageFlags = info->slots[i].stages == GPU_STAGE_ALL ? VK_SHADER_STAGE_ALL :
        (((info->slots[i].stages & GPU_STAGE_VERTEX) ? VK_SHADER_STAGE_VERTEX_BIT : 0) |
        ((info->slots[i].stages & GPU_STAGE_FRAGMENT) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0) |
        ((info->slots[i].stages & GPU_STAGE_COMPUTE) ? VK_SHADER_STAGE_COMPUTE_BIT : 0))
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
    layout->descriptorCounts[info->slots[i].type]++;
  }

  return true;
}

void gpu_layout_destroy(gpu_layout* layout) {
  condemn(layout->handle, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
}

// Shader

bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info) {
  for (uint32_t i = 0; i < COUNTOF(info->stages) && info->stages[i].code; i++) {
    VkShaderModuleCreateInfo moduleInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = info->stages[i].length,
      .pCode = info->stages[i].code
    };

    VK(vkCreateShaderModule(state.device, &moduleInfo, NULL, &shader->handles[i]), "Failed to load shader") {
      return false;
    }
  }

  VkDescriptorSetLayout layouts[4];
  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pSetLayouts = layouts,
    .pushConstantRangeCount = info->pushConstantSize > 0,
    .pPushConstantRanges = &(VkPushConstantRange) {
      .stageFlags = info->stages[1].code ?
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT :
        VK_SHADER_STAGE_COMPUTE_BIT,
      .offset = 0,
      .size = info->pushConstantSize
    }
  };

  for (uint32_t i = 0; i < COUNTOF(info->layouts) && info->layouts[i]; i++) {
    layouts[i] = info->layouts[i]->handle;
    pipelineLayoutInfo.setLayoutCount++;
  }

  VK(vkCreatePipelineLayout(state.device, &pipelineLayoutInfo, NULL, &shader->pipelineLayout), "Failed to create pipeline layout") {
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

// Bundles

bool gpu_bundle_pool_init(gpu_bundle_pool* pool, gpu_bundle_pool_info* info) {
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
    for (uint32_t i = 0; i < COUNTOF(sizes); i++) {
      sizes[i].descriptorCount = info->layout->descriptorCounts[i] * info->count;
    }
  } else {
    for (uint32_t i = 0; i < info->count; i++) {
      for (uint32_t j = 0; j < COUNTOF(sizes); j++) {
        sizes[j].descriptorCount += info->contents[i].layout->descriptorCounts[j];
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

  VK(vkCreateDescriptorPool(state.device, &poolInfo, NULL, &pool->handle), "Could not create bundle pool") {
    return false;
  }

  VkDescriptorSetLayout layouts[512];
  for (uint32_t i = 0; i < info->count; i+= COUNTOF(layouts)) {
    uint32_t chunk = MIN(info->count - i, COUNTOF(layouts));

    for (uint32_t j = 0; j < chunk; j++) {
      layouts[j] = info->layout ? info->layout->handle : info->contents[i + j].layout->handle;
    }

    VkDescriptorSetAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool->handle,
      .descriptorSetCount = chunk,
      .pSetLayouts = layouts
    };

    VK(vkAllocateDescriptorSets(state.device, &allocateInfo, &info->bundles[i].handle), "Could not allocate descriptor sets") {
      gpu_bundle_pool_destroy(pool);
      return false;
    }
  }

  return true;
}

void gpu_bundle_pool_destroy(gpu_bundle_pool* pool) {
  condemn(pool->handle, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
}

void gpu_bundle_write(gpu_bundle** bundles, gpu_bundle_info* infos, uint32_t count) {
  VkDescriptorBufferInfo buffers[256];
  VkDescriptorImageInfo images[256];
  VkWriteDescriptorSet writes[256];
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
    gpu_bundle_info* info = &infos[i];
    for (uint32_t j = 0; j < info->count; j++) {
      gpu_binding* binding = &info->bindings[j];
      VkDescriptorType type = types[binding->type];
      bool texture = type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      bool sampler = type == VK_DESCRIPTOR_TYPE_SAMPLER;
      bool image = texture || sampler;

      writes[writeCount++] = (VkWriteDescriptorSet) {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = bundles[i]->handle,
        .dstBinding = binding->number,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = type,
        .pBufferInfo = &buffers[bufferCount],
        .pImageInfo = &images[imageCount]
      };

      if (sampler) {
        images[imageCount++] = (VkDescriptorImageInfo) {
          .sampler = binding->sampler->handle
        };
      } else if (texture) {
        images[imageCount++] = (VkDescriptorImageInfo) {
          .imageView = binding->texture->view,
          .imageLayout = binding->texture->layout
        };
      } else {
        buffers[bufferCount++] = (VkDescriptorBufferInfo) {
          .buffer = binding->buffer.object->handle,
          .offset = binding->buffer.offset + binding->buffer.object->offset,
          .range = binding->buffer.extent
        };
      }

      if ((image ? imageCount >= COUNTOF(images) : bufferCount >= COUNTOF(buffers)) || writeCount >= COUNTOF(writes)) {
        vkUpdateDescriptorSets(state.device, writeCount, writes, 0, NULL);
        bufferCount = imageCount = writeCount = 0;
      }
    }
  }

  if (writeCount > 0) {
    vkUpdateDescriptorSets(state.device, writeCount, writes, 0, NULL);
  }
}

// Pipeline

bool gpu_pipeline_init_graphics(gpu_pipeline* pipeline, gpu_pipeline_info* info) {
  static const VkPrimitiveTopology topologies[] = {
    [GPU_DRAW_POINTS] = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    [GPU_DRAW_LINES] = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    [GPU_DRAW_TRIANGLES] = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
  };

  static const VkFormat attributeTypes[] = {
    [GPU_TYPE_I8x4] = VK_FORMAT_R8G8B8A8_SINT,
    [GPU_TYPE_U8x4] = VK_FORMAT_R8G8B8A8_UINT,
    [GPU_TYPE_SN8x4] = VK_FORMAT_R8G8B8A8_SNORM,
    [GPU_TYPE_UN8x4] = VK_FORMAT_R8G8B8A8_UNORM,
    [GPU_TYPE_UN10x3] = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
    [GPU_TYPE_I16] = VK_FORMAT_R16_SINT,
    [GPU_TYPE_I16x2] = VK_FORMAT_R16G16_SINT,
    [GPU_TYPE_I16x4] = VK_FORMAT_R16G16B16A16_SINT,
    [GPU_TYPE_U16] = VK_FORMAT_R16_UINT,
    [GPU_TYPE_U16x2] = VK_FORMAT_R16G16_UINT,
    [GPU_TYPE_U16x4] = VK_FORMAT_R16G16B16A16_UINT,
    [GPU_TYPE_SN16x2] = VK_FORMAT_R16G16_SNORM,
    [GPU_TYPE_SN16x4] = VK_FORMAT_R16G16B16A16_SNORM,
    [GPU_TYPE_UN16x2] = VK_FORMAT_R16G16_UNORM,
    [GPU_TYPE_UN16x4] = VK_FORMAT_R16G16B16A16_UNORM,
    [GPU_TYPE_I32] = VK_FORMAT_R32_SINT,
    [GPU_TYPE_I32x2] = VK_FORMAT_R32G32_SINT,
    [GPU_TYPE_I32x3] = VK_FORMAT_R32G32B32_SINT,
    [GPU_TYPE_I32x4] = VK_FORMAT_R32G32B32A32_SINT,
    [GPU_TYPE_U32] = VK_FORMAT_R32_UINT,
    [GPU_TYPE_U32x2] = VK_FORMAT_R32G32_UINT,
    [GPU_TYPE_U32x3] = VK_FORMAT_R32G32B32_UINT,
    [GPU_TYPE_U32x4] = VK_FORMAT_R32G32B32A32_UINT,
    [GPU_TYPE_F16x2] = VK_FORMAT_R16G16_SFLOAT,
    [GPU_TYPE_F16x4] = VK_FORMAT_R16G16B16A16_SFLOAT,
    [GPU_TYPE_F32] = VK_FORMAT_R32_SFLOAT,
    [GPU_TYPE_F32x2] = VK_FORMAT_R32G32_SFLOAT,
    [GPU_TYPE_F32x3] = VK_FORMAT_R32G32B32_SFLOAT,
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

  VkVertexInputBindingDescription vertexBuffers[16];
  for (uint32_t i = 0; i < info->vertex.bufferCount; i++) {
    vertexBuffers[i] = (VkVertexInputBindingDescription) {
      .binding = i,
      .stride = info->vertex.bufferStrides[i],
      .inputRate = (info->vertex.instancedBuffers & (1 << i)) ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX
    };
  }

  VkVertexInputAttributeDescription vertexAttributes[COUNTOF(info->vertex.attributes)];
  for (uint32_t i = 0; i < info->vertex.attributeCount; i++) {
    vertexAttributes[i] = (VkVertexInputAttributeDescription) {
      .location = info->vertex.attributes[i].location,
      .binding = info->vertex.attributes[i].buffer,
      .format = attributeTypes[info->vertex.attributes[i].type],
      .offset = info->vertex.attributes[i].offset
    };
  }

  VkPipelineVertexInputStateCreateInfo vertexInput = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = info->vertex.bufferCount,
    .pVertexBindingDescriptions = vertexBuffers,
    .vertexAttributeDescriptionCount = info->vertex.attributeCount,
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
    .depthClampEnable = info->rasterizer.depthClamp,
    .polygonMode = info->rasterizer.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
    .cullMode = cullModes[info->rasterizer.cullMode],
    .frontFace = frontFaces[info->rasterizer.winding],
    .depthBiasEnable = info->rasterizer.depthOffset != 0.f || info->rasterizer.depthOffsetSloped != 0.f,
    .depthBiasConstantFactor = info->rasterizer.depthOffset,
    .depthBiasSlopeFactor = info->rasterizer.depthOffsetSloped,
    .lineWidth = 1.f
  };

  VkPipelineMultisampleStateCreateInfo multisample = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = info->multisample.count,
    .alphaToCoverageEnable = info->multisample.alphaToCoverage,
    .alphaToOneEnable = info->multisample.alphaToOne
  };

  VkStencilOpState stencil = {
    .failOp = stencilOps[info->stencil.failOp],
    .passOp = stencilOps[info->stencil.passOp],
    .depthFailOp = stencilOps[info->stencil.depthFailOp],
    .compareOp = compareOps[info->stencil.test],
    .compareMask = info->stencil.testMask,
    .writeMask = info->stencil.writeMask,
    .reference = info->stencil.value
  };

  VkPipelineDepthStencilStateCreateInfo depthStencil = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = info->depth.test != GPU_COMPARE_NONE,
    .depthWriteEnable = info->depth.write,
    .depthCompareOp = compareOps[info->depth.test],
    .stencilTestEnable =
      info->stencil.test != GPU_COMPARE_NONE ||
      info->stencil.failOp != GPU_STENCIL_KEEP ||
      info->stencil.passOp != GPU_STENCIL_KEEP ||
      info->stencil.depthFailOp != GPU_STENCIL_KEEP,
    .front = stencil,
    .back = stencil
  };

  VkPipelineColorBlendAttachmentState colorAttachments[4];
  for (uint32_t i = 0; i < info->attachmentCount; i++) {
    colorAttachments[i] = (VkPipelineColorBlendAttachmentState) {
      .blendEnable = info->color[i].blend.enabled,
      .srcColorBlendFactor = blendFactors[info->color[i].blend.color.src],
      .dstColorBlendFactor = blendFactors[info->color[i].blend.color.dst],
      .colorBlendOp = blendOps[info->color[i].blend.color.op],
      .srcAlphaBlendFactor = blendFactors[info->color[i].blend.alpha.src],
      .dstAlphaBlendFactor = blendFactors[info->color[i].blend.alpha.dst],
      .alphaBlendOp = blendOps[info->color[i].blend.alpha.op],
      .colorWriteMask = info->color[i].mask
    };
  }

  VkPipelineColorBlendStateCreateInfo colorBlend = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = info->attachmentCount,
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
  CHECK(info->flagCount <= COUNTOF(constants), "Too many specialization constants") return false;

  for (uint32_t i = 0; i < info->flagCount; i++) {
    gpu_shader_flag* flag = &info->flags[i];

    switch (flag->type) {
      case GPU_FLAG_B32: constants[i] = flag->value == 0. ? VK_FALSE : VK_TRUE; break;
      case GPU_FLAG_I32: constants[i] = (uint32_t) flag->value; break;
      case GPU_FLAG_U32: constants[i] = (uint32_t) flag->value; break;
      case GPU_FLAG_F32: memcpy(&constants[i], &(float) { flag->value }, sizeof(float)); break;
      default: flag->value = 0;
    }

    entries[i] = (VkSpecializationMapEntry) {
      .constantID = flag->id,
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

  VkPipelineShaderStageCreateInfo shaders[2] = {
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = info->shader->handles[0],
      .pName = "main",
      .pSpecializationInfo = &specialization
    },
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = info->shader->handles[1],
      .pName = "main",
      .pSpecializationInfo = &specialization
    }
  };

  bool resolve = info->multisample.count > 1;
  bool depth = info->depth.format;

  gpu_pass_info pass = {
    .count = (info->attachmentCount << resolve) + depth,
    .views = info->viewCount,
    .samples = info->multisample.count,
    .resolve = resolve,
    .depth.format = convertFormat(info->depth.format, LINEAR),
    .depth.layout = info->depth.format ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
    .depth.load = GPU_LOAD_OP_CLEAR,
    .depth.save = GPU_SAVE_OP_DISCARD
  };

  for (uint32_t i = 0; i < info->attachmentCount; i++) {
    pass.color[i].format = convertFormat(info->color[i].format, info->color[i].srgb);
    pass.color[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    pass.color[i].resolveLayout = resolve ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    pass.color[i].load = GPU_LOAD_OP_CLEAR;
    pass.color[i].save = GPU_SAVE_OP_KEEP;
  }

  VkGraphicsPipelineCreateInfo pipelineInfo = (VkGraphicsPipelineCreateInfo) {
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
    .layout = info->shader->pipelineLayout,
    .renderPass = getCachedRenderPass(&pass, false)
  };

  VK(vkCreateGraphicsPipelines(state.device, state.pipelineCache, 1, &pipelineInfo, NULL, &pipeline->handle), "Could not create pipeline") {
    return false;
  }

  nickname(pipeline->handle, VK_OBJECT_TYPE_PIPELINE, info->label);
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
      .constantID = flag->id,
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

  VK(vkCreateComputePipelines(state.device, state.pipelineCache, 1, &pipelineInfo, NULL, &pipeline->handle), "Could not create compute pipeline") {
    return false;
  }

  nickname(pipeline->handle, VK_OBJECT_TYPE_PIPELINE, info->label);
  return true;
}

void gpu_pipeline_destroy(gpu_pipeline* pipeline) {
  condemn(pipeline->handle, VK_OBJECT_TYPE_PIPELINE);
}

void gpu_pipeline_get_cache(void* data, size_t* size) {
  if (vkGetPipelineCacheData(state.device, state.pipelineCache, size, data) != VK_SUCCESS) {
    *size = 0;
  }
}

// Tally

bool gpu_tally_init(gpu_tally* tally, gpu_tally_info* info) {
  VkQueryType queryTypes[] = {
    [GPU_TALLY_TIME] = VK_QUERY_TYPE_TIMESTAMP,
    [GPU_TALLY_SHADER] = VK_QUERY_TYPE_PIPELINE_STATISTICS,
    [GPU_TALLY_PIXEL] = VK_QUERY_TYPE_OCCLUSION
  };

  VkQueryPoolCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
    .queryType = queryTypes[info->type],
    .queryCount = info->count,
    .pipelineStatistics = info->type == GPU_TALLY_SHADER ? (
      VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
      VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
      VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT
    ) : 0
  };

  VK(vkCreateQueryPool(state.device, &createInfo, NULL, &tally->handle), "Could not create query pool") {
    return false;
  }

  return true;
}

void gpu_tally_destroy(gpu_tally* tally) {
  condemn(tally->handle, VK_OBJECT_TYPE_QUERY_POOL);
}

// Stream

gpu_stream* gpu_stream_begin(const char* label) {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & TICK_MASK];
  CHECK(state.streamCount < COUNTOF(tick->streams), "Too many passes") return NULL;
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

void gpu_render_begin(gpu_stream* stream, gpu_canvas* canvas) {
  gpu_texture* texture = canvas->color[0].texture ? canvas->color[0].texture : canvas->depth.texture;

  gpu_pass_info pass = {
    .views = texture->layers,
    .samples = texture->samples,
    .resolve = !!canvas->color[0].resolve
  };

  VkImageView images[9];
  VkClearValue clears[9];

  for (uint32_t i = 0; i < COUNTOF(canvas->color) && canvas->color[i].texture; i++) {
    images[i] = canvas->color[i].texture->view;
    memcpy(clears[i].color.float32, canvas->color[i].clear, 4 * sizeof(float));
    pass.color[i].format = convertFormat(canvas->color[i].texture->format, canvas->color[i].texture->srgb);
    pass.color[i].layout = canvas->color[i].texture->layout;
    pass.color[i].load = canvas->color[i].load;
    pass.color[i].save = canvas->color[i].save;
    pass.count++;
  }

  if (pass.resolve) {
    for (uint32_t i = 0; i < pass.count; i++) {
      images[pass.count + i] = canvas->color[i].resolve->view;
      pass.color[i].resolveLayout = canvas->color[i].resolve->layout;
    }
    pass.count <<= 1;
  }

  if (canvas->depth.texture) {
    uint32_t index = pass.count++;
    images[index] = canvas->depth.texture->view;
    clears[index].depthStencil.depth = canvas->depth.clear.depth;
    clears[index].depthStencil.stencil = canvas->depth.clear.stencil;
    pass.depth.format = convertFormat(canvas->depth.texture->format, LINEAR);
    pass.depth.layout = canvas->depth.texture->layout;
    pass.depth.load = canvas->depth.load;
    pass.depth.save = canvas->depth.save;
  }

  VkRenderPass renderPass = getCachedRenderPass(&pass, true);
  VkFramebuffer framebuffer = getCachedFramebuffer(renderPass, images, pass.count, canvas->size);

  VkRenderPassBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = renderPass,
    .framebuffer = framebuffer,
    .renderArea = { { 0, 0 }, { canvas->size[0], canvas->size[1] } },
    .clearValueCount = pass.count,
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

void gpu_push_constants(gpu_stream* stream, gpu_shader* shader, void* data, uint32_t size) {
  VkShaderStageFlags stages = shader->handles[1] ? (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT) : VK_SHADER_STAGE_COMPUTE_BIT;
  vkCmdPushConstants(stream->commands, shader->pipelineLayout, stages, 0, size, data);
}

void gpu_bind_pipeline(gpu_stream* stream, gpu_pipeline* pipeline, bool compute) {
  VkPipelineBindPoint bindPoint = compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkCmdBindPipeline(stream->commands, bindPoint, pipeline->handle);
}

void gpu_bind_bundles(gpu_stream* stream, gpu_shader* shader, gpu_bundle** bundles, uint32_t first, uint32_t count, uint32_t* dynamicOffsets, uint32_t dynamicOffsetCount) {
  VkDescriptorSet sets[COUNTOF(((gpu_shader_info*) NULL)->layouts)];
  for (uint32_t i = 0; i < count; i++) {
    sets[i] = bundles[i]->handle;
  }
  VkPipelineBindPoint bindPoint = shader->handles[1] ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE;
  vkCmdBindDescriptorSets(stream->commands, bindPoint, shader->pipelineLayout, first, count, sets, dynamicOffsetCount, dynamicOffsets);
}

void gpu_bind_vertex_buffers(gpu_stream* stream, gpu_buffer** buffers, uint32_t* offsets, uint32_t first, uint32_t count) {
  VkBuffer handles[COUNTOF(((gpu_pipeline_info*) NULL)->vertex.bufferStrides)];
  uint64_t offsets64[COUNTOF(handles)];
  for (uint32_t i = 0; i < count; i++) {
    handles[i] = buffers[i]->handle;
    offsets64[i] = buffers[i]->offset + (offsets ? offsets[i] : 0);
  }
  vkCmdBindVertexBuffers(stream->commands, first, count, handles, offsets64);
}

void gpu_bind_index_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, gpu_index_type type) {
  vkCmdBindIndexBuffer(stream->commands, buffer->handle, buffer->offset + offset, (VkIndexType) type);
}

void gpu_draw(gpu_stream* stream, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t baseInstance) {
  vkCmdDraw(stream->commands, vertexCount, instanceCount, firstVertex, baseInstance);
}

void gpu_draw_indexed(gpu_stream* stream, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex, uint32_t baseInstance) {
  vkCmdDrawIndexed(stream->commands, indexCount, instanceCount, firstIndex, baseVertex, baseInstance);
}

void gpu_draw_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount, uint32_t stride) {
  vkCmdDrawIndirect(stream->commands, buffer->handle, buffer->offset + offset, drawCount, stride ? stride : 16);
}

void gpu_draw_indirect_indexed(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount, uint32_t stride) {
  vkCmdDrawIndexedIndirect(stream->commands, buffer->handle, buffer->offset + offset, drawCount, stride ? stride : 20);
}

void gpu_compute(gpu_stream* stream, uint32_t x, uint32_t y, uint32_t z) {
  vkCmdDispatch(stream->commands, x, y, z);
}

void gpu_compute_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset) {
  vkCmdDispatchIndirect(stream->commands, buffer->handle, buffer->offset + offset);
}

void gpu_copy_buffers(gpu_stream* stream, gpu_buffer* src, gpu_buffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size) {
  vkCmdCopyBuffer(stream->commands, src->handle, dst->handle, 1, &(VkBufferCopy) {
    .srcOffset = src->offset + srcOffset,
    .dstOffset = dst->offset + dstOffset,
    .size = size
  });
}

void gpu_copy_textures(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t size[3]) {
  vkCmdCopyImage(stream->commands, src->handle, VK_IMAGE_LAYOUT_GENERAL, dst->handle, VK_IMAGE_LAYOUT_GENERAL, 1, &(VkImageCopy) {
    .srcSubresource = {
      .aspectMask = src->aspect,
      .mipLevel = srcOffset[3],
      .baseArrayLayer = src->layers ? srcOffset[2] : 0,
      .layerCount = src->layers ? size[2] : 1
    },
    .dstSubresource = {
      .aspectMask = dst->aspect,
      .mipLevel = dstOffset[3],
      .baseArrayLayer = dst->layers ? dstOffset[2] : 0,
      .layerCount = dst->layers ? size[2] : 1
    },
    .srcOffset = { srcOffset[0], srcOffset[1], src->layers ? 0 : srcOffset[2] },
    .dstOffset = { dstOffset[0], dstOffset[1], dst->layers ? 0 : dstOffset[2] },
    .extent = { size[0], size[1], size[2] }
  });
}

void gpu_copy_buffer_texture(gpu_stream* stream, gpu_buffer* src, gpu_texture* dst, uint32_t srcOffset, uint32_t dstOffset[4], uint32_t extent[3]) {
  VkBufferImageCopy region = {
    .bufferOffset = src->offset + srcOffset,
    .imageSubresource.aspectMask = dst->aspect,
    .imageSubresource.mipLevel = dstOffset[3],
    .imageSubresource.baseArrayLayer = dst->layers ? dstOffset[2] : 0,
    .imageSubresource.layerCount = dst->layers ? extent[2] : 1,
    .imageOffset = { dstOffset[0], dstOffset[1], dst->layers ? 0 : dstOffset[2] },
    .imageExtent = { extent[0], extent[1], dst->layers ? 1 : extent[2] }
  };

  vkCmdCopyBufferToImage(stream->commands, src->handle, dst->handle, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
}

void gpu_copy_texture_buffer(gpu_stream* stream, gpu_texture* src, gpu_buffer* dst, uint32_t srcOffset[4], uint32_t dstOffset, uint32_t extent[3]) {
  VkBufferImageCopy region = {
    .bufferOffset = dst->offset + dstOffset,
    .imageSubresource.aspectMask = src->aspect,
    .imageSubresource.mipLevel = srcOffset[3],
    .imageSubresource.baseArrayLayer = src->layers ? srcOffset[2] : 0,
    .imageSubresource.layerCount = src->layers ? extent[2] : 1,
    .imageOffset = { srcOffset[0], srcOffset[1], src->layers ? 0 : srcOffset[2] },
    .imageExtent = { extent[0], extent[1], src->layers ? 1 : extent[2] }
  };

  vkCmdCopyImageToBuffer(stream->commands, src->handle, VK_IMAGE_LAYOUT_GENERAL, dst->handle, 1, &region);
}

void gpu_copy_tally_buffer(gpu_stream* stream, gpu_tally* src, gpu_buffer* dst, uint32_t srcIndex, uint32_t dstOffset, uint32_t count, uint32_t stride) {
  vkCmdCopyQueryPoolResults(stream->commands, src->handle, srcIndex, count, dst->handle, dst->offset + dstOffset, stride, VK_QUERY_RESULT_WAIT_BIT);
}

void gpu_clear_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t size) {
  vkCmdFillBuffer(stream->commands, buffer->handle, buffer->offset + offset, size, 0);
}

void gpu_clear_texture(gpu_stream* stream, gpu_texture* texture, float value[4], uint32_t layer, uint32_t layerCount, uint32_t level, uint32_t levelCount) {
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

void gpu_clear_tally(gpu_stream* stream, gpu_tally* tally, uint32_t index, uint32_t count) {
  vkCmdResetQueryPool(stream->commands, tally->handle, index, count);
}

void gpu_blit(gpu_stream* stream, gpu_texture* src, gpu_texture* dst, uint32_t srcOffset[4], uint32_t dstOffset[4], uint32_t srcExtent[3], uint32_t dstExtent[3], gpu_filter filter) {
  VkImageBlit region = {
    .srcSubresource = {
      .aspectMask = src->aspect,
      .mipLevel = srcOffset[3],
      .baseArrayLayer = src->layers ? srcOffset[2] : 0,
      .layerCount = src->layers ? srcExtent[2] : 1
    },
    .dstSubresource = {
      .aspectMask = dst->aspect,
      .mipLevel = dstOffset[3],
      .baseArrayLayer = dst->layers ? dstOffset[2] : 0,
      .layerCount = dst->layers ? dstExtent[2] : 1
    },
    .srcOffsets[0] = { srcOffset[0], srcOffset[1], src->layers ? 0 : srcOffset[2] },
    .dstOffsets[0] = { dstOffset[0], dstOffset[1], dst->layers ? 0 : dstOffset[2] },
    .srcOffsets[1] = { srcOffset[0] + srcExtent[0], srcOffset[1] + srcExtent[1], src->layers ? 1 : srcOffset[2] + srcExtent[2] },
    .dstOffsets[1] = { dstOffset[0] + dstExtent[0], dstOffset[1] + dstExtent[1], dst->layers ? 1 : dstOffset[2] + dstExtent[2] }
  };

  static const VkFilter filters[] = {
    [GPU_FILTER_NEAREST] = VK_FILTER_NEAREST,
    [GPU_FILTER_LINEAR] = VK_FILTER_LINEAR
  };

  vkCmdBlitImage(stream->commands, src->handle, VK_IMAGE_LAYOUT_GENERAL, dst->handle, VK_IMAGE_LAYOUT_GENERAL, 1, &region, filters[filter]);
}

void gpu_sync(gpu_stream* stream, gpu_barrier* barriers, uint32_t count) {
  VkMemoryBarrier memoryBarrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER };
  VkPipelineStageFlags src = 0;
  VkPipelineStageFlags dst = 0;

  for (uint32_t i = 0; i < count; i++) {
    gpu_barrier* barrier = &barriers[i];
    src |= convertPhase(barrier->prev, false);
    dst |= convertPhase(barrier->next, true);
    memoryBarrier.srcAccessMask |= convertCache(barrier->flush);
    memoryBarrier.dstAccessMask |= convertCache(barrier->clear);
  }

  if (src && dst) {
    vkCmdPipelineBarrier(stream->commands, src, dst, 0, 1, &memoryBarrier, 0, NULL, 0, NULL);
  }
}

void gpu_tally_begin(gpu_stream* stream, gpu_tally* tally, uint32_t index) {
  vkCmdBeginQuery(stream->commands, tally->handle, index, 0);
}

void gpu_tally_end(gpu_stream* stream, gpu_tally* tally, uint32_t index) {
  vkCmdEndQuery(stream->commands, tally->handle, index);
}

void gpu_tally_mark(gpu_stream* stream, gpu_tally* tally, uint32_t index) {
  vkCmdWriteTimestamp(stream->commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, tally->handle, index);
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
  state.library = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!state.library) state.library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  CHECK(state.library, "Failed to load vulkan library") return gpu_destroy(), false;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym(state.library, "vkGetInstanceProcAddr");
#endif
  GPU_FOREACH_ANONYMOUS(GPU_LOAD_ANONYMOUS);

  { // Instance
    struct {
      bool validation;
      bool portability;
      bool debug;
    } supports = { 0 };

    // Layers

    struct { const char* name; bool shouldEnable; bool* isEnabled; } layers[] = {
      { "VK_LAYER_KHRONOS_validation", config->debug, &supports.validation }
    };

    const char* enabledLayers[1];
    uint32_t enabledLayerCount = 0;

    VkLayerProperties layerInfo[32];
    uint32_t count = COUNTOF(layerInfo);
    VK(vkEnumerateInstanceLayerProperties(&count, layerInfo), "Failed to enumerate instance layers") return gpu_destroy(), false;

    for (uint32_t i = 0; i < COUNTOF(layers); i++) {
      if (!layers[i].shouldEnable) continue;
      if (hasLayer(layerInfo, count, layers[i].name)) {
        CHECK(enabledLayerCount < COUNTOF(enabledLayers), "Too many layers") return gpu_destroy(), false;
        if (layers[i].isEnabled) *layers[i].isEnabled = true;
        enabledLayers[enabledLayerCount++] = layers[i].name;
      }
    }

    // Extensions

    struct { const char* name; bool shouldEnable; bool* isEnabled; } extensions[] = {
      { "VK_KHR_portability_enumeration", true, &supports.portability },
      { "VK_EXT_debug_utils", config->debug, &supports.debug }
    };

    const char* enabledExtensions[32];
    uint32_t enabledExtensionCount = 0;

    if (state.config.vk.getInstanceExtensions) {
      const char** instanceExtensions = state.config.vk.getInstanceExtensions(&enabledExtensionCount);
      CHECK(enabledExtensionCount < COUNTOF(enabledExtensions), "Too many instance extensions") return gpu_destroy(), false;
      for (uint32_t i = 0; i < enabledExtensionCount; i++) {
        enabledExtensions[i] = instanceExtensions[i];
      }
    }

    VkExtensionProperties extensionInfo[256];
    count = COUNTOF(extensionInfo);
    VK(vkEnumerateInstanceExtensionProperties(NULL, &count, extensionInfo), "Failed to enumerate instance extensions") return gpu_destroy(), false;

    for (uint32_t i = 0; i < COUNTOF(extensions); i++) {
      if (!extensions[i].shouldEnable) continue;
      if (hasExtension(extensionInfo, count, extensions[i].name)) {
        CHECK(enabledExtensionCount < COUNTOF(enabledExtensions), "Too many instance extensions") return gpu_destroy(), false;
        if (extensions[i].isEnabled) *extensions[i].isEnabled = true;
        enabledExtensions[enabledExtensionCount++] = extensions[i].name;
      }
    }

    if (state.config.debug && !supports.validation && state.config.callback) {
      state.config.callback(state.config.userdata, "Warning: GPU debug mode is enabled, but validation layer is not supported", false);
    }

    if (state.config.debug && !supports.debug) {
      state.config.debug = false;
    }

    VkInstanceCreateInfo instanceInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
#ifdef VK_KHR_portability_enumeration
      .flags = supports.portability ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR : 0,
#endif
      .pApplicationInfo = &(VkApplicationInfo) {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pEngineName = config->engineName,
        .engineVersion = VK_MAKE_VERSION(config->engineVersion[0], config->engineVersion[1], config->engineVersion[2]),
        .apiVersion = VK_MAKE_VERSION(1, 1, 0)
      },
      .enabledLayerCount = enabledLayerCount,
      .ppEnabledLayerNames = enabledLayers,
      .enabledExtensionCount = enabledExtensionCount,
      .ppEnabledExtensionNames = enabledExtensions
    };

    if (state.config.vk.createInstance) {
      VK(state.config.vk.createInstance(&instanceInfo, NULL, (uintptr_t) &state.instance, (void*) vkGetInstanceProcAddr), "Instance creation failed") return gpu_destroy(), false;
    } else {
      VK(vkCreateInstance(&instanceInfo, NULL, &state.instance), "Instance creation failed") return gpu_destroy(), false;
    }

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

  // Surface
  if (state.config.vk.surface && state.config.vk.createSurface) {
    VK(state.config.vk.createSurface(state.instance, (void**) &state.surface), "Surface creation failed") return gpu_destroy(), false;
  }

  { // Device
    if (state.config.vk.getPhysicalDevice) {
      state.config.vk.getPhysicalDevice(state.instance, (uintptr_t) &state.adapter);
    } else {
      uint32_t deviceCount = 1;
      VK(vkEnumeratePhysicalDevices(state.instance, &deviceCount, &state.adapter), "Physical device enumeration failed") return gpu_destroy(), false;
    }

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
      config->limits->textureSize2D = limits->maxImageDimension2D;
      config->limits->textureSize3D = limits->maxImageDimension3D;
      config->limits->textureSizeCube = limits->maxImageDimensionCube;
      config->limits->textureLayers = limits->maxImageArrayLayers;
      config->limits->renderSize[0] = limits->maxFramebufferWidth;
      config->limits->renderSize[1] = limits->maxFramebufferHeight;
      config->limits->renderSize[2] = MAX(multiviewProperties.maxMultiviewViewCount, 1);
      config->limits->uniformBuffersPerStage = limits->maxPerStageDescriptorUniformBuffers;
      config->limits->storageBuffersPerStage = limits->maxPerStageDescriptorStorageBuffers;
      config->limits->sampledTexturesPerStage = limits->maxPerStageDescriptorSampledImages;
      config->limits->storageTexturesPerStage = limits->maxPerStageDescriptorStorageImages;
      config->limits->samplersPerStage = limits->maxPerStageDescriptorSamplers;
      config->limits->uniformBufferRange = limits->maxUniformBufferRange;
      config->limits->storageBufferRange = limits->maxStorageBufferRange;
      config->limits->uniformBufferAlign = limits->minUniformBufferOffsetAlignment;
      config->limits->storageBufferAlign = limits->minStorageBufferOffsetAlignment;
      config->limits->vertexAttributes = MIN(limits->maxVertexInputAttributes, COUNTOF(((gpu_pipeline_info*) NULL)->vertex.attributes));
      config->limits->vertexBuffers = MIN(limits->maxVertexInputBindings, COUNTOF(((gpu_pipeline_info*) NULL)->vertex.bufferStrides));
      config->limits->vertexBufferStride = MIN(limits->maxVertexInputBindingStride, UINT16_MAX);
      config->limits->vertexShaderOutputs = limits->maxVertexOutputComponents;
      config->limits->clipDistances = limits->maxClipDistances;
      config->limits->cullDistances = limits->maxCullDistances;
      config->limits->clipAndCullDistances = limits->maxCombinedClipAndCullDistances;
      config->limits->workgroupCount[0] = limits->maxComputeWorkGroupCount[0];
      config->limits->workgroupCount[1] = limits->maxComputeWorkGroupCount[1];
      config->limits->workgroupCount[2] = limits->maxComputeWorkGroupCount[2];
      config->limits->workgroupSize[0] = limits->maxComputeWorkGroupSize[0];
      config->limits->workgroupSize[1] = limits->maxComputeWorkGroupSize[1];
      config->limits->workgroupSize[2] = limits->maxComputeWorkGroupSize[2];
      config->limits->totalWorkgroupSize = limits->maxComputeWorkGroupInvocations;
      config->limits->computeSharedMemory = limits->maxComputeSharedMemorySize;
      config->limits->pushConstantSize = limits->maxPushConstantsSize;
      config->limits->indirectDrawCount = limits->maxDrawIndirectCount;
      config->limits->instances = multiviewProperties.maxMultiviewInstanceIndex;
      config->limits->timestampPeriod = limits->timestampPeriod;
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
      config->features->shaderTally = enable->pipelineStatisticsQuery = supports->pipelineStatisticsQuery;
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
    uint32_t requiredQueueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    vkGetPhysicalDeviceQueueFamilyProperties(state.adapter, &queueFamilyCount, queueFamilies);
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
      VkBool32 presentable = VK_TRUE;

      if (state.surface) {
        vkGetPhysicalDeviceSurfaceSupportKHR(state.adapter, i, state.surface, &presentable);
      }

      if (presentable && (queueFamilies[i].queueFlags & requiredQueueFlags) == requiredQueueFlags) {
        state.queueFamilyIndex = i;
        break;
      }
    }
    CHECK(state.queueFamilyIndex != ~0u, "Queue selection failed") return gpu_destroy(), false;

    struct {
      bool swapchain;
    } supports = { 0 };

    struct { const char* name; bool shouldEnable; bool* isEnabled; } extensions[] = {
      { "VK_KHR_swapchain", state.surface, &supports.swapchain },
      { "VK_KHR_portability_subset", true, NULL }
    };

    const char* enabledExtensions[4];
    uint32_t enabledExtensionCount = 0;

    VkExtensionProperties extensionInfo[256];
    uint32_t count = COUNTOF(extensionInfo);
    VK(vkEnumerateDeviceExtensionProperties(state.adapter, NULL, &count, extensionInfo), "Failed to enumerate device extensions") return gpu_destroy(), false;

    for (uint32_t i = 0; i < COUNTOF(extensions); i++) {
      if (!extensions[i].shouldEnable) continue;
      if (hasExtension(extensionInfo, count, extensions[i].name)) {
        CHECK(enabledExtensionCount < COUNTOF(enabledExtensions), "Too many device extensions") return gpu_destroy(), false;
        if (extensions[i].isEnabled) *extensions[i].isEnabled = true;
        enabledExtensions[enabledExtensionCount++] = extensions[i].name;
      }
    }

    CHECK(supports.swapchain || !state.surface, "Swapchain extension not supported") return gpu_destroy(), false;

    VkDeviceCreateInfo deviceInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = config->features ? &enabledFeatures : NULL,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &(VkDeviceQueueCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = state.queueFamilyIndex,
        .pQueuePriorities = &(float) { 1.f },
        .queueCount = 1
      },
      .enabledExtensionCount = enabledExtensionCount,
      .ppEnabledExtensionNames = enabledExtensions
    };

    if (state.config.vk.createDevice) {
      VK(state.config.vk.createDevice(state.instance, &deviceInfo, NULL, (uintptr_t) &state.device, (void*) vkGetInstanceProcAddr), "Device creation failed") return gpu_destroy(), false;
    } else {
      VK(vkCreateDevice(state.adapter, &deviceInfo, NULL, &state.device), "Device creation failed") return gpu_destroy(), false;
    }
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
      [GPU_MEMORY_BUFFER_MAP_STREAM] = {
        .usage =
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .flags = hostVisible | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
      },
      [GPU_MEMORY_BUFFER_MAP_STAGING] = {
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .flags = hostVisible
      },
      [GPU_MEMORY_BUFFER_MAP_READBACK] = {
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
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

        if ((memoryTypes[j].propertyFlags & bufferFlags[i].flags) == bufferFlags[i].flags) {
          allocator->memoryFlags = memoryTypes[j].propertyFlags;
          allocator->memoryType = j;
          break;
        }

        if ((memoryTypes[j].propertyFlags & fallback) == fallback) {
          allocator->memoryFlags = memoryTypes[j].propertyFlags;
          allocator->memoryType = j;
        }
      }
    }

    // Textures

    VkImageUsageFlags transient = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

    struct { VkFormat format; VkImageUsageFlags usage; } imageFlags[] = {
      [GPU_MEMORY_TEXTURE_COLOR] = { VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_D16] = { VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_D32F] = { VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_D24S8] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_D32FS8] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_LAZY_COLOR] = { VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | transient },
      [GPU_MEMORY_TEXTURE_LAZY_D16] = { VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient },
      [GPU_MEMORY_TEXTURE_LAZY_D32F] = { VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient },
      [GPU_MEMORY_TEXTURE_LAZY_D24S8] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient },
      [GPU_MEMORY_TEXTURE_LAZY_D32FS8] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient }
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

  if (state.surface) {
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.adapter, state.surface, &state.surfaceCapabilities);

    VkSurfaceFormatKHR formats[32];
    uint32_t formatCount = COUNTOF(formats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(state.adapter, state.surface, &formatCount, formats);

    for (uint32_t i = 0; i < formatCount; i++) {
      if (formats[i].format == VK_FORMAT_R8G8B8A8_SRGB || formats[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
        state.surfaceFormat = formats[i];
        break;
      }
    }

    VK(state.surfaceFormat.format == VK_FORMAT_UNDEFINED ? VK_ERROR_FORMAT_NOT_SUPPORTED : VK_SUCCESS, "No supported surface formats") return gpu_destroy(), false;

    uint32_t width = state.surfaceCapabilities.currentExtent.width;
    uint32_t height = state.surfaceCapabilities.currentExtent.height;

    createSwapchain(width, height);
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

  // Pipeline cache

  VkPipelineCacheCreateInfo cacheInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
  };

  // Not using VkPipelineCacheHeaderVersionOne since it's missing from Android headers
  if (config->vk.cacheSize >= 16 + VK_UUID_SIZE) {
    uint32_t headerSize, headerVersion;
    memcpy(&headerSize, config->vk.cacheData, 4);
    memcpy(&headerVersion, (char*) config->vk.cacheData + 4, 4);
    if (headerSize == 16 + VK_UUID_SIZE && headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE) {
      cacheInfo.initialDataSize = config->vk.cacheSize;
      cacheInfo.pInitialData = config->vk.cacheData;
    }
  }

  VK(vkCreatePipelineCache(state.device, &cacheInfo, NULL, &state.pipelineCache), "Pipeline cache creation failed") return gpu_destroy(), false;

  state.tick[CPU] = COUNTOF(state.ticks) - 1;
  state.currentSwapchainTexture = ~0u;
  return true;
}

void gpu_destroy(void) {
  if (state.device) vkDeviceWaitIdle(state.device);
  state.tick[GPU] = state.tick[CPU];
  expunge();
  if (state.pipelineCache) vkDestroyPipelineCache(state.device, state.pipelineCache, NULL);
  for (uint32_t i = 0; i < COUNTOF(state.scratchpad); i++) {
    if (state.scratchpad[i].buffer) vkDestroyBuffer(state.device, state.scratchpad[i].buffer, NULL);
  }
  for (uint32_t i = 0; i < COUNTOF(state.ticks); i++) {
    gpu_tick* tick = &state.ticks[i];
    if (tick->pool) vkDestroyCommandPool(state.device, tick->pool, NULL);
    if (tick->semaphores[0]) vkDestroySemaphore(state.device, tick->semaphores[0], NULL);
    if (tick->semaphores[1]) vkDestroySemaphore(state.device, tick->semaphores[1], NULL);
    if (tick->fence) vkDestroyFence(state.device, tick->fence, NULL);
  }
  for (uint32_t i = 0; i < COUNTOF(state.framebuffers); i++) {
    for (uint32_t j = 0; j < COUNTOF(state.framebuffers[0]); j++) {
      VkFramebuffer framebuffer = state.framebuffers[i][j].object;
      if (framebuffer) vkDestroyFramebuffer(state.device, framebuffer, NULL);
    }
  }
  for (uint32_t i = 0; i < COUNTOF(state.renderpasses); i++) {
    for (uint32_t j = 0; j < COUNTOF(state.renderpasses[0]); j++) {
      VkRenderPass pass = state.renderpasses[i][j].object;
      if (pass) vkDestroyRenderPass(state.device, pass, NULL);
    }
  }
  for (uint32_t i = 0; i < COUNTOF(state.memory); i++) {
    if (state.memory[i].handle) vkFreeMemory(state.device, state.memory[i].handle, NULL);
  }
  for (uint32_t i = 0; i < COUNTOF(state.swapchainTextures); i++) {
    if (state.swapchainTextures[i].view) vkDestroyImageView(state.device, state.swapchainTextures[i].view, NULL);
  }
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
  gpu_wait_tick(++state.tick[CPU] - COUNTOF(state.ticks));
  gpu_tick* tick = &state.ticks[state.tick[CPU] & TICK_MASK];
  VK(vkResetFences(state.device, 1, &tick->fence), "Fence reset failed") return 0;
  VK(vkResetCommandPool(state.device, tick->pool, 0), "Command pool reset failed") return 0;
  state.scratchpad[GPU_MAP_STREAM].cursor = 0;
  state.scratchpad[GPU_MAP_READBACK].cursor = 0;
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

  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = !!state.swapchainSemaphore,
    .pWaitSemaphores = &state.swapchainSemaphore,
    .pWaitDstStageMask = &waitStage,
    .commandBufferCount = count,
    .pCommandBuffers = commands
  };

  VK(vkQueueSubmit(state.queue, 1, &submit, tick->fence), "Queue submit failed") {}
  state.swapchainSemaphore = VK_NULL_HANDLE;
}

void gpu_present() {
  VkSemaphore semaphore = state.ticks[state.tick[CPU] & TICK_MASK].semaphores[1];

  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &semaphore
  };

  VK(vkQueueSubmit(state.queue, 1, &submit, VK_NULL_HANDLE), "Queue submit failed") {}

  VkPresentInfoKHR present = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &semaphore,
    .swapchainCount = 1,
    .pSwapchains = &state.swapchain,
    .pImageIndices = &state.currentSwapchainTexture
  };

  VkResult result = vkQueuePresentKHR(state.queue, &present);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    state.swapchainValid = false;
  } else {
    vcheck(result, "Queue present failed");
  }

  state.currentSwapchainTexture = ~0u;
}

bool gpu_is_complete(uint32_t tick) {
  return state.tick[GPU] >= tick;
}

bool gpu_wait_tick(uint32_t tick) {
  if (state.tick[GPU] < tick) {
    VkFence fence = state.ticks[tick & TICK_MASK].fence;
    VK(vkWaitForFences(state.device, 1, &fence, VK_FALSE, ~0ull), "Fence wait failed") return false;
    state.tick[GPU] = tick;
    return true;
  } else {
    return false;
  }
}

void gpu_wait_idle() {
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

static uint32_t hash32(uint32_t initial, void* data, uint32_t size) {
  const uint8_t* bytes = data;
  uint32_t hash = initial;
  for (uint32_t i = 0; i < size; i++) {
    hash = (hash ^ bytes[i]) * 16777619;
  }
  return hash;
}

static gpu_memory* gpu_allocate(gpu_memory_type type, VkMemoryRequirements info, VkDeviceSize* offset) {
  gpu_allocator* allocator = &state.allocators[state.allocatorLookup[type]];

  static const uint32_t blockSizes[] = {
    [GPU_MEMORY_BUFFER_GPU] = 1 << 26,
    [GPU_MEMORY_BUFFER_MAP_STREAM] = 0,
    [GPU_MEMORY_BUFFER_MAP_STAGING] = 0,
    [GPU_MEMORY_BUFFER_MAP_READBACK] = 0,
    [GPU_MEMORY_TEXTURE_COLOR] = 1 << 28,
    [GPU_MEMORY_TEXTURE_D16] = 1 << 28,
    [GPU_MEMORY_TEXTURE_D32F] = 1 << 28,
    [GPU_MEMORY_TEXTURE_D24S8] = 1 << 28,
    [GPU_MEMORY_TEXTURE_D32FS8] = 1 << 28,
    [GPU_MEMORY_TEXTURE_LAZY_COLOR] = 1 << 28,
    [GPU_MEMORY_TEXTURE_LAZY_D16] = 1 << 28,
    [GPU_MEMORY_TEXTURE_LAZY_D32F] = 1 << 28,
    [GPU_MEMORY_TEXTURE_LAZY_D24S8] = 1 << 28,
    [GPU_MEMORY_TEXTURE_LAZY_D32FS8] = 1 << 28
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
      allocator->block->refs = 1;
      *offset = 0;
      return memory;
    }
  }

  check(false, "Out of GPU memory");
  return NULL;
}

static void gpu_release(gpu_memory* memory) {
  if (memory && --memory->refs == 0) {
    condemn(memory->handle, VK_OBJECT_TYPE_DEVICE_MEMORY);
    memory->pointer = NULL;
    memory->handle = NULL;

    for (uint32_t i = 0; i < COUNTOF(state.allocators); i++) {
      if (state.allocators[i].block == memory) {
        state.allocators[i].block = NULL;
        state.allocators[i].cursor = 0;
      }
    }
  }
}

static void condemn(void* handle, VkObjectType type) {
  if (!handle) return;
  gpu_morgue* morgue = &state.morgue;
  check(morgue->head - morgue->tail < COUNTOF(morgue->data), "Morgue overflow (too many objects waiting to be deleted)");
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
      case VK_OBJECT_TYPE_DESCRIPTOR_POOL: vkDestroyDescriptorPool(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_PIPELINE_LAYOUT: vkDestroyPipelineLayout(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_PIPELINE: vkDestroyPipeline(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_QUERY_POOL: vkDestroyQueryPool(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_RENDER_PASS: vkDestroyRenderPass(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_FRAMEBUFFER: vkDestroyFramebuffer(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_DEVICE_MEMORY: vkFreeMemory(state.device, victim->handle, NULL); break;
      default: check(false, "Unreachable"); break;
    }
  }
}

static bool hasLayer(VkLayerProperties* layers, uint32_t count, const char* layer) {
  for (uint32_t i = 0; i < count; i++) {
    if (!strcmp(layers[i].layerName, layer)) {
      return true;
    }
  }
  return false;
}

static bool hasExtension(VkExtensionProperties* extensions, uint32_t count, const char* extension) {
  for (uint32_t i = 0; i < count; i++) {
    if (!strcmp(extensions[i].extensionName, extension)) {
      return true;
    }
  }
  return false;
}

static void createSwapchain(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    state.swapchainValid = false;
    return;
  }

  VkSwapchainKHR oldSwapchain = state.swapchain;

  if (oldSwapchain) {
    vkDeviceWaitIdle(state.device);
  }

  VkSwapchainCreateInfoKHR swapchainInfo = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = state.surface,
    .minImageCount = state.surfaceCapabilities.minImageCount,
    .imageFormat = state.surfaceFormat.format,
    .imageColorSpace = state.surfaceFormat.colorSpace,
    .imageExtent = { width, height },
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = state.config.vk.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR,
    .clipped = VK_TRUE,
    .oldSwapchain = oldSwapchain
  };

  VK(vkCreateSwapchainKHR(state.device, &swapchainInfo, NULL, &state.swapchain), "Swapchain creation failed") return;

  if (oldSwapchain) {
    for (uint32_t i = 0; i < COUNTOF(state.swapchainTextures); i++) {
      if (state.swapchainTextures[i].view) {
        vkDestroyImageView(state.device, state.swapchainTextures[i].view, NULL);
      }
    }

    memset(state.swapchainTextures, 0, sizeof(state.swapchainTextures));
    vkDestroySwapchainKHR(state.device, oldSwapchain, NULL);
  }

  uint32_t imageCount;
  VkImage images[COUNTOF(state.swapchainTextures)];
  VK(vkGetSwapchainImagesKHR(state.device, state.swapchain, &imageCount, NULL), "Failed to get swapchain images") return;
  VK(imageCount > COUNTOF(images) ? VK_ERROR_TOO_MANY_OBJECTS : VK_SUCCESS, "Failed to get swapchain images") return;
  VK(vkGetSwapchainImagesKHR(state.device, state.swapchain, &imageCount, images), "Failed to get swapchain images") return;

  for (uint32_t i = 0; i < imageCount; i++) {
    gpu_texture* texture = &state.swapchainTextures[i];

    texture->handle = images[i];
    texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    texture->layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    texture->samples = 1;
    texture->memory = ~0u;
    texture->layers = 1;
    texture->format = GPU_FORMAT_SURFACE;
    texture->srgb = true;

    gpu_texture_view_info view = {
      .source = texture,
      .type = GPU_TEXTURE_2D
    };

    CHECK(gpu_texture_init_view(texture, &view), "Swapchain texture view creation failed") return;
  }

  state.swapchainValid = true;
}

// Ugliness until we can use dynamic rendering
static VkRenderPass getCachedRenderPass(gpu_pass_info* pass, bool exact) {
  bool depth = pass->depth.layout != VK_IMAGE_LAYOUT_UNDEFINED;
  uint32_t count = (pass->count - depth) >> pass->resolve;

  uint32_t lower[] = {
    count > 0 ? pass->color[0].format : 0xff,
    count > 1 ? pass->color[1].format : 0xff,
    count > 2 ? pass->color[2].format : 0xff,
    count > 3 ? pass->color[3].format : 0xff,
    depth ? pass->depth.format : 0xff,
    pass->samples,
    pass->resolve,
    pass->views
  };

  uint32_t upper[] = {
    count > 0 ? pass->color[0].load : 0xff,
    count > 1 ? pass->color[1].load : 0xff,
    count > 2 ? pass->color[2].load : 0xff,
    count > 3 ? pass->color[3].load : 0xff,
    count > 0 ? pass->color[0].save : 0xff,
    count > 1 ? pass->color[1].save : 0xff,
    count > 2 ? pass->color[2].save : 0xff,
    count > 3 ? pass->color[3].save : 0xff,
    depth ? pass->depth.load : 0xff,
    depth ? pass->depth.save : 0xff,
    count > 0 ? pass->color[0].layout : 0x00,
    count > 1 ? pass->color[1].layout : 0x00,
    count > 2 ? pass->color[2].layout : 0x00,
    count > 3 ? pass->color[3].layout : 0x00,
    pass->resolve && count > 0 ? pass->color[0].resolveLayout : 0x00,
    pass->resolve && count > 1 ? pass->color[1].resolveLayout : 0x00,
    pass->resolve && count > 2 ? pass->color[2].resolveLayout : 0x00,
    pass->resolve && count > 3 ? pass->color[3].resolveLayout : 0x00,
    depth ? pass->depth.layout : 0x00,
    0
  };

  // The lower half of the hash contains format, sample, multiview info, which is all that's needed
  // to select a "compatible" render pass (which is all that's needed for creating pipelines).
  // The upper half of the hash contains load/store info and usage flags (for layout transitions),
  // which is necessary to select an "exact" match when e.g. actually beginning a render pass
  uint64_t hash = ((uint64_t) hash32(HASH_SEED, upper, sizeof(upper)) << 32) | hash32(HASH_SEED, lower, sizeof(lower));
  uint64_t mask = exact ? ~0ull : ~0u;

  // Search for a pass, they are always stored in MRU order, which requires moving it to the first
  // column if you end up using it, and shifting down the rest (dunno if that's actually worth it)
  uint32_t rows = COUNTOF(state.renderpasses);
  uint32_t cols = COUNTOF(state.renderpasses[0]);
  gpu_cache_entry* row = state.renderpasses[hash & (rows - 1)];
  for (uint32_t i = 0; i < cols && row[i].object; i++) {
    if ((row[i].hash & mask) == hash) {
      gpu_cache_entry entry = row[i];
      if (i > 0) {
        for (uint32_t j = i; j >= 1; j--) {
          row[j] = row[j - 1];
        }
        row[0] = entry;
      }
      return entry.object;
    }
  }

  // If no render pass was found, make a new one, potentially condemning and evicting an old one

  static const VkAttachmentLoadOp loadOps[] = {
    [GPU_LOAD_OP_CLEAR] = VK_ATTACHMENT_LOAD_OP_CLEAR,
    [GPU_LOAD_OP_DISCARD] = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    [GPU_LOAD_OP_KEEP] = VK_ATTACHMENT_LOAD_OP_LOAD
  };

  static const VkAttachmentStoreOp storeOps[] = {
    [GPU_SAVE_OP_KEEP] = VK_ATTACHMENT_STORE_OP_STORE,
    [GPU_SAVE_OP_DISCARD] = VK_ATTACHMENT_STORE_OP_DONT_CARE
  };

  VkAttachmentDescription attachments[9];
  VkAttachmentReference references[9];

  for (uint32_t i = 0; i < count; i++) {
    references[i].attachment = i;
    references[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    bool surface = pass->color[i].format == state.surfaceFormat.format; // FIXME
    bool discard = surface || pass->color[i].load != GPU_LOAD_OP_KEEP;
    attachments[i] = (VkAttachmentDescription) {
      .format = pass->color[i].format,
      .samples = pass->samples,
      .loadOp = loadOps[pass->color[i].load],
      .storeOp = pass->resolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : storeOps[pass->color[i].save],
      .initialLayout = discard ? VK_IMAGE_LAYOUT_UNDEFINED : pass->color[i].layout,
      .finalLayout = surface && !pass->resolve ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : pass->color[i].layout
    };
  }

  if (pass->resolve) {
    for (uint32_t i = 0; i < count; i++) {
      uint32_t index = count + i;
      references[index].attachment = index;
      references[index].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      bool surface = pass->color[i].format == state.surfaceFormat.format; // FIXME
      attachments[index] = (VkAttachmentDescription) {
        .format = pass->color[i].format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = storeOps[pass->color[i].save],
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = surface ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : pass->color[i].resolveLayout
      };
    }
  }

  if (depth) {
    uint32_t index = pass->count - 1;
    references[index].attachment = index;
    references[index].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[index] = (VkAttachmentDescription) {
      .format = pass->depth.format,
      .samples = pass->samples,
      .loadOp = loadOps[pass->depth.load],
      .storeOp = storeOps[pass->depth.save],
      .stencilLoadOp = loadOps[pass->depth.load],
      .stencilStoreOp = storeOps[pass->depth.save],
      .initialLayout = pass->depth.load == GPU_LOAD_OP_KEEP ? pass->depth.layout : VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = pass->depth.layout
    };
  }

  VkSubpassDescription subpass = {
    .colorAttachmentCount = count,
    .pColorAttachments = &references[0],
    .pResolveAttachments = pass->resolve ? &references[count] : NULL,
    .pDepthStencilAttachment = depth ? &references[pass->count - 1] : NULL
  };

  VkRenderPassMultiviewCreateInfo multiview = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
    .subpassCount = 1,
    .pViewMasks = (uint32_t[1]) { (1 << pass->views) - 1 }
  };

  VkRenderPassCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .pNext = pass->views > 0 ? &multiview : NULL,
    .attachmentCount = pass->count,
    .pAttachments = attachments,
    .subpassCount = 1,
    .pSubpasses = &subpass
  };

  VkRenderPass handle;
  VK(vkCreateRenderPass(state.device, &info, NULL, &handle), "Could not create render pass") {
    return VK_NULL_HANDLE;
  }

  condemn(row[cols - 1].object, VK_OBJECT_TYPE_RENDER_PASS);
  memmove(row + 1, row, (cols - 1) * sizeof(row[0]));

  row[0].object = handle;
  row[0].hash = hash;

  return handle;
}

VkFramebuffer getCachedFramebuffer(VkRenderPass pass, VkImageView images[9], uint32_t imageCount, uint32_t size[2]) {
  uint32_t hash = HASH_SEED;
  hash = hash32(hash, images, imageCount * sizeof(images[0]));
  hash = hash32(hash, size, 2 * sizeof(uint32_t));
  hash = hash32(hash, &pass, sizeof(pass));

  uint32_t rows = COUNTOF(state.framebuffers);
  uint32_t cols = COUNTOF(state.framebuffers[0]);
  gpu_cache_entry* row = state.framebuffers[hash & (rows - 1)];
  for (uint32_t i = 0; i < cols; i++) {
    if ((row[i].hash & ~0u) == hash) {
      row[i].hash = ((uint64_t) state.tick[CPU] << 32) | hash;
      return row[i].object;
    }
  }

  VkFramebufferCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass = pass,
    .attachmentCount = imageCount,
    .pAttachments = images,
    .width = size[0],
    .height = size[1],
    .layers = 1
  };

  gpu_cache_entry* entry = &row[0];
  for (uint32_t i = 1; i < cols; i++) {
    if (!row[i].object || row[i].hash < entry->hash) {
      entry = &row[i];
    }
  }

  if (entry->object && gpu_is_complete(entry->hash >> 32)) {
    vkDestroyFramebuffer(state.device, entry->object, NULL);
  } else {
    condemn(entry->object, VK_OBJECT_TYPE_FRAMEBUFFER);
  }

  VkFramebuffer framebuffer;
  VK(vkCreateFramebuffer(state.device, &info, NULL, &framebuffer), "Failed to create framebuffer") {
    return VK_NULL_HANDLE;
  }

  entry->object = framebuffer;
  entry->hash = ((uint64_t) state.tick[CPU] << 32) | hash;

  return framebuffer;
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
    [GPU_FORMAT_D32F] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT },
    [GPU_FORMAT_D24S8] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
    [GPU_FORMAT_D32FS8] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT },
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

  if (format == GPU_FORMAT_SURFACE) {
    return state.surfaceFormat.format;
  }

  return formats[format][colorspace];
}

static VkPipelineStageFlags convertPhase(gpu_phase phase, bool dst) {
  VkPipelineStageFlags flags = 0;
  if (phase & GPU_PHASE_INDIRECT) flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
  if (phase & GPU_PHASE_INPUT_INDEX) flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
  if (phase & GPU_PHASE_INPUT_VERTEX) flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
  if (phase & GPU_PHASE_SHADER_VERTEX) flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
  if (phase & GPU_PHASE_SHADER_FRAGMENT) flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  if (phase & GPU_PHASE_SHADER_COMPUTE) flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  if (phase & GPU_PHASE_DEPTH_EARLY) flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  if (phase & GPU_PHASE_DEPTH_LATE) flags |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  if (phase & GPU_PHASE_COLOR) flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  if (phase & GPU_PHASE_TRANSFER) flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
  if (phase & GPU_PHASE_ALL) flags |= dst ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
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
  if (cache & GPU_CACHE_COLOR_READ) flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
  if (cache & GPU_CACHE_COLOR_WRITE) flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  if (cache & GPU_CACHE_TRANSFER_READ) flags |= VK_ACCESS_TRANSFER_READ_BIT;
  if (cache & GPU_CACHE_TRANSFER_WRITE) flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
  return flags;
}

static VkBool32 relay(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userdata) {
  if (state.config.callback) {
    state.config.callback(state.config.userdata, data->pMessage, false);
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
#define CASE(x) case x: state.config.callback(state.config.userdata, "Vulkan error: " #x, true); break;
  switch (result) {
    CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
    CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CASE(VK_ERROR_INITIALIZATION_FAILED);
    CASE(VK_ERROR_DEVICE_LOST);
    CASE(VK_ERROR_MEMORY_MAP_FAILED);
    CASE(VK_ERROR_LAYER_NOT_PRESENT);
    CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
    CASE(VK_ERROR_FEATURE_NOT_PRESENT);
    CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
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
