#include "gpu.h"
#include <string.h>
#include <threads.h>
#include <stdatomic.h>

#ifdef _WIN32
#define THREAD_LOCAL __declspec(thread)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#ifdef __APPLE__
#include <stdlib.h>
#endif
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
typedef struct gpu_upload gpu_upload;

struct gpu_buffer {
  VkBuffer handle;
  gpu_memory* memory;
  VkDeviceSize offset;
};

struct gpu_tree {
  VkAccelerationStructureKHR handle;
  VkAccelerationStructureGeometryKHR* geometries;
  VkAccelerationStructureBuildRangeInfoKHR* ranges;
  VkBuildAccelerationStructureFlagBitsKHR flags;
  gpu_buffer buffer;
  gpu_buffer scratch;
};

struct gpu_texture {
  VkImage handle;
  VkImageView view;
  gpu_memory* memory;
  VkDeviceSize offset;
  VkImageAspectFlagBits aspect;
  VkImageLayout layout;
  uint32_t layers;
  uint8_t samples;
  uint8_t baseLevel;
  uint8_t format;
  bool hostCopy;
  bool imported;
  bool srgb;
};

struct gpu_sampler {
  VkSampler handle;
};

struct gpu_layout {
  VkDescriptorSetLayout handle;
  uint32_t descriptorCounts[9];
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
  gpu_stream* next;
};

size_t gpu_sizeof_buffer(void) { return sizeof(gpu_buffer); }
size_t gpu_sizeof_tree(void) { return sizeof(gpu_tree); }
size_t gpu_sizeof_texture(void) { return sizeof(gpu_texture); }
size_t gpu_sizeof_sampler(void) { return sizeof(gpu_sampler); }
size_t gpu_sizeof_layout(void) { return sizeof(gpu_layout); }
size_t gpu_sizeof_shader(void) { return sizeof(gpu_shader); }
size_t gpu_sizeof_bundle_pool(void) { return sizeof(gpu_bundle_pool); }
size_t gpu_sizeof_bundle(void) { return sizeof(gpu_bundle); }
size_t gpu_sizeof_pipeline(void) { return sizeof(gpu_pipeline); }
size_t gpu_sizeof_tally(void) { return sizeof(gpu_tally); }

// Internals

#define FRAME_DEPTH 2

// GPU memory blocks are divided into 16 KB pages. Allocators have arrays with
// an entry for every page, which they use to track regions of allocated and
// free spaces.

#define GPU_PAGE_SIZE (1 << 14)
#define GPU_MAX_PAGES ((1 << 26) / GPU_PAGE_SIZE)

typedef enum {
  GPU_MEMORY_BUFFER_STATIC,
  GPU_MEMORY_BUFFER_STREAM,
  GPU_MEMORY_BUFFER_UPLOAD,
  GPU_MEMORY_BUFFER_DOWNLOAD,
  GPU_MEMORY_BUFFER_TREE,
  GPU_MEMORY_TEXTURE_COLOR,
  GPU_MEMORY_TEXTURE_HOST_COLOR,
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

struct gpu_memory {
  VkDeviceMemory handle;
  void* pointer;
  uint32_t refs;
  gpu_memory_type type;
};

typedef struct {
  uint16_t allocated : 1;
  uint16_t pageCount : 15;
} gpu_alloc_entry;

typedef struct {
  gpu_memory* block;
  uint32_t pageCount;
  uint32_t heapIndex;
  uint32_t memoryType;
  uint32_t fallbackMemoryType;
  VkMemoryPropertyFlags memoryFlags;
  gpu_alloc_entry regions[GPU_MAX_PAGES];
} gpu_allocator;

struct gpu_upload {
  gpu_upload* next;
  gpu_buffer buffer;
  VkImage image;
  VkImageLayout layout;
  VkImageAspectFlags aspect;
  VkBufferImageCopy copies[16];
  uint32_t copyCount;
};

typedef struct gpu_victim {
  struct gpu_victim* next;
  VkObjectType type;
  uint32_t tick;
  void* handle;
} gpu_victim;

typedef struct {
  gpu_victim* head;
  gpu_victim* tail;
  gpu_victim* pool;
  mtx_t lock;
} gpu_morgue;

typedef struct {
  VkSurfaceKHR handle;
  VkSwapchainKHR swapchain;
  VkSurfaceCapabilitiesKHR capabilities;
  VkSurfaceFormatKHR vkformat;
  gpu_texture_format format;
  uint32_t acquireTick[FRAME_DEPTH];
  VkSemaphore acquireSemaphores[FRAME_DEPTH];
  VkSemaphore acquireSemaphore;
  VkSemaphore presentSemaphores[8];
  gpu_texture images[8];
  uint32_t imageIndex;
  uint32_t width;
  uint32_t height;
  bool vsync;
  bool valid;
} gpu_surface;

typedef struct gpu_stream_pool {
  struct gpu_stream_pool* next;
  VkCommandPool handle;
  gpu_stream* head;
  gpu_stream* tail;
  uint32_t tick;
} gpu_stream_pool;

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
  bool dynamicRendering;
  bool timelineSemaphore;
  bool scalarBlockLayout;
  bool foveation;
  bool pipelineCacheControl;
  bool memoryBudget;
  bool accelerationStructure;
  bool bufferDeviceAddress;
  bool descriptorIndexing;
  bool deferredHostOperations;
  bool shaderFloatControls;
  bool spirv14;
  bool rayQuery;
  bool copy2;
  bool formatFlags2;
  bool hostImageCopy;
} gpu_extensions;

// State

typedef struct gpu_thread_state {
  struct gpu_thread_state* next;
  gpu_stream_pool* streamPools;
  gpu_stream_pool* activeStreamPool;
  char error[255];
  bool initialized;
} gpu_thread_state;

static THREAD_LOCAL gpu_thread_state thread;

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
  uint32_t tick;
  uint32_t frame;
  VkSemaphore semaphore;
  VkPipelineCache pipelineCache;
  VkDebugUtilsMessengerEXT messenger;
  gpu_allocator allocators[GPU_MEMORY_COUNT];
  uint8_t allocatorLookup[GPU_MEMORY_COUNT];
  mtx_t allocatorLock;
  gpu_memory memory[1024];
  _Atomic(gpu_thread_state*) threads;
  _Atomic(gpu_upload*) uploads;
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
#define FRAME_MASK (FRAME_DEPTH - 1)

static gpu_memory* allocate(gpu_memory_type type, VkMemoryRequirements info, VkDeviceSize* offset);
static void release(gpu_memory* memory, VkDeviceSize offset);
static void condemn(void* handle, VkObjectType type);
static void expunge(uint64_t tick);
static uint64_t getFinishedTick(void);
static bool hasLayer(VkLayerProperties* layers, uint32_t count, const char* layer);
static bool hasExtension(VkExtensionProperties* extensions, uint32_t count, const char* extension);
static VkBufferUsageFlags getBufferUsage(gpu_buffer_type type);
static bool transitionAttachment(gpu_texture* texture, bool begin, bool resolve, bool discard, VkImageMemoryBarrier2KHR* barrier);
static VkImageLayout getNaturalLayout(uint32_t usage);
static VkFormat convertFormat(gpu_texture_format format, int colorspace);
static VkFormat convertAttributeType(gpu_attribute_type type);
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
  X(vkGetPhysicalDeviceMemoryProperties2)\
  X(vkGetPhysicalDeviceFormatProperties)\
  X(vkGetPhysicalDeviceQueueFamilyProperties)\
  X(vkGetPhysicalDeviceSurfaceSupportKHR)\
  X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)\
  X(vkGetPhysicalDeviceSurfaceFormatsKHR)\
  X(vkGetPhysicalDeviceImageFormatProperties2)\
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
  X(vkWaitSemaphoresKHR)\
  X(vkGetSemaphoreCounterValueKHR)\
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
  X(vkGetBufferDeviceAddressKHR)\
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
  X(vkCmdBeginRenderingKHR)\
  X(vkCmdEndRenderingKHR)\
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
  X(vkCmdDispatchIndirect)\
  X(vkGetAccelerationStructureBuildSizesKHR)\
  X(vkCreateAccelerationStructureKHR)\
  X(vkDestroyAccelerationStructureKHR)\
  X(vkCmdBuildAccelerationStructuresKHR)\
  X(vkCopyMemoryToImageEXT)\
  X(vkTransitionImageLayoutEXT)

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

  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(state.device, buffer->handle, &requirements);

  if ((buffer->memory = allocate((gpu_memory_type) info->type, requirements, &buffer->offset)) == NULL) {
    vkDestroyBuffer(state.device, buffer->handle, NULL);
    return false;
  }

  VK(vkBindBufferMemory(state.device, buffer->handle, buffer->memory->handle, buffer->offset), "vkBindBufferMemory") {
    vkDestroyBuffer(state.device, buffer->handle, NULL);
    release(buffer->memory, buffer->offset);
    return false;
  }

  if (info->pointer) {
    *info->pointer = buffer->memory->pointer ? (char*) buffer->memory->pointer + buffer->offset : NULL;
  }

  return true;
}

void gpu_buffer_destroy(gpu_buffer* buffer) {
  if (!buffer->memory) return;
  condemn(buffer->handle, VK_OBJECT_TYPE_BUFFER);
  release(buffer->memory, buffer->offset);
}

gpu_address gpu_buffer_get_address(gpu_buffer* buffer, uint32_t offset) {
  return vkGetBufferDeviceAddressKHR(state.device, &(VkBufferDeviceAddressInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR,
    .buffer = buffer->handle
  }) + offset;
}

void gpu_buffer_flush(gpu_buffer* buffer, uint32_t offset, uint32_t extent) {
  //
}

// Tree

