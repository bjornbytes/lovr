#include "gpu.h"
#include <string.h>

#ifdef _WIN32
#define THREAD_LOCAL __declspec(thread)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define THREAD_LOCAL __thread
#include <dlfcn.h>
#endif

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#elif defined(__linux__) && !defined(__ANDROID__)
#define VK_USE_PLATFORM_XCB_KHR
#endif

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

// Objects

typedef struct gpu_memory gpu_memory;

struct gpu_buffer {
  VkBuffer handle;
  gpu_memory* memory;
};

struct gpu_texture {
  VkImage handle;
  VkImageView view;
  gpu_memory* memory;
  VkImageAspectFlagBits aspect;
  VkImageLayout layout;
  uint32_t layers;
  uint8_t baseLevel;
  uint8_t format;
  bool imported;
  bool srgb;
};

struct gpu_sampler {
  VkSampler handle;
};

struct gpu_layout {
  VkDescriptorSetLayout handle;
  uint32_t descriptorCounts[8];
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

struct gpu_pass {
  VkRenderPass handle;
  uint8_t colorCount;
  uint8_t samples;
  uint8_t loadMask;
  bool depthLoad;
  bool surface;
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

// Internals

struct gpu_memory {
  VkDeviceMemory handle;
  void* pointer;
  uint32_t refs;
};

typedef enum {
  GPU_MEMORY_BUFFER_STATIC,
  GPU_MEMORY_BUFFER_STREAM,
  GPU_MEMORY_BUFFER_UPLOAD,
  GPU_MEMORY_BUFFER_DOWNLOAD,
  GPU_MEMORY_TEXTURE_COLOR,
  GPU_MEMORY_TEXTURE_D16,
  GPU_MEMORY_TEXTURE_D24,
  GPU_MEMORY_TEXTURE_D32F,
  GPU_MEMORY_TEXTURE_D24S8,
  GPU_MEMORY_TEXTURE_D32FS8,
  GPU_MEMORY_TEXTURE_LAZY_COLOR,
  GPU_MEMORY_TEXTURE_LAZY_D16,
  GPU_MEMORY_TEXTURE_LAZY_D24,
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
  VkSurfaceKHR handle;
  VkSwapchainKHR swapchain;
  VkSurfaceCapabilitiesKHR capabilities;
  VkSurfaceFormatKHR format;
  VkSemaphore semaphore;
  gpu_texture images[8];
  uint32_t imageIndex;
  bool vsync;
  bool valid;
} gpu_surface;

typedef struct {
  VkCommandPool pool;
  gpu_stream streams[64];
  VkSemaphore semaphores[2];
  VkFence fence;
} gpu_tick;

typedef struct {
  bool portability;
  bool validation;
  bool debug;
  bool shaderDebug;
  bool surface;
  bool surfaceOS;
  bool swapchain;
  bool colorspace;
  bool depthResolve;
  bool formatList;
  bool renderPass2;
  bool synchronization2;
  bool scalarBlockLayout;
  bool foveation;
} gpu_extensions;

// State

static THREAD_LOCAL struct {
  char error[256];
} thread;

static struct {
  void* library;
  gpu_config config;
  gpu_extensions extensions;
  gpu_surface surface;
  VkInstance instance;
  VkPhysicalDevice adapter;
  VkDevice device;
  VkQueue queue;
  uint32_t queueFamilyIndex;
  VkPipelineCache pipelineCache;
  VkDebugUtilsMessengerEXT messenger;
  gpu_allocator allocators[GPU_MEMORY_COUNT];
  uint8_t allocatorLookup[GPU_MEMORY_COUNT];
  gpu_memory memory[1024];
  uint32_t streamCount;
  uint32_t tick[2];
  gpu_tick ticks[2];
  gpu_morgue morgue;
} state;

// Helpers

enum { CPU, GPU };
enum { LINEAR, SRGB };

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#define ALIGN(p, n) (((uintptr_t) (p) + (n - 1)) & ~(n - 1))
#define LOG(s) if (state.config.fnLog) state.config.fnLog(state.config.userdata, s)
#define VK(f, s) if (!vkcheck(f, s))
#define ASSERT(c, s) if (!(c) && (error(s), true))
#define TICK_MASK (COUNTOF(state.ticks) - 1)
#define MORGUE_MASK (COUNTOF(state.morgue.data) - 1)

static gpu_memory* allocate(gpu_memory_type type, VkMemoryRequirements info, VkDeviceSize* offset);
static void release(gpu_memory* memory);
static void condemn(void* handle, VkObjectType type);
static void expunge(void);
static bool hasLayer(VkLayerProperties* layers, uint32_t count, const char* layer);
static bool hasExtension(VkExtensionProperties* extensions, uint32_t count, const char* extension);
static VkBufferUsageFlags getBufferUsage(gpu_buffer_type type);
static bool transitionAttachment(gpu_texture* texture, bool begin, bool resolve, bool discard, VkImageMemoryBarrier2KHR* barrier);
static VkImageLayout getNaturalLayout(uint32_t usage, VkImageAspectFlags aspect);
static VkFormat convertFormat(gpu_texture_format format, int colorspace);
static VkPipelineStageFlags2 convertPhase(gpu_phase phase, bool dst);
static VkAccessFlags2 convertCache(gpu_cache cache);
static VkBool32 relay(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userdata);
static void nickname(void* object, VkObjectType type, const char* name);
static bool vkcheck(VkResult result, const char* function);
static void vkerror(VkResult result, const char* function);
static void error(const char* message);

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
  X(vkCmdPipelineBarrier2KHR)\
  X(vkCreateQueryPool)\
  X(vkDestroyQueryPool)\
  X(vkCmdResetQueryPool)\
  X(vkCmdBeginQuery)\
  X(vkCmdEndQuery)\
  X(vkCmdWriteTimestamp)\
  X(vkCmdCopyQueryPoolResults)\
  X(vkGetQueryPoolResults)\
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
  X(vkCreateRenderPass2KHR)\
  X(vkDestroyRenderPass)\
  X(vkCmdBeginRenderPass2KHR)\
  X(vkCmdEndRenderPass2KHR)\
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
GPU_DECLARE(vkGetInstanceProcAddr)
GPU_FOREACH_ANONYMOUS(GPU_DECLARE)
GPU_FOREACH_INSTANCE(GPU_DECLARE)
GPU_FOREACH_DEVICE(GPU_DECLARE)

// Buffer

bool gpu_buffer_init(gpu_buffer* buffer, gpu_buffer_info* info) {
  if (info->handle) {
    buffer->handle = (VkBuffer) info->handle;
    buffer->memory = NULL;
    nickname(buffer->handle, VK_OBJECT_TYPE_BUFFER, info->label);
    return true;
  }

  VkBufferCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = info->size,
    .usage = getBufferUsage(info->type)
  };

  VK(vkCreateBuffer(state.device, &createInfo, NULL, &buffer->handle), "vkCreateBuffer") {
    return false;
  }

  nickname(buffer->handle, VK_OBJECT_TYPE_BUFFER, info->label);

  VkDeviceSize offset;
  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(state.device, buffer->handle, &requirements);

  if ((buffer->memory = allocate((gpu_memory_type) info->type, requirements, &offset)) == NULL) {
    vkDestroyBuffer(state.device, buffer->handle, NULL);
    return false;
  }

  VK(vkBindBufferMemory(state.device, buffer->handle, buffer->memory->handle, offset), "vkBindBufferMemory") {
    vkDestroyBuffer(state.device, buffer->handle, NULL);
    release(buffer->memory);
    return false;
  }

  if (info->pointer) {
    *info->pointer = buffer->memory->pointer ? (char*) buffer->memory->pointer + offset : NULL;
  }

