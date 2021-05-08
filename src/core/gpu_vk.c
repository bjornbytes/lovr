#include "gpu.h"
#include <string.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#ifdef _WIN32
#include <windows.h>
#define VULKAN_LIBRARY "vulkan-1.dll"
#define SYMTYPE FARPROC
typedef CRITICAL_SECTION gpu_mutex;
#else
#include <dlfcn.h>
#include <pthread.h>
#define VULKAN_LIBRARY "libvulkan.so"
#define SYMTYPE void*
typedef pthread_mutex_t gpu_mutex;
#endif

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#define GPU_THROW(s) if (state.config.callback) { state.config.callback(state.config.userdata, s, true); }
#define GPU_CHECK(c, s) if (!(c)) { GPU_THROW(s); }
#define GPU_VK(f) do { VkResult r = (f); GPU_CHECK(r >= 0, getErrorString(r)); } while (0)
#define HASH_SEED 2166136261
#define QUERY_CHUNK 64

// Objects

struct gpu_buffer {
  VkBuffer handle;
  VkDeviceMemory memory;
  uint32_t ticks[2];
  char* data[2];
  bool region;
  bool keep;
};

struct gpu_texture {
  VkImage handle;
  VkImageView view;
  VkDeviceMemory memory;
  VkImageViewType type;
  VkFormat format;
  VkImageLayout layout;
  VkPipelineStageFlagBits users;
  VkAccessFlagBits writer;
  VkImageAspectFlagBits aspect;
  VkSampleCountFlagBits samples;
  gpu_texture* source;
};

struct gpu_sampler {
  VkSampler handle;
};

struct gpu_pass {
  VkRenderPass handle;
  uint16_t samples;
  uint16_t count;
  uint16_t views;
  bool resolve;
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

struct gpu_batch {
  VkCommandBuffer commands;
};

// Pools

typedef struct {
  VkCommandPool pool;
  gpu_batch batch[32];
  uint32_t batchCount;
  uint32_t renderPassMask;
  VkFence fence;
  VkSemaphore waitSemaphore;
  VkSemaphore tellSemaphore;
  bool wait, tell;
} gpu_tick;

typedef struct {
  VkDescriptorPool handle;
  uint32_t tick;
  uint32_t next;
} gpu_binding_puddle;

typedef struct {
  gpu_binding_puddle data[16];
  uint32_t count;
  uint32_t head;
  uint32_t tail;
} gpu_binding_pool;

typedef struct {
  void* handle;
  VkObjectType type;
  uint32_t tick;
} gpu_ref;

typedef struct {
  gpu_ref data[256];
  gpu_mutex lock;
  uint32_t head;
  uint32_t tail;
} gpu_morgue;

typedef struct {
  VkBuffer buffer;
  VkDeviceMemory memory;
  void* data;
  uint32_t tick;
  uint32_t next;
} gpu_scratchpad;

typedef struct {
  gpu_scratchpad data[128];
  uint32_t cursor;
  uint32_t count;
  uint32_t head;
  uint32_t tail;
} gpu_scratchpad_pool;

typedef struct {
  gpu_read_fn* fn;
  void* userdata;
  uint8_t* data;
  uint32_t size;
  uint32_t tick;
} gpu_readback;

typedef struct {
  gpu_readback data[128];
  uint32_t head;
  uint32_t tail;
} gpu_readback_pool;

typedef struct {
  uint32_t hash;
  uint32_t tick;
  void* object;
} gpu_cache_entry;

// State

static struct {
  void* library;
  VkInstance instance;
  VkDevice device;
  VkQueue queue;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  gpu_texture backbuffers[4];
  uint32_t currentBackbuffer;
  VkPipelineCache pipelineCache;
  VkDebugUtilsMessengerEXT messenger;
  VkPhysicalDeviceMemoryProperties memoryProperties;
  VkMemoryRequirements scratchMemoryRequirements;
  uint32_t scratchMemoryType;
  uint32_t queueFamilyIndex;
  uint32_t tick[2];
  gpu_tick ticks[4];
  gpu_batch batch;
  gpu_morgue morgue;
  gpu_cache_entry framebuffers[16][4];
  gpu_scratchpad_pool scratchpads;
  gpu_readback_pool readbacks;
  gpu_binding_pool bindings;
  VkQueryPool queryPool;
  uint32_t queryCount;
  gpu_config config;
} state;

// Helpers

enum { CPU, GPU };
enum { LINEAR, SRGB };

typedef struct {
  VkBuffer buffer;
  uint32_t offset;
  uint8_t* data;
} gpu_mapping;

static void hash32(uint32_t* hash, void* data, uint32_t size);
static gpu_cache_entry* cacheFetch(gpu_cache_entry* cache, uint32_t rows, uint32_t cols, uint32_t hash);
static gpu_cache_entry* cacheEvict(gpu_cache_entry* cache, uint32_t rows, uint32_t cols, uint32_t hash);
static gpu_mapping scratch(uint32_t size);
static void ketchup(void);
static void stall(uint32_t until);
static void readback(void);
static void condemn(void* handle, VkObjectType type);
static void expunge(void);
static gpu_batch* begin(bool render);
static VkFormat convertFormat(gpu_texture_format format, int colorspace);
static uint64_t getTextureRegionSize(VkFormat format, uint16_t extent[3]);
static void nickname(void* object, VkObjectType type, const char* name);
static VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* context);
static const char* getErrorString(VkResult result);
static void gpu_mutex_init(gpu_mutex* mutex);
static void gpu_mutex_destroy(gpu_mutex* mutex);
static void gpu_mutex_lock(gpu_mutex* mutex);
static void gpu_mutex_unlock(gpu_mutex* mutex);
static void* gpu_dlopen(const char* library);
static SYMTYPE gpu_dlsym(void* library, const char* symbol);
static void gpu_dlclose(void* library);

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

// Entry