bool gpu_tree_init(gpu_tree* tree, gpu_tree_info* info) {
  VkAccelerationStructureGeometryKHR geometry;

  if (info->type == GPU_TREE_TOP) {
    geometry = (VkAccelerationStructureGeometryKHR) {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
      .geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR
    };
  } else {
    tree->geometries = state.config.fnAlloc(info->capacity * sizeof(*tree->geometries));
    tree->ranges = state.config.fnAlloc(info->capacity * sizeof(*tree->ranges));
    ASSERT(tree->geometries && tree->ranges, "Out of memory") return false;

    gpu_geometry_info* geometry = info->geometries;

    for (uint32_t i = 0; i < info->capacity; i++, geometry++) {
      tree->geometries[i] = (VkAccelerationStructureGeometryKHR) {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
        .geometry.triangles = (VkAccelerationStructureGeometryTrianglesDataKHR) {
          .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
          .vertexFormat = convertAttributeType(geometry->vertexType),
          .vertexStride = geometry->vertexStride,
          .maxVertex = geometry->vertexCount,
          .indexType = geometry->indexOffset == ~0u ? VK_INDEX_TYPE_NONE_KHR : (VkIndexType) geometry->indexType,
          .transformData.deviceAddress = geometry->transformOffset == ~0u ? 0 : 1
        }
      };

      tree->ranges[i] = (VkAccelerationStructureBuildRangeInfoKHR) {
        .primitiveCount = geometry->triangleCount,
        .primitiveOffset = geometry->indexOffset == ~0u ? geometry->vertexOffset : geometry->indexOffset,
        .firstVertex = geometry->baseVertex,
        .transformOffset = geometry->transformOffset == ~0u ? 0 : geometry->transformOffset
      };
    }
  }

  tree->flags =
    ((info->flags & GPU_TREE_WILL_UPDATE) ? VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR : 0) |
    ((info->flags & GPU_TREE_FAST_TRACE) ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR : 0) |
    ((info->flags & GPU_TREE_FAST_BUILD) ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR : 0) |
    ((info->flags & GPU_TREE_LOW_MEMORY) ? VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR : 0);

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = info->type == GPU_TREE_TOP ?
      VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR :
      VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags = tree->flags,
    .geometryCount = info->type == GPU_TREE_TOP ? 1 : info->capacity,
    .pGeometries = info->type == GPU_TREE_TOP ? &geometry : tree->geometries
  };

  VkAccelerationStructureBuildSizesInfoKHR sizes = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
  VkAccelerationStructureBuildTypeKHR buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;

  uint32_t* counts;
  if (info->type == GPU_TREE_TOP) {
    counts = &info->capacity;
  } else if (info->capacity == 1) {
    counts = &info->geometries[0].triangleCount;
  } else {
    counts = state.config.fnAlloc(info->capacity * sizeof(uint32_t));
    ASSERT(counts, "Out of memory") return false;
    for (uint32_t i = 0; i < info->capacity; i++) {
      counts[i] = info->geometries[i].triangleCount;
    }
  }

  vkGetAccelerationStructureBuildSizesKHR(state.device, buildType, &buildInfo, counts, &sizes);

  if (info->type == GPU_TREE_BOTTOM && info->capacity > 1) {
    state.config.fnFree(counts);
  }

  gpu_buffer_info bufferInfo = {
    .type = GPU_BUFFER_TREE,
    .size = sizes.accelerationStructureSize,
    .label = "Tree Buffer"
  };

  if (!gpu_buffer_init(&tree->buffer, &bufferInfo)) {
    return false;
  }

  gpu_buffer_info scratchInfo = {
    .type = GPU_BUFFER_STATIC,
    .size = MAX(sizes.updateScratchSize, sizes.buildScratchSize) + 256,
    .label = "Scratch Buffer"
  };

  if (!gpu_buffer_init(&tree->scratch, &scratchInfo)) {
    gpu_buffer_destroy(&tree->buffer);
    return false;
  }

  VkAccelerationStructureCreateInfoKHR createInfo = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = tree->buffer.handle,
    .size = sizes.accelerationStructureSize,
    .type = buildInfo.type
  };

  VK(vkCreateAccelerationStructureKHR(state.device, &createInfo, NULL, &tree->handle), "vkCreateAccelerationStructureKHR") {
    gpu_buffer_destroy(&tree->scratch);
    gpu_buffer_destroy(&tree->buffer);
    return false;
  }

  return true;
}

void gpu_tree_destroy(gpu_tree* tree) {
  condemn(tree->handle, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);
  state.config.fnFree(tree->geometries);
  state.config.fnFree(tree->ranges);
  gpu_buffer_destroy(&tree->buffer);
  gpu_buffer_destroy(&tree->scratch);
}

gpu_address gpu_tree_get_address(gpu_tree* tree) {
  return gpu_buffer_get_address(&tree->buffer, 0);
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

  texture->layout = getNaturalLayout(info->usage);
  texture->samples = info->samples;
  texture->layers = info->type == GPU_TEXTURE_3D ? 0 : info->size[2];
  texture->baseLevel = 0;
  texture->format = info->format;
  texture->hostCopy = false;
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
      ((info->usage == GPU_TEXTURE_RENDER) ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0)
  };

  if (info->usage & GPU_TEXTURE_UPLOAD) {
    VkHostImageCopyDevicePerformanceQueryEXT performance = {
      .sType = VK_STRUCTURE_TYPE_HOST_IMAGE_COPY_DEVICE_PERFORMANCE_QUERY_EXT
    };

    if (state.extensions.hostImageCopy) {
      VkPhysicalDeviceImageFormatInfo2 formatInfo = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .format = imageInfo.format,
        .type = imageInfo.imageType,
        .usage = imageInfo.usage | VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT,
        .flags = imageInfo.flags
      };

      VkImageFormatProperties2 properties = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &performance
      };

      vkGetPhysicalDeviceImageFormatProperties2(state.adapter, &formatInfo, &properties);
    }

    if (performance.optimalDeviceAccess) {
      imageInfo.usage |= VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
      texture->hostCopy = true;
    } else {
      imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
  }

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
    default: memoryType = transient ? GPU_MEMORY_TEXTURE_LAZY_COLOR : texture->hostCopy ? GPU_MEMORY_TEXTURE_HOST_COLOR : GPU_MEMORY_TEXTURE_COLOR; break;
  }

  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(state.device, texture->handle, &requirements);

  if ((texture->memory = allocate(memoryType, requirements, &texture->offset)) == NULL) {
    vkDestroyImage(state.device, texture->handle, NULL);
    return false;
  }

  VK(vkBindImageMemory(state.device, texture->handle, texture->memory->handle, texture->offset), "vkBindImageMemory") {
    vkDestroyImage(state.device, texture->handle, NULL);
    release(texture->memory, texture->offset);
    return false;
  }

  if (!gpu_texture_init_view(texture, &viewInfo)) {
    vkDestroyImage(state.device, texture->handle, NULL);
    release(texture->memory, texture->offset);
    return false;
  }

  return true;
}

bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info) {
  if (texture != info->source) {
    uint32_t layers = info->layerCount ? info->layerCount : (info->source->layers - info->layerIndex);
    texture->handle = info->source->handle;
    texture->memory = NULL;
    texture->imported = false;
    texture->layout = info->source->layout;
    texture->samples = info->source->samples;
    texture->layers = info->type == GPU_TEXTURE_3D ? 0 : layers;
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
      .layerCount = info->source->layers && info->layerCount ? info->layerCount : VK_REMAINING_ARRAY_LAYERS
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
  release(texture->memory, texture->offset);
}

bool gpu_texture_upload(gpu_texture* texture, gpu_upload_info* info) {
  VkImage image = texture->handle;
  uint32_t layers = info->extent[2];
  uint32_t levels = info->extent[3];

  VkImageSubresourceRange subresource = {
    .aspectMask = texture->aspect,
    .levelCount = VK_REMAINING_MIP_LEVELS,
    .layerCount = VK_REMAINING_ARRAY_LAYERS
  };

  if (texture->hostCopy) {
    vkTransitionImageLayoutEXT(state.device, 1, &(VkHostImageLayoutTransitionInfoEXT) {
      .sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,
      .image = image,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_GENERAL,
      .subresourceRange = subresource
    });

    VkMemoryToImageCopyEXT stack[16];
    VkMemoryToImageCopyEXT* regions = levels * layers > COUNTOF(stack) ?
      state.config.fnAlloc(levels * layers * sizeof(*regions)) :
      stack;

    for (uint32_t i = 0; i < levels; i++) {
      for (uint32_t j = 0; j < layers; j++) {
        regions[i * layers + j] = (VkMemoryToImageCopyEXT) {
          .sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT,
          .pHostPointer = info->layers[i * layers + j],
          .imageSubresource.aspectMask = texture->aspect,
          .imageSubresource.mipLevel = i,
          .imageSubresource.baseArrayLayer = texture->layers ? j : 0,
          .imageSubresource.layerCount = 1,
          .imageOffset.z = texture->layers ? 0 : j,
          .imageExtent.width = MAX(info->extent[0] >> i, 1),
          .imageExtent.height = MAX(info->extent[1] >> i, 1),
          .imageExtent.depth = 1
        };
      }
    }

    vkCopyMemoryToImageEXT(state.device, &(VkCopyMemoryToImageInfoEXT) {
      .sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT,
      .dstImage = image,
      .dstImageLayout = VK_IMAGE_LAYOUT_GENERAL,
      .regionCount = levels * layers,
      .pRegions = regions
    });

    if (regions != stack) state.config.fnFree(regions);

    vkTransitionImageLayoutEXT(state.device, 1, &(VkHostImageLayoutTransitionInfoEXT) {
      .sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,
      .image = image,
      .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
      .newLayout = texture->layout,
      .subresourceRange = subresource
    });
  } else {
    gpu_upload* upload = state.config.fnAlloc(sizeof(gpu_upload));

    if (!upload) {
      return false;
    }

    // Copy everything out of the texture to avoid having to care if the texture is destroyed
    upload->image = texture->handle;
    upload->layout = texture->layout;
    upload->aspect = texture->aspect;
    upload->copyCount = 0;

    if (levels > 0) {
      void* data;

      gpu_buffer_info buffer = {
        .type = GPU_BUFFER_UPLOAD,
        .pointer = &data,
        .label = "Texture Upload"
      };

      for (uint32_t i = 0; i < levels; i++) {
        buffer.size += layers * info->layerSizes[i];
      }

      if (!gpu_buffer_init(&upload->buffer, &buffer)) {
        state.config.fnFree(upload);
        return false;
      }

      uint32_t cursor = 0;
      for (uint32_t i = 0; i < levels; i++) {
        upload->copies[upload->copyCount++] = (VkBufferImageCopy) {
          .bufferOffset = cursor,
          .imageSubresource.aspectMask = texture->aspect,
          .imageSubresource.mipLevel = i,
          .imageSubresource.baseArrayLayer = 0,
          .imageSubresource.layerCount = texture->layers ? layers : 1,
          .imageExtent.width = MAX(info->extent[0] >> i, 1),
          .imageExtent.height = MAX(info->extent[1] >> i, 1),
          .imageExtent.depth = texture->layers ? 1 : MAX(layers >> i, 1)
        };

        for (uint32_t j = 0; j < layers; j++) {
          memcpy((char*) data + cursor, info->layers[i * layers + j], info->layerSizes[i]);
          cursor += info->layerSizes[i];
        }
      }
    }

    upload->next = state.uploads;
    while (!atomic_compare_exchange_strong(&state.uploads, &upload->next, upload)) {
      continue;
    }
  }

  return true;
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

  surface->vkformat.format = VK_FORMAT_UNDEFINED;

  if (info->hdr && state.extensions.colorspace) {
    for (uint32_t i = 0; i < formatCount; i++) {
      if (formats[i].format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && formats[i].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
        surface->format = GPU_FORMAT_RGB10A2;
        surface->vkformat = formats[i];
        break;
      }
    }
  }

  if (!surface->vkformat.format) {
    for (uint32_t i = 0; i < formatCount; i++) {
      if (formats[i].format == VK_FORMAT_R8G8B8A8_SRGB) {
        surface->format = GPU_FORMAT_RGBA8;
        surface->vkformat = formats[i];
        break;
      } else if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
        surface->format = GPU_FORMAT_BGRA8;
        surface->vkformat = formats[i];
        break;
      }
    }
  }

  ASSERT(surface->vkformat.format != VK_FORMAT_UNDEFINED, "No supported swapchain texture format is available") {
    LOG("Surface unavailable because no supported texture format is available");
    vkDestroySurfaceKHR(state.instance, surface->handle, NULL);
    return false;
  }

  surface->imageIndex = ~0u;
  surface->vsync = info->vsync;

  gpu_surface_resize(info->width, info->height);

  return true;
}