  return true;
}

void gpu_buffer_destroy(gpu_buffer* buffer) {
  if (!buffer->memory) return;
  condemn(buffer->handle, VK_OBJECT_TYPE_BUFFER);
  release(buffer->memory);
}

// Texture

bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info) {
  static const VkImageType imageTypes[] = {
    [GPU_TEXTURE_2D] = VK_IMAGE_TYPE_2D,
    [GPU_TEXTURE_3D] = VK_IMAGE_TYPE_3D,
    [GPU_TEXTURE_CUBE] = VK_IMAGE_TYPE_2D,
    [GPU_TEXTURE_ARRAY] = VK_IMAGE_TYPE_2D
  };

  switch (info->format) {
    case GPU_FORMAT_D16: texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT; break;
    case GPU_FORMAT_D24: texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT; break;
    case GPU_FORMAT_D32F: texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT; break;
    case GPU_FORMAT_D24S8: texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT; break;
    case GPU_FORMAT_D32FS8: texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT; break;
    default: texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT; break;
  }

  texture->layout = getNaturalLayout(info->usage, texture->aspect);
  texture->layers = info->type == GPU_TEXTURE_3D ? 0 : info->size[2];
  texture->baseLevel = 0;
  texture->format = info->format;
  texture->srgb = info->srgb;

  gpu_texture_view_info viewInfo = {
    .source = texture,
    .type = info->type,
    .usage = info->usage
  };

  if (info->handle) {
    texture->memory = NULL;
    texture->imported = true;
    texture->handle = (VkImage) info->handle;
    nickname(texture->handle, VK_OBJECT_TYPE_IMAGE, info->label);
    return gpu_texture_init_view(texture, &viewInfo);
  } else {
    texture->imported = false;
  }

  bool mutableFormat = info->srgb && (info->usage & GPU_TEXTURE_STORAGE);

  VkImageCreateInfo imageInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags =
      (info->type == GPU_TEXTURE_3D ? VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT : 0) |
      (info->type == GPU_TEXTURE_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0) |
      (mutableFormat ? (VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT) : 0),
    .imageType = imageTypes[info->type],
    .format = convertFormat(texture->format, info->srgb),
    .extent.width = info->size[0],
    .extent.height = info->size[1],
    .extent.depth = texture->layers ? 1 : info->size[2],
    .mipLevels = info->mipmaps,
    .arrayLayers = texture->layers ? texture->layers : 1,
    .samples = info->samples ? info->samples : 1,
    .usage =
      (((info->usage & GPU_TEXTURE_RENDER) && texture->aspect == VK_IMAGE_ASPECT_COLOR_BIT) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0) |
      (((info->usage & GPU_TEXTURE_RENDER) && texture->aspect != VK_IMAGE_ASPECT_COLOR_BIT) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0) |
      ((info->usage & GPU_TEXTURE_SAMPLE) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0) |
      ((info->usage & GPU_TEXTURE_STORAGE) ? VK_IMAGE_USAGE_STORAGE_BIT : 0) |
      ((info->usage & GPU_TEXTURE_COPY_SRC) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0) |
      ((info->usage & GPU_TEXTURE_COPY_DST) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0) |
      ((info->usage & GPU_TEXTURE_FOVEATION) ? VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT : 0) |
      ((info->usage == GPU_TEXTURE_RENDER) ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0) |
      (info->upload.levelCount > 0 ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0) |
      (info->upload.generateMipmaps ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0)
  };

  VkFormat formats[2];
  VkImageFormatListCreateInfo imageFormatList;
  if (mutableFormat && state.extensions.formatList) {
    imageFormatList = (VkImageFormatListCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
      .viewFormatCount = COUNTOF(formats),
      .pViewFormats = formats
    };

    formats[0] = imageInfo.format;
    formats[1] = convertFormat(texture->format, LINEAR);
    imageFormatList.pNext = imageInfo.pNext;
    imageInfo.pNext = &imageFormatList;
  }

  VK(vkCreateImage(state.device, &imageInfo, NULL, &texture->handle), "vkCreateImage") {
    return false;
  }

  nickname(texture->handle, VK_OBJECT_TYPE_IMAGE, info->label);

  gpu_memory_type memoryType;
  bool transient = info->usage == GPU_TEXTURE_RENDER;

  switch (info->format) {
    case GPU_FORMAT_D16: memoryType = transient ? GPU_MEMORY_TEXTURE_LAZY_D16 : GPU_MEMORY_TEXTURE_D16; break;
    case GPU_FORMAT_D24: memoryType = transient ? GPU_MEMORY_TEXTURE_LAZY_D24 : GPU_MEMORY_TEXTURE_D24; break;
    case GPU_FORMAT_D32F: memoryType = transient ? GPU_MEMORY_TEXTURE_LAZY_D32F : GPU_MEMORY_TEXTURE_D32F; break;
    case GPU_FORMAT_D24S8: memoryType = transient ? GPU_MEMORY_TEXTURE_LAZY_D24S8 : GPU_MEMORY_TEXTURE_D24S8; break;
    case GPU_FORMAT_D32FS8: memoryType = transient ? GPU_MEMORY_TEXTURE_LAZY_D32FS8 : GPU_MEMORY_TEXTURE_D32FS8; break;
    default: memoryType = transient ? GPU_MEMORY_TEXTURE_LAZY_COLOR : GPU_MEMORY_TEXTURE_COLOR; break;
  }

  VkDeviceSize offset;
  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(state.device, texture->handle, &requirements);

  if ((texture->memory = allocate(memoryType, requirements, &offset)) == NULL) {
    vkDestroyImage(state.device, texture->handle, NULL);
    return false;
  }

  VK(vkBindImageMemory(state.device, texture->handle, texture->memory->handle, offset), "vkBindImageMemory") {
    vkDestroyImage(state.device, texture->handle, NULL);
    release(texture->memory);
    return false;
  }

  if (!gpu_texture_init_view(texture, &viewInfo)) {
    vkDestroyImage(state.device, texture->handle, NULL);
    release(texture->memory);
    return false;
  }

  if (info->upload.stream) {
    VkImage image = texture->handle;
    VkCommandBuffer commands = info->upload.stream->commands;
    uint32_t levelCount = info->upload.levelCount;
    gpu_buffer* buffer = info->upload.buffer;

    VkImageMemoryBarrier2KHR transition = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
      .image = image,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .subresourceRange.aspectMask = texture->aspect,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS,
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS
    };

    VkDependencyInfoKHR barrier = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
      .pImageMemoryBarriers = &transition,
      .imageMemoryBarrierCount = 1
    };

    if (levelCount > 0) {
      transition.srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
      transition.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
      transition.srcAccessMask = VK_ACCESS_2_NONE_KHR;
      transition.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
      transition.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      vkCmdPipelineBarrier2KHR(commands, &barrier);

      VkBufferImageCopy copies[16];
      for (uint32_t i = 0; i < levelCount; i++) {
        copies[i] = (VkBufferImageCopy) {
          .bufferOffset = info->upload.levelOffsets[i],
          .imageSubresource.aspectMask = texture->aspect,
          .imageSubresource.mipLevel = i,
          .imageSubresource.baseArrayLayer = 0,
          .imageSubresource.layerCount = texture->layers ? info->size[2] : 1,
          .imageExtent.width = MAX(info->size[0] >> i, 1),
          .imageExtent.height = MAX(info->size[1] >> i, 1),
          .imageExtent.depth = texture->layers ? 1 : MAX(info->size[2] >> i, 1)
        };
      }

      vkCmdCopyBufferToImage(commands, buffer->handle, image, transition.newLayout, levelCount, copies);

      // Generate mipmaps
      if (info->upload.generateMipmaps) {
        transition.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
        transition.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;
        transition.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
        transition.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT_KHR;
        transition.oldLayout = transition.newLayout;
        transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        transition.subresourceRange.baseMipLevel = 0;
        transition.subresourceRange.levelCount = levelCount;
        vkCmdPipelineBarrier2KHR(commands, &barrier);

        for (uint32_t i = levelCount; i < info->mipmaps; i++) {
          transition.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
          transition.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;
          transition.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
          transition.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
          transition.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
          transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
          transition.subresourceRange.baseMipLevel = i;
          transition.subresourceRange.levelCount = 1;
          vkCmdPipelineBarrier2KHR(commands, &barrier);

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

          transition.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;
          transition.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;
          transition.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
          transition.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT_KHR;
          transition.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
          transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
          transition.subresourceRange.baseMipLevel = i;
          transition.subresourceRange.levelCount = 1;
          vkCmdPipelineBarrier2KHR(commands, &barrier);
        }
      }
    }

    // Transition to natural layout
    transition.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR | VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;
    transition.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
    transition.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
    transition.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
    transition.oldLayout = transition.newLayout;
    transition.newLayout = texture->layout;
    transition.subresourceRange.baseMipLevel = 0;
    transition.subresourceRange.levelCount = info->mipmaps;
    vkCmdPipelineBarrier2KHR(commands, &barrier);
  }

  return true;
}

bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info) {
  if (texture != info->source) {
    texture->handle = info->source->handle;
    texture->memory = NULL;
    texture->imported = false;
    texture->layout = info->source->layout;
    texture->layers = info->layerCount ? info->layerCount : (info->source->layers - info->layerIndex);
    texture->baseLevel = info->levelIndex;
    texture->format = info->source->format;
    texture->srgb = info->srgb;

    if (info->aspect == 0) {
      texture->aspect = info->source->aspect;
    } else {
      texture->aspect =
        ((info->aspect & GPU_ASPECT_COLOR) ? VK_IMAGE_ASPECT_COLOR_BIT : 0) |
        ((info->aspect & GPU_ASPECT_DEPTH) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
        ((info->aspect & GPU_ASPECT_STENCIL) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
    }
  }

  VkImageViewType type;
  switch (info->type) {
    case GPU_TEXTURE_2D: type = VK_IMAGE_VIEW_TYPE_2D; break;
    case GPU_TEXTURE_3D: type = VK_IMAGE_VIEW_TYPE_3D; break;
    case GPU_TEXTURE_CUBE: type = texture->layers > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE; break;
    case GPU_TEXTURE_ARRAY: type = VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
  }

  VkImageViewUsageCreateInfo viewUsage = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
    .usage =
      ((info->usage & GPU_TEXTURE_SAMPLE) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0) |
      (((info->usage & GPU_TEXTURE_RENDER) && texture->aspect == VK_IMAGE_ASPECT_COLOR_BIT) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0) |
      (((info->usage & GPU_TEXTURE_RENDER) && texture->aspect != VK_IMAGE_ASPECT_COLOR_BIT) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0) |
      ((info->usage & GPU_TEXTURE_STORAGE) && !texture->srgb ? VK_IMAGE_USAGE_STORAGE_BIT : 0) |
      ((info->usage & GPU_TEXTURE_FOVEATION) ? VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT : 0)
  };

  if (viewUsage.usage == 0) {
    texture->view = VK_NULL_HANDLE;
    return true;
  }

  VkImageViewCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = &viewUsage,
    .image = info->source->handle,
    .viewType = type,
    .format = convertFormat(texture->format, texture->srgb),
    .subresourceRange = {
      .aspectMask = texture->aspect,
      .baseMipLevel = info->levelIndex,
      .levelCount = info->levelCount ? info->levelCount : VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = info ? info->layerIndex : 0,
      .layerCount = info->layerCount ? info->layerCount : VK_REMAINING_ARRAY_LAYERS
    }
  };

  VK(vkCreateImageView(state.device, &createInfo, NULL, &texture->view), "vkCreateImageView") {
    return false;
  }

  nickname(texture->view, VK_OBJECT_TYPE_IMAGE_VIEW, info->label);

  return true;
}

void gpu_texture_destroy(gpu_texture* texture) {
  condemn(texture->view, VK_OBJECT_TYPE_IMAGE_VIEW);
  if (texture->imported) return;
  if (!texture->memory) return;
  condemn(texture->handle, VK_OBJECT_TYPE_IMAGE);
  release(texture->memory);
}

// Surface