bool gpu_init(gpu_config* config) {
  state.config = *config;

  // Load
  state.library = gpu_dlopen(VULKAN_LIBRARY);
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) gpu_dlsym(state.library, "vkGetInstanceProcAddr");
  GPU_FOREACH_ANONYMOUS(GPU_LOAD_ANONYMOUS);

  { // Instance
    const char* layers[] = {
      "VK_LAYER_KHRONOS_validation"
    };

    const char* extensions[3];
    uint32_t extensionCount = 0;

    if (state.config.debug) {
      extensions[extensionCount++] = "VK_EXT_debug_utils";
    }

    if (state.config.vk.getExtraInstanceExtensions) {
      uint32_t extraExtensionCount;
      const char** extraExtensions = state.config.vk.getExtraInstanceExtensions(&extraExtensionCount);

      if (extraExtensionCount + extensionCount > COUNTOF(extensions)) {
        gpu_destroy();
        return false;
      }

      for (uint32_t i = 0; i < extraExtensionCount; i++) {
        extensions[extensionCount++] = extraExtensions[i];
      }
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

    if (vkCreateInstance(&instanceInfo, NULL, &state.instance)) {
      gpu_destroy();
      return false;
    }

    GPU_FOREACH_INSTANCE(GPU_LOAD_INSTANCE);

    if (state.config.debug) {
      VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = debugCallback
      };

      if (vkCreateDebugUtilsMessengerEXT(state.instance, &messengerInfo, NULL, &state.messenger)) {
        gpu_destroy();
        return false;
      }
    }
  }

  // Surface
  if (state.config.vk.createSurface) {
    if (state.config.vk.createSurface(state.instance, (void**) &state.surface)) {
      gpu_destroy();
      return false;
    }
  }

  VkPhysicalDevice physicalDevice;

  { // Device
    uint32_t deviceCount = 1;
    if (vkEnumeratePhysicalDevices(state.instance, &deviceCount, &physicalDevice) < 0) {
      gpu_destroy();
      return false;
    }

    VkQueueFamilyProperties queueFamilies[4];
    uint32_t queueFamilyCount = COUNTOF(queueFamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

    state.queueFamilyIndex = ~0u;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
      VkBool32 presentable = VK_TRUE;

      if (state.surface) {
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, state.surface, &presentable);
      }

      uint32_t flags = queueFamilies[i].queueFlags;
      if ((flags & VK_QUEUE_GRAPHICS_BIT) && (flags & VK_QUEUE_COMPUTE_BIT) && presentable) {
        state.queueFamilyIndex = i;
        break;
      }
    }

    if (state.queueFamilyIndex == ~0u) {
      gpu_destroy();
      return false;
    }

    VkDeviceQueueCreateInfo queueInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = state.queueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = &(float) { 1.f }
    };

    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &state.memoryProperties);

    if (config->limits) {
      VkPhysicalDeviceMaintenance3Properties maintenance3Limits = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES };
      VkPhysicalDeviceMultiviewProperties multiviewLimits = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES, .pNext = &maintenance3Limits };
      VkPhysicalDeviceProperties2 properties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &multiviewLimits };
      VkPhysicalDeviceLimits* deviceLimits = &properties2.properties.limits;
      vkGetPhysicalDeviceProperties2(physicalDevice, &properties2);
      config->limits->textureSize2D = MIN(deviceLimits->maxImageDimension2D, UINT16_MAX);
      config->limits->textureSize3D = MIN(deviceLimits->maxImageDimension3D, UINT16_MAX);
      config->limits->textureSizeCube = MIN(deviceLimits->maxImageDimensionCube, UINT16_MAX);
      config->limits->textureLayers = MIN(deviceLimits->maxImageArrayLayers, UINT16_MAX);
      config->limits->renderSize[0] = deviceLimits->maxFramebufferWidth;
      config->limits->renderSize[1] = deviceLimits->maxFramebufferHeight;
      config->limits->renderViews = multiviewLimits.maxMultiviewViewCount;
      config->limits->bundleCount = MIN(deviceLimits->maxBoundDescriptorSets, COUNTOF(((gpu_shader*) NULL)->layouts));
      config->limits->bundleSlots = maintenance3Limits.maxPerSetDescriptors;
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
      config->limits->allocationSize = maintenance3Limits.maxMemoryAllocationSize;
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
      vkGetPhysicalDeviceFeatures2(physicalDevice, &root);

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
      for (uint32_t i = 0; i < GPU_TEXTURE_FORMAT_COUNT; i++) {
        vkGetPhysicalDeviceFormatProperties(physicalDevice, convertFormat(i, LINEAR), &formatProperties);
        uint32_t blitMask = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
        uint32_t flags = formatProperties.optimalTilingFeatures;
        config->features->formats[i] =
          ((flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ? GPU_FORMAT_FEATURE_SAMPLE : 0) |
          ((flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? GPU_FORMAT_FEATURE_RENDER_COLOR : 0) |
          ((flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ? GPU_FORMAT_FEATURE_RENDER_DEPTH : 0) |
          ((flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) ? GPU_FORMAT_FEATURE_BLEND : 0) |
          ((flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ? GPU_FORMAT_FEATURE_FILTER : 0) |
          ((flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) ? GPU_FORMAT_FEATURE_STORAGE : 0) |
          ((flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT) ? GPU_FORMAT_FEATURE_ATOMIC : 0) |
          ((flags & blitMask) == blitMask ? GPU_FORMAT_FEATURE_BLIT : 0);
      }
    }

    const char* extension = "VK_KHR_swapchain";

    VkDeviceCreateInfo deviceInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = config->features ? &enabledFeatures : NULL,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queueInfo,
      .enabledExtensionCount = state.surface ? 1 : 0,
      .ppEnabledExtensionNames = &extension
    };

    if (vkCreateDevice(physicalDevice, &deviceInfo, NULL, &state.device)) {
      gpu_destroy();
      return false;
    }

    vkGetDeviceQueue(state.device, state.queueFamilyIndex, 0, &state.queue);

    GPU_FOREACH_DEVICE(GPU_LOAD_DEVICE);
  }

  // Swapchain
  if (state.surface) {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, state.surface, &surfaceCapabilities);

    VkSurfaceFormatKHR formats[16];
    uint32_t formatCount = COUNTOF(formats);
    VkSurfaceFormatKHR surfaceFormat = { .format = VK_FORMAT_UNDEFINED };
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, state.surface, &formatCount, formats);

    for (uint32_t i = 0; i < formatCount; i++) {
      if (formats[i].format == VK_FORMAT_R8G8B8A8_SRGB || formats[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
        surfaceFormat = formats[i];
        break;
      }
    }

    if (surfaceFormat.format == VK_FORMAT_UNDEFINED) {
      gpu_destroy();
      return false;
    }

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

    if (vkCreateSwapchainKHR(state.device, &swapchainInfo, NULL, &state.swapchain)) {
      gpu_destroy();
      return false;
    }

    uint32_t imageCount;
    if (vkGetSwapchainImagesKHR(state.device, state.swapchain, &imageCount, NULL)) {
      gpu_destroy();
      return false;
    }

    if (imageCount > COUNTOF(state.backbuffers)) {
      gpu_destroy();
      return false;
    }

    VkImage images[COUNTOF(state.backbuffers)];
    if (vkGetSwapchainImagesKHR(state.device, state.swapchain, &imageCount, images)) {
      gpu_destroy();
      return false;
    }

    for (uint32_t i = 0; i < imageCount; i++) {
      gpu_texture* texture = &state.backbuffers[i];

      texture->handle = images[i];
      texture->format = surfaceFormat.format;
      texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
      texture->samples = VK_SAMPLE_COUNT_1_BIT;

      gpu_texture_view_info view = {
        .source = texture,
        .type = GPU_TEXTURE_TYPE_2D
      };

      if (!gpu_texture_init_view(texture, &view)) {
        gpu_destroy();
        return false;
      }
    }
  }

  { // Pipeline cache
    VkPipelineCacheCreateInfo info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    if (vkCreatePipelineCache(state.device, &info, NULL, &state.pipelineCache)) {
      gpu_destroy();
      return false;
    }
  }

  // Tick resources
  for (uint32_t i = 0; i < COUNTOF(state.ticks); i++) {
    VkCommandPoolCreateInfo poolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = state.queueFamilyIndex
    };

    if (vkCreateCommandPool(state.device, &poolInfo, NULL, &state.ticks[i].pool)) {
      gpu_destroy();
      return false;
    }

    VkCommandBufferAllocateInfo allocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = state.ticks[i].pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = COUNTOF(state.ticks[i].batch)
    };

    if (vkAllocateCommandBuffers(state.device, &allocateInfo, &state.ticks[i].batch[0].commands)) {
      gpu_destroy();
      return false;
    }

    VkFenceCreateInfo fenceInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    if (vkCreateFence(state.device, &fenceInfo, NULL, &state.ticks[i].fence)) {
      gpu_destroy();
      return false;
    }

    VkSemaphoreCreateInfo semaphoreInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    if (vkCreateSemaphore(state.device, &semaphoreInfo, NULL, &state.ticks[i].waitSemaphore)) {
      gpu_destroy();
      return false;
    }

    if (vkCreateSemaphore(state.device, &semaphoreInfo, NULL, &state.ticks[i].tellSemaphore)) {
      gpu_destroy();
      return false;
    }
  }

  // Query pool
  if (state.config.debug) {
    VkQueryPoolCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType = VK_QUERY_TYPE_TIMESTAMP,
      .queryCount = COUNTOF(state.ticks) * QUERY_CHUNK
    };

    if (vkCreateQueryPool(state.device, &info, NULL, &state.queryPool)) {
      gpu_destroy();
      return false;
    }
  }

  gpu_mutex_init(&state.morgue.lock);
  state.scratchpads.head = ~0u;
  state.scratchpads.tail = ~0u;
  state.currentBackbuffer = ~0u;
  return true;
}

void gpu_destroy(void) {
  if (state.device) vkDeviceWaitIdle(state.device);
  state.tick[GPU] = state.tick[CPU];
  readback();
  expunge();
  for (uint32_t i = 0; i < state.scratchpads.count; i++) {
    vkDestroyBuffer(state.device, state.scratchpads.data[i].buffer, NULL);
    vkFreeMemory(state.device, state.scratchpads.data[i].memory, NULL);
  }
  for (uint32_t i = 0; i < COUNTOF(state.framebuffers); i++) {
    for (uint32_t j = 0; j < COUNTOF(state.framebuffers[0]); j++) {
      if (state.framebuffers[i][j].object) vkDestroyFramebuffer(state.device, state.framebuffers[i][j].object, NULL);
    }
  }
  for (uint32_t i = 0; i < COUNTOF(state.ticks); i++) {
    gpu_tick* tick = &state.ticks[i];
    if (tick->waitSemaphore) vkDestroySemaphore(state.device, tick->waitSemaphore, NULL);
    if (tick->tellSemaphore) vkDestroySemaphore(state.device, tick->tellSemaphore, NULL);
    if (tick->fence) vkDestroyFence(state.device, tick->fence, NULL);
    if (tick->pool) vkDestroyCommandPool(state.device, tick->pool, NULL);
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
  gpu_dlclose(state.library);
  gpu_mutex_destroy(&state.morgue.lock);
  memset(&state, 0, sizeof(state));
}

void gpu_begin() {
  gpu_tick* tick = &state.ticks[state.tick[CPU]++ & 0x3];

  GPU_VK(vkWaitForFences(state.device, 1, &tick->fence, VK_FALSE, ~0ull));
  GPU_VK(vkResetFences(state.device, 1, &tick->fence));
  GPU_VK(vkResetCommandPool(state.device, tick->pool, 0));

  ketchup();
  readback();
  expunge();

  tick->wait = false;
  tick->tell = false;
  tick->batchCount = 0;
  state.batch = *begin(false);
  state.queryCount = 0;
}

void gpu_flush() {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0x3];
  gpu_batch_end(&state.batch);

  VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = tick->wait,
    .pWaitSemaphores = &tick->waitSemaphore,
    .pWaitDstStageMask = &waitMask,
    .commandBufferCount = tick->batchCount,
    .pCommandBuffers = &tick->batch[0].commands,
    .signalSemaphoreCount = tick->tell,
    .pSignalSemaphores = &tick->tellSemaphore
  };

  GPU_VK(vkQueueSubmit(state.queue, 1, &submit, tick->fence));
}

void gpu_debug_push(const char* label) {
  if (state.config.debug) {
    vkCmdBeginDebugUtilsLabelEXT(state.batch.commands, &(VkDebugUtilsLabelEXT) {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pLabelName = label
    });
  }
}

void gpu_debug_pop() {
  if (state.config.debug) {
    vkCmdEndDebugUtilsLabelEXT(state.batch.commands);
  }
}