gpu_texture_format gpu_surface_get_format(void) {
  return state.surface.format;
}

bool gpu_surface_is_hdr(void) {
  return state.surface.vkformat.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && state.surface.vkformat.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT;
}

bool gpu_surface_resize(uint32_t width, uint32_t height) {
  gpu_surface* surface = &state.surface;

  surface->valid = false;
  surface->width = 0;
  surface->height = 0;

  if (surface->swapchain) {
    VK(vkDeviceWaitIdle(state.device), "vkDeviceWaitIdle") {
      return false;
    }
  }

  VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.adapter, surface->handle, &surface->capabilities), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR") {
    return false;
  }

  if (width == ~0u || height == ~0u) {
    width = surface->capabilities.currentExtent.width;
    height = surface->capabilities.currentExtent.height;
  }

  width = MIN(width, surface->capabilities.maxImageExtent.width);
  width = MAX(width, surface->capabilities.minImageExtent.width);
  height = MIN(height, surface->capabilities.maxImageExtent.height);
  height = MAX(height, surface->capabilities.minImageExtent.height);

  if (width == 0 || height == 0) {
    return true;
  }

  VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  if (surface->capabilities.supportedUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) {
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }

  if (surface->capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
    usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  if (surface->capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  VkSwapchainCreateInfoKHR swapchainInfo = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = surface->handle,
    .minImageCount = surface->capabilities.minImageCount,
    .imageFormat = surface->vkformat.format,
    .imageColorSpace = surface->vkformat.colorSpace,
    .imageExtent = { width, height },
    .imageArrayLayers = 1,
    .imageUsage = usage,
    .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = surface->vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR,
    .clipped = VK_TRUE,
    .oldSwapchain = surface->swapchain
  };

  VK(vkCreateSwapchainKHR(state.device, &swapchainInfo, NULL, &surface->swapchain), "vkCreateSwapchainKHR") {
    return false;
  }

  if (swapchainInfo.oldSwapchain) {
    for (uint32_t i = 0; i < FRAME_DEPTH; i++) {
      vkDestroySemaphore(state.device, surface->acquireSemaphores[i], NULL);
      surface->acquireSemaphores[i] = VK_NULL_HANDLE;
      surface->acquireTick[i] = 0;
    }

    for (uint32_t i = 0; i < COUNTOF(surface->images); i++) {
      vkDestroySemaphore(state.device, surface->presentSemaphores[i], NULL);
      vkDestroyImageView(state.device, surface->images[i].view, NULL);
      surface->presentSemaphores[i] = VK_NULL_HANDLE;
      surface->images[i].view = VK_NULL_HANDLE;
    }

    vkDestroySwapchainKHR(state.device, swapchainInfo.oldSwapchain, NULL);
  }

  uint32_t imageCount;
  VkImage images[COUNTOF(surface->images)];
  VK(vkGetSwapchainImagesKHR(state.device, surface->swapchain, &imageCount, NULL), "vkGetSwapchainImagesKHR") {
    goto fail;
  }

  ASSERT(imageCount <= COUNTOF(images), "Too many swapchain images!") {
    goto fail;
  }

  VK(vkGetSwapchainImagesKHR(state.device, surface->swapchain, &imageCount, images), "vkGetSwapchainImagesKHR") {
    goto fail;
  }

  for (uint32_t i = 0; i < imageCount; i++) {
    gpu_texture* texture = &surface->images[i];

    texture->handle = images[i];
    texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    texture->layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    texture->memory = NULL;
    texture->samples = 1;
    texture->layers = 1;
    texture->format = surface->format;
    texture->srgb = surface->vkformat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    gpu_texture_view_info view = {
      .source = texture,
      .type = GPU_TEXTURE_2D,
      .usage = GPU_TEXTURE_RENDER
    };

    if (!gpu_texture_init_view(texture, &view)) {
      goto fail;
    }
  }

  VkSemaphoreCreateInfo semaphoreInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

  for (uint32_t i = 0; i < FRAME_DEPTH; i++) {
    VK(vkCreateSemaphore(state.device, &semaphoreInfo, NULL, &surface->acquireSemaphores[i]), "vkCreateSemaphore") {
      goto fail;
    }
  }

  for (uint32_t i = 0; i < imageCount; i++) {
    VK(vkCreateSemaphore(state.device, &semaphoreInfo, NULL, &surface->presentSemaphores[i]), "vkCreateSemaphore") {
      goto fail;
    }
  }

  surface->width = width;
  surface->height = height;
  surface->valid = true;
  return true;
fail:
  for (uint32_t i = 0; i < FRAME_DEPTH; i++) {
    vkDestroySemaphore(state.device, surface->acquireSemaphores[i], NULL);
    surface->acquireSemaphores[i] = VK_NULL_HANDLE;
  }

  for (uint32_t i = 0; i < COUNTOF(surface->images); i++) {
    vkDestroySemaphore(state.device, surface->presentSemaphores[i], NULL);
    vkDestroyImageView(state.device, surface->images[i].view, NULL);
    surface->presentSemaphores[i] = VK_NULL_HANDLE;
    surface->images[i].view = VK_NULL_HANDLE;
  }

  vkDestroySwapchainKHR(state.device, surface->swapchain, NULL);
  surface->swapchain = VK_NULL_HANDLE;
  return false;
}

bool gpu_surface_acquire(gpu_texture** texture, uint32_t* width, uint32_t* height) {
  gpu_surface* surface = &state.surface;

  *width = surface->width;
  *height = surface->height;

  if (!surface->valid) {
    *texture = NULL;
    return true;
  }

  gpu_wait_tick(surface->acquireTick[state.frame & FRAME_MASK]);
  VkSemaphore semaphore = surface->acquireSemaphores[state.frame & FRAME_MASK];
  VkResult result = vkAcquireNextImageKHR(state.device, surface->swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &surface->imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    gpu_surface_resize(~0u, ~0u);
    return gpu_surface_acquire(texture, width, height);
  } else if (result < 0) {
    vkerror(result, "vkAcquireNextImageKHR");
    return false;
  }

  *texture = &surface->images[surface->imageIndex];
  surface->acquireSemaphore = semaphore;
  return true;
}