bool gpu_surface_init(gpu_surface_info* info) {
  ASSERT(state.extensions.surface, "GPU does not support VK_KHR_surface extension") return false;
  ASSERT(state.extensions.surfaceOS, "GPU does not support OS surface extension") return false;
  ASSERT(state.extensions.swapchain, "GPU does not support VK_KHR_swapchain extension") return false;

  gpu_surface* surface = &state.surface;

#if defined(_WIN32)
  VkWin32SurfaceCreateInfoKHR surfaceInfo = {
    .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
    .hinstance = (HINSTANCE) info->win32.instance,
    .hwnd = (HWND) info->win32.window
  };
  GPU_DECLARE(vkCreateWin32SurfaceKHR);
  GPU_LOAD_INSTANCE(vkCreateWin32SurfaceKHR);
  VK(vkCreateWin32SurfaceKHR(state.instance, &surfaceInfo, NULL, &surface->handle), "vkCreateWin32SurfaceKHR") {
    return false;
  }
#elif defined(__APPLE__)
  VkMetalSurfaceCreateInfoEXT surfaceInfo = {
    .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
    .pLayer = (const CAMetalLayer*) info->macos.layer
  };
  GPU_DECLARE(vkCreateMetalSurfaceEXT);
  GPU_LOAD_INSTANCE(vkCreateMetalSurfaceEXT);
  VK(vkCreateMetalSurfaceEXT(state.instance, &surfaceInfo, NULL, &surface->handle), "vkCreateMetalSurfaceEXT") {
    return false;
  }
#elif defined(__linux__) && !defined(__ANDROID__)
  VkXcbSurfaceCreateInfoKHR surfaceInfo = {
    .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
    .connection = (xcb_connection_t*) info->xcb.connection,
    .window = (xcb_window_t) info->xcb.window
  };
  GPU_DECLARE(vkCreateXcbSurfaceKHR);
  GPU_LOAD_INSTANCE(vkCreateXcbSurfaceKHR);
  VK(vkCreateXcbSurfaceKHR(state.instance, &surfaceInfo, NULL, &surface->handle), "vkCreateXcbSurfaceKHR") {
    return false;
  }
#endif

  VkBool32 presentable;
  vkGetPhysicalDeviceSurfaceSupportKHR(state.adapter, state.queueFamilyIndex, surface->handle, &presentable);

  // The most correct thing to do is to incorporate presentation support into the init-time process
  // for selecting a physical device and queue family.  We currently choose not to do this
  // deliberately, because A) it's more complicated, B) in normal circumstances OpenXR picks the
  // physical device, not us, and C) we don't support multiple GPUs or multiple queues, so we
  // aren't able to support the tricky case and would just end up failing/erroring anyway.
  ASSERT(presentable, "Surface unavailable because the GPU used for rendering does not support presentation") {
    vkDestroySurfaceKHR(state.instance, surface->handle, NULL);
    return false;
  }

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.adapter, surface->handle, &surface->capabilities);

  VkSurfaceFormatKHR formats[64];
  uint32_t formatCount = COUNTOF(formats);
  vkGetPhysicalDeviceSurfaceFormatsKHR(state.adapter, surface->handle, &formatCount, formats);

  for (uint32_t i = 0; i < formatCount; i++) {
    if (formats[i].format == VK_FORMAT_R8G8B8A8_SRGB || formats[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
      surface->format = formats[i];
      break;
    }
  }

  ASSERT(surface->format.format != VK_FORMAT_UNDEFINED, "No supported swapchain texture format is available") {
    LOG("Surface unavailable because no supported texture format is available");
    vkDestroySurfaceKHR(state.instance, surface->handle, NULL);
    return false;
  }

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.adapter, surface->handle, &surface->capabilities);

  surface->imageIndex = ~0u;
  surface->vsync = info->vsync;

  gpu_surface_resize(info->width, info->height);
  return true;
}

bool gpu_surface_resize(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    state.surface.valid = false;
    return true;
  }

  gpu_surface* surface = &state.surface;
  VkSwapchainKHR oldSwapchain = surface->swapchain;

  if (oldSwapchain) {
    vkDeviceWaitIdle(state.device);
    surface->swapchain = VK_NULL_HANDLE;
  }

  VkSwapchainCreateInfoKHR swapchainInfo = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = surface->handle,
    .minImageCount = surface->capabilities.minImageCount,
    .imageFormat = surface->format.format,
    .imageColorSpace = surface->format.colorSpace,
    .imageExtent = { width, height },
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = surface->vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR,
    .clipped = VK_TRUE,
    .oldSwapchain = oldSwapchain
  };

  VK(vkCreateSwapchainKHR(state.device, &swapchainInfo, NULL, &surface->swapchain), "vkCreateSwapchainKHR") {
    return false;
  }

  if (oldSwapchain) {
    for (uint32_t i = 0; i < COUNTOF(surface->images); i++) {
      if (surface->images[i].view) {
        vkDestroyImageView(state.device, surface->images[i].view, NULL);
      }
    }

    memset(surface->images, 0, sizeof(surface->images));
    vkDestroySwapchainKHR(state.device, oldSwapchain, NULL);
  }

  uint32_t imageCount;
  VkImage images[COUNTOF(surface->images)];
  VK(vkGetSwapchainImagesKHR(state.device, surface->swapchain, &imageCount, NULL), "vkGetSwapchainImagesKHR") {
    vkDestroySwapchainKHR(state.device, oldSwapchain, NULL);
    surface->swapchain = VK_NULL_HANDLE;
    return false;
  }

  ASSERT(imageCount <= COUNTOF(images), "Too many swapchain images!") {
    vkDestroySwapchainKHR(state.device, oldSwapchain, NULL);
    surface->swapchain = VK_NULL_HANDLE;
    return false;
  }

  VK(vkGetSwapchainImagesKHR(state.device, surface->swapchain, &imageCount, images), "vkGetSwapchainImagesKHR") {
    vkDestroySwapchainKHR(state.device, oldSwapchain, NULL);
    surface->swapchain = VK_NULL_HANDLE;
    return false;
  }

  for (uint32_t i = 0; i < imageCount; i++) {
    gpu_texture* texture = &surface->images[i];

    texture->handle = images[i];
    texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    texture->layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    texture->memory = NULL;
    texture->layers = 1;
    texture->format = GPU_FORMAT_SURFACE;
    texture->srgb = true;

    gpu_texture_view_info view = {
      .source = texture,
      .type = GPU_TEXTURE_2D,
      .usage = GPU_TEXTURE_RENDER
    };

    if (!gpu_texture_init_view(texture, &view)) {
      vkDestroySwapchainKHR(state.device, surface->swapchain, NULL);
      surface->swapchain = VK_NULL_HANDLE;
      return false;
    }
  }

  surface->valid = true;
  return true;
}

bool gpu_surface_acquire(gpu_texture** texture) {
  if (!state.surface.valid) {
    *texture = NULL;
    return true;
  }

  gpu_surface* surface = &state.surface;
  gpu_tick* tick = &state.ticks[state.tick[CPU] & TICK_MASK];
  VkResult result = vkAcquireNextImageKHR(state.device, surface->swapchain, UINT64_MAX, tick->semaphores[0], VK_NULL_HANDLE, &surface->imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    surface->imageIndex = ~0u;
    surface->valid = false;
    *texture = NULL;
    return true;
  } else if (result < 0) {
    vkerror(result, "vkAcquireNextImageKHR");
    return false;
  }

  surface->semaphore = tick->semaphores[0];
  *texture = &surface->images[surface->imageIndex];
  return true;
}

bool gpu_surface_present(void) {
  VkSemaphore semaphore = state.ticks[state.tick[CPU] & TICK_MASK].semaphores[1];

  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &semaphore
  };

  VK(vkQueueSubmit(state.queue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit") {
    return false;
  }

  VkPresentInfoKHR present = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &semaphore,
    .swapchainCount = 1,
    .pSwapchains = &state.surface.swapchain,
    .pImageIndices = &state.surface.imageIndex
  };

  VkResult result = vkQueuePresentKHR(state.queue, &present);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    state.surface.valid = false;
  } else if (result < 0) {
    // TODO wait on semaphore, for errors that don't wait on it
    vkerror(result, "vkQueuePresentKHR");
    return false;
  }

  state.surface.imageIndex = ~0u;
  return true;
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
    [GPU_WRAP_MIRROR] = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    [GPU_WRAP_BORDER] = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
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

  VK(vkCreateSampler(state.device, &samplerInfo, NULL, &sampler->handle), "vkCreateSampler") {
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
    [GPU_SLOT_TEXTURE_WITH_SAMPLER] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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
      .stageFlags =
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

  VK(vkCreateDescriptorSetLayout(state.device, &layoutInfo, NULL, &layout->handle), "vkCreateDescriptorSetLayout") {
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
  VkShaderStageFlags stageFlags = 0;
  for (uint32_t i = 0; i < info->stageCount; i++) {
    switch (info->stages[i].stage) {
      case GPU_STAGE_VERTEX: stageFlags |= VK_SHADER_STAGE_VERTEX_BIT; break;
      case GPU_STAGE_FRAGMENT: stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT; break;
      case GPU_STAGE_COMPUTE: stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT; break;
      default: return false;
    }
  }

  shader->handles[0] = VK_NULL_HANDLE;
  shader->handles[1] = VK_NULL_HANDLE;
  shader->pipelineLayout = VK_NULL_HANDLE;

  for (uint32_t i = 0; i < info->stageCount; i++) {
    VkShaderModuleCreateInfo moduleInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = info->stages[i].length,
      .pCode = info->stages[i].code
    };

    VK(vkCreateShaderModule(state.device, &moduleInfo, NULL, &shader->handles[i]), "vkCreateShaderModule") {
      gpu_shader_destroy(shader);
      return false;
    }

    nickname(shader->handles[i], VK_OBJECT_TYPE_SHADER_MODULE, info->label);
  }

  VkDescriptorSetLayout layouts[4];
  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pSetLayouts = layouts,
    .pushConstantRangeCount = info->pushConstantSize > 0,
    .pPushConstantRanges = &(VkPushConstantRange) {
      .stageFlags = stageFlags,
      .offset = 0,
      .size = info->pushConstantSize
    }
  };

  for (uint32_t i = 0; i < COUNTOF(info->layouts) && info->layouts[i]; i++) {
    layouts[i] = info->layouts[i]->handle;
    pipelineLayoutInfo.setLayoutCount++;
  }

  VK(vkCreatePipelineLayout(state.device, &pipelineLayoutInfo, NULL, &shader->pipelineLayout), "vkCreatePipelineLayout") {
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
  VkDescriptorPoolSize sizes[] = {
    [GPU_SLOT_UNIFORM_BUFFER] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0 },
    [GPU_SLOT_STORAGE_BUFFER] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0 },
    [GPU_SLOT_UNIFORM_BUFFER_DYNAMIC] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0 },
    [GPU_SLOT_STORAGE_BUFFER_DYNAMIC] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 0 },
    [GPU_SLOT_TEXTURE_WITH_SAMPLER] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0 },
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

  VK(vkCreateDescriptorPool(state.device, &poolInfo, NULL, &pool->handle), "vkCreateDescriptorPool") {
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

    VK(vkAllocateDescriptorSets(state.device, &allocateInfo, &info->bundles[i].handle), "vkAllocateDescriptorSets") {
      vkDestroyDescriptorPool(state.device, pool->handle, NULL);
      return false;
    }
  }

  return true;
}