void gpu_timer_mark() {
  if (state.config.debug) {
    if (state.queryCount == 0) {
      uint32_t queryIndex = (state.tick[CPU] & 0x3) * QUERY_CHUNK;
      vkCmdResetQueryPool(state.batch.commands, state.queryPool, queryIndex, QUERY_CHUNK);
    }

    vkCmdWriteTimestamp(state.batch.commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, state.queryPool, state.queryCount++);
  }
}

void gpu_timer_read(gpu_read_fn* fn, void* userdata) {
  if (!state.config.debug || state.queryCount == 0) {
    return;
  }

  uint32_t size = sizeof(uint64_t) * state.queryCount;
  gpu_mapping mapped = scratch(size);

  uint32_t queryIndex = (state.tick[CPU] & 0x3) * QUERY_CHUNK;
  VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT;
  vkCmdCopyQueryPoolResults(state.batch.commands, state.queryPool, queryIndex, state.queryCount, mapped.buffer, mapped.offset, sizeof(uint64_t), flags);

  gpu_readback_pool* readbacks = &state.readbacks;
  GPU_CHECK(readbacks->head - readbacks->tail != COUNTOF(readbacks->data), "Too many GPU readbacks"); // TODO emergency flush instead of throw
  readbacks->data[readbacks->head++ & 0x7f] = (gpu_readback) {
    .fn = fn,
    .userdata = userdata,
    .data = mapped.data,
    .size = size,
    .tick = state.tick[CPU]
  };
}

// Buffer

size_t gpu_sizeof_buffer() {
  return sizeof(gpu_buffer);
}

bool gpu_buffer_init(gpu_buffer* buffer, gpu_buffer_info* info) {
  VkBufferUsageFlags usage =
    ((info->flags & GPU_BUFFER_FLAG_VERTEX) ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : 0) |
    ((info->flags & GPU_BUFFER_FLAG_INDEX) ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT : 0) |
    ((info->flags & GPU_BUFFER_FLAG_UNIFORM) ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0) |
    ((info->flags & GPU_BUFFER_FLAG_STORAGE) ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0) |
    ((info->flags & GPU_BUFFER_FLAG_INDIRECT) ? VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT : 0) |
    ((info->flags & GPU_BUFFER_FLAG_COPY_SRC) ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0) |
    ((info->flags & GPU_BUFFER_FLAG_COPY_DST) ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0);

  bool mappable = info->flags & GPU_BUFFER_FLAG_MAPPABLE;

  VkBufferCreateInfo bufferInfo = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = info->size << mappable,
    .usage = usage
  };

  if (vkCreateBuffer(state.device, &bufferInfo, NULL, &buffer->handle)) {
    return false;
  }

  nickname(buffer->handle, VK_OBJECT_TYPE_BUFFER, info->label);

  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(state.device, buffer->handle, &requirements);

  uint32_t mask, fallback;
  if (!mappable) {
    mask = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    fallback = 0;
  } else if (info->flags & GPU_BUFFER_FLAG_PRESERVE) {
    mask = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    fallback = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  } else {
    mask = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    fallback = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  }

  uint32_t memoryType = ~0u;
  bool hostVisible;

search:
  for (uint32_t i = 0; i < state.memoryProperties.memoryTypeCount; i++) {
    uint32_t flags = state.memoryProperties.memoryTypes[i].propertyFlags;
    if ((flags & mask) != mask || ~requirements.memoryTypeBits & (1 << i)) continue;
    hostVisible = flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    memoryType = i;
    break;
  }

  if (memoryType == ~0u && fallback && mask != fallback) {
    mask = fallback;
    goto search;
  }

  if (memoryType == ~0u) {
    vkDestroyBuffer(state.device, buffer->handle, NULL);
    return false;
  }

  VkMemoryAllocateInfo memoryInfo = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = requirements.size,
    .memoryTypeIndex = memoryType
  };

  if (vkAllocateMemory(state.device, &memoryInfo, NULL, &buffer->memory)) {
    vkDestroyBuffer(state.device, buffer->handle, NULL);
    return false;
  }

  if (vkBindBufferMemory(state.device, buffer->handle, buffer->memory, 0)) {
    vkDestroyBuffer(state.device, buffer->handle, NULL);
    vkFreeMemory(state.device, buffer->memory, NULL);
    return false;
  }

  void* data = NULL;
  if (hostVisible && vkMapMemory(state.device, buffer->memory, 0, VK_WHOLE_SIZE, 0, &data)) {
    vkDestroyBuffer(state.device, buffer->handle, NULL);
    vkFreeMemory(state.device, buffer->memory, NULL);
    return false;
  }

  // If initial pointer is needed and memory was mappable, return mapped pointer
  if (info->pointer && hostVisible) {
    *info->pointer = data;
  }

  // If initial pointer is needed and memory is not mappable, use staging buffer (TODO 16MB limit)
  if (info->pointer && !hostVisible) {
    gpu_mapping mapped = scratch(info->size);

    VkBufferCopy region = {
      .srcOffset = mapped.offset,
      .dstOffset = 0,
      .size = info->size
    };

    vkCmdCopyBuffer(state.batch.commands, buffer->handle, mapped.buffer, 1, &region);
    *info->pointer = mapped.data;
  }

  if (mappable) {
    buffer->data[0] = data;
    buffer->data[1] = buffer->data[0] + info->size;
    buffer->ticks[0] = state.tick[CPU];
    buffer->ticks[1] = state.tick[CPU];
    buffer->keep = info->flags & GPU_BUFFER_FLAG_PRESERVE;
  }

  return true;
}

void gpu_buffer_destroy(gpu_buffer* buffer) {
  condemn(buffer->handle, VK_OBJECT_TYPE_BUFFER);
  condemn(buffer->memory, VK_OBJECT_TYPE_DEVICE_MEMORY);
  memset(buffer, 0, sizeof(*buffer));
}

void* gpu_buffer_map(gpu_buffer* buffer) {
  if (!buffer->data[0]) return NULL;

  // If the current region is active or the GPU has already caught up to the current region, use it
  if (buffer->ticks[buffer->region] == state.tick[CPU] || state.tick[GPU] >= buffer->ticks[buffer->region]) {
    buffer->ticks[buffer->region] = state.tick[CPU];
    return buffer->data[buffer->region];
  }

  // Otherwise, the GPU is using the current region, so switch to the other half, stalling if needed
  buffer->region = !buffer->region;
  if (buffer->ticks[buffer->region] > state.tick[GPU]) {
    ketchup();

    if (buffer->ticks[buffer->region] > state.tick[GPU]) {
      stall(buffer->ticks[buffer->region]);
    }
  }

  // Copy old data to new region if needed
  if (buffer->keep) {
    memcpy(buffer->data[buffer->region], buffer->data[!buffer->region], buffer->data[1] - buffer->data[0]);
  }

  return buffer->data[buffer->region];
}

void gpu_buffer_clear(gpu_buffer* buffer, uint64_t offset, uint64_t size) {
  if (buffer->data[0]) {
    void* data = gpu_buffer_map(buffer);
    memset((char*) data + offset, 0, size);
  } else {
    vkCmdFillBuffer(state.batch.commands, buffer->handle, offset, size, 0);
  }
}

void gpu_buffer_copy(gpu_buffer* src, gpu_buffer* dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
  VkBufferCopy region = {
    .srcOffset = srcOffset,
    .dstOffset = dstOffset,
    .size = size
  };

  vkCmdCopyBuffer(state.batch.commands, src->handle, dst->handle, 1, &region);
}

void gpu_buffer_read(gpu_buffer* buffer, uint64_t offset, uint64_t size, gpu_read_fn* fn, void* userdata) {
  gpu_mapping mapped = scratch(size);

  VkBufferCopy region = {
    .srcOffset = offset,
    .dstOffset = mapped.offset,
    .size = size
  };

  if (buffer->data[0]) {
    region.srcOffset += (buffer->data[1] - buffer->data[0]) * buffer->region;
  }

  vkCmdCopyBuffer(state.batch.commands, buffer->handle, mapped.buffer, 1, &region);

  gpu_readback_pool* pool = &state.readbacks;
  GPU_CHECK(pool->head - pool->tail != COUNTOF(pool->data), "Too many GPU readbacks"); // TODO emergency flush instead of throw
  pool->data[pool->head++ & 0x7f] = (gpu_readback) {
    .fn = fn,
    .userdata = userdata,
    .data = mapped.data,
    .size = size,
    .tick = state.tick[CPU]
  };
}

// Texture

size_t gpu_sizeof_texture() {
  return sizeof(gpu_texture);
}

bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info) {
  memset(texture, 0, sizeof(*texture));

  VkImageType type;
  VkImageCreateFlags flags = 0;
  switch (info->type) {
    case GPU_TEXTURE_TYPE_2D: type = VK_IMAGE_TYPE_2D; break;
    case GPU_TEXTURE_TYPE_3D: type = VK_IMAGE_TYPE_3D; flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT; break;
    case GPU_TEXTURE_TYPE_CUBE: type = VK_IMAGE_TYPE_2D; flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; break;
    case GPU_TEXTURE_TYPE_ARRAY: type = VK_IMAGE_TYPE_2D; break;
    default: return false;
  }

  texture->format = convertFormat(info->format, info->srgb);
  texture->samples = info->samples;

  if (info->format == GPU_TEXTURE_FORMAT_D24S8) {
    texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  } else if (info->format == GPU_TEXTURE_FORMAT_D16 || info->format == GPU_TEXTURE_FORMAT_D32F) {
    texture->aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else {
    texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  bool depth = texture->aspect & VK_IMAGE_ASPECT_DEPTH_BIT;
  VkImageUsageFlags usage =
    (((info->flags & GPU_TEXTURE_FLAG_RENDER) && !depth) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0) |
    (((info->flags & GPU_TEXTURE_FLAG_RENDER) && depth) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0) |
    ((info->flags & GPU_TEXTURE_FLAG_SAMPLE) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0) |
    ((info->flags & GPU_TEXTURE_FLAG_STORAGE) ? VK_IMAGE_USAGE_STORAGE_BIT : 0) |
    ((info->flags & GPU_TEXTURE_FLAG_COPY_SRC) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0) |
    ((info->flags & GPU_TEXTURE_FLAG_COPY_DST) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0) |
    ((info->flags & GPU_TEXTURE_FLAG_TRANSIENT) ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0);

  bool array = info->type = GPU_TEXTURE_TYPE_ARRAY;
  VkImageCreateInfo imageInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = flags,
    .imageType = type,
    .format = texture->format,
    .extent.width = info->size[0],
    .extent.height = info->size[1],
    .extent.depth = array ? 1 : info->size[2],
    .mipLevels = info->mipmaps,
    .arrayLayers = array ? info->size[2] : 1,
    .samples = info->samples,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = usage
  };

  if (vkCreateImage(state.device, &imageInfo, NULL, &texture->handle)) {
    return false;
  }

  nickname(texture->handle, VK_OBJECT_TYPE_IMAGE, info->label);

  uint32_t memoryType = ~0u;
  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(state.device, texture->handle, &requirements);
  for (uint32_t i = 0; i < state.memoryProperties.memoryTypeCount; i++) {
    uint32_t flags = state.memoryProperties.memoryTypes[i].propertyFlags;
    uint32_t mask = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if ((flags & mask) == mask && (requirements.memoryTypeBits & (1 << i))) {
      memoryType = i;
      break;
    }
  }

  if (memoryType == ~0u) {
    vkDestroyImage(state.device, texture->handle, NULL);
    return false;
  }

  VkMemoryAllocateInfo memoryInfo = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = requirements.size,
    .memoryTypeIndex = memoryType
  };

  if (vkAllocateMemory(state.device, &memoryInfo, NULL, &texture->memory)) {
    vkDestroyImage(state.device, texture->handle, NULL);
    return false;
  }

  if (vkBindImageMemory(state.device, texture->handle, texture->memory, 0)) {
    vkDestroyImage(state.device, texture->handle, NULL);
    vkFreeMemory(state.device, texture->memory, NULL);
    return false;
  }

  texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;

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
    texture->format = info->source->format;
    texture->memory = VK_NULL_HANDLE;
    texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;
    texture->aspect = info->source->aspect;
    texture->samples = info->source->samples;
    texture->source = info->source;
  }

  switch (info->type) {
    case GPU_TEXTURE_TYPE_2D: texture->type = VK_IMAGE_VIEW_TYPE_2D; break;
    case GPU_TEXTURE_TYPE_3D: texture->type = VK_IMAGE_VIEW_TYPE_3D; break;
    case GPU_TEXTURE_TYPE_CUBE: texture->type = VK_IMAGE_VIEW_TYPE_CUBE; break;
    case GPU_TEXTURE_TYPE_ARRAY: texture->type = VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
    default: return false;
  }

  VkImageViewCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = info->source->handle,
    .viewType = texture->type,
    .format = texture->format,
    .subresourceRange = {
      .aspectMask = texture->aspect,
      .baseMipLevel = info ? info->levelIndex : 0,
      .levelCount = (info && info->levelCount) ? info->levelCount : VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = info ? info->layerIndex : 0,
      .layerCount = (info && info->layerCount) ? info->layerCount : VK_REMAINING_ARRAY_LAYERS
    }
  };

  GPU_VK(vkCreateImageView(state.device, &createInfo, NULL, &texture->view));

  return true;
}

void gpu_texture_destroy(gpu_texture* texture) {
  condemn(texture->handle, VK_OBJECT_TYPE_IMAGE);
  condemn(texture->memory, VK_OBJECT_TYPE_DEVICE_MEMORY);
  condemn(texture->view, VK_OBJECT_TYPE_IMAGE_VIEW);
  memset(texture, 0, sizeof(*texture));
}

void* gpu_texture_map(gpu_texture* texture, uint16_t offset[4], uint16_t extent[3]) {
  uint64_t bytes = getTextureRegionSize(texture->format, extent);
  gpu_mapping mapped = scratch(bytes);

  bool array = texture->type == VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  VkBufferImageCopy region = {
    .bufferOffset = mapped.offset,
    .imageSubresource.aspectMask = texture->aspect,
    .imageSubresource.mipLevel = offset[3],
    .imageSubresource.baseArrayLayer = array ? offset[2] : 0,
    .imageSubresource.layerCount = array ? extent[2] : 1,
    .imageOffset = { offset[0], offset[1], array ? 0 : offset[2] },
    .imageExtent = { extent[0], extent[1], array ? 1 : extent[2] }
  };

  vkCmdCopyBufferToImage(state.batch.commands, mapped.buffer, texture->handle, texture->layout, 1, &region);

  return mapped.data;
}

void gpu_texture_read(gpu_texture* texture, uint16_t offset[4], uint16_t extent[3], gpu_read_fn* fn, void* userdata) {
  uint64_t bytes = getTextureRegionSize(texture->format, extent);
  gpu_mapping mapped = scratch(bytes);

  bool array = texture->type == VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  VkBufferImageCopy region = {
    .bufferOffset = mapped.offset,
    .imageSubresource.aspectMask = texture->aspect,
    .imageSubresource.mipLevel = offset[3],
    .imageSubresource.baseArrayLayer = array ? offset[2] : 0,
    .imageSubresource.layerCount = array ? extent[2] : 1,
    .imageOffset = { offset[0], offset[1], array ? 0 : offset[2] },
    .imageExtent = { extent[0], extent[1], array ? 1 : extent[2] }
  };

  vkCmdCopyImageToBuffer(state.batch.commands, texture->handle, texture->layout, mapped.buffer, 1, &region);

  gpu_readback_pool* pool = &state.readbacks;
  GPU_CHECK(pool->head - pool->tail != COUNTOF(pool->data), "Too many GPU readbacks"); // TODO emergency flush instead of throw
  pool->data[pool->head++ & 0x7f] = (gpu_readback) {
    .fn = fn,
    .userdata = userdata,
    .data = mapped.data,
    .size = bytes,
    .tick = state.tick[CPU]
  };
}

void gpu_texture_copy(gpu_texture* src, gpu_texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t size[3]) {
  bool srcArray = src->type == GPU_TEXTURE_TYPE_ARRAY;
  bool dstArray = dst->type == GPU_TEXTURE_TYPE_ARRAY;

  VkImageCopy region = {
    .srcSubresource = {
      .aspectMask = src->aspect,
      .mipLevel = srcOffset[3],
      .baseArrayLayer = srcArray ? srcOffset[2] : 0,
      .layerCount = srcArray ? size[2] : 1
    },
    .dstSubresource = {
      .aspectMask = dst->aspect,
      .mipLevel = dstOffset[3],
      .baseArrayLayer = dstArray ? dstOffset[2] : 0,
      .layerCount = dstArray ? size[2] : 1
    },
    .srcOffset = { srcOffset[0], srcOffset[1], srcArray ? 0 : srcOffset[2] },
    .dstOffset = { dstOffset[0], dstOffset[1], dstArray ? 0 : dstOffset[2] },
    .extent = { size[0], size[1], size[2] }
  };

  vkCmdCopyImage(state.batch.commands, src->handle, src->layout, dst->handle, dst->layout, 1, &region);
}

void gpu_texture_blit(gpu_texture* src, gpu_texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t srcExtent[3], uint16_t dstExtent[3], bool nearest) {
  bool srcArray = src->type == GPU_TEXTURE_TYPE_ARRAY;
  bool dstArray = dst->type == GPU_TEXTURE_TYPE_ARRAY;

  VkImageBlit region = {
    .srcSubresource = {
      .aspectMask = src->aspect,
      .mipLevel = srcOffset[3],
      .baseArrayLayer = srcArray ? srcOffset[2] : 0,
      .layerCount = srcArray ? srcExtent[2] : 1
    },
    .dstSubresource = {
      .aspectMask = dst->aspect,
      .mipLevel = dstOffset[3],
      .baseArrayLayer = dstArray ? dstOffset[2] : 0,
      .layerCount = dstArray ? dstExtent[2] : 1
    },
    .srcOffsets[0] = { srcOffset[0], srcOffset[1], srcArray ? 0 : srcOffset[2] },
    .dstOffsets[0] = { dstOffset[0], dstOffset[1], dstArray ? 0 : dstOffset[2] },
    .srcOffsets[1] = { srcOffset[0] + srcExtent[0], srcOffset[1] + srcExtent[1], srcArray ? 1 : srcOffset[2] + srcExtent[2] },
    .dstOffsets[1] = { dstOffset[0] + dstExtent[0], dstOffset[1] + dstExtent[1], dstArray ? 1 : dstOffset[2] + dstExtent[2] }
  };

  VkFilter filter = nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
  vkCmdBlitImage(state.batch.commands, src->handle, src->layout, dst->handle, dst->layout, 1, &region, filter);
}