bool gpu_surface_present(void) {
  gpu_surface* surface = &state.surface;

  VkSemaphore presentSemaphore = surface->presentSemaphores[surface->imageIndex];
  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &(VkTimelineSemaphoreSubmitInfo) {
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .waitSemaphoreValueCount = 1,
      .pWaitSemaphoreValues = (uint64_t[1]) { state.tick },
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = (uint64_t[1]) { 0 }
    },
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &state.semaphore,
    .pWaitDstStageMask = &waitStage,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &presentSemaphore
  };

  VK(vkQueueSubmit(state.queue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit") {
    return false;
  }

  VkPresentInfoKHR present = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &presentSemaphore,
    .swapchainCount = 1,
    .pSwapchains = &surface->swapchain,
    .pImageIndices = &surface->imageIndex
  };

  VkResult result = vkQueuePresentKHR(state.queue, &present);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    gpu_surface_resize(~0u, ~0u);
  } else if (result < 0) {
    // TODO wait on semaphore, for errors that don't wait on it
    vkerror(result, "vkQueuePresentKHR");
    return false;
  }

  state.surface.imageIndex = ~0u;
  state.frame++;
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
    [GPU_SLOT_SAMPLER] = VK_DESCRIPTOR_TYPE_SAMPLER,
    [GPU_SLOT_TREE] = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
  };

  VkDescriptorSetLayoutBinding bindings[32];
  for (uint32_t i = 0; i < info->count; i++) {
    bindings[i] = (VkDescriptorSetLayoutBinding) {
      .binding = info->slots[i].number,
      .descriptorType = types[info->slots[i].type],
      .descriptorCount = MAX(1, info->slots[i].arraySize),
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
    layout->descriptorCounts[info->slots[i].type] += MAX(1, info->slots[i].arraySize);
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
    [GPU_SLOT_SAMPLER] = { VK_DESCRIPTOR_TYPE_SAMPLER, 0 },
    [GPU_SLOT_TREE] = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 0 }
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
  VkWriteDescriptorSetAccelerationStructureKHR treeInfo[256];
  VkWriteDescriptorSet writes[256];
  uint32_t bufferCount = 0;
  uint32_t imageCount = 0;
  uint32_t treeCount = 0;
  uint32_t writeCount = 0;

  static const VkDescriptorType types[] = {
    [GPU_SLOT_UNIFORM_BUFFER] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [GPU_SLOT_STORAGE_BUFFER] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    [GPU_SLOT_UNIFORM_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    [GPU_SLOT_STORAGE_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    [GPU_SLOT_TEXTURE_WITH_SAMPLER] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [GPU_SLOT_SAMPLED_TEXTURE] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    [GPU_SLOT_STORAGE_TEXTURE] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    [GPU_SLOT_SAMPLER] = VK_DESCRIPTOR_TYPE_SAMPLER,
    [GPU_SLOT_TREE] = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
  };

  for (uint32_t i = 0; i < count; i++) {
    gpu_bundle_info* info = &infos[i];
    for (uint32_t j = 0; j < info->count; j++) {
      gpu_binding* binding = &info->bindings[j];
      VkDescriptorType type = types[binding->type];
      gpu_buffer_binding* buffers = binding->count > 0 ? binding->buffers : &binding->buffer;
      gpu_texture_binding* textures = binding->count > 0 ? binding->textures : &binding->texture;
      bool image = binding->type > GPU_SLOT_STORAGE_BUFFER_DYNAMIC;
      bool tree = binding->type == GPU_SLOT_TREE;

      uint32_t index = 0;
      uint32_t descriptorCount = MAX(binding->count, 1);

      while (index < descriptorCount) {
        uint32_t available =
          tree ? COUNTOF(treeInfo) - treeCount :
          image ? COUNTOF(imageInfo) - imageCount :
          COUNTOF(bufferInfo) - bufferCount;

        uint32_t chunk = MIN(descriptorCount - index, available);

        writes[writeCount++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = tree ? &treeInfo[treeCount] : NULL,
          .dstSet = bundles[i]->handle,
          .dstBinding = binding->number,
          .dstArrayElement = index,
          .descriptorCount = chunk,
          .descriptorType = type,
          .pBufferInfo = &bufferInfo[bufferCount],
          .pImageInfo = &imageInfo[imageCount]
        };

        if (tree) {
          for (uint32_t n = 0; n < chunk; n++, index++) {
            treeInfo[treeCount++] = (VkWriteDescriptorSetAccelerationStructureKHR) {
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
              .accelerationStructureCount = 1,
              .pAccelerationStructures = &binding->tree->handle
            };
          }
        } else if (image) {
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

        bool flush =
          bufferCount == COUNTOF(bufferInfo) ||
          imageCount == COUNTOF(imageInfo) ||
          treeCount == COUNTOF(treeInfo) ||
          writeCount == COUNTOF(writes);

        if (flush) {
          vkUpdateDescriptorSets(state.device, writeCount, writes, 0, NULL);
          bufferCount = imageCount = treeCount = writeCount = 0;
        }
      }
    }
  }

  if (writeCount > 0) {
    vkUpdateDescriptorSets(state.device, writeCount, writes, 0, NULL);
  }
}

// Pipeline

bool gpu_pipeline_init_graphics(gpu_pipeline* pipeline, gpu_pipeline_info* info, bool* slow) {
  static const VkPrimitiveTopology topologies[] = {
    [GPU_DRAW_POINTS] = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    [GPU_DRAW_LINES] = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    [GPU_DRAW_TRIANGLES] = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
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
    [GPU_BLEND_ONE_MINUS_DST_ALPHA] = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    [GPU_BLEND_SRC_ALPHA_SATURATED] = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE
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
      .format = convertAttributeType(info->vertex.attributes[i].type),
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

  VkFormat colorFormats[4];
  VkPipelineRenderingCreateInfoKHR renderingInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
    .viewMask = (1 << info->viewCount) - 1,
    .colorAttachmentCount = info->attachmentCount,
    .pColorAttachmentFormats = colorFormats,
    .depthAttachmentFormat = info->depth.format ? convertFormat(info->depth.format, LINEAR) : VK_FORMAT_UNDEFINED
  };

  for (uint32_t i = 0; i < info->attachmentCount; i++) {
    colorFormats[i] = convertFormat(info->color[i].format, info->color[i].srgb);
  }

  if (info->depth.format == GPU_FORMAT_D24S8 || info->depth.format == GPU_FORMAT_D32FS8) {
    renderingInfo.stencilAttachmentFormat = renderingInfo.depthAttachmentFormat;
  }

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
    .layout = info->shader->pipelineLayout
  };

  if (state.extensions.pipelineCacheControl && slow) {
    pipelineInfo.flags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT;
  }

  if (state.extensions.dynamicRendering) {
    pipelineInfo.pNext = &renderingInfo;
    if (info->foveated) {
      pipelineInfo.flags |= VK_PIPELINE_CREATE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT;
    }
  } else {
    bool depth = info->depth.format;
    uint32_t colorCount = info->attachmentCount;
    VkAttachmentDescription2 attachments[6];
    VkAttachmentReference2 references[6];

    for (uint32_t i = 0; i < colorCount; i++) {
      references[i] = (VkAttachmentReference2) {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
        .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .attachment = i
      };

      attachments[i] = (VkAttachmentDescription2) {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        .format = convertFormat(info->color[i].format, info->color[i].srgb),
        .samples = info->multisample.count,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR
      };
    }

    if (depth) {
      uint32_t index = colorCount;

      references[index] = (VkAttachmentReference2) {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
        .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .attachment = index
      };

      attachments[index] = (VkAttachmentDescription2) {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        .format = convertFormat(info->depth.format, LINEAR),
        .samples = info->multisample.count,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR
      };
    }

    if (info->foveated) {
      uint32_t index = colorCount + depth;

      attachments[index] = (VkAttachmentDescription2) {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        .format = VK_FORMAT_R8G8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
        .finalLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT
      };
    }

    VkSubpassDescription2 subpass = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
      .viewMask = (1 << info->viewCount) - 1,
      .colorAttachmentCount = colorCount,
      .pColorAttachments = &references[0],
      .pDepthStencilAttachment = depth ? &references[colorCount] : NULL
    };

    VkRenderPassCreateInfo2 passInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
      .pNext = info->foveated ? &(VkRenderPassFragmentDensityMapCreateInfoEXT) {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT,
        .fragmentDensityMapAttachment = {
          .attachment = colorCount + depth,
          .layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT
        }
      } : NULL,
      .attachmentCount = colorCount + depth + info->foveated,
      .pAttachments = attachments,
      .subpassCount = 1,
      .pSubpasses = &subpass
    };

    VK(vkCreateRenderPass2KHR(state.device, &passInfo, NULL, &pipelineInfo.renderPass), "vkCreateRenderPass2KHR") {
      if (constants != stackConstants) state.config.fnFree(constants);
      if (entries != stackEntries) state.config.fnFree(entries);
      return false;
    }

    condemn(pipelineInfo.renderPass, VK_OBJECT_TYPE_RENDER_PASS);
  }

  VkResult result = vkCreateGraphicsPipelines(state.device, state.pipelineCache, 1, &pipelineInfo, NULL, &pipeline->handle);
  if (constants != stackConstants) state.config.fnFree(constants);
  if (entries != stackEntries) state.config.fnFree(entries);

  if (!vkcheck(result, "vkCreateGraphicsPipelines")) {
    return false;
  } else if (result == VK_PIPELINE_COMPILE_REQUIRED_EXT) {
    *slow = true;
    return true;
  } else {
    if (slow) *slow = false;
    return true;
  }
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
  if (!thread.initialized) {
    thread.initialized = true;
    thread.next = state.threads;
    while (!atomic_compare_exchange_strong(&state.threads, &thread.next, &thread)) {
      continue;
    }
  }

  gpu_stream_pool* pool = thread.activeStreamPool;

  if (!pool) {
    // Find an existing pool to reuse
    for (gpu_stream_pool* p = thread.streamPools; p; p = p->next) {
      if (gpu_is_complete(p->tick)) {
        pool = p;
        VK(vkResetCommandPool(state.device, pool->handle, 0), "vkResetCommandPool") return NULL;
        pool->tail = NULL;
        break;
      }
    }

    // No pool available?  Make a new one
    if (!pool) {
      pool = state.config.fnAlloc(sizeof(gpu_stream_pool));
      ASSERT(pool, "Out of memory") return NULL;

      VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = state.queueFamilyIndex
      };

      VK(vkCreateCommandPool(state.device, &poolInfo, NULL, &pool->handle), "vkCreateCommandPool") {
        state.config.fnFree(pool);
        return NULL;
      }

      pool->next = thread.streamPools;
      thread.streamPools = pool;

      pool->head = NULL;
      pool->tail = NULL;
      pool->tick = 0;
    }

    thread.activeStreamPool = pool;
  }

  gpu_stream* stream = pool->tail ? pool->tail->next : pool->head;

  if (!stream) {
    stream = state.config.fnAlloc(sizeof(gpu_stream));
    ASSERT(stream, "Out of memory") return NULL;
    stream->next = NULL;

    VkCommandBufferAllocateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pool->handle,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1
    };

    VK(vkAllocateCommandBuffers(state.device, &info, &stream->commands), "vkAllocateCommandBuffers") {
      state.config.fnFree(stream);
      return NULL;
    }

    if (pool->tail) {
      pool->tail->next = stream;
    } else {
      pool->head = stream;
    }
  }

  VkCommandBufferBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };

  VK(vkBeginCommandBuffer(stream->commands, &beginfo), "vkBeginCommandBuffer") return NULL;
  nickname(stream->commands, VK_OBJECT_TYPE_COMMAND_BUFFER, label);
  pool->tail = stream;
  return stream;
}

bool gpu_stream_end(gpu_stream* stream) {
  VK(vkEndCommandBuffer(stream->commands), "vkEndCommandBuffer") return false;
  return true;
}