void gpu_bundle_pool_destroy(gpu_bundle_pool* pool) {
  condemn(pool->handle, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
}

void gpu_bundle_write(gpu_bundle** bundles, gpu_bundle_info* infos, uint32_t count) {
  VkDescriptorBufferInfo bufferInfo[256];
  VkDescriptorImageInfo imageInfo[256];
  VkWriteDescriptorSet writes[256];
  uint32_t bufferCount = 0;
  uint32_t imageCount = 0;
  uint32_t writeCount = 0;

  static const VkDescriptorType types[] = {
    [GPU_SLOT_UNIFORM_BUFFER] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [GPU_SLOT_STORAGE_BUFFER] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    [GPU_SLOT_UNIFORM_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    [GPU_SLOT_STORAGE_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    [GPU_SLOT_TEXTURE_WITH_SAMPLER] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [GPU_SLOT_SAMPLED_TEXTURE] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    [GPU_SLOT_STORAGE_TEXTURE] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    [GPU_SLOT_SAMPLER] = VK_DESCRIPTOR_TYPE_SAMPLER
  };

  for (uint32_t i = 0; i < count; i++) {
    gpu_bundle_info* info = &infos[i];
    for (uint32_t j = 0; j < info->count; j++) {
      gpu_binding* binding = &info->bindings[j];
      VkDescriptorType type = types[binding->type];
      gpu_buffer_binding* buffers = binding->count > 0 ? binding->buffers : &binding->buffer;
      gpu_texture_binding* textures = binding->count > 0 ? binding->textures : &binding->texture;
      bool image = binding->type > GPU_SLOT_STORAGE_BUFFER_DYNAMIC;

      uint32_t index = 0;
      uint32_t descriptorCount = MAX(binding->count, 1);

      while (index < descriptorCount) {
        uint32_t available = image ? COUNTOF(imageInfo) - imageCount : COUNTOF(bufferInfo) - bufferCount;
        uint32_t chunk = MIN(descriptorCount - index, available);

        writes[writeCount++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = bundles[i]->handle,
          .dstBinding = binding->number,
          .dstArrayElement = index,
          .descriptorCount = chunk,
          .descriptorType = type,
          .pBufferInfo = &bufferInfo[bufferCount],
          .pImageInfo = &imageInfo[imageCount]
        };

        if (image) {
          for (uint32_t n = 0; n < chunk; n++, index++) {
            imageInfo[imageCount++] = (VkDescriptorImageInfo) {
              .imageView = textures[index].object ? textures[index].object->view : NULL,
              .imageLayout = textures[index].object ? textures[index].object->layout : VK_IMAGE_LAYOUT_UNDEFINED,
              .sampler = textures[index].sampler ? textures[index].sampler->handle : NULL
            };
          }
        } else {
          for (uint32_t n = 0; n < chunk; n++, index++) {
            bufferInfo[bufferCount++] = (VkDescriptorBufferInfo) {
              .buffer = buffers[index].object->handle,
              .offset = buffers[index].offset,
              .range = buffers[index].extent
            };
          }
        }

        if ((image ? imageCount >= COUNTOF(imageInfo) : bufferCount >= COUNTOF(bufferInfo)) || writeCount >= COUNTOF(writes)) {
          vkUpdateDescriptorSets(state.device, writeCount, writes, 0, NULL);
          bufferCount = imageCount = writeCount = 0;
        }
      }
    }
  }

  if (writeCount > 0) {
    vkUpdateDescriptorSets(state.device, writeCount, writes, 0, NULL);
  }
}

// Canvas

bool gpu_pass_init(gpu_pass* pass, gpu_pass_info* info) {
  static const VkAttachmentLoadOp loadOps[] = {
    [GPU_LOAD_OP_CLEAR] = VK_ATTACHMENT_LOAD_OP_CLEAR,
    [GPU_LOAD_OP_DISCARD] = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    [GPU_LOAD_OP_KEEP] = VK_ATTACHMENT_LOAD_OP_LOAD
  };

  static const VkAttachmentStoreOp storeOps[] = {
    [GPU_SAVE_OP_KEEP] = VK_ATTACHMENT_STORE_OP_STORE,
    [GPU_SAVE_OP_DISCARD] = VK_ATTACHMENT_STORE_OP_DONT_CARE
  };

  VkAttachmentDescription2 attachments[10];
  VkAttachmentReference2 references[10];
  bool hasColorResolve = false;
  uint32_t attachmentCount = 0;

  for (uint32_t i = 0; i < info->colorCount; i++) {
    uint32_t index = attachmentCount++;

    references[index] = (VkAttachmentReference2) {
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
      .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .attachment = i
    };

    attachments[index] = (VkAttachmentDescription2) {
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
      .format = convertFormat(info->color[i].format, info->color[i].srgb),
      .samples = info->samples,
      .loadOp = loadOps[info->color[i].load],
      .storeOp = info->color[i].resolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : storeOps[info->color[i].save],
      .initialLayout = references[i].layout,
      .finalLayout = references[i].layout
    };

    hasColorResolve |= info->color[i].resolve;
  }

  if (hasColorResolve) {
    for (uint32_t i = 0; i < info->colorCount; i++) {
      uint32_t referenceIndex = info->colorCount + i;

      references[referenceIndex] = (VkAttachmentReference2) {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
        .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .attachment = info->color[i].resolve ? attachmentCount : VK_ATTACHMENT_UNUSED
      };

      if (info->color[i].resolve) {
        attachments[attachmentCount++] = (VkAttachmentDescription2) {
          .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
          .format = attachments[i].format,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .initialLayout = references[referenceIndex].layout,
          .finalLayout = references[referenceIndex].layout
        };
      }
    }
  }

  bool depth = !!info->depth.format;

  if (depth) {
    uint32_t referenceIndex = info->colorCount << hasColorResolve;
    uint32_t index = attachmentCount++;

    references[referenceIndex] = (VkAttachmentReference2) {
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
      .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .attachment = index
    };

    attachments[index] = (VkAttachmentDescription2) {
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
      .format = convertFormat(info->depth.format, LINEAR),
      .samples = info->samples,
      .loadOp = loadOps[info->depth.load],
      .storeOp = info->depth.resolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : storeOps[info->depth.save],
      .stencilLoadOp = loadOps[info->depth.stencilLoad],
      .stencilStoreOp = info->depth.resolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : storeOps[info->depth.stencilSave],
      .initialLayout = references[referenceIndex].layout,
      .finalLayout = references[referenceIndex].layout
    };

    if (info->depth.resolve) {
      uint32_t referenceIndex = (info->colorCount << hasColorResolve) + 1;
      uint32_t index = attachmentCount++;

      references[referenceIndex] = (VkAttachmentReference2) {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
        .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .attachment = index
      };

      attachments[index] = (VkAttachmentDescription2) {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        .format = attachments[index - 1].format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = storeOps[info->depth.stencilSave],
        .initialLayout = references[referenceIndex].layout,
        .finalLayout = references[referenceIndex].layout
      };
    }
  }

  if (info->foveated) {
    attachments[attachmentCount++] = (VkAttachmentDescription2) {
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
      .format = VK_FORMAT_R8G8_UNORM,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
      .finalLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT
    };
  }

  uint32_t referenceCount = (info->colorCount << hasColorResolve) + (depth << info->depth.resolve);

  VkSubpassDescription2 subpass = {
    .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
    .pNext = info->depth.resolve ? &(VkSubpassDescriptionDepthStencilResolve) {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE,
      .depthResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
      .stencilResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
      .pDepthStencilResolveAttachment = &references[referenceCount - 1]
    } : NULL,
    .viewMask = (1 << info->views) - 1,
    .colorAttachmentCount = info->colorCount,
    .pColorAttachments = &references[0],
    .pResolveAttachments = hasColorResolve ? &references[info->colorCount] : NULL,
    .pDepthStencilAttachment = depth ? &references[referenceCount - 1 - info->depth.resolve] : NULL
  };

  VkRenderPassCreateInfo2 createInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
    .pNext = info->foveated ? &(VkRenderPassFragmentDensityMapCreateInfoEXT) {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT,
      .fragmentDensityMapAttachment = {
        .attachment = attachmentCount - 1,
        .layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT
      }
    } : NULL,
    .attachmentCount = attachmentCount,
    .pAttachments = attachments,
    .subpassCount = 1,
    .pSubpasses = &subpass
  };

  VK(vkCreateRenderPass2KHR(state.device, &createInfo, NULL, &pass->handle), "vkCreateRenderPass2KHR") {
    return false;
  }

  pass->colorCount = info->colorCount;
  pass->samples = info->samples;
  pass->loadMask = 0;

  for (uint32_t i = 0; i < pass->colorCount; i++) {
    pass->loadMask |= (info->color[i].load == GPU_LOAD_OP_KEEP) ? (1 << i) : 0;
  }

  pass->depthLoad = info->depth.load == GPU_LOAD_OP_KEEP;
  pass->surface = info->surface;

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

  static const VkFormat attributeTypes[] = {
    [GPU_TYPE_I8x4] = VK_FORMAT_R8G8B8A8_SINT,
    [GPU_TYPE_U8x4] = VK_FORMAT_R8G8B8A8_UINT,
    [GPU_TYPE_SN8x4] = VK_FORMAT_R8G8B8A8_SNORM,
    [GPU_TYPE_UN8x4] = VK_FORMAT_R8G8B8A8_UNORM,
    [GPU_TYPE_SN10x3] = VK_FORMAT_A2B10G10R10_SNORM_PACK32,
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
    .rasterizationSamples = info->pass->samples,
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
    .depthTestEnable = info->depth.test != GPU_COMPARE_NONE || info->depth.write,
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
  for (uint32_t i = 0; i < info->pass->colorCount; i++) {
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
    .attachmentCount = info->pass->colorCount,
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

  gpu_flag_value stackConstants[32];
  VkSpecializationMapEntry stackEntries[32];
  gpu_flag_value* constants = stackConstants;
  VkSpecializationMapEntry* entries = stackEntries;

  if (info->flagCount > COUNTOF(stackConstants)) {
    constants = state.config.fnAlloc(info->flagCount * sizeof(gpu_flag_value));
    ASSERT(constants, "Out of memory") return false;
    entries = state.config.fnAlloc(info->flagCount * sizeof(VkSpecializationMapEntry));
    ASSERT(entries, "Out of memory") return state.config.fnFree(constants), false;
  }

  for (uint32_t i = 0; i < info->flagCount; i++) {
    constants[i] = info->flags[i].value;
    entries[i] = (VkSpecializationMapEntry) {
      .constantID = info->flags[i].id,
      .offset = i * sizeof(uint32_t),
      .size = sizeof(uint32_t)
    };
  }

  VkSpecializationInfo specialization = {
    .mapEntryCount = info->flagCount,
    .pMapEntries = entries,
    .dataSize = info->flagCount * sizeof(gpu_flag_value),
    .pData = (const void*) constants
  };

  uint32_t stageCount = info->shader->handles[1] ? 2 : 1;

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

  VkGraphicsPipelineCreateInfo pipelineInfo = (VkGraphicsPipelineCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = stageCount,
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
    .renderPass = info->pass->handle
  };

  VK(vkCreateGraphicsPipelines(state.device, state.pipelineCache, 1, &pipelineInfo, NULL, &pipeline->handle), "vkCreateGraphicsPipelines") {
    if (constants != stackConstants) state.config.fnFree(constants);
    if (entries != stackEntries) state.config.fnFree(entries);
    return false;
  }

  nickname(pipeline->handle, VK_OBJECT_TYPE_PIPELINE, info->label);
  if (constants != stackConstants) state.config.fnFree(constants);
  if (entries != stackEntries) state.config.fnFree(entries);
  return true;
}

bool gpu_pipeline_init_compute(gpu_pipeline* pipeline, gpu_compute_pipeline_info* info) {
  gpu_flag_value stackConstants[32];
  VkSpecializationMapEntry stackEntries[32];
  gpu_flag_value* constants = stackConstants;
  VkSpecializationMapEntry* entries = stackEntries;

  if (info->flagCount > COUNTOF(stackConstants)) {
    constants = state.config.fnAlloc(info->flagCount * sizeof(gpu_flag_value));
    ASSERT(constants, "Out of memory") return false;
    entries = state.config.fnAlloc(info->flagCount * sizeof(VkSpecializationMapEntry));
    ASSERT(entries, "Out of memory") return state.config.fnFree(constants), false;
  }

  for (uint32_t i = 0; i < info->flagCount; i++) {
    constants[i] = info->flags[i].value;
    entries[i] = (VkSpecializationMapEntry) {
      .constantID = info->flags[i].id,
      .offset = i * sizeof(uint32_t),
      .size = sizeof(uint32_t)
    };
  }

  VkSpecializationInfo specialization = {
    .mapEntryCount = info->flagCount,
    .pMapEntries = entries,
    .dataSize = info->flagCount * sizeof(uint32_t),
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

  VK(vkCreateComputePipelines(state.device, state.pipelineCache, 1, &pipelineInfo, NULL, &pipeline->handle), "vkCreateComputePipelines") {
    if (constants != stackConstants) state.config.fnFree(constants);
    if (entries != stackEntries) state.config.fnFree(entries);
    return false;
  }

  nickname(pipeline->handle, VK_OBJECT_TYPE_PIPELINE, info->label);
  if (constants != stackConstants) state.config.fnFree(constants);
  if (entries != stackEntries) state.config.fnFree(entries);
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
    [GPU_TALLY_PIXEL] = VK_QUERY_TYPE_OCCLUSION
  };

  VkQueryPoolCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
    .queryType = queryTypes[info->type],
    .queryCount = info->count
  };

  VK(vkCreateQueryPool(state.device, &createInfo, NULL, &tally->handle), "vkCreateQueryPool") {
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
  ASSERT(state.streamCount < COUNTOF(tick->streams), "Too many passes") return NULL;
  gpu_stream* stream = &tick->streams[state.streamCount];
  nickname(stream->commands, VK_OBJECT_TYPE_COMMAND_BUFFER, label);

  VkCommandBufferBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };

  VK(vkBeginCommandBuffer(stream->commands, &beginfo), "vkBeginCommandBuffer") return NULL;
  state.streamCount++;
  return stream;
}

bool gpu_stream_end(gpu_stream* stream) {
  VK(vkEndCommandBuffer(stream->commands), "vkEndCommandBuffer") return false;
  return true;
}

void gpu_render_begin(gpu_stream* stream, gpu_canvas* canvas) {
  gpu_pass* pass = canvas->pass;

  // Framebuffer

  VkImageView images[11];
  VkClearValue clears[11];
  uint32_t attachmentCount = 0;

  for (uint32_t i = 0; i < pass->colorCount; i++) {
    images[i] = canvas->color[i].texture->view;
    memcpy(clears[i].color.float32, canvas->color[i].clear, 4 * sizeof(float));
    attachmentCount++;
  }

  for (uint32_t i = 0; i < pass->colorCount; i++) {
    if (canvas->color[i].resolve) images[attachmentCount++] = canvas->color[i].resolve->view;
  }

  if (canvas->depth.texture) {
    uint32_t index = attachmentCount++;
    images[index] = canvas->depth.texture->view;
    clears[index].depthStencil.depth = canvas->depth.clear;
    clears[index].depthStencil.stencil = canvas->depth.stencilClear;

    if (canvas->depth.resolve) {
      images[attachmentCount++] = canvas->depth.resolve->view;
    }
  }

  if (canvas->foveation) {
    uint32_t index = attachmentCount++;
    images[index] = canvas->foveation->view;
  }

  VkFramebufferCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass = pass->handle,
    .attachmentCount = attachmentCount,
    .pAttachments = images,
    .width = canvas->width,
    .height = canvas->height,
    .layers = 1
  };

  VkFramebuffer framebuffer;
  VK(vkCreateFramebuffer(state.device, &info, NULL, &framebuffer), "vkCreateFramebuffer") {} // Ignoring error
  condemn(framebuffer, VK_OBJECT_TYPE_FRAMEBUFFER);

  // Layout transitions

  uint32_t barrierCount = 0;
  VkImageMemoryBarrier2KHR barriers[10];

  bool BEGIN = true;
  bool RESOLVE = true;

  for (uint32_t i = 0; i < pass->colorCount; i++) {
    bool DISCARD = ~pass->loadMask & (1 << i);
    barrierCount += transitionAttachment(canvas->color[i].texture, BEGIN, !RESOLVE, DISCARD, &barriers[barrierCount]);
    barrierCount += transitionAttachment(canvas->color[i].resolve, BEGIN, RESOLVE, true, &barriers[barrierCount]);
  }

  if (canvas->depth.texture) {
    bool DISCARD = !pass->depthLoad;
    barrierCount += transitionAttachment(canvas->depth.texture, BEGIN, !RESOLVE, DISCARD, &barriers[barrierCount]);
    barrierCount += transitionAttachment(canvas->depth.resolve, BEGIN, RESOLVE, true, &barriers[barrierCount]);
  }

  if (barrierCount > 0) {
    vkCmdPipelineBarrier2KHR(stream->commands, &(VkDependencyInfoKHR) {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
      .imageMemoryBarrierCount = barrierCount,
      .pImageMemoryBarriers = barriers
    });
  }

  // Do it!

  VkRenderPassBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = pass->handle,
    .framebuffer = framebuffer,
    .renderArea.offset = { canvas->area[0], canvas->area[1] },
    .renderArea.extent.width = canvas->area[2] ? canvas->area[2] : canvas->width,
    .renderArea.extent.height = canvas->area[3] ? canvas->area[3] : canvas->height,
    .clearValueCount = attachmentCount,
    .pClearValues = clears
  };

  vkCmdBeginRenderPass2KHR(stream->commands, &beginfo, &(VkSubpassBeginInfo) {
    .sType = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO
  });
}

void gpu_render_end(gpu_stream* stream, gpu_canvas* canvas) {
  vkCmdEndRenderPass2KHR(stream->commands, &(VkSubpassEndInfo) {
    .sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO
  });

  gpu_pass* pass = canvas->pass;

  // Layout transitions

  uint32_t barrierCount = 0;
  VkImageMemoryBarrier2KHR barriers[10];

  bool BEGIN = true;
  bool RESOLVE = true;
  bool DISCARD = true;

  for (uint32_t i = 0; i < pass->colorCount; i++) {
    barrierCount += transitionAttachment(canvas->color[i].texture, !BEGIN, !RESOLVE, !DISCARD, &barriers[barrierCount]);
    barrierCount += transitionAttachment(canvas->color[i].resolve, !BEGIN, RESOLVE, !DISCARD, &barriers[barrierCount]);
  }

  barrierCount += transitionAttachment(canvas->depth.texture, !BEGIN, !RESOLVE, !DISCARD, &barriers[barrierCount]);
  barrierCount += transitionAttachment(canvas->depth.resolve, !BEGIN, RESOLVE, !DISCARD, &barriers[barrierCount]);

  if (barrierCount > 0) {
    vkCmdPipelineBarrier2KHR(stream->commands, &(VkDependencyInfoKHR) {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
      .imageMemoryBarrierCount = barrierCount,
      .pImageMemoryBarriers = barriers
    });
  }
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

void gpu_bind_pipeline(gpu_stream* stream, gpu_pipeline* pipeline, gpu_pipeline_type type) {
  VkPipelineBindPoint pipelineTypes[] = {
    [GPU_PIPELINE_GRAPHICS] = VK_PIPELINE_BIND_POINT_GRAPHICS,
    [GPU_PIPELINE_COMPUTE] = VK_PIPELINE_BIND_POINT_COMPUTE
  };
  vkCmdBindPipeline(stream->commands, pipelineTypes[type], pipeline->handle);
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
    offsets64[i] = offsets ? offsets[i] : 0;
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

void gpu_draw_indirect(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount, uint32_t stride) {
  vkCmdDrawIndirect(stream->commands, buffer->handle, offset, drawCount, stride ? stride : 16);
}

void gpu_draw_indirect_indexed(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t drawCount, uint32_t stride) {
  vkCmdDrawIndexedIndirect(stream->commands, buffer->handle, offset, drawCount, stride ? stride : 20);
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
    .bufferOffset = srcOffset,
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
    .bufferOffset = dstOffset,
    .imageSubresource.aspectMask = src->aspect,
    .imageSubresource.mipLevel = srcOffset[3],
    .imageSubresource.baseArrayLayer = src->layers ? srcOffset[2] : 0,
    .imageSubresource.layerCount = src->layers ? extent[2] : 1,
    .imageOffset = { srcOffset[0], srcOffset[1], src->layers ? 0 : srcOffset[2] },
    .imageExtent = { extent[0], extent[1], src->layers ? 1 : extent[2] }
  };

  vkCmdCopyImageToBuffer(stream->commands, src->handle, VK_IMAGE_LAYOUT_GENERAL, dst->handle, 1, &region);
}

void gpu_copy_tally_buffer(gpu_stream* stream, gpu_tally* src, gpu_buffer* dst, uint32_t srcIndex, uint32_t dstOffset, uint32_t count) {
  vkCmdCopyQueryPoolResults(stream->commands, src->handle, srcIndex, count, dst->handle, dstOffset, 4, VK_QUERY_RESULT_WAIT_BIT);
}

void gpu_clear_buffer(gpu_stream* stream, gpu_buffer* buffer, uint32_t offset, uint32_t extent, uint32_t value) {
  vkCmdFillBuffer(stream->commands, buffer->handle, offset, extent, value);
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
  VkMemoryBarrier2KHR memoryBarrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR };

  for (uint32_t i = 0; i < count; i++) {
    gpu_barrier* barrier = &barriers[i];
    memoryBarrier.srcStageMask |= convertPhase(barrier->prev, false);
    memoryBarrier.dstStageMask |= convertPhase(barrier->next, true);
    memoryBarrier.srcAccessMask |= convertCache(barrier->flush);
    memoryBarrier.dstAccessMask |= convertCache(barrier->clear);
  }

  if (memoryBarrier.srcStageMask && memoryBarrier.dstStageMask) {
    vkCmdPipelineBarrier2KHR(stream->commands, &(VkDependencyInfoKHR) {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
      .pMemoryBarriers = &memoryBarrier,
      .memoryBarrierCount = 1
    });
  }
}

void gpu_tally_begin(gpu_stream* stream, gpu_tally* tally, uint32_t index) {
  vkCmdBeginQuery(stream->commands, tally->handle, index, 0);
}

void gpu_tally_finish(gpu_stream* stream, gpu_tally* tally, uint32_t index) {
  vkCmdEndQuery(stream->commands, tally->handle, index);
}

void gpu_tally_mark(gpu_stream* stream, gpu_tally* tally, uint32_t index) {
  vkCmdWriteTimestamp(stream->commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, tally->handle, index);
}

// Acquires an OpenXR swapchain texture, transitioning it to the natural layout
void gpu_xr_acquire(gpu_stream* stream, gpu_texture* texture) {
  vkCmdPipelineBarrier2KHR(stream->commands, &(VkDependencyInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &(VkImageMemoryBarrier2KHR) {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
      .srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR,
      .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      .srcAccessMask = VK_ACCESS_2_NONE_KHR,
      .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
      .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .newLayout = texture->layout,
      .image = texture->handle,
      .subresourceRange.aspectMask = texture->aspect,
      .subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS,
      .subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS
    }
  });
}

// Releases an OpenXR swapchain texture, transitioning it back to the layout expected by OpenXR
void gpu_xr_release(gpu_stream* stream, gpu_texture* texture) {
  vkCmdPipelineBarrier2KHR(stream->commands, &(VkDependencyInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &(VkImageMemoryBarrier2KHR) {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
      .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      .dstStageMask = VK_PIPELINE_STAGE_2_NONE_KHR,
      .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
      .dstAccessMask = VK_ACCESS_2_NONE_KHR,
      .oldLayout = texture->layout,
      .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .image = texture->handle,
      .subresourceRange.aspectMask = texture->aspect,
      .subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS,
      .subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS
    }
  });
}

// Entry

bool gpu_init(gpu_config* config) {
  state.config = *config;

  // Load
#ifdef _WIN32
  state.library = LoadLibraryA("vulkan-1.dll");
  ASSERT(state.library, "Failed to load vulkan library") goto fail;
  vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) GetProcAddress(state.library, "vkGetInstanceProcAddr");
#elif __APPLE__
  state.library = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
  if (!state.library) state.library = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
  ASSERT(state.library, "Failed to load vulkan library") goto fail;
  vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym(state.library, "vkGetInstanceProcAddr");
#else
  state.library = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!state.library) state.library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  ASSERT(state.library, "Failed to load vulkan library") goto fail;
  vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym(state.library, "vkGetInstanceProcAddr");