void gpu_texture_clear(gpu_texture* texture, uint16_t layer, uint16_t layerCount, uint16_t level, uint16_t levelCount, float color[4]) {
  VkClearColorValue clear;
  memcpy(clear.float32, color, 4 * sizeof(float));

  // 3D textures don't support clearing individual slices
  if (texture->type == GPU_TEXTURE_TYPE_ARRAY) {
    layer = 0;
    layerCount = 1;
  }

  VkImageSubresourceRange range = {
    .aspectMask = texture->aspect,
    .baseMipLevel = level,
    .levelCount = levelCount,
    .baseArrayLayer = layer,
    .layerCount = layerCount
  };

  vkCmdClearColorImage(state.batch.commands, texture->handle, texture->layout, &clear, 1, &range);
}

// Sampler

size_t gpu_sizeof_sampler() {
  return sizeof(gpu_sampler);
}

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
    .maxLod = info->lodClamp[1]
  };

  if (vkCreateSampler(state.device, &samplerInfo, NULL, &sampler->handle)) {
    return false;
  }

  return true;
}

void gpu_sampler_destroy(gpu_sampler* sampler) {
  condemn(sampler->handle, VK_OBJECT_TYPE_SAMPLER);
  memset(sampler, 0, sizeof(*sampler));
}

// Shader

size_t gpu_sizeof_shader() {
  return sizeof(gpu_shader);
}

bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info) {
  memset(shader, 0, sizeof(*shader));

  if (info->compute.code) {
    shader->type = VK_PIPELINE_BIND_POINT_COMPUTE;

    VkShaderModuleCreateInfo moduleInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = info->compute.size,
      .pCode = info->compute.code
    };

    if (vkCreateShaderModule(state.device, &moduleInfo, NULL, &shader->handles[0])) {
      return false;
    }

    shader->pipelineInfo[0] = (VkPipelineShaderStageCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = shader->handles[0],
      .pName = info->compute.entry ? info->compute.entry : "main"
    };
  } else {
    shader->type = VK_PIPELINE_BIND_POINT_GRAPHICS;

    for (uint32_t i = 0; i < 2; i++) {
      gpu_shader_source* source = i == 0 ? &info->vertex : &info->fragment;
      VkShaderStageFlags stage = i == 0 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;

      VkShaderModuleCreateInfo moduleInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = source->size,
        .pCode = source->code
      };

      if (vkCreateShaderModule(state.device, &moduleInfo, NULL, &shader->handles[i])) {
        return false;
      }

      shader->pipelineInfo[i] = (VkPipelineShaderStageCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = stage,
        .module = shader->handles[i],
        .pName = source->entry ? source->entry : "main"
      };
    }
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

    GPU_VK(vkCreateDescriptorSetLayout(state.device, &layoutInfo, NULL, &shader->layouts[i]));

    if (info->slotCount[i] > 0) {
      pipelineLayoutInfo.setLayoutCount = i + 1;
    }
  }

  GPU_VK(vkCreatePipelineLayout(state.device, &pipelineLayoutInfo, NULL, &shader->pipelineLayout));

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
  memset(shader, 0, sizeof(*shader));
}

// Bundle

size_t gpu_sizeof_bundle() {
  return sizeof(gpu_bundle);
}

bool gpu_bundle_init(gpu_bundle* bundle, gpu_bundle_info* info) {
  VkDescriptorSetAllocateInfo allocateInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = VK_NULL_HANDLE,
    .descriptorSetCount = 1,
    .pSetLayouts = &info->shader->layouts[info->group]
  };

  // Try to allocate bindings from the current pool
  gpu_binding_pool* pool = &state.bindings;

  if (pool->head != ~0u) {
    allocateInfo.descriptorPool = pool->data[pool->head].handle;

    VkResult result = vkAllocateDescriptorSets(state.device, &allocateInfo, &bundle->handle);

    if (result != VK_SUCCESS && result != VK_ERROR_OUT_OF_POOL_MEMORY) {
      return false; // XXX
    }
  }

  // If that didn't work, there either wasn't a puddle available or the current one overflowed.
  if (!bundle->handle) {
    gpu_binding_puddle* puddle = NULL;
    uint32_t index = ~0u;

    // Make sure we have an up to date GPU tick before trying to reuse a pool
    ketchup();

    // If there's a crusty old puddle laying around that isn't even being used by anyone, use it
    if (pool->tail != ~0u && state.tick[GPU] >= pool->data[pool->tail].tick) {
      index = pool->tail;
      puddle = &pool->data[index];
      pool->tail = puddle->next;
      vkResetDescriptorPool(state.device, puddle->handle, 0);
    } else {
      // Otherwise, the pool is totally out of space, it needs a new puddle.
      GPU_CHECK(pool->count < COUNTOF(pool->data), "GPU binding pool overflow");
      index = pool->count++;
      puddle = &pool->data[index];

      // TODO dynamically adjust sizes
      VkDescriptorPoolSize sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 64 },
      };

      VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1024,
        .poolSizeCount = COUNTOF(sizes),
        .pPoolSizes = sizes
      };

      if (vkCreateDescriptorPool(state.device, &descriptorPoolInfo, NULL, &puddle->handle)) {
        return false; // XXX
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
    if (vkAllocateDescriptorSets(state.device, &allocateInfo, &bundle->handle)) {
      return false;
    }
  }

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
  memset(bundle, 0, sizeof(*bundle));
}

// Pass