void gpu_render_begin(gpu_stream* stream, gpu_canvas* canvas) {
  static const VkAttachmentLoadOp loadOps[] = {
    [GPU_LOAD_OP_CLEAR] = VK_ATTACHMENT_LOAD_OP_CLEAR,
    [GPU_LOAD_OP_DISCARD] = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    [GPU_LOAD_OP_KEEP] = VK_ATTACHMENT_LOAD_OP_LOAD
  };

  static const VkAttachmentStoreOp storeOps[] = {
    [GPU_SAVE_OP_KEEP] = VK_ATTACHMENT_STORE_OP_STORE,
    [GPU_SAVE_OP_DISCARD] = VK_ATTACHMENT_STORE_OP_DONT_CARE
  };

  // Layout transitions

  uint32_t barrierCount = 0;
  VkImageMemoryBarrier2KHR barriers[10];

  bool BEGIN = true;
  bool RESOLVE = true;

  for (uint32_t i = 0; i < 4 && canvas->color[i].texture; i++) {
    bool DISCARD = canvas->color[i].load != GPU_LOAD_OP_KEEP;
    barrierCount += transitionAttachment(canvas->color[i].texture, BEGIN, !RESOLVE, DISCARD, &barriers[barrierCount]);
    barrierCount += transitionAttachment(canvas->color[i].resolve, BEGIN, RESOLVE, true, &barriers[barrierCount]);
  }

  if (canvas->depth.texture) {
    bool DISCARD = canvas->depth.load != GPU_LOAD_OP_KEEP;
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

  // Begin pass

  if (state.extensions.dynamicRendering) {
    uint32_t colorAttachmentCount = 0;
    VkRenderingAttachmentInfo color[4], depth, stencil;

    for (uint32_t i = 0; i < 4 && canvas->color[i].texture; i++, colorAttachmentCount++) {
      color[i] = (VkRenderingAttachmentInfo) {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = canvas->color[i].texture->view,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .resolveMode = canvas->color[i].resolve ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
        .resolveImageView = canvas->color[i].resolve ? canvas->color[i].resolve->view : VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .loadOp = loadOps[canvas->color[i].load],
        .storeOp = storeOps[canvas->color[i].save],
        .clearValue.color.float32 = {
          canvas->color[i].clear[0],
          canvas->color[i].clear[1],
          canvas->color[i].clear[2],
          canvas->color[i].clear[3]
        }
      };
    }

    bool hasStencil = canvas->depth.texture && (canvas->depth.texture->aspect & VK_IMAGE_ASPECT_STENCIL_BIT);

    if (canvas->depth.texture) {
      depth = (VkRenderingAttachmentInfo) {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = canvas->depth.texture->view,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .resolveMode = canvas->depth.resolve ? VK_RESOLVE_MODE_SAMPLE_ZERO_BIT : VK_RESOLVE_MODE_NONE,
        .resolveImageView = canvas->depth.resolve ? canvas->depth.resolve->view : VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .loadOp = loadOps[canvas->depth.load],
        .storeOp = storeOps[canvas->depth.save],
        .clearValue.depthStencil.depth = canvas->depth.clear
      };

      if (hasStencil) {
        stencil = depth;
        stencil.loadOp = loadOps[canvas->depth.stencilLoad];
        stencil.storeOp = storeOps[canvas->depth.stencilSave];
        stencil.clearValue.depthStencil.stencil = canvas->depth.stencilClear;
      }
    }

    VkRenderingFragmentDensityMapAttachmentInfoEXT foveation;

    if (canvas->foveation) {
      foveation = (VkRenderingFragmentDensityMapAttachmentInfoEXT) {
        .sType = VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT,
        .imageView = canvas->foveation->view,
        .imageLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT
      };
    }

    uint32_t views = colorAttachmentCount > 0 ? canvas->color[0].texture->layers : canvas->depth.texture->layers;

    vkCmdBeginRenderingKHR(stream->commands, &(VkRenderingInfo) {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = canvas->foveation ? &foveation : NULL,
      .renderArea = {
        .offset = { canvas->area[0], canvas->area[1] },
        .extent = { canvas->area[2] ? canvas->area[2] : canvas->width, canvas->area[3] ? canvas->area[3] : canvas->height }
      },
      .layerCount = 1,
      .viewMask = (1 << views) - 1,
      .colorAttachmentCount = colorAttachmentCount,
      .pColorAttachments = color,
      .pDepthAttachment = canvas->depth.texture ? &depth : NULL,
      .pStencilAttachment = hasStencil ? &stencil : NULL
    });
  } else {
    uint32_t attachmentCount = 0;
    uint32_t colorAttachmentCount = 0;
    VkAttachmentDescription2 attachments[11];
    VkAttachmentReference2 references[11];
    bool hasColorResolve = false;
    bool hasDepthResolve = !!canvas->depth.resolve;
    bool depth = !!canvas->depth.texture;

    for (uint32_t i = 0; i < 4 && canvas->color[i].texture; i++, colorAttachmentCount++) {
      uint32_t index = attachmentCount++;

      references[index] = (VkAttachmentReference2) {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
        .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .attachment = i
      };

      attachments[index] = (VkAttachmentDescription2) {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        .format = convertFormat(canvas->color[i].texture->format, canvas->color[i].texture->srgb),
        .samples = canvas->color[i].texture->samples,
        .loadOp = loadOps[canvas->color[i].load],
        .storeOp = canvas->color[i].resolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : storeOps[canvas->color[i].save],
        .initialLayout = references[i].layout,
        .finalLayout = references[i].layout
      };

      hasColorResolve |= !!canvas->color[i].resolve;
    }

    if (hasColorResolve) {
      for (uint32_t i = 0; i < colorAttachmentCount; i++) {
        uint32_t referenceIndex = colorAttachmentCount + i;

        references[referenceIndex] = (VkAttachmentReference2) {
          .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
          .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
          .attachment = canvas->color[i].resolve ? attachmentCount : VK_ATTACHMENT_UNUSED
        };

        if (canvas->color[i].resolve) {
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

    if (depth) {
      uint32_t referenceIndex = colorAttachmentCount << hasColorResolve;
      uint32_t index = attachmentCount++;

      references[referenceIndex] = (VkAttachmentReference2) {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
        .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .attachment = index
      };

      attachments[index] = (VkAttachmentDescription2) {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        .format = convertFormat(canvas->depth.texture->format, LINEAR),
        .samples = canvas->depth.texture->samples,
        .loadOp = loadOps[canvas->depth.load],
        .storeOp = canvas->depth.resolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : storeOps[canvas->depth.save],
        .stencilLoadOp = loadOps[canvas->depth.stencilLoad],
        .stencilStoreOp = canvas->depth.resolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : storeOps[canvas->depth.stencilSave],
        .initialLayout = references[referenceIndex].layout,
        .finalLayout = references[referenceIndex].layout
      };

      if (canvas->depth.resolve) {
        uint32_t referenceIndex = (colorAttachmentCount << hasColorResolve) + 1;
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
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .stencilStoreOp = storeOps[canvas->depth.stencilSave],
          .initialLayout = references[referenceIndex].layout,
          .finalLayout = references[referenceIndex].layout
        };
      }
    }

    if (canvas->foveation) {
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

    uint32_t referenceCount = (colorAttachmentCount << hasColorResolve) + (depth << hasDepthResolve);
    uint32_t views = colorAttachmentCount > 0 ? canvas->color[0].texture->layers : canvas->depth.texture->layers;

    VkSubpassDescription2 subpass = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
      .pNext = canvas->depth.resolve ? &(VkSubpassDescriptionDepthStencilResolve) {
        .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE,
        .depthResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
        .stencilResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
        .pDepthStencilResolveAttachment = &references[referenceCount - 1]
      } : NULL,
      .viewMask = (1 << views) - 1,
      .colorAttachmentCount = colorAttachmentCount,
      .pColorAttachments = &references[0],
      .pResolveAttachments = hasColorResolve ? &references[colorAttachmentCount] : NULL,
      .pDepthStencilAttachment = canvas->depth.texture ? &references[referenceCount - 1 - hasDepthResolve] : NULL
    };

    VkRenderPassCreateInfo2 passInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
      .pNext = canvas->foveation ? &(VkRenderPassFragmentDensityMapCreateInfoEXT) {
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

    VkRenderPass renderPass;
    VK(vkCreateRenderPass2KHR(state.device, &passInfo, NULL, &renderPass), "vkCreateRenderPass2KHR") {} // Ignoring error
    condemn(renderPass, VK_OBJECT_TYPE_RENDER_PASS);

    // Framebuffer

    VkImageView images[11];
    VkClearValue clears[11];
    uint32_t imageCount = 0;

    for (uint32_t i = 0; i < 4 && canvas->color[i].texture; i++) {
      images[i] = canvas->color[i].texture->view;
      memcpy(clears[i].color.float32, canvas->color[i].clear, 4 * sizeof(float));
      imageCount++;
    }

    for (uint32_t i = 0; i < 4 && canvas->color[i].texture; i++) {
      if (canvas->color[i].resolve) images[imageCount++] = canvas->color[i].resolve->view;
    }

    if (canvas->depth.texture) {
      uint32_t index = imageCount++;
      images[index] = canvas->depth.texture->view;
      clears[index].depthStencil.depth = canvas->depth.clear;
      clears[index].depthStencil.stencil = canvas->depth.stencilClear;

      if (canvas->depth.resolve) {
        images[imageCount++] = canvas->depth.resolve->view;
      }
    }

    if (canvas->foveation) {
      uint32_t index = imageCount++;
      images[index] = canvas->foveation->view;
    }

    VkFramebufferCreateInfo framebufferInfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = renderPass,
      .attachmentCount = imageCount,
      .pAttachments = images,
      .width = canvas->width,
      .height = canvas->height,
      .layers = 1
    };

    VkFramebuffer framebuffer;
    VK(vkCreateFramebuffer(state.device, &framebufferInfo, NULL, &framebuffer), "vkCreateFramebuffer") {} // Ignoring error
    condemn(framebuffer, VK_OBJECT_TYPE_FRAMEBUFFER);

    // Do it!

    VkRenderPassBeginInfo beginfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = renderPass,
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
}

void gpu_render_end(gpu_stream* stream, gpu_canvas* canvas) {
  if (state.extensions.dynamicRendering) {
    vkCmdEndRenderingKHR(stream->commands);
  } else {
    vkCmdEndRenderPass2KHR(stream->commands, &(VkSubpassEndInfo) {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO
    });
  }

  // Layout transitions

  uint32_t barrierCount = 0;
  VkImageMemoryBarrier2KHR barriers[10];

  bool BEGIN = true;
  bool RESOLVE = true;
  bool DISCARD = true;

  for (uint32_t i = 0; i < 4 && canvas->color[i].texture; i++) {
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
    .baseArrayLayer = texture->layers ? layer : 0,
    .layerCount = texture->layers ? layerCount : 1
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

void gpu_build_tree(gpu_stream* stream, gpu_tree* tree, gpu_build_info* info) {
  VkAccelerationStructureGeometryKHR geometry;

  if (info->type == GPU_TREE_TOP) {
    geometry = (VkAccelerationStructureGeometryKHR) {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
      .geometry.instances = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data.deviceAddress = info->instances
      }
    };
  } else {
    for (uint32_t i = 0; i < info->count; i++) {
      tree->geometries[i].geometry.triangles.vertexData.deviceAddress = info->vertices;
      tree->geometries[i].geometry.triangles.indexData.deviceAddress = info->indices;
      tree->geometries[i].geometry.triangles.transformData.deviceAddress = info->transforms;
    }
  }

  VkAccelerationStructureBuildGeometryInfoKHR build = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = info->type == GPU_TREE_TOP ?
      VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR :
      VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags = tree->flags,
    .mode = info->mode == GPU_TREE_BUILD ?
      VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR :
      VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR,
    .srcAccelerationStructure = tree->handle,
    .dstAccelerationStructure = tree->handle,
    .geometryCount = info->type == GPU_TREE_TOP ? 1 : info->count,
    .pGeometries = info->type == GPU_TREE_TOP ? &geometry : tree->geometries,
    .scratchData = ALIGN(gpu_buffer_get_address(&tree->scratch, 0), 256)
  };

  VkAccelerationStructureBuildRangeInfoKHR range = { .primitiveCount = info->count };

  const VkAccelerationStructureBuildRangeInfoKHR* ranges[1] = {
    info->type == GPU_TREE_TOP ? &range : tree->ranges
  };

  vkCmdBuildAccelerationStructuresKHR(stream->commands, 1, &build, ranges);
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
  state.library = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
  if (!state.library) state.library = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
  if (!state.library) state.library = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
  if (!state.library && !getenv("DYLD_FALLBACK_LIBRARY_PATH")) state.library = dlopen("/usr/local/lib/libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
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
          LOG("Warning: GPU debugging is enabled, but validation layer is not installed, so no debug logs will be shown");
        }
      } else {
        LOG("Warning: GPU debugging is enabled, but debug extension is not supported");
      }
    }
  }

  { // Device
    if (config->vk.getPhysicalDevice) {
      config->vk.getPhysicalDevice(state.instance, (uintptr_t) &state.adapter);
    }

    if (!state.adapter) {
      uint32_t deviceCount = 1;
      VK(vkEnumeratePhysicalDevices(state.instance, &deviceCount, &state.adapter), "vkEnumeratePhysicalDevices") goto fail;
    }

    if (!state.adapter) {
      error("No suitable graphics device available");
      goto fail;
    }

    // Device Extensions

    struct { const char* name; bool shouldEnable; bool* flag; } extensions[] = {
      { "VK_KHR_acceleration_structure", true, &state.extensions.accelerationStructure },
      { "VK_KHR_buffer_device_address", true, &state.extensions.bufferDeviceAddress },
      { "VK_KHR_create_renderpass2", true, &state.extensions.renderPass2 },
      { "VK_KHR_deferred_host_operations", true, &state.extensions.deferredHostOperations },
      { "VK_KHR_swapchain", true, &state.extensions.swapchain },
      { "VK_KHR_portability_subset", true, &state.extensions.portability },
      { "VK_KHR_depth_stencil_resolve", true, &state.extensions.depthResolve },
      { "VK_KHR_ray_query", true, &state.extensions.rayQuery },
      { "VK_KHR_shader_non_semantic_info", config->debug, &state.extensions.shaderDebug },
      { "VK_KHR_shader_float_controls", true, &state.extensions.shaderFloatControls },
      { "VK_KHR_spirv_1_4", true, &state.extensions.spirv14 },
      { "VK_KHR_image_format_list", true, &state.extensions.formatList },
      { "VK_KHR_synchronization2", true, &state.extensions.synchronization2 },
      { "VK_KHR_dynamic_rendering", true, &state.extensions.dynamicRendering },
      { "VK_KHR_timeline_semaphore", true, &state.extensions.timelineSemaphore },
      { "VK_KHR_copy_commands2", true, &state.extensions.copy2 },
      { "VK_KHR_format_feature_flags2", true, &state.extensions.formatFlags2 },
      { "VK_EXT_descriptor_indexing", true, &state.extensions.descriptorIndexing },
      { "VK_EXT_scalar_block_layout", true, &state.extensions.scalarBlockLayout },
      { "VK_EXT_fragment_density_map", true, &state.extensions.foveation },
      { "VK_EXT_pipeline_creation_cache_control", true, &state.extensions.pipelineCacheControl },
      { "VK_EXT_memory_budget", true, &state.extensions.memoryBudget },
      { "VK_EXT_host_image_copy", true, &state.extensions.hostImageCopy }
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
    ASSERT(state.extensions.timelineSemaphore, "GPU driver is missing required Vulkan extension VK_KHR_timeline_semaphore") goto fail;

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

    #define CHAIN(s, x) x.pNext = s.pNext; s.pNext = &x

    VkPhysicalDeviceFeatures2 supported = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES };
    VkPhysicalDeviceShaderDrawParameterFeatures shaderDrawParameterFeatures = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES };
    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR };
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR };
    VkPhysicalDeviceScalarBlockLayoutFeaturesEXT scalarBlockLayoutFeatures = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT };
    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeatures = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT };
    VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT pipelineCreationCacheControlFeatures = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES_EXT };
    VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timelineSemaphoreFeatures = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR };
    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufferDeviceAddressFeatures = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
    VkPhysicalDeviceHostImageCopyFeaturesEXT hostImageCopyFeatures = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT };

    if (state.extensions.foveation) {
      CHAIN(supported, fragmentDensityMapFeatures);
    }

    vkGetPhysicalDeviceFeatures2(state.adapter, &supported);

    VkPhysicalDeviceFeatures2 enabled = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

    enabled.features.fullDrawIndexUint32 = true;
    enabled.features.imageCubeArray = true;
    enabled.features.independentBlend = true;
    enabled.features.sampleRateShading = true;
    enabled.features.samplerAnisotropy = supported.features.samplerAnisotropy;
    enabled.features.multiDrawIndirect = supported.features.multiDrawIndirect;
    enabled.features.shaderClipDistance = supported.features.shaderClipDistance;
    enabled.features.shaderCullDistance = supported.features.shaderCullDistance;
    enabled.features.largePoints = supported.features.largePoints;
    enabled.features.textureCompressionBC = supported.features.textureCompressionBC;
    enabled.features.textureCompressionASTC_LDR = supported.features.textureCompressionASTC_LDR;
    enabled.features.fillModeNonSolid = supported.features.fillModeNonSolid;
    enabled.features.depthClamp = supported.features.depthClamp;
    enabled.features.drawIndirectFirstInstance = supported.features.drawIndirectFirstInstance;
    enabled.features.shaderFloat64 = supported.features.shaderFloat64;
    enabled.features.shaderInt64 = supported.features.shaderInt64;
    enabled.features.shaderInt16 = supported.features.shaderInt16;

    multiviewFeatures.multiview = true;
    CHAIN(enabled, multiviewFeatures);

    shaderDrawParameterFeatures.shaderDrawParameters = true;
    CHAIN(enabled, shaderDrawParameterFeatures);

    synchronization2Features.synchronization2 = true;
    CHAIN(enabled, synchronization2Features);

    timelineSemaphoreFeatures.timelineSemaphore = true;
    CHAIN(enabled, timelineSemaphoreFeatures);

    if (state.extensions.dynamicRendering) {
      dynamicRenderingFeatures.dynamicRendering = true;
      CHAIN(enabled, dynamicRenderingFeatures);
    }

    if (state.extensions.scalarBlockLayout) {
      scalarBlockLayoutFeatures.scalarBlockLayout = true;
      CHAIN(enabled, scalarBlockLayoutFeatures);
    }

    if (state.extensions.foveation) {
      // Note: vkGetPhysicalDeviceFeatures2 writes all supported features to fragmentDensityMapFeatures
      CHAIN(enabled, fragmentDensityMapFeatures);
    }

    if (state.extensions.pipelineCacheControl) {
      pipelineCreationCacheControlFeatures.pipelineCreationCacheControl = true;
      CHAIN(enabled, pipelineCreationCacheControlFeatures);
    }

    if (state.extensions.bufferDeviceAddress) {
      bufferDeviceAddressFeatures.bufferDeviceAddress = true;
      CHAIN(enabled, bufferDeviceAddressFeatures);
    }

    if (state.extensions.accelerationStructure) {
      accelerationStructureFeatures.accelerationStructure = true;
      CHAIN(enabled, accelerationStructureFeatures);
    }

    if (state.extensions.rayQuery) {
      rayQueryFeatures.rayQuery = true;
      CHAIN(enabled, rayQueryFeatures);
    }

    if (state.extensions.hostImageCopy) {
      hostImageCopyFeatures.hostImageCopy = true;
      CHAIN(enabled, hostImageCopyFeatures);
    }

    if (config->features) {
      config->features->textureBC = enabled.features.textureCompressionBC;
      config->features->textureASTC = enabled.features.textureCompressionASTC_LDR;
      config->features->wireframe = enabled.features.fillModeNonSolid;
      config->features->depthClamp = enabled.features.depthClamp;
      config->features->depthResolve = state.extensions.depthResolve;
      config->features->foveation = state.extensions.foveation;
      config->features->rayQuery = state.extensions.rayQuery && state.extensions.accelerationStructure;
      config->features->indirectDrawFirstInstance = enabled.features.drawIndirectFirstInstance;
      config->features->packedBuffers = state.extensions.scalarBlockLayout;
      config->features->shaderDebug = state.extensions.shaderDebug;
      config->features->subgroupVote = subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_VOTE_BIT;
      config->features->subgroupArithmetic = subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT;
      config->features->subgroupBallot = subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT;
      config->features->subgroupShuffle = subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT;
      config->features->subgroupShuffleRelative = subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT;
      config->features->subgroupClustered = subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_CLUSTERED_BIT;
      config->features->subgroupQuad = subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT;
      config->features->float64 = enabled.features.shaderFloat64;
      config->features->int64 = enabled.features.shaderInt64;
      config->features->int16 = enabled.features.shaderInt16;

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
      .pNext = &enabled,
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
    VkPhysicalDeviceMemoryProperties2 memory = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
    vkGetPhysicalDeviceMemoryProperties2(state.adapter, &memory);
    VkMemoryType* memoryTypes = memory.memoryProperties.memoryTypes;

    VkMemoryPropertyFlags hostVisible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // Buffers

    // There are 4 types of buffer memory, which use different strategies/memory types:
    // - STATIC: Regular device-local memory.  Not necessarily mappable, fast to read on GPU.
    // - STREAM: Used to "stream" data to the GPU, to be read by shaders.  This tries to use the
    //   special 256MB memory type present on discrete GPUs because it's both device local and host-
    //   visible and that supposedly makes it fast.
    // - UPLOAD: Used to stage data to upload to buffers/textures.  Can only be used for transfers.
    //   Uses uncached host-visible memory to not pollute the CPU cache or waste the STREAM memory.
    // - DOWNLOAD: Used for readbacks.  Uses cached memory when available since reading from
    //   uncached memory on the CPU is super duper slow.
    VkMemoryPropertyFlags bufferFlags[] = {
      [GPU_MEMORY_BUFFER_STATIC] = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      [GPU_MEMORY_BUFFER_STREAM] = hostVisible | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      [GPU_MEMORY_BUFFER_UPLOAD] = hostVisible,
      [GPU_MEMORY_BUFFER_DOWNLOAD] = hostVisible | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
      [GPU_MEMORY_BUFFER_TREE] = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    for (uint32_t i = 0; i < COUNTOF(bufferFlags); i++) {
      gpu_allocator* allocator = &state.allocators[i];
      state.allocatorLookup[i] = i;

      if (i == GPU_MEMORY_BUFFER_TREE && !state.extensions.accelerationStructure) {
        continue;
      }

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

      VkMemoryPropertyFlags fallback = (bufferFlags[i] & hostVisible) ? hostVisible : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      // Find fallback memory type
      for (uint32_t j = 0; j < memory.memoryProperties.memoryTypeCount; j++) {
        if (requirements.memoryTypeBits & (1 << j)) {
          if ((memoryTypes[j].propertyFlags & fallback) == fallback) {
            allocator->memoryFlags = memoryTypes[j].propertyFlags;
            allocator->heapIndex = memoryTypes[j].heapIndex;
            allocator->fallbackMemoryType = j;
            allocator->memoryType = j;
            break;
          }
        }
      }

      // Find memory type
      for (uint32_t j = 0; j < memory.memoryProperties.memoryTypeCount; j++) {
        if (requirements.memoryTypeBits & (1 << j)) {
          if ((memoryTypes[j].propertyFlags & bufferFlags[i]) == bufferFlags[i]) {
            allocator->memoryFlags = memoryTypes[j].propertyFlags;
            allocator->heapIndex = memoryTypes[j].heapIndex;
            allocator->memoryType = j;
            break;
          }
        }
      }
    }

    // Textures

    VkImageUsageFlags transient = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    VkImageUsageFlags hostCopy = state.extensions.hostImageCopy ? VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT : 0;

    struct { VkFormat format; VkImageUsageFlags usage; } imageFlags[] = {
      [GPU_MEMORY_TEXTURE_COLOR] = { VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT },
      [GPU_MEMORY_TEXTURE_HOST_COLOR] = { VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | hostCopy },
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
      for (uint32_t j = 0; j < memory.memoryProperties.memoryTypeCount; j++) {
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
        state.allocators[index].heapIndex = memoryTypes[memoryType].heapIndex;
        state.allocators[index].fallbackMemoryType = memoryType;
        state.allocators[index].memoryType = memoryType;
        state.allocatorLookup[i] = index;
      }
    }

    mtx_init(&state.allocatorLock, mtx_plain);
  }

  // Semaphore

  VkSemaphoreCreateInfo timelineSemaphoreInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = &(VkSemaphoreTypeCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE
    }
  };

  VK(vkCreateSemaphore(state.device, &timelineSemaphoreInfo, NULL, &state.semaphore), "vkCreateSemaphore") goto fail;

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

  mtx_init(&state.morgue.lock, mtx_plain);

  return true;