#endif
  GPU_FOREACH_ANONYMOUS(GPU_LOAD_ANONYMOUS);

  { // Layers
    struct { const char* name; bool shouldEnable; bool* flag; } layers[] = {
      { "VK_LAYER_KHRONOS_validation", config->debug, &state.extensions.validation }
    };

    uint32_t layerCount = 0;
    VK(vkEnumerateInstanceLayerProperties(&layerCount, NULL), "vkEnumerateInstanceLayerProperties") goto fail;
    VkLayerProperties* layerInfo = config->fnAlloc(layerCount * sizeof(*layerInfo));
    ASSERT(layerInfo, "Out of memory") goto fail;
    VK(vkEnumerateInstanceLayerProperties(&layerCount, layerInfo), "vkEnumerateInstanceLayerProperties") goto fail;

    uint32_t enabledLayerCount = 0;
    const char* enabledLayers[COUNTOF(layers)];
    for (uint32_t i = 0; i < COUNTOF(layers); i++) {
      if (layers[i].shouldEnable && hasLayer(layerInfo, layerCount, layers[i].name)) {
        enabledLayers[enabledLayerCount++] = layers[i].name;
        *layers[i].flag = true;
      }
    }

    config->fnFree(layerInfo);

    // Instance Extensions

    struct { const char* name; bool shouldEnable; bool* flag; } extensions[] = {
      { "VK_KHR_portability_enumeration", true, &state.extensions.portability },
      { "VK_EXT_debug_utils", config->debug, &state.extensions.debug },
      { "VK_EXT_swapchain_colorspace", true, &state.extensions.colorspace },
      { "VK_KHR_surface", true, &state.extensions.surface },
#if defined(_WIN32)
      { "VK_KHR_win32_surface", true, &state.extensions.surfaceOS },
#elif defined(__APPLE__)
      { "VK_EXT_metal_surface", true, &state.extensions.surfaceOS },
#elif defined(__linux__) && !defined(__ANDROID__)
      { "VK_KHR_xcb_surface", true, &state.extensions.surfaceOS },
#endif
    };

    uint32_t extensionCount = 0;
    VK(vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL), "vkEnumerateInstanceExtensionProperties") goto fail;
    VkExtensionProperties* extensionInfo = config->fnAlloc(extensionCount * sizeof(*extensionInfo));
    ASSERT(extensionInfo, "Out of memory") goto fail;
    VK(vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensionInfo), "vkEnumerateInstanceExtensionProperties") goto fail;

    uint32_t enabledExtensionCount = 0;
    const char* enabledExtensions[COUNTOF(extensions)];
    for (uint32_t i = 0; i < COUNTOF(extensions); i++) {
      if (extensions[i].shouldEnable && hasExtension(extensionInfo, extensionCount, extensions[i].name)) {
        enabledExtensions[enabledExtensionCount++] = extensions[i].name;
        *extensions[i].flag = true;
      }
    }

    config->fnFree(extensionInfo);

    // Instance

    VkInstanceCreateInfo instanceInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .flags = state.extensions.portability ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR : 0,
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

    if (config->vk.createInstance) {
      VK(config->vk.createInstance(&instanceInfo, NULL, (uintptr_t) &state.instance, (void*) vkGetInstanceProcAddr), "vkCreateInstance") goto fail;
    } else {
      VK(vkCreateInstance(&instanceInfo, NULL, &state.instance), "vkCreateInstance") goto fail;
    }

    GPU_FOREACH_INSTANCE(GPU_LOAD_INSTANCE);

    if (config->debug && config->fnLog) {
      if (state.extensions.debug) {
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
          .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
          .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
          .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
          .pfnUserCallback = relay
        };

        VK(vkCreateDebugUtilsMessengerEXT(state.instance, &messengerInfo, NULL, &state.messenger), "vkCreateDebugUtilsMessengerEXT") goto fail;

        if (!state.extensions.validation) {
          LOG("Warning: GPU debugging is enabled, but validation layer is not installed");
        }
      } else {
        LOG("Warning: GPU debugging is enabled, but debug extension is not supported");
      }
    }
  }

  { // Device
    if (config->vk.getPhysicalDevice) {
      config->vk.getPhysicalDevice(state.instance, (uintptr_t) &state.adapter);
    } else {
      uint32_t deviceCount = 1;
      VK(vkEnumeratePhysicalDevices(state.instance, &deviceCount, &state.adapter), "vkEnumeratePhysicalDevices") goto fail;
    }

    // Device Extensions

    struct { const char* name; bool shouldEnable; bool* flag; } extensions[] = {
      { "VK_KHR_create_renderpass2", true, &state.extensions.renderPass2 },
      { "VK_KHR_swapchain", true, &state.extensions.swapchain },
      { "VK_KHR_portability_subset", true, &state.extensions.portability },
      { "VK_KHR_depth_stencil_resolve", true, &state.extensions.depthResolve },
      { "VK_KHR_shader_non_semantic_info", config->debug, &state.extensions.shaderDebug },
      { "VK_KHR_image_format_list", true, &state.extensions.formatList },
      { "VK_KHR_synchronization2", true, &state.extensions.synchronization2 },
      { "VK_EXT_scalar_block_layout", true, &state.extensions.scalarBlockLayout },
      { "VK_EXT_fragment_density_map", true, &state.extensions.foveation }
    };

    uint32_t extensionCount = 0;
    VK(vkEnumerateDeviceExtensionProperties(state.adapter, NULL, &extensionCount, NULL), "vkEnumerateDeviceExtensionProperties") goto fail;
    VkExtensionProperties* extensionInfo = config->fnAlloc(extensionCount * sizeof(*extensionInfo));
    ASSERT(extensionInfo, "Out of memory") goto fail;
    VK(vkEnumerateDeviceExtensionProperties(state.adapter, NULL, &extensionCount, extensionInfo), "vkEnumerateDeviceExtensionProperties") goto fail;

    uint32_t enabledExtensionCount = 0;
    const char* enabledExtensions[COUNTOF(extensions)];
    for (uint32_t i = 0; i < COUNTOF(extensions); i++) {
      if (extensions[i].shouldEnable && hasExtension(extensionInfo, extensionCount, extensions[i].name)) {
        enabledExtensions[enabledExtensionCount++] = extensions[i].name;
        *extensions[i].flag = true;
      }
    }

    ASSERT(state.extensions.renderPass2, "GPU driver is missing required Vulkan extension VK_KHR_render_pass2") goto fail;
    ASSERT(state.extensions.synchronization2, "GPU driver is missing required Vulkan extension VK_KHR_synchronization2") goto fail;

    config->fnFree(extensionInfo);

    // Device Info

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

    // Limits

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

    // Features

    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT
    };

    VkPhysicalDeviceScalarBlockLayoutFeaturesEXT scalarBlockLayoutFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT
    };

    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR
    };

    VkPhysicalDeviceShaderDrawParameterFeatures shaderDrawParameterFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES,
      .pNext = &synchronization2Features
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

      if (state.extensions.foveation) {
        fragmentDensityMapFeatures.pNext = features2.pNext;
        features2.pNext = &fragmentDensityMapFeatures;
      }

      vkGetPhysicalDeviceFeatures2(state.adapter, &features2);

      // Required features
      enable->fullDrawIndexUint32 = true;
      enable->imageCubeArray = true;
      enable->independentBlend = true;
      enable->sampleRateShading = true;
      synchronization2Features.synchronization2 = true;
      multiviewFeatures.multiview = true;
      shaderDrawParameterFeatures.shaderDrawParameters = true;

      // Internal features (exposed as limits)
      enable->samplerAnisotropy = supports->samplerAnisotropy;
      enable->multiDrawIndirect = supports->multiDrawIndirect;
      enable->shaderClipDistance = supports->shaderClipDistance;
      enable->shaderCullDistance = supports->shaderCullDistance;
      enable->largePoints = supports->largePoints;

      // Optional features (currently always enabled when supported)
      config->features->textureBC = (enable->textureCompressionBC = supports->textureCompressionBC);
      config->features->textureASTC = (enable->textureCompressionASTC_LDR = supports->textureCompressionASTC_LDR);
      config->features->wireframe = (enable->fillModeNonSolid = supports->fillModeNonSolid);
      config->features->depthClamp = (enable->depthClamp = supports->depthClamp);
      config->features->indirectDrawFirstInstance = (enable->drawIndirectFirstInstance = supports->drawIndirectFirstInstance);
      config->features->float64 = (enable->shaderFloat64 = supports->shaderFloat64);
      config->features->int64 = (enable->shaderInt64 = supports->shaderInt64);
      config->features->int16 = (enable->shaderInt16 = supports->shaderInt16);

      // Extension "features"
      config->features->depthResolve = state.extensions.depthResolve;
      config->features->packedBuffers = state.extensions.scalarBlockLayout;
      config->features->shaderDebug = state.extensions.shaderDebug;

      if (state.extensions.scalarBlockLayout) {
        scalarBlockLayoutFeatures.scalarBlockLayout = true;
        scalarBlockLayoutFeatures.pNext = enabledFeatures.pNext;
        enabledFeatures.pNext = &scalarBlockLayoutFeatures;
      }

      if (state.extensions.foveation && fragmentDensityMapFeatures.fragmentDensityMap) {
        fragmentDensityMapFeatures.fragmentDensityMapDynamic = false;
        fragmentDensityMapFeatures.fragmentDensityMapNonSubsampledImages = true;
        fragmentDensityMapFeatures.pNext = enabledFeatures.pNext;
        enabledFeatures.pNext = &fragmentDensityMapFeatures;
        config->features->foveation = true;
      }

      // Formats
      for (uint32_t i = 0; i < GPU_FORMAT_COUNT; i++) {
        for (int j = 0; j < 2; j++) {
          VkFormat format = convertFormat(i, j);
          if (j == 1 && convertFormat(i, 0) == format) {
            config->features->formats[i][j] = config->features->formats[i][0];
          } else {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(state.adapter, format, &formatProperties);
            uint32_t sampleMask = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
            uint32_t renderMask = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
            uint32_t blitMask = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
            uint32_t flags = formatProperties.optimalTilingFeatures;
            config->features->formats[i][j] =
              ((flags & sampleMask) ? GPU_FEATURE_SAMPLE : 0) |
              ((flags & renderMask) == renderMask ? GPU_FEATURE_RENDER : 0) |
              ((flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ? GPU_FEATURE_RENDER : 0) |
              ((flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) ? GPU_FEATURE_STORAGE : 0) |
              ((flags & blitMask) == blitMask ? GPU_FEATURE_BLIT : 0);
          }
        }
      }

      // Sample counts
      for (uint32_t i = 1; i <= 16; i++) {
        VkPhysicalDeviceLimits* limits = &properties2.properties.limits;
        if (~limits->framebufferColorSampleCounts & i) continue;
        if (~limits->framebufferDepthSampleCounts & i) continue;
        if (~limits->framebufferStencilSampleCounts & i) continue;
        if (~limits->sampledImageColorSampleCounts & i) continue;
        if (~limits->sampledImageDepthSampleCounts & i) continue;
        config->features->sampleCounts |= i;
      }
    }

    // Queue Family

    state.queueFamilyIndex = ~0u;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(state.adapter, &queueFamilyCount, NULL);
    VkQueueFamilyProperties* queueFamilies = config->fnAlloc(queueFamilyCount * sizeof(*queueFamilies));
    ASSERT(queueFamilies, "Out of memory") goto fail;
    vkGetPhysicalDeviceQueueFamilyProperties(state.adapter, &queueFamilyCount, queueFamilies);
    uint32_t mask = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
      if ((queueFamilies[i].queueFlags & mask) == mask) {
        state.queueFamilyIndex = i;
        break;
      }
    }

    config->fnFree(queueFamilies);
    ASSERT(state.queueFamilyIndex != ~0u, "No GPU queue families available") goto fail;

    // Device

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

    if (config->vk.createDevice) {
      VK(config->vk.createDevice(state.instance, &deviceInfo, NULL, (uintptr_t) &state.device, (void*) vkGetInstanceProcAddr), "vkCreateDevice") goto fail;
    } else {
      VK(vkCreateDevice(state.adapter, &deviceInfo, NULL, &state.device), "vkCreateDevice") goto fail;
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

    // There are 4 types of buffer memory, which use different strategies/memory types:
    // - STATIC: Regular device-local memory.  Not necessarily mappable, fast to read on GPU.
    // - STREAM: Used to "stream" data to the GPU, to be read by shaders.  This tries to use the
    //   special 256MB memory type present on discrete GPUs because it's both device local and host-
    //   visible and that supposedly makes it fast.  A single buffer is allocated with a "zone" for
    //   each tick.  If one of the zones fills up, a new bigger buffer is allocated.  It's important
    //   to have one buffer and keep it alive since streaming is expected to happen very frequently.
    // - UPLOAD: Used to stage data to upload to buffers/textures.  Can only be used for transfers.
    //   Uses uncached host-visible memory to not pollute the CPU cache or waste the STREAM memory.
    // - DOWNLOAD: Used for readbacks.  Uses cached memory when available since reading from
    //   uncached memory on the CPU is super duper slow.
    VkMemoryPropertyFlags bufferFlags[] = {
      [GPU_MEMORY_BUFFER_STATIC] = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      [GPU_MEMORY_BUFFER_STREAM] = hostVisible | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      [GPU_MEMORY_BUFFER_UPLOAD] = hostVisible,
      [GPU_MEMORY_BUFFER_DOWNLOAD] = hostVisible | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
    };

    for (uint32_t i = 0; i < COUNTOF(bufferFlags); i++) {
      gpu_allocator* allocator = &state.allocators[i];
      state.allocatorLookup[i] = i;

      VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = getBufferUsage(i),
        .size = 4
      };

      VkBuffer buffer;
      VkMemoryRequirements requirements;
      vkCreateBuffer(state.device, &info, NULL, &buffer);
      vkGetBufferMemoryRequirements(state.device, buffer, &requirements);
      vkDestroyBuffer(state.device, buffer, NULL);

      VkMemoryPropertyFlags fallback = i == GPU_MEMORY_BUFFER_STATIC ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : hostVisible;

      for (uint32_t j = 0; j < memoryProperties.memoryTypeCount; j++) {
        if (~requirements.memoryTypeBits & (1 << j)) {
          continue;
        }

        if ((memoryTypes[j].propertyFlags & bufferFlags[i]) == bufferFlags[i]) {
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
      [GPU_MEMORY_TEXTURE_D24] = { VK_FORMAT_X8_D24_UNORM_PACK32, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_D32F] = { VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_D24S8] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_D32FS8] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_LAZY_COLOR] = { VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | transient },
      [GPU_MEMORY_TEXTURE_LAZY_D16] = { VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient },
      [GPU_MEMORY_TEXTURE_LAZY_D24] = { VK_FORMAT_X8_D24_UNORM_PACK32, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient },
      [GPU_MEMORY_TEXTURE_LAZY_D32F] = { VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient },
      [GPU_MEMORY_TEXTURE_LAZY_D24S8] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient },
      [GPU_MEMORY_TEXTURE_LAZY_D32FS8] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | transient }
    };

    uint32_t allocatorCount = GPU_MEMORY_TEXTURE_COLOR;

    for (uint32_t i = GPU_MEMORY_TEXTURE_COLOR; i < COUNTOF(imageFlags); i++) {
      VkFormatProperties formatProperties;
      vkGetPhysicalDeviceFormatProperties(state.adapter, imageFlags[i].format, &formatProperties);
      if (formatProperties.optimalTilingFeatures == 0) {
        state.allocatorLookup[i] = 0xff;
        continue;
      }

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

    VK(vkCreateCommandPool(state.device, &poolInfo, NULL, &state.ticks[i].pool), "vkCreateCommandPool") goto fail;

    VkCommandBufferAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = state.ticks[i].pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = COUNTOF(state.ticks[i].streams)
    };

    VkCommandBuffer* commandBuffers = &state.ticks[i].streams[0].commands;
    VK(vkAllocateCommandBuffers(state.device, &allocateInfo, commandBuffers), "vkAllocateCommandBuffers") goto fail;

    VkSemaphoreCreateInfo semaphoreInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VK(vkCreateSemaphore(state.device, &semaphoreInfo, NULL, &state.ticks[i].semaphores[0]), "vkCreateSemaphore") goto fail;
    VK(vkCreateSemaphore(state.device, &semaphoreInfo, NULL, &state.ticks[i].semaphores[1]), "vkCreateSemaphore") goto fail;

    VkFenceCreateInfo fenceInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VK(vkCreateFence(state.device, &fenceInfo, NULL, &state.ticks[i].fence), "vkCreateFence") goto fail;
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

  VK(vkCreatePipelineCache(state.device, &cacheInfo, NULL, &state.pipelineCache), "vkCreatePipelineCache") goto fail;

  state.tick[CPU] = COUNTOF(state.ticks) - 1;
  return true;