size_t gpu_sizeof_pass() {
  return sizeof(gpu_pass);
}

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
      .format = convertFormat(info->color[i].format, info->color[i].srgb),
      .samples = info->samples,
      .loadOp = loadOps[info->color[i].load],
      .storeOp = info->resolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : storeOps[info->color[i].save],
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
  }

  if (info->resolve) {
    for (uint32_t i = 0; i < info->count; i++) {
      uint32_t index = info->count + i;
      references[index].attachment = index;
      references[index].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      attachments[index] = (VkAttachmentDescription) {
        .format = convertFormat(info->color[i].format, info->color[i].srgb),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = storeOps[info->color[i].save],
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
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
      .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
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
    .pNext = info->views > 1 ? &multiview : NULL,
    .attachmentCount = (info->count << info->resolve) + !!info->depth.format,
    .pAttachments = attachments,
    .subpassCount = 1,
    .pSubpasses = &subpass
  };

  if (vkCreateRenderPass(state.device, &createInfo, NULL, &pass->handle)) {
    return false;
  }

  nickname(pass, VK_OBJECT_TYPE_RENDER_PASS, info->label);
  pass->count = info->count;
  pass->samples = info->samples;
  pass->views = info->views;
  return true;
}

void gpu_pass_destroy(gpu_pass* pass) {
  condemn(pass->handle, VK_OBJECT_TYPE_RENDER_PASS);
  memset(pass, 0, sizeof(*pass));
}

// Pipeline

size_t gpu_sizeof_pipeline() {
  return sizeof(gpu_pipeline);
}

bool gpu_pipeline_init_graphics(gpu_pipeline* pipeline, gpu_pipeline_info* info) {
  static const VkPrimitiveTopology topologies[] = {
    [GPU_DRAW_POINTS] = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    [GPU_DRAW_LINES] = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    [GPU_DRAW_TRIANGLES] = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
  };

  static const VkFormat vertexFormats[] = {
    [GPU_FORMAT_NONE] = VK_FORMAT_UNDEFINED,
    [GPU_FORMAT_I8x2] = VK_FORMAT_R8G8_SINT,
    [GPU_FORMAT_I8x4] = VK_FORMAT_R8G8B8A8_SINT,
    [GPU_FORMAT_U8x2] = VK_FORMAT_R8G8_UINT,
    [GPU_FORMAT_U8x4] = VK_FORMAT_R8G8B8A8_UINT,
    [GPU_FORMAT_SN8x2] = VK_FORMAT_R8G8_SNORM,
    [GPU_FORMAT_SN8x4] = VK_FORMAT_R8G8B8A8_SNORM,
    [GPU_FORMAT_UN8x2] = VK_FORMAT_R8G8_UNORM,
    [GPU_FORMAT_UN8x4] = VK_FORMAT_R8G8B8A8_UNORM,
    [GPU_FORMAT_I16x2] = VK_FORMAT_R16G16_SINT,
    [GPU_FORMAT_I16x4] = VK_FORMAT_R16G16B16A16_SINT,
    [GPU_FORMAT_U16x2] = VK_FORMAT_R16G16_UINT,
    [GPU_FORMAT_U16x4] = VK_FORMAT_R16G16B16A16_UINT,
    [GPU_FORMAT_SN16x2] = VK_FORMAT_R16G16_SNORM,
    [GPU_FORMAT_SN16x4] = VK_FORMAT_R16G16B16A16_SNORM,
    [GPU_FORMAT_UN16x2] = VK_FORMAT_R16G16_UNORM,
    [GPU_FORMAT_UN16x4] = VK_FORMAT_R16G16B16A16_UNORM,
    [GPU_FORMAT_F16x2] = VK_FORMAT_R16G16_SFLOAT,
    [GPU_FORMAT_F16x4] = VK_FORMAT_R16G16B16A16_SFLOAT,
    [GPU_FORMAT_F32x1] = VK_FORMAT_R32_SFLOAT,
    [GPU_FORMAT_F32x2] = VK_FORMAT_R32G32_SFLOAT,
    [GPU_FORMAT_F32x3] = VK_FORMAT_R32G32B32_SFLOAT,
    [GPU_FORMAT_F32x4] = VK_FORMAT_R32G32B32A32_SFLOAT,
    [GPU_FORMAT_I32x1] = VK_FORMAT_R32_SINT,
    [GPU_FORMAT_I32x2] = VK_FORMAT_R32G32_SINT,
    [GPU_FORMAT_I32x3] = VK_FORMAT_R32G32B32_SINT,
    [GPU_FORMAT_I32x4] = VK_FORMAT_R32G32B32A32_SINT,
    [GPU_FORMAT_U32x1] = VK_FORMAT_R32_UINT,
    [GPU_FORMAT_U32x2] = VK_FORMAT_R32G32_UINT,
    [GPU_FORMAT_U32x3] = VK_FORMAT_R32G32B32_UINT,
    [GPU_FORMAT_U32x4] = VK_FORMAT_R32G32B32A32_UINT
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

  uint32_t vertexAttributeCount = 0;
  VkVertexInputAttributeDescription vertexAttributes[COUNTOF(info->attributes)];
  for (uint32_t i = 0; i < COUNTOF(info->attributes) && info->attributes[i].format != GPU_FORMAT_NONE; i++, vertexAttributeCount++) {
    vertexAttributes[vertexAttributeCount++] = (VkVertexInputAttributeDescription) {
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
    .vertexAttributeDescriptionCount = vertexAttributeCount,
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
    VkColorComponentFlagBits colorMask = info->colorMask[i];

    if (info->colorMask[i] == GPU_COLOR_MASK_RGBA) {
      colorMask = 0xf;
    } else if (info->colorMask[i] == GPU_COLOR_MASK_NONE) {
      colorMask = 0x0;
    }

    colorAttachments[i] = (VkPipelineColorBlendAttachmentState) {
      .blendEnable = info->blend[i].enabled,
      .srcColorBlendFactor = blendFactors[info->blend[i].color.src],
      .dstColorBlendFactor = blendFactors[info->blend[i].color.dst],
      .colorBlendOp = blendOps[info->blend[i].color.op],
      .srcAlphaBlendFactor = blendFactors[info->blend[i].alpha.src],
      .dstAlphaBlendFactor = blendFactors[info->blend[i].alpha.dst],
      .alphaBlendOp = blendOps[info->blend[i].alpha.op],
      .colorWriteMask = colorMask
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

  if (vkCreateGraphicsPipelines(state.device, state.pipelineCache, 1, &pipelineInfo, NULL, &pipeline->handle)) {
    return false;
  }

  nickname(pipeline, VK_OBJECT_TYPE_PIPELINE, info->label);
  pipeline->type = VK_PIPELINE_BIND_POINT_GRAPHICS;
  return true;
}

bool gpu_pipeline_init_compute(gpu_pipeline* pipeline, gpu_shader* shader, const char* label) {
  VkComputePipelineCreateInfo pipelineInfo = {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage = shader->pipelineInfo[0],
    .layout = shader->pipelineLayout
  };

  if (vkCreateComputePipelines(state.device, state.pipelineCache, 1, &pipelineInfo, NULL, &pipeline->handle)) {
    return false;
  }

  nickname(pipeline, VK_OBJECT_TYPE_PIPELINE, label);
  pipeline->type = VK_PIPELINE_BIND_POINT_COMPUTE;
  return true;
}

void gpu_pipeline_destroy(gpu_pipeline* pipeline) {
  condemn(pipeline->handle, VK_OBJECT_TYPE_PIPELINE);
}

// Batch

gpu_batch* gpu_begin_render(gpu_canvas* canvas) {
  bool resolve = canvas->pass->samples > 1 && canvas->color[0].resolve;

  // Framebuffer
  VkClearValue clears[9];
  VkImageView textures[9];
  uint32_t count = 0;

  for (uint32_t i = 0; i < COUNTOF(canvas->color) && canvas->color[i].texture; i++, count++) {
    memcpy(&clears[i].color.float32, canvas->color[i].clear, 4 * sizeof(float));
    textures[i] = canvas->color[i].texture->view;
  }

  if (resolve) {
    for (uint32_t i = 0; i < count; i++) {
      textures[count + i] = canvas->color[i].resolve->view;
    }
  }

  if (canvas->depth.texture) {
    uint32_t index = count << resolve;
    clears[index].depthStencil.depth = canvas->depth.clear.depth;
    clears[index].depthStencil.stencil = canvas->depth.clear.stencil;
    textures[index] = canvas->depth.texture->view;
  }

  uint32_t hash = HASH_SEED;
  hash32(&hash, textures, (count << resolve) + !!canvas->depth.texture);
  hash32(&hash, &canvas->pass->handle, sizeof(canvas->pass->handle));
  hash32(&hash, canvas->size, 2 * sizeof(uint32_t));

  VkFramebuffer framebuffer;
  uint32_t rows = COUNTOF(state.framebuffers);
  uint32_t cols = COUNTOF(state.framebuffers[0]);
  gpu_cache_entry* entry = cacheFetch(state.framebuffers[0], rows, cols, hash);

  if (!entry) {
    VkFramebufferCreateInfo framebufferInfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = canvas->pass->handle,
      .attachmentCount = (count << resolve) + !!canvas->depth.texture,
      .pAttachments = textures,
      .width = canvas->size[0],
      .height = canvas->size[1],
      .layers = 1
    };

    entry = cacheEvict(state.framebuffers[0], rows, cols, hash);
    condemn(entry->object, VK_OBJECT_TYPE_FRAMEBUFFER);
    GPU_VK(vkCreateFramebuffer(state.device, &framebufferInfo, NULL, (VkFramebuffer*) &entry->object));
    entry->hash = hash;
  }

  framebuffer = entry->object;
  entry->tick = state.tick[CPU];

  // Begin!
  VkRenderPassBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = canvas->pass->handle,
    .framebuffer = framebuffer,
    .renderArea = { { 0, 0 }, { canvas->size[0], canvas->size[1] } },
    .clearValueCount = COUNTOF(clears),
    .pClearValues = clears
  };

  gpu_batch* batch = begin(true);
  vkCmdBeginRenderPass(batch->commands, &beginfo, VK_SUBPASS_CONTENTS_INLINE);
  return batch;
}

gpu_batch* gpu_begin_compute() {
  return begin(false);
}

void gpu_batch_end(gpu_batch* batch) {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0x3];

  uint32_t index = tick->batchCount++;

  for (;;) {
    if (index >= COUNTOF(tick->batch)) return;
    if (tick->batch[index].commands == batch->commands) break;
    index++;
  }

  if (tick->renderPassMask & (1 << index)) {
    vkCmdEndRenderPass(batch->commands);
  }

  if (index != tick->batchCount) {
    VkCommandBuffer tmp = tick->batch[tick->batchCount].commands;
    tick->batch[tick->batchCount].commands = batch->commands;
    tick->batch[index].commands = tmp;
  }

  GPU_VK(vkEndCommandBuffer(state.batch.commands));
  state.batch = *batch;
}

void gpu_batch_bind_pipeline(gpu_batch* batch, gpu_pipeline* pipeline) {
  vkCmdBindPipeline(batch->commands, pipeline->type, pipeline->handle);
}

void gpu_batch_bind_bundle(gpu_batch* batch, gpu_shader* shader, uint32_t group, gpu_bundle* bundle, uint32_t* offsets, uint32_t offsetCount) {
  vkCmdBindDescriptorSets(batch->commands, shader->type, shader->pipelineLayout, group, 1, &bundle->handle, offsetCount, offsets);
}

void gpu_batch_bind_vertex_buffers(gpu_batch* batch, gpu_buffer** buffers, uint64_t* offsets, uint32_t count) {
  VkBuffer handles[8];
  for (uint32_t i = 0; i < count; i++) {
    handles[i] = buffers[i]->handle;
  }
  vkCmdBindVertexBuffers(batch->commands, 0, count, handles, offsets);
}

void gpu_batch_bind_index_buffer(gpu_batch* batch, gpu_buffer* buffer, uint64_t offset, gpu_index_type type) {
  static const VkIndexType types[] = {
    [GPU_INDEX_U16] = VK_INDEX_TYPE_UINT16,
    [GPU_INDEX_U32] = VK_INDEX_TYPE_UINT32
  };

  vkCmdBindIndexBuffer(batch->commands, buffer->handle, offset, types[type]);
}

void gpu_batch_draw(gpu_batch* batch, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) {
  vkCmdDraw(batch->commands, vertexCount, instanceCount, firstVertex, 0);
}

void gpu_batch_draw_indexed(gpu_batch* batch, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex) {
  vkCmdDrawIndexed(batch->commands, indexCount, instanceCount, firstIndex, baseVertex, 0);
}

void gpu_batch_draw_indirect(gpu_batch* batch, gpu_buffer* buffer, uint64_t offset, uint32_t drawCount) {
  vkCmdDrawIndirect(batch->commands, buffer->handle, offset, drawCount, 16);
}

void gpu_batch_draw_indirect_indexed(gpu_batch* batch, gpu_buffer* buffer, uint64_t offset, uint32_t drawCount) {
  vkCmdDrawIndexedIndirect(batch->commands, buffer->handle, offset, drawCount, 20);
}

void gpu_batch_compute(gpu_batch* batch, gpu_shader* shader, uint32_t x, uint32_t y, uint32_t z) {
  vkCmdDispatch(batch->commands, x, y, z);
}

void gpu_batch_compute_indirect(gpu_batch* batch, gpu_shader* shader, gpu_buffer* buffer, uint64_t offset) {
  vkCmdDispatchIndirect(batch->commands, buffer->handle, offset);
}

// Surface

gpu_texture* gpu_surface_acquire() {
  if (!state.swapchain || state.currentBackbuffer != ~0u) {
    return NULL;
  }

  gpu_begin();
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0x3];
  GPU_VK(vkAcquireNextImageKHR(state.device, state.swapchain, UINT64_MAX, tick->waitSemaphore, VK_NULL_HANDLE, &state.currentBackbuffer));
  tick->wait = true;

  // Transition backbuffer to general layout (TODO autosync instead)
  gpu_texture* texture = &state.backbuffers[state.currentBackbuffer];
  VkPipelineStageFlags src = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkPipelineStageFlags dst = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = texture->handle,
    .subresourceRange = { texture->aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS }
  };

  vkCmdPipelineBarrier(state.batch.commands, src, dst, 0, 0, NULL, 0, NULL, 1, &barrier);
  texture->layout = VK_IMAGE_LAYOUT_GENERAL;
  gpu_flush();

  return texture;
}