fail:
  gpu_destroy();
  return false;
}

void gpu_destroy(void) {
  if (state.device) vkDeviceWaitIdle(state.device);
  expunge(UINT64_MAX);
  for (gpu_thread_state* t = state.threads, *next; t; t = next) {
    for (gpu_stream_pool* pool = t->streamPools, *next; pool; pool = next) {
      vkDestroyCommandPool(state.device, pool->handle, NULL);
      for (gpu_stream* stream = pool->head, *next; stream; stream = next) {
        next = stream->next;
        state.config.fnFree(stream);
      }
      next = pool->next;
      state.config.fnFree(pool);
    }
    next = t->next;
    memset(t, 0, sizeof(*t));
  }
  for (gpu_victim* victim = state.morgue.pool, *next; victim; victim = next) {
    next = victim->next;
    state.config.fnFree(victim);
  }
  mtx_destroy(&state.morgue.lock);
  mtx_destroy(&state.allocatorLock);
  if (state.pipelineCache) vkDestroyPipelineCache(state.device, state.pipelineCache, NULL);
  if (state.semaphore) vkDestroySemaphore(state.device, state.semaphore, NULL);
  for (uint32_t i = 0; i < COUNTOF(state.memory); i++) {
    if (state.memory[i].handle) vkFreeMemory(state.device, state.memory[i].handle, NULL);
  }
  for (uint32_t i = 0; i < FRAME_DEPTH; i++) {
    if (state.surface.acquireSemaphores[i]) vkDestroySemaphore(state.device, state.surface.acquireSemaphores[i], NULL);
  }
  for (uint32_t i = 0; i < COUNTOF(state.surface.images); i++) {
    if (state.surface.presentSemaphores[i]) vkDestroySemaphore(state.device, state.surface.presentSemaphores[i], NULL);
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

char* gpu_get_error(void) {
  return thread.error;
}

bool gpu_get_memory_info(uint64_t* budget, uint64_t* usage) {
  if (!state.extensions.memoryBudget) {
    return false;
  }

  VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetInfo = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT
  };

  VkPhysicalDeviceMemoryProperties2 properties = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
    .pNext = &budgetInfo
  };

  vkGetPhysicalDeviceMemoryProperties2(state.adapter, &properties);

  // This currently only exposes the stats for the "primary" VRAM heap, which is the heap(s) used
  // for STATIC buffers and COLOR textures.  Usually these use the same heap, but if they're
  // different then we sum them together.  This isn't perfect, but is usually good enough.
  uint32_t bufferHeap = state.allocators[state.allocatorLookup[GPU_MEMORY_BUFFER_STATIC]].heapIndex;
  uint32_t textureHeap = state.allocators[state.allocatorLookup[GPU_MEMORY_TEXTURE_COLOR]].heapIndex;

  *budget = budgetInfo.heapBudget[bufferHeap];
  *usage = budgetInfo.heapUsage[bufferHeap];

  if (textureHeap != bufferHeap) {
    *budget += budgetInfo.heapUsage[textureHeap];
    *usage += budgetInfo.heapUsage[textureHeap];
  }

  return true;
}