fail:
  gpu_destroy();
  return false;
}

void gpu_destroy(void) {
  if (state.device) vkDeviceWaitIdle(state.device);
  state.tick[GPU] = state.tick[CPU];
  expunge();
  if (state.pipelineCache) vkDestroyPipelineCache(state.device, state.pipelineCache, NULL);
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
  for (uint32_t i = 0; i < COUNTOF(state.surface.images); i++) {
    if (state.surface.images[i].view) vkDestroyImageView(state.device, state.surface.images[i].view, NULL);
  }
  if (state.surface.swapchain) vkDestroySwapchainKHR(state.device, state.surface.swapchain, NULL);
  if (state.device) vkDestroyDevice(state.device, NULL);
  if (state.surface.handle) vkDestroySurfaceKHR(state.instance, state.surface.handle, NULL);
  if (state.messenger) vkDestroyDebugUtilsMessengerEXT(state.instance, state.messenger, NULL);
  if (state.instance) vkDestroyInstance(state.instance, NULL);
#ifdef _WIN32
  if (state.library) FreeLibrary(state.library);
#else
  if (state.library) dlclose(state.library);
#endif
  memset(&state, 0, sizeof(state));
}

const char* gpu_get_error(void) {
  return thread.error;
}

bool gpu_begin(uint32_t* t) {
  uint32_t nextTick = state.tick[CPU] + 1;
  if (!gpu_wait_tick(nextTick - COUNTOF(state.ticks), NULL)) {
    return false;
  }

  gpu_tick* tick = &state.ticks[nextTick & TICK_MASK];
  VK(vkResetFences(state.device, 1, &tick->fence), "vkResetFences") return false;
  VK(vkResetCommandPool(state.device, tick->pool, 0), "vkResetCommandPool") return false;
  state.streamCount = 0;
  expunge();

  state.tick[CPU] = nextTick;
  if (t) *t = nextTick;
  return true;
}