void gpu_surface_present() {
  if (!state.swapchain || state.currentBackbuffer == ~0u) {
    return;
  }

  gpu_begin();
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0x3];
  tick->tell = true;

  // Transition backbuffer to general layout (TODO autosync instead)
  gpu_texture* texture = &state.backbuffers[state.currentBackbuffer];
  VkPipelineStageFlags src = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkPipelineStageFlags dst = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = texture->handle,
    .subresourceRange = { texture->aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS }
  };

  vkCmdPipelineBarrier(state.batch.commands, src, dst, 0, 0, NULL, 0, NULL, 1, &barrier);
  texture->layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  gpu_flush();

  VkPresentInfoKHR info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &tick->tellSemaphore,
    .swapchainCount = 1,
    .pSwapchains = &state.swapchain,
    .pImageIndices = &state.currentBackbuffer
  };

  GPU_VK(vkQueuePresentKHR(state.queue, &info));
  state.currentBackbuffer = ~0u;
}

// Helpers

static void hash32(uint32_t* hash, void* data, uint32_t size) {
  const uint8_t* bytes = data;
  for (uint32_t i = 0; i < size; i++) {
    *hash = (*hash ^ bytes[i]) * 16777619;
  }
}

static gpu_cache_entry* cacheFetch(gpu_cache_entry* cache, uint32_t rows, uint32_t cols, uint32_t hash) {
  uint32_t row = hash & (rows - 1);
  gpu_cache_entry* entries = cache + row * cols;
  for (uint32_t i = 0; i < cols; i++) {
    if (entries[i].hash == hash) {
      return &entries[i];
    }
  }
  return NULL;
}

static gpu_cache_entry* cacheEvict(gpu_cache_entry* cache, uint32_t rows, uint32_t cols, uint32_t hash) {
  uint32_t row = hash & (rows - 1);
  gpu_cache_entry* entries = cache + row * cols;
  for (uint32_t i = 0; i < cols; i++) {
    if (!entries[i].object) {
      return &entries[i];
    }
  }
  gpu_cache_entry* oldest = entries;
  for (uint32_t i = 1; i < cols; i++) {
    if (entries[i].tick < oldest->tick) {
      oldest = &entries[i];
    }
  }
  return oldest;
}

#define SCRATCHPAD_SIZE (16 * 1024 * 1024)
static gpu_mapping scratch(uint32_t size) {
  gpu_scratchpad_pool* pool = &state.scratchpads;
  gpu_scratchpad* scratchpad;
  uint32_t index;

  // If there's an active scratchpad and it has enough space, just use that
  if (pool->head != ~0u && pool->cursor + size <= SCRATCHPAD_SIZE) {
    index = pool->head;
    scratchpad = &pool->data[index];
  } else {
    // Otherwise, see if it's possible to reuse the oldest existing scratch buffer
    GPU_CHECK(size <= SCRATCHPAD_SIZE, "Tried to map too much scratch memory");
    ketchup();
    if (pool->tail != ~0u && state.tick[GPU] >= pool->data[pool->tail].tick) {
      index = pool->tail;
      scratchpad = &pool->data[index];
      pool->tail = scratchpad->next;
    } else {
      // As a last resort, allocate a new scratchpad
      GPU_CHECK(pool->count < COUNTOF(pool->data), "GPU scratchpad pool overflow");
      index = pool->count++;
      scratchpad = &pool->data[index];

      VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = SCRATCHPAD_SIZE,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
      };

      GPU_VK(vkCreateBuffer(state.device, &bufferInfo, NULL, &scratchpad->buffer));

      // Cache memory requirements if needed
      if (state.scratchMemoryRequirements.size == 0) {
        state.scratchMemoryType = ~0u;
        vkGetBufferMemoryRequirements(state.device, scratchpad->buffer, &state.scratchMemoryRequirements);
        for (uint32_t i = 0; i < state.memoryProperties.memoryTypeCount; i++) {
          uint32_t flags = state.memoryProperties.memoryTypes[i].propertyFlags;
          uint32_t mask = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
          if ((flags & mask) == mask && (state.scratchMemoryRequirements.memoryTypeBits & (1 << i))) {
            state.scratchMemoryType = i;
            break;
          }
        }

        if (state.scratchMemoryType == ~0u) {
          vkDestroyBuffer(state.device, scratchpad->buffer, NULL);
          return (gpu_mapping) { .data = NULL };
        }
      }

      VkMemoryAllocateInfo memoryInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = state.scratchMemoryRequirements.size,
        .memoryTypeIndex = state.scratchMemoryType
      };

      if (vkAllocateMemory(state.device, &memoryInfo, NULL, &scratchpad->memory)) {
        vkDestroyBuffer(state.device, scratchpad->buffer, NULL);
        return (gpu_mapping) { .data = NULL };
      }

      if (vkBindBufferMemory(state.device, scratchpad->buffer, scratchpad->memory, 0)) {
        vkDestroyBuffer(state.device, scratchpad->buffer, NULL);
        vkFreeMemory(state.device, scratchpad->memory, NULL);
        return (gpu_mapping) { .data = NULL };
      }

      if (vkMapMemory(state.device, scratchpad->memory, 0, VK_WHOLE_SIZE, 0, &scratchpad->data)) {
        vkDestroyBuffer(state.device, scratchpad->buffer, NULL);
        vkFreeMemory(state.device, scratchpad->memory, NULL);
        return (gpu_mapping) { .data = NULL };
      }
    }

    if (pool->tail == ~0u) {
      pool->tail = index;
    }

    if (pool->head != ~0u) {
      pool->data[pool->head].next = index;
    }

    pool->cursor = 0;
    pool->head = index;
    scratchpad->next = ~0u;
  }

  gpu_mapping mapping = {
    .buffer = scratchpad->buffer,
    .data = (uint8_t*) scratchpad->data + pool->cursor,
    .offset = pool->cursor
  };

  scratchpad->tick = state.tick[CPU];
  pool->cursor += size;

  return mapping;
}

static void ketchup() {
  while (state.tick[GPU] + 1 < state.tick[CPU]) {
    gpu_tick* next = &state.ticks[(state.tick[GPU] + 1) & 0x3];
    VkResult result = vkGetFenceStatus(state.device, next->fence);
    switch (result) {
      case VK_SUCCESS: state.tick[GPU]++; continue;
      case VK_NOT_READY: return;
      default: GPU_THROW(getErrorString(result)); return;
    }
  }
}

static void stall(uint32_t until) {
  gpu_tick* tick = &state.ticks[until & 0x3];
  GPU_VK(vkWaitForFences(state.device, 1, &tick->fence, VK_FALSE, ~0ull));
  state.tick[GPU] = until;
}

static void readback() {
  gpu_readback_pool* pool = &state.readbacks;
  while (pool->tail != pool->head && state.tick[GPU] >= pool->data[pool->tail & 0x7f].tick) {
    gpu_readback* readback = &pool->data[pool->tail++ & 0x7f];
    readback->fn(readback->data, readback->size, readback->userdata);
  }
}

static void condemn(void* handle, VkObjectType type) {
  if (!handle) return;
  gpu_morgue* morgue = &state.morgue;
  gpu_mutex_lock(&morgue->lock);
  GPU_CHECK(morgue->head - morgue->tail != COUNTOF(morgue->data), "GPU morgue overflow"); // TODO ketchup and expunge if morgue is full
  morgue->data[morgue->head++ & 0xff] = (gpu_ref) { handle, type, state.tick[CPU] };
  gpu_mutex_unlock(&morgue->lock);
}

static void expunge() {
  gpu_morgue* morgue = &state.morgue;
  gpu_mutex_lock(&morgue->lock);
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
      default: GPU_THROW("Unreachable"); break;
    }
  }
  gpu_mutex_unlock(&morgue->lock);
}

static gpu_batch* begin(bool render) {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0x3];
  GPU_CHECK(tick->batchCount < COUNTOF(tick->batch), "Too many batches");

  uint32_t index = tick->batchCount++; // TODO not batch count, need 2 counts
  tick->renderPassMask = render ? (tick->renderPassMask | (1 << index)) : (tick->renderPassMask & ~(1 << index));
  gpu_batch* batch = &tick->batch[index];

  VkCommandBufferBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };

  GPU_VK(vkBeginCommandBuffer(batch->commands, &beginfo));
  return batch;
}