bool gpu_submit(gpu_stream** streams, uint32_t count, uint32_t tick) {
  gpu_upload* upload = atomic_exchange(&state.uploads, NULL);
  gpu_stream* stream = upload ? gpu_stream_begin("Texture Uploads") : NULL;

  while (upload) {
    VkImageSubresourceRange subresource = {
      .aspectMask = upload->aspect,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .layerCount = VK_REMAINING_ARRAY_LAYERS
    };

    if (upload->copyCount > 0) {
      VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

      vkCmdPipelineBarrier2KHR(stream->commands, &(VkDependencyInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &(VkImageMemoryBarrier2KHR) {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
          .image = upload->image,
          .subresourceRange = subresource,
          .srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR,
          .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR,
          .srcAccessMask = VK_ACCESS_2_NONE_KHR,
          .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = layout
        }
      });

      vkCmdCopyBufferToImage(stream->commands, upload->buffer.handle, upload->image, layout, upload->copyCount, upload->copies);

      vkCmdPipelineBarrier2KHR(stream->commands, &(VkDependencyInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &(VkImageMemoryBarrier2KHR) {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
          .image = upload->image,
          .subresourceRange = subresource,
          .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR,
          .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, // TODO improve
          .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
          .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
          .oldLayout = layout,
          .newLayout = upload->layout
        }
      });

      condemn(upload->buffer.handle, VK_OBJECT_TYPE_BUFFER);
      release(upload->buffer.memory, upload->buffer.offset);
    } else {
      vkCmdPipelineBarrier2KHR(stream->commands, &(VkDependencyInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &(VkImageMemoryBarrier2KHR) {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
          .image = upload->image,
          .subresourceRange = subresource,
          .srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR,
          .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, // TODO improve
          .srcAccessMask = VK_ACCESS_2_NONE_KHR,
          .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = upload->layout
        }
      });
    }

    gpu_upload* next = upload->next;
    state.config.fnFree(upload);
    upload = next;
  }

  if (stream) {
    VK(vkEndCommandBuffer(stream->commands), "vkEndCommandBuffer") {
      return false;
    }

    VkSubmitInfo submit = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &stream->commands
    };

    VK(vkQueueSubmit(state.queue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit") {
      return false;
    }
  }

  VkCommandBuffer stack[64];
  VkCommandBuffer* commandBuffers = stack;

  if (count > COUNTOF(stack)) {
    commandBuffers = state.config.fnAlloc(count * sizeof(VkCommandBuffer));
    ASSERT(commandBuffers, "Out of memory") return false;
  }

  for (uint32_t i = 0; i < count; i++) {
    commandBuffers[i] = streams[i]->commands;
  }

  for (gpu_thread_state* t = state.threads; t; t = t->next) {
    if (t->activeStreamPool) {
      t->activeStreamPool->tick = tick;
      t->activeStreamPool = NULL;
    }
  }

  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &(VkTimelineSemaphoreSubmitInfo) {
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .waitSemaphoreValueCount = !!state.surface.acquireSemaphore,
      .pWaitSemaphoreValues = (uint64_t[1]) { 0 },
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = (uint64_t[1]) { tick }
    },
    .waitSemaphoreCount = !!state.surface.acquireSemaphore,
    .pWaitSemaphores = &state.surface.acquireSemaphore,
    .pWaitDstStageMask = &waitStage,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &state.semaphore,
    .commandBufferCount = count,
    .pCommandBuffers = commandBuffers
  };

  VK(vkQueueSubmit(state.queue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit") {
    if (commandBuffers != stack) state.config.fnFree(commandBuffers);
    return false;
  }

  expunge(getFinishedTick());

  if (commandBuffers != stack) state.config.fnFree(commandBuffers);

  if (state.surface.acquireSemaphore) {
    state.surface.acquireSemaphore = VK_NULL_HANDLE;
    state.surface.acquireTick[state.frame & FRAME_MASK] = tick;
  }

  state.tick = tick;
  return true;
}

bool gpu_is_complete(uint32_t tick) {
  return getFinishedTick() >= (uint64_t) tick;
}

bool gpu_wait_tick(uint32_t tick) {
  VkSemaphoreWaitInfo info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
    .semaphoreCount = 1,
    .pSemaphores = &state.semaphore,
    .pValues = (uint64_t[1]) { tick }
  };

  VK(vkWaitSemaphoresKHR(state.device, &info, UINT64_MAX), "vkWaitSemaphores") return false;
  return true;
}