bool gpu_submit(gpu_stream** streams, uint32_t count) {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & TICK_MASK];

  VkCommandBuffer commands[COUNTOF(tick->streams)];
  for (uint32_t i = 0; i < count; i++) {
    commands[i] = streams[i]->commands;
  }

  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = !!state.surface.semaphore,
    .pWaitSemaphores = &state.surface.semaphore,
    .pWaitDstStageMask = &waitStage,
    .commandBufferCount = count,
    .pCommandBuffers = commands
  };

  VK(vkQueueSubmit(state.queue, 1, &submit, tick->fence), "vkQueueSubmit") return false;
  state.surface.semaphore = VK_NULL_HANDLE;
  return true;
}

bool gpu_is_complete(uint32_t tick) {
  return state.tick[GPU] >= tick;
}

bool gpu_wait_tick(uint32_t tick, bool* waited) {
  if (state.tick[GPU] < tick) {
    VkFence fence = state.ticks[tick & TICK_MASK].fence;
    VK(vkWaitForFences(state.device, 1, &fence, VK_FALSE, ~0ull), "vkWaitForFences") return false;
    if (waited) *waited = true;
    state.tick[GPU] = tick;
    return true;
  } else {
    if (waited) *waited = false;
    return true;
  }
}

bool gpu_wait_idle(void) {
  VK(vkDeviceWaitIdle(state.device), "vkDeviceWaitIdle") return false;
  state.tick[GPU] = state.tick[CPU];
  return true;
}