static VkFormat convertFormat(gpu_texture_format format, int colorspace) {
  static const VkFormat formats[][2] = {
    [GPU_TEXTURE_FORMAT_R8] = { VK_FORMAT_R8_UNORM, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_RG8] = { VK_FORMAT_R8G8_UNORM, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_RGBA8] = { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB },
    [GPU_TEXTURE_FORMAT_R16] = { VK_FORMAT_R16_UNORM, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_RG16] = { VK_FORMAT_R16G16_UNORM, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_RGBA16] = { VK_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_R16F] = { VK_FORMAT_R16_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_RG16F] = { VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_RGBA16F] = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_R32F] = { VK_FORMAT_R32_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_RG32F] = { VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_RGBA32F] = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_RGB565] = { VK_FORMAT_R5G6B5_UNORM_PACK16, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_RGB5A1] = { VK_FORMAT_R5G5B5A1_UNORM_PACK16, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_RGB10A2] = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_RG11B10F] = { VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_D16] = { VK_FORMAT_D16_UNORM, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_D24S8] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_D32F] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_BC6] = { VK_FORMAT_BC6H_SFLOAT_BLOCK, VK_FORMAT_UNDEFINED },
    [GPU_TEXTURE_FORMAT_BC7] = { VK_FORMAT_BC7_UNORM_BLOCK, VK_FORMAT_BC7_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_4x4] = { VK_FORMAT_ASTC_4x4_UNORM_BLOCK, VK_FORMAT_ASTC_4x4_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_5x4] = { VK_FORMAT_ASTC_5x4_UNORM_BLOCK, VK_FORMAT_ASTC_5x4_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_5x5] = { VK_FORMAT_ASTC_5x5_UNORM_BLOCK, VK_FORMAT_ASTC_5x5_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_6x5] = { VK_FORMAT_ASTC_6x5_UNORM_BLOCK, VK_FORMAT_ASTC_6x5_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_6x6] = { VK_FORMAT_ASTC_6x6_UNORM_BLOCK, VK_FORMAT_ASTC_6x6_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_8x5] = { VK_FORMAT_ASTC_8x5_UNORM_BLOCK, VK_FORMAT_ASTC_8x5_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_8x6] = { VK_FORMAT_ASTC_8x6_UNORM_BLOCK, VK_FORMAT_ASTC_8x6_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_8x8] = { VK_FORMAT_ASTC_8x8_UNORM_BLOCK, VK_FORMAT_ASTC_8x8_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_10x5] = { VK_FORMAT_ASTC_10x5_UNORM_BLOCK, VK_FORMAT_ASTC_10x5_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_10x6] = { VK_FORMAT_ASTC_10x6_UNORM_BLOCK, VK_FORMAT_ASTC_10x6_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_10x8] = { VK_FORMAT_ASTC_10x8_UNORM_BLOCK, VK_FORMAT_ASTC_10x8_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_10x10] = { VK_FORMAT_ASTC_10x10_UNORM_BLOCK, VK_FORMAT_ASTC_10x10_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_12x10] = { VK_FORMAT_ASTC_12x10_UNORM_BLOCK, VK_FORMAT_ASTC_12x10_SRGB_BLOCK },
    [GPU_TEXTURE_FORMAT_ASTC_12x12] = { VK_FORMAT_ASTC_12x12_UNORM_BLOCK, VK_FORMAT_ASTC_12x12_SRGB_BLOCK }
  };

  return formats[format][colorspace];
}

static uint64_t getTextureRegionSize(VkFormat format, uint16_t extent[3]) {
  switch (format) {
    case VK_FORMAT_R8_UNORM:
      return extent[0] * extent[1] * extent[2];
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_D16_UNORM:
      return extent[0] * extent[1] * extent[2] * 2;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT:
      return extent[0] * extent[1] * extent[2] * 4;
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32_SFLOAT:
      return extent[0] * extent[1] * extent[2] * 8;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
      return extent[0] * extent[1] * extent[2] * 16;
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return ((extent[0] + 3) / 4) * ((extent[1] + 3) / 4) * extent[2] * 16;
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return ((extent[0] + 4) / 5) * ((extent[1] + 3) / 4) * extent[2] * 16;
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return ((extent[0] + 4) / 5) * ((extent[1] + 4) / 5) * extent[2] * 16;
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return ((extent[0] + 5) / 6) * ((extent[1] + 4) / 5) * extent[2] * 16;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return ((extent[0] + 5) / 6) * ((extent[1] + 5) / 6) * extent[2] * 16;
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return ((extent[0] + 7) / 8) * ((extent[1] + 4) / 5) * extent[2] * 16;
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return ((extent[0] + 7) / 8) * ((extent[1] + 5) / 6) * extent[2] * 16;
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return ((extent[0] + 7) / 8) * ((extent[1] + 7) / 8) * extent[2] * 16;
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return ((extent[0] + 9) / 10) * ((extent[1] + 4) / 5) * extent[2] * 16;
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return ((extent[0] + 9) / 10) * ((extent[1] + 5) / 6) * extent[2] * 16;
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return ((extent[0] + 9) / 10) * ((extent[1] + 7) / 8) * extent[2] * 16;
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return ((extent[0] + 9) / 10) * ((extent[1] + 9) / 10) * extent[2] * 16;
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return ((extent[0] + 11) / 12) * ((extent[1] + 9) / 10) * extent[2] * 16;
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return ((extent[0] + 11) / 12) * ((extent[1] + 11) / 12) * extent[2] * 16;
    default: return 0;
  }
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

    GPU_VK(vkSetDebugUtilsObjectNameEXT(state.device, &info));
  }
}

static VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userdata) {
  if (state.config.callback) {
    bool severe = severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    state.config.callback(state.config.userdata, data->pMessage, severe);
  }

  return VK_FALSE;
}

static const char* getErrorString(VkResult result) {
  switch (result) {
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "Out of CPU memory";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "Out of GPU memory";
    case VK_ERROR_MEMORY_MAP_FAILED: return "Could not map memory";
    case VK_ERROR_DEVICE_LOST: return "Lost connection to GPU";
    case VK_ERROR_TOO_MANY_OBJECTS: return "Too many objects";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "Unsupported format";
    default: return NULL;
  }
}

#ifdef _WIN32
static void gpu_mutex_init(gpu_mutex* mutex) { InitializeCriticalSection(mutex); }
static void gpu_mutex_destroy(gpu_mutex* mutex) { DeleteCriticalSection(mutex); }
static void gpu_mutex_lock(gpu_mutex* mutex) { EnterCriticalSection(mutex); }
static void gpu_mutex_unlock(gpu_mutex* mutex) { LeaveCriticalSection(mutex); }
static void* gpu_dlopen(const char* library) { return LoadLibraryA(library); }
static FARPROC gpu_dlsym(void* library, const char* symbol) { return GetProcAddress(library, symbol); }
static void gpu_dlclose(void* library) { FreeLibrary(library); }
#else
static void gpu_mutex_init(gpu_mutex* mutex) { pthread_mutex_init(mutex, NULL); }
static void gpu_mutex_destroy(gpu_mutex* mutex) { pthread_mutex_destroy(mutex); }
static void gpu_mutex_lock(gpu_mutex* mutex) { pthread_mutex_lock(mutex); }
static void gpu_mutex_unlock(gpu_mutex* mutex) { pthread_mutex_unlock(mutex); }
static void* gpu_dlopen(const char* library) { return dlopen(library, RTLD_NOW | RTLD_LOCAL); }
static void* gpu_dlsym(void* library, const char* symbol) { return dlsym(library, symbol); }
static void gpu_dlclose(void* library) { dlclose(library); }
#endif

/*
Comments
## General
- Vulkan 1.1 is targeted.
- The first physical device is used.
- There's almost no error handling.  The intent is to use validation layers or handle the errors at
  a higher level if needed, so they don't need to be duplicated for each backend.
- There is no dynamic memory allocation in this layer.  All state is in ~24kb of static bss data.
- Writing to storage resources from vertex/fragment shaders is currently not allowed.
- Texel buffers are not supported.
- ~0u aka -1 is commonly used as a "nil" value.
## Threads
- Currently, the only thread safe operation is destruction of objects.
- Multithreaded render passes are planned.
## Ticks
- There is no concept of a frame.  There are ticks.  Every sequence of commands between gpu_begin
  and gpu_flush is a tick.  We keep track of the current CPU tick (the tick being recorded) and the
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
- gpu_begin opens an initial command buffer.  Assuming no pass is active, the trailing command
  buffer in the tick is always open.  It gets closed during gpu_flush.
- If you run out of command buffers, the current recommendation is to split the workload into
  multiple queue submissions.  In the future it may be possible to support an unbounded number of
  command buffers using dynamic memory allocation.
## Buffers
- Mappable buffers are double buffered, each tick uses one region, stalling if needed.
- Memory type strategy:
  - Non-mappable buffers just want device local memory (could be picky about which kind, but meh)
  - Buffers that preserve their contents really want cached memory since they copy old data when
    switching to a new region, which counts as reading from a mapped pointer.  using uncached memory
    is acceptable, but those reads will be slow.
  - Transient buffers prefer the special 256MB memory block on AMD/NV that's both device local +
    host visible, since they're meant for temp per-frame vertex/uniform streams.  Falling back to
    regular uncached host visible memory is perfectly fine, though.
  - Host-visible buffers always use coherent memory.
*/