bool gpu_wait_idle(void) {
  VK(vkDeviceWaitIdle(state.device), "vkDeviceWaitIdle") return false;
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
    [GPU_MEMORY_BUFFER_TREE] = 1 << 24,
    [GPU_MEMORY_TEXTURE_COLOR] = 1 << 26,
    [GPU_MEMORY_TEXTURE_HOST_COLOR] = 1 << 26,
    [GPU_MEMORY_TEXTURE_D16] = 1 << 26,
    [GPU_MEMORY_TEXTURE_D24] = 1 << 26,
    [GPU_MEMORY_TEXTURE_D32F] = 1 << 26,
    [GPU_MEMORY_TEXTURE_D24S8] = 1 << 26,
    [GPU_MEMORY_TEXTURE_D32FS8] = 1 << 26,
    [GPU_MEMORY_TEXTURE_LAZY_COLOR] = 1 << 26,
    [GPU_MEMORY_TEXTURE_LAZY_D16] = 1 << 26,
    [GPU_MEMORY_TEXTURE_LAZY_D24] = 1 << 26,
    [GPU_MEMORY_TEXTURE_LAZY_D32F] = 1 << 26,
    [GPU_MEMORY_TEXTURE_LAZY_D24S8] = 1 << 26,
    [GPU_MEMORY_TEXTURE_LAZY_D32FS8] = 1 << 26
  };

  uint32_t blockSize = blockSizes[type];
  ASSERT(blockSize <= (GPU_MAX_PAGES * GPU_PAGE_SIZE), "Block size larger than allocator can handle") return NULL;

  uint32_t align = MAX(info.alignment, GPU_PAGE_SIZE);
  uint32_t requiredPages = ALIGN(info.size, align) / GPU_PAGE_SIZE;

  mtx_lock(&state.allocatorLock);

  if (allocator->block) {
    // Search through regions for a free region of sufficient size
    for (uint32_t i = 0; i < allocator->pageCount; i += allocator->regions[i].pageCount) {
      gpu_alloc_entry* region = &allocator->regions[i];

      // The alignment may require us to start the allocation later than the
      // beginning of the region
      uint32_t offsetPages = (ALIGN(i * GPU_PAGE_SIZE, align) / GPU_PAGE_SIZE) - i;
      uint32_t totalPages = offsetPages + requiredPages;

      if (!region->allocated && totalPages <= region->pageCount) {
        region->allocated = true;

        // If there's leftover room, mark the leftover free region, and shrink
        // the current region
        if (totalPages < region->pageCount) {
          gpu_alloc_entry* freeRegion = &allocator->regions[i + totalPages];
          freeRegion->allocated = false;
          freeRegion->pageCount = region->pageCount - totalPages;
          region->pageCount = totalPages;

          // Coalesce with the next region, if possible
          uint32_t nextIndex = i + totalPages + freeRegion->pageCount;
          if (nextIndex < allocator->pageCount) {
            gpu_alloc_entry* nextRegion = &allocator->regions[nextIndex];
            if (!nextRegion->allocated) {
              freeRegion->pageCount += nextRegion->pageCount;
            }
          }
        }

        allocator->block->refs++;
        *offset = (i + offsetPages) * GPU_PAGE_SIZE;
        mtx_unlock(&state.allocatorLock);
        return allocator->block;
      }
    }
  }

  VkMemoryAllocateFlags flags = 0;
  if (type == GPU_MEMORY_BUFFER_STATIC || type == GPU_MEMORY_BUFFER_STREAM || type == GPU_MEMORY_BUFFER_TREE) {
    flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
  }

  // If there wasn't an active block or it overflowed, find an empty block to allocate
  for (uint32_t i = 0; i < COUNTOF(state.memory); i++) {
    if (!state.memory[i].handle) {
      gpu_memory* memory = &state.memory[i];

      VkMemoryAllocateFlagsInfo flagInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = flags
      };

      VkMemoryAllocateInfo memoryInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = flags ? &flagInfo : NULL,
        .allocationSize = MAX(blockSize, info.size),
        .memoryTypeIndex = allocator->memoryType
      };

      VkResult result = vkAllocateMemory(state.device, &memoryInfo, NULL, &memory->handle);

      if (result < 0) {
        // If memory allocation failed, try the fallback memory type, if one exists
        if (allocator->fallbackMemoryType != allocator->memoryType) {
          memoryInfo.memoryTypeIndex = allocator->fallbackMemoryType;
          result = vkAllocateMemory(state.device, &memoryInfo, NULL, &memory->handle);
        }

        VK(result, "vkAllocateMemory") {
          allocator->block = NULL;
          mtx_unlock(&state.allocatorLock);
          return NULL;
        }
      }

      if (allocator->memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        VK(vkMapMemory(state.device, memory->handle, 0, VK_WHOLE_SIZE, 0, &memory->pointer), "vkMapMemory") {
          vkFreeMemory(state.device, memory->handle, NULL);
          memory->handle = NULL;
          mtx_unlock(&state.allocatorLock);
          return NULL;
        }
      } else {
        memory->pointer = NULL;
      }

      memory->type = type;
      memory->refs = 1;

      // Memory only receives an allocator if it can host multiple allocations
      // (i.e. it has a fixed block size and this block is not full/oversized)
      if (blockSize && (requiredPages * GPU_PAGE_SIZE) < blockSize) {
        allocator->block = memory;

        // Mark the initial region
        allocator->pageCount = blockSize / GPU_PAGE_SIZE;
        allocator->regions[0].allocated = true;
        allocator->regions[0].pageCount = requiredPages;

        // Mark the free region at the end. We know there's a free region
        // because we don't let allocators manage full or oversized blocks
        allocator->regions[requiredPages].allocated = false;
        allocator->regions[requiredPages].pageCount = allocator->pageCount - requiredPages;
      }

      *offset = 0;
      mtx_unlock(&state.allocatorLock);
      return memory;
    }
  }

  error("Out of GPU memory blocks");
  mtx_unlock(&state.allocatorLock);
  return NULL;
}

static void release(gpu_memory* memory, VkDeviceSize offset) {
  if (!memory) {
    return;
  }

  gpu_allocator* allocator = &state.allocators[state.allocatorLookup[memory->type]];

  mtx_lock(&state.allocatorLock);

  if (--memory->refs == 0) {
    // If the allocator manages this block, reset it, otherwise free the memory
    if (allocator->block == memory) {
      gpu_alloc_entry* region = &allocator->regions[0];
      region->allocated = false;
      region->pageCount = allocator->pageCount;
    } else {
      condemn(memory->handle, VK_OBJECT_TYPE_DEVICE_MEMORY);
      memory->handle = NULL;
    }
  } else if (allocator->block == memory) {
    // Mark the region for this allocation as free
    uint32_t pageOffset = offset / GPU_PAGE_SIZE;
    gpu_alloc_entry* region = &allocator->regions[pageOffset];
    region->allocated = false;

    // See if we can coalesce this region with the next region
    uint32_t nextOffset = pageOffset + region->pageCount;
    if (nextOffset < allocator->pageCount) {
      gpu_alloc_entry* nextRegion = &allocator->regions[nextOffset];
      if (!nextRegion->allocated) {
        region->pageCount += nextRegion->pageCount;
      }
    }

    // See if we can coalesce with the previous region. We have to loop
    // through regions from the beginning because we don't know where the
    // last region started
    for (uint32_t i = 0; i < pageOffset; i += allocator->regions[i].pageCount) {
      gpu_alloc_entry* previousRegion = &allocator->regions[i];
      if (i + previousRegion->pageCount == pageOffset) {
        // We found the previous region, coalesce if it's free
        if (!previousRegion->allocated) {
          previousRegion->pageCount += region->pageCount;
        }
      }
    }
  }

  mtx_unlock(&state.allocatorLock);
}

static void condemn(void* handle, VkObjectType type) {
  if (!handle) return;

  gpu_morgue* morgue = &state.morgue;
  gpu_victim* victim = morgue->pool;

  mtx_lock(&morgue->lock);

  if (victim) {
    morgue->pool = victim->next;
  } else {
    victim = state.config.fnAlloc(sizeof(*victim));
  }

  victim->next = NULL;
  victim->type = type;
  victim->tick = state.tick + 1;
  victim->handle = handle;

  if (morgue->tail) {
    morgue->tail->next = victim;
  } else {
    morgue->head = victim;
  }

  morgue->tail = victim;

  mtx_unlock(&morgue->lock);
}

static void expunge(uint64_t tick) {
  gpu_morgue* morgue = &state.morgue;

  mtx_lock(&morgue->lock);

  while (morgue->head && tick >= morgue->head->tick) {
    gpu_victim* victim = morgue->head;

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
      case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR: vkDestroyAccelerationStructureKHR(state.device, victim->handle, NULL); break;
      case VK_OBJECT_TYPE_DEVICE_MEMORY: vkFreeMemory(state.device, victim->handle, NULL); break;
      default: LOG("Trying to destroy invalid Vulkan object type!"); break;
    }

    morgue->head = victim->next;
    if (!morgue->head) {
      morgue->tail = NULL;
    }

    victim->next = morgue->pool;
    morgue->pool = victim;
  }

  mtx_unlock(&morgue->lock);
}

static uint64_t getFinishedTick(void) {
  uint64_t value;
  VK(vkGetSemaphoreCounterValueKHR(state.device, state.semaphore, &value), "vkGetSemaphoreCounterValue") return 0;
  return value;
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
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        (state.extensions.bufferDeviceAddress ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0) |
        (state.extensions.accelerationStructure ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR : 0);
    case GPU_BUFFER_STREAM:
      return
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        (state.extensions.bufferDeviceAddress ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0) |
        (state.extensions.accelerationStructure ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR : 0);
    case GPU_BUFFER_UPLOAD:
      return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    case GPU_BUFFER_DOWNLOAD:
      return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case GPU_BUFFER_TREE:
      return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
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

static VkImageLayout getNaturalLayout(uint32_t usage) {
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
    [GPU_FORMAT_BGRA8] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB },
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

  return formats[format][colorspace];
}

static VkFormat convertAttributeType(gpu_attribute_type type) {
  static const VkFormat types[] = {
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

  return types[type];
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
  if (phase & GPU_PHASE_TREE_BUILD) flags |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
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
  if (cache & GPU_CACHE_TREE_INPUT) flags |= VK_ACCESS_2_SHADER_READ_BIT_KHR;
  if (cache & GPU_CACHE_TREE_READ) flags |= VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
  if (cache & GPU_CACHE_TREE_WRITE) flags |= VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
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