uintptr_t gpu_vk_get_instance(void) {
  return (uintptr_t) state.instance;
}

uintptr_t gpu_vk_get_physical_device(void) {
  return (uintptr_t) state.adapter;
}

uintptr_t gpu_vk_get_device(void) {
  return (uintptr_t) state.device;
}

uintptr_t gpu_vk_get_queue(uint32_t* queueFamilyIndex, uint32_t* queueIndex) {
  return *queueFamilyIndex = state.queueFamilyIndex, *queueIndex = 0, (uintptr_t) state.queue;
}

// Helpers

static gpu_memory* allocate(gpu_memory_type type, VkMemoryRequirements info, VkDeviceSize* offset) {
  gpu_allocator* allocator = &state.allocators[state.allocatorLookup[type]];

  static const uint32_t blockSizes[] = {
    [GPU_MEMORY_BUFFER_STATIC] = 1 << 26,
    [GPU_MEMORY_BUFFER_STREAM] = 0,
    [GPU_MEMORY_BUFFER_UPLOAD] = 0,
    [GPU_MEMORY_BUFFER_DOWNLOAD] = 0,
    [GPU_MEMORY_TEXTURE_COLOR] = 1 << 28,
    [GPU_MEMORY_TEXTURE_D16] = 1 << 28,
    [GPU_MEMORY_TEXTURE_D24] = 1 << 28,
    [GPU_MEMORY_TEXTURE_D32F] = 1 << 28,
    [GPU_MEMORY_TEXTURE_D24S8] = 1 << 28,
    [GPU_MEMORY_TEXTURE_D32FS8] = 1 << 28,
    [GPU_MEMORY_TEXTURE_LAZY_COLOR] = 1 << 28,
    [GPU_MEMORY_TEXTURE_LAZY_D16] = 1 << 28,
    [GPU_MEMORY_TEXTURE_LAZY_D24] = 1 << 28,
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
      } else {
        memory->pointer = NULL;
      }

      allocator->block = memory;
      allocator->cursor = info.size;
      allocator->block->refs = 1;
      *offset = 0;
      return memory;
    }
  }

  error("Out of GPU memory blocks");
  return NULL;
}

static void release(gpu_memory* memory) {
  if (memory && --memory->refs == 0) {
    condemn(memory->handle, VK_OBJECT_TYPE_DEVICE_MEMORY);
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

  // If the morgue is full, try expunging to reclaim some space
  if (morgue->head - morgue->tail >= COUNTOF(morgue->data)) {
    expunge();

    // If that didn't work, wait for the GPU to be done with the oldest victim and retry
    if (morgue->head - morgue->tail >= COUNTOF(morgue->data)) {
      gpu_wait_tick(morgue->data[morgue->tail & MORGUE_MASK].tick, NULL);
      expunge();
    }

    // The following should be unreachable
    ASSERT(morgue->head - morgue->tail < COUNTOF(morgue->data), "Morgue overflow!") return;
  }

  morgue->data[morgue->head++ & MORGUE_MASK] = (gpu_victim) { handle, type, state.tick[CPU] };
}

static void expunge(void) {
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
      default: LOG("Trying to destroy invalid Vulkan object type!"); break;
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

static VkBufferUsageFlags getBufferUsage(gpu_buffer_type type) {
  switch (type) {
    case GPU_BUFFER_STATIC:
      return
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GPU_BUFFER_STREAM:
      return
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    case GPU_BUFFER_UPLOAD:
      return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    case GPU_BUFFER_DOWNLOAD:
      return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    default: return 0;
  }
}

static bool transitionAttachment(gpu_texture* texture, bool begin, bool resolve, bool discard, VkImageMemoryBarrier2KHR* barrier) {
  if (!texture || texture->layout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR) {
    return false;
  }

  bool depth = texture->aspect != VK_IMAGE_ASPECT_COLOR_BIT;

  VkPipelineStageFlags2 stage = (depth && !resolve) ?
    (VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR ) :
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

  VkAccessFlags2 access = (depth && !resolve) ?
    (VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR) :
    (VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR);

  if (begin) {
    *barrier = (VkImageMemoryBarrier2KHR) {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
      .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      .srcAccessMask = VK_ACCESS_2_NONE_KHR,
      .dstStageMask = stage,
      .dstAccessMask = access,
      .oldLayout = discard || resolve ? VK_IMAGE_LAYOUT_UNDEFINED : texture->layout,
      .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .image = texture->handle,
      .subresourceRange.aspectMask = texture->aspect,
      .subresourceRange.baseMipLevel = texture->baseLevel,
      .subresourceRange.levelCount = 1,
      .subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS
    };
  } else {
    *barrier = (VkImageMemoryBarrier2KHR) {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
      .srcStageMask = stage,
      .srcAccessMask = access,
      .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
      .dstAccessMask = VK_ACCESS_2_NONE_KHR,
      .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .newLayout = texture->layout,
      .image = texture->handle,
      .subresourceRange.aspectMask = texture->aspect,
      .subresourceRange.baseMipLevel = texture->baseLevel,
      .subresourceRange.levelCount = 1,
      .subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS
    };
  }

  return true;
}

static VkImageLayout getNaturalLayout(uint32_t usage, VkImageAspectFlags aspect) {
  if (usage & (GPU_TEXTURE_STORAGE | GPU_TEXTURE_COPY_SRC | GPU_TEXTURE_COPY_DST)) {
    return VK_IMAGE_LAYOUT_GENERAL;
  } else if (usage & GPU_TEXTURE_SAMPLE) {
    return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;
  } else {
    return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
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
    [GPU_FORMAT_D24] = { VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_X8_D24_UNORM_PACK32 },
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
    return state.surface.format.format;
  }

  return formats[format][colorspace];
}

static VkPipelineStageFlags2 convertPhase(gpu_phase phase, bool dst) {
  VkPipelineStageFlags2 flags = 0;
  if (phase & GPU_PHASE_INDIRECT) flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR;
  if (phase & GPU_PHASE_INPUT_INDEX) flags |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR;
  if (phase & GPU_PHASE_INPUT_VERTEX) flags |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR;
  if (phase & GPU_PHASE_SHADER_VERTEX) flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;
  if (phase & GPU_PHASE_SHADER_FRAGMENT) flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
  if (phase & GPU_PHASE_SHADER_COMPUTE) flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
  if (phase & GPU_PHASE_DEPTH_EARLY) flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR;
  if (phase & GPU_PHASE_DEPTH_LATE) flags |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR;
  if (phase & GPU_PHASE_COLOR) flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
  if (phase & GPU_PHASE_COPY) flags |= VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
  if (phase & GPU_PHASE_CLEAR) flags |= VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR;
  if (phase & GPU_PHASE_BLIT) flags |= VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;
  return flags;
}

static VkAccessFlags2 convertCache(gpu_cache cache) {
  VkAccessFlags2 flags = 0;
  if (cache & GPU_CACHE_INDIRECT) flags |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR;
  if (cache & GPU_CACHE_INDEX) flags |= VK_ACCESS_2_INDEX_READ_BIT_KHR;
  if (cache & GPU_CACHE_VERTEX) flags |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR;
  if (cache & GPU_CACHE_UNIFORM) flags |= VK_ACCESS_2_UNIFORM_READ_BIT_KHR;
  if (cache & GPU_CACHE_TEXTURE) flags |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR;
  if (cache & GPU_CACHE_STORAGE_READ) flags |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR;
  if (cache & GPU_CACHE_STORAGE_WRITE) flags |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR;
  if (cache & GPU_CACHE_DEPTH_READ) flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR;
  if (cache & GPU_CACHE_DEPTH_WRITE) flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR;
  if (cache & GPU_CACHE_COLOR_READ) flags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR;
  if (cache & GPU_CACHE_COLOR_WRITE) flags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;
  if (cache & GPU_CACHE_TRANSFER_READ) flags |= VK_ACCESS_2_TRANSFER_READ_BIT_KHR;
  if (cache & GPU_CACHE_TRANSFER_WRITE) flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
  return flags;
}

static VkBool32 relay(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userdata) {
  LOG(data->pMessage);
  return VK_FALSE;
}

static void nickname(void* handle, VkObjectType type, const char* name) {
  if (name && state.extensions.debug) {
    union { uint64_t u64; void* p; } pointer = { .p = handle };

    VkDebugUtilsObjectNameInfoEXT info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = type,
      .objectHandle = pointer.u64,
      .pObjectName = name
    };

    // Success is optional
    vkSetDebugUtilsObjectNameEXT(state.device, &info);
  }
}

static bool vkcheck(VkResult result, const char* function) {
  if (result >= VK_SUCCESS) {
    return true;
  } else {
    vkerror(result, function);
    return false;
  }
}

static void vkerror(VkResult result, const char* function) {
  const char* suffix = "";
  switch (result) {
#define CASE(x) case x: suffix = " failed with " #x; break;
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
    default: suffix = " failed with unknown error"; break;
#undef CASE
  }

  size_t length1 = strlen(function);
  size_t length2 = strlen(suffix);

  if (length1 < sizeof(thread.error)) {
    memcpy(thread.error, function, length1 + 1);
    if (length1 + length2 < sizeof(thread.error)) {
      memcpy(thread.error + length1, suffix, length2 + 1);
    }
  } else {
    size_t length = sizeof(thread.error) - 1;
    memcpy(thread.error, function, length);
    thread.error[length] = '\0';
  }
}

static void error(const char* error) {
  size_t length = strlen(error);
  length = MIN(length, sizeof(thread.error));
  memcpy(thread.error, error, length);
  thread.error[length] = '\0';
}
