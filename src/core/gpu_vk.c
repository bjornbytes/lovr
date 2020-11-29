#include "gpu.h"
#include <string.h>

#if defined __linux__
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#ifdef _WIN32
#include <windows.h>
#define VULKAN_LIBRARY "vulkan-1.dll"
#define __thread __declspec(thread)
typedef CRITICAL_SECTION gpu_mutex;
static void gpu_mutex_init(gpu_mutex* mutex) { InitializeCriticalSection(mutex); }
static void gpu_mutex_destroy(gpu_mutex* mutex) { DeleteCriticalSection(mutex); }
static void gpu_mutex_lock(gpu_mutex* mutex) { EnterCriticalSection(mutex); }
static void gpu_mutex_unlock(gpu_mutex* mutex) { LeaveCriticalSection(mutex); }
static void* gpu_dlopen(const char* library) { return LoadLibraryA(library); }
static FARPROC gpu_dlsym(void* library, const char* symbol) { return GetProcAddress(library, symbol); }
static void gpu_dlclose(void* library) { FreeLibrary(library); }
#else
#include <dlfcn.h>
#include <pthread.h>
#define VULKAN_LIBRARY "libvulkan.so"
typedef pthread_mutex_t gpu_mutex;
static void gpu_mutex_init(gpu_mutex* mutex) { pthread_mutex_init(mutex, NULL); }
static void gpu_mutex_destroy(gpu_mutex* mutex) { pthread_mutex_destroy(mutex); }
static void gpu_mutex_lock(gpu_mutex* mutex) { pthread_mutex_lock(mutex); }
static void gpu_mutex_unlock(gpu_mutex* mutex) { pthread_mutex_unlock(mutex); }
static void* gpu_dlopen(const char* library) { return dlopen(library, RTLD_NOW | RTLD_LOCAL); }
static void* gpu_dlsym(void* library, const char* symbol) { return dlsym(library, symbol); }
static void gpu_dlclose(void* library) { dlclose(library); }
#endif

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#define QUERY_CHUNK 64
#define SCRATCHPAD_SIZE (16 * 1024 * 1024)
#define VOIDP_TO_U64(x) (((union { uint64_t u; void* p; }) { .p = x }).u)
#define GPU_THROW(s) if (state.config.callback) { state.config.callback(state.config.userdata, s, true); }
#define GPU_CHECK(c, s) if (!(c)) { GPU_THROW(s); }
#define GPU_VK(f) do { VkResult r = (f); GPU_CHECK(r >= 0, getErrorString(r)); } while (0)

// Objects

struct gpu_buffer {
  VkBuffer handle;
  VkDeviceMemory memory;
};

struct gpu_texture {
  VkImage handle;
  VkFormat format;
  VkImageView view;
  VkImageViewType type;
  VkDeviceMemory memory;
  VkImageLayout layout;
  VkImageAspectFlagBits aspect;
  VkSampleCountFlagBits samples;
  gpu_texture* source;
};

struct gpu_sampler {
  VkSampler handle;
};

struct gpu_canvas {
  VkRenderPass handle;
  VkFramebuffer framebuffer;
  VkRect2D renderArea;
  VkViewport viewport;
  uint32_t colorAttachmentCount;
  VkClearValue clears[9];
  VkSampleCountFlagBits samples;
};

struct gpu_shader {
  VkShaderModule handles[2];
  VkPipelineShaderStageCreateInfo pipelineInfo[2];
  VkDescriptorSetLayout layouts[4];
  gpu_bundle_layout layoutInfo[4];
  VkPipelineLayout pipelineLayout;
};

struct gpu_bundle {
  VkDescriptorSetLayout layout;
  VkDescriptorSet handle;
  bool immutable;
};

struct gpu_pipeline {
  VkPipeline handle;
  VkIndexType indexType;
};

struct gpu_batch {
  VkCommandBuffer cmd;
  uint32_t next;
  uint32_t tick;
};

// Stream

enum { CPU, GPU };

typedef struct {
  VkCommandBuffer commandBuffers[3];
  VkCommandPool pool;
  VkFence fence;
  struct {
    VkSemaphore wait;
    VkSemaphore tell;
  } semaphores;
  bool wait, tell;
} gpu_tick;

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
  VkCommandPool commandPool;
  gpu_batch data[256];
  uint32_t count;
  uint32_t head;
  uint32_t tail;
} gpu_batch_pool;

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

// State

static __thread gpu_batch_pool batches;

static struct {
  VkInstance instance;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  VkQueue queue;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  gpu_texture backbuffers[4];
  uint32_t currentBackbuffer;
  VkDescriptorPool descriptorPool;
  VkDebugUtilsMessengerEXT messenger;
  VkPhysicalDeviceMemoryProperties memoryProperties;
  VkMemoryRequirements scratchMemoryRequirements;
  uint32_t scratchMemoryType;
  uint32_t queueFamilyIndex;
  uint32_t tick[2];
  gpu_tick ticks[16];
  gpu_morgue morgue;
  VkQueryPool queries;
  uint32_t queryCount;
  gpu_scratchpad_pool scratchpads;
  gpu_readback_pool readbacks;
  VkCommandBuffer uploads;
  VkCommandBuffer work;
  VkCommandBuffer downloads;
  gpu_features features;
  gpu_limits limits;
  gpu_config config;
  void* library;
} state;

// Helpers

typedef struct {
  VkBuffer buffer;
  uint32_t offset;
  uint8_t* data;
} gpu_mapping;

static void execute(gpu_batch** batches, uint32_t count);
static gpu_mapping scratch(uint32_t size);
static void readquack(void);
static void condemn(void* handle, VkObjectType type);
static void expunge(void);
static bool createDescriptorSetLayout(gpu_bundle_layout* layout, VkDescriptorSetLayout* handle);
static bool loadShader(gpu_shader_source* source, VkShaderStageFlagBits stage, VkShaderModule* handle, VkPipelineShaderStageCreateInfo* pipelineInfo, gpu_bundle_layout* layouts);
static uint64_t getTextureRegionSize(VkFormat format, uint16_t extent[3]);
static void nickname(void* object, VkObjectType type, const char* name);
static VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* context);
static const char* getErrorString(VkResult result);

enum { LINEAR, SRGB };
static const VkFormat textureFormats[][2] = {
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

// Maps gpu_binding_type + dynamic flag to VkDescriptorType
static const VkDescriptorType descriptorTypes[][2] = {
  [GPU_BINDING_UNIFORM_BUFFER] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC },
  [GPU_BINDING_STORAGE_BUFFER] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC },
  [GPU_BINDING_SAMPLED_TEXTURE] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ~0u },
  [GPU_BINDING_STORAGE_TEXTURE] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ~0u }
};

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
  X(vkGetPhysicalDeviceFeatures)\
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
  X(vkBeginDebugUtilsLabelEXT)\
  X(vkEndDebugUtilsLabelEXT)\
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
  X(vkWaitForFences)\
  X(vkResetFences)\
  X(vkCreateSemaphore)\
  X(vkDestroySemaphore)\
  X(vkCmdPipelineBarrier)\
  X(vkCreateQueryPool)\
  X(vkDestroyQueryPool)\
  X(vkResetQueryPool)\
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
  X(vkCmdCopyBufferToImage)\
  X(vkCmdCopyImageToBuffer)\
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
  X(vkFreeDescriptorSets)\
  X(vkUpdateDescriptorSets)\
  X(vkCreateGraphicsPipelines)\
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
  X(vkCmdDispatchIndirect)\
  X(vkCmdExecuteCommands)

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
        .messageSeverity =
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType =
          VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
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

  { // Device
    uint32_t deviceCount = 1;
    if (vkEnumeratePhysicalDevices(state.instance, &deviceCount, &state.physicalDevice) < 0) {
      gpu_destroy();
      return false;
    }

    VkQueueFamilyProperties queueFamilies[4];
    uint32_t queueFamilyCount = COUNTOF(queueFamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(state.physicalDevice, &queueFamilyCount, queueFamilies);

    state.queueFamilyIndex = ~0u;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
      VkBool32 presentable = VK_TRUE;

      if (state.surface) {
        vkGetPhysicalDeviceSurfaceSupportKHR(state.physicalDevice, i, state.surface, &presentable);
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

    vkGetPhysicalDeviceMemoryProperties(state.physicalDevice, &state.memoryProperties);

    VkDeviceQueueCreateInfo queueInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = state.queueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = &(float) { 1.f }
    };

    const char* extension = "VK_KHR_swapchain";

    VkDeviceCreateInfo deviceInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &(VkPhysicalDeviceFeatures2) {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &(VkPhysicalDeviceMultiviewFeatures) {
          .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
          .multiview = VK_TRUE
        }
      },
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queueInfo,
      .enabledExtensionCount = state.surface ? 1 : 0,
      .ppEnabledExtensionNames = &extension
    };

    if (vkCreateDevice(state.physicalDevice, &deviceInfo, NULL, &state.device)) {
      gpu_destroy();
      return false;
    }

    vkGetDeviceQueue(state.device, state.queueFamilyIndex, 0, &state.queue);

    GPU_FOREACH_DEVICE(GPU_LOAD_DEVICE);
  }

  // Swapchain
  if (state.surface) {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.physicalDevice, state.surface, &surfaceCapabilities);

    VkSurfaceFormatKHR formats[16];
    uint32_t formatCount = COUNTOF(formats);
    VkSurfaceFormatKHR surfaceFormat = { .format = VK_FORMAT_UNDEFINED };
    vkGetPhysicalDeviceSurfaceFormatsKHR(state.physicalDevice, state.surface, &formatCount, formats);

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
      .presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR, // Disables vsync, may not be supported (TODO)
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

      // TODO swizzle?
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

  // Frame resources
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

    VkCommandBufferAllocateInfo commandBufferInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = state.ticks[i].pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = COUNTOF(state.ticks[i].commandBuffers)
    };

    if (vkAllocateCommandBuffers(state.device, &commandBufferInfo, state.ticks[i].commandBuffers)) {
      gpu_destroy();
      return false;
    }

    VkSemaphoreCreateInfo semaphoreInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    if (vkCreateSemaphore(state.device, &semaphoreInfo, NULL, &state.ticks[i].semaphores.wait)) {
      gpu_destroy();
      return false;
    }

    if (vkCreateSemaphore(state.device, &semaphoreInfo, NULL, &state.ticks[i].semaphores.tell)) {
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
  }

  { // Descriptor Pool
    VkDescriptorPoolSize poolSizes[] = {
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 256 },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 256 },
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 }
    };

    VkDescriptorPoolCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 1024,
      .poolSizeCount = COUNTOF(poolSizes),
      .pPoolSizes = poolSizes
    };

    if (vkCreateDescriptorPool(state.device, &info, NULL, &state.descriptorPool)) {
      gpu_destroy();
      return false;
    }
  }

  // Query Pool
  if (state.debug) {
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
  state.tick[CPU] = COUNTOF(state.ticks);
  state.tick[GPU] = ~0u;
  state.scratchpads.head = ~0u;
  state.scratchpads.tail = ~0u;
  state.currentBackbuffer = ~0u;
  return true;
}

void gpu_destroy(void) {
  if (state.device) vkDeviceWaitIdle(state.device);
  state.tick[GPU] = state.tick[CPU];
  readquack();
  expunge();
  for (uint32_t i = 0; i < state.scratchpads.count; i++) {
    vkDestroyBuffer(state.device, state.scratchpads.data[i].buffer, NULL);
    vkFreeMemory(state.device, state.scratchpads.data[i].memory, NULL);
  }
  for (uint32_t i = 0; i < COUNTOF(state.ticks); i++) {
    gpu_tick* tick = &state.ticks[i];
    if (tick->semaphores.wait) vkDestroySemaphore(state.device, tick->semaphores.wait, NULL);
    if (tick->semaphores.tell) vkDestroySemaphore(state.device, tick->semaphores.tell, NULL);
    if (tick->fence) vkDestroyFence(state.device, tick->fence, NULL);
    if (tick->pool) vkDestroyCommandPool(state.device, tick->pool, NULL);
  }
  for (uint32_t i = 0; i < COUNTOF(state.backbuffers); i++) {
    if (state.backbuffers[i].view) vkDestroyImageView(state.device, state.backbuffers[i].view, NULL);
  }
  if (state.queryPool) vkDestroyQueryPool(state.device, state.queryPool, NULL);
  if (state.descriptorPool) vkDestroyDescriptorPool(state.device, state.descriptorPool, NULL);
  if (state.swapchain) vkDestroySwapchainKHR(state.device, state.swapchain, NULL);
  if (state.device) vkDestroyDevice(state.device, NULL);
  if (state.surface) vkDestroySurfaceKHR(state.instance, state.surface, NULL);
  if (state.messenger) vkDestroyDebugUtilsMessengerEXT(state.instance, state.messenger, NULL);
  if (state.instance) vkDestroyInstance(state.instance, NULL);
  gpu_dlclose(state.library);
  gpu_mutex_destroy(&state.morgue.lock);
  memset(&state, 0, sizeof(state));
}

void gpu_thread_attach() {
  gpu_batch_pool* pool = &batches;

  if (pool->commandPool) {
    return;
  }

  VkCommandPoolCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = state.queueFamilyIndex
  };

  GPU_VK(vkCreateCommandPool(state.device, &info, NULL, &pool->commandPool));
  pool->head = ~0u;
  pool->tail = ~0u;
}

void gpu_thread_detach() {
  vkDeviceWaitIdle(state.device);
  gpu_batch_pool* pool = &batches;
  vkDestroyCommandPool(state.device, pool->commandPool, NULL);
  memset(pool, 0, sizeof(*pool));
}

void gpu_get_features(gpu_features* features) {
  VkPhysicalDeviceFeatures deviceFeatures;
  vkGetPhysicalDeviceFeatures(state.physicalDevice, &deviceFeatures);
  features->astc = deviceFeatures.textureCompressionASTC_LDR;
  features->bptc = deviceFeatures.textureCompressionBC;

  VkFormatProperties formatProperties;
  for (uint32_t i = 0; i < COUNTOF(textureFormats); i++) {
    vkGetPhysicalDeviceFormatProperties(state.physicalDevice, textureFormats[i][LINEAR], &formatProperties);
    uint32_t blitMask = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    uint32_t flags = formatProperties.optimalTilingFeatures;
    state.features.formats[i] =
      ((flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ? GPU_FORMAT_FEATURE_SAMPLE : 0) |
      ((flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? GPU_FORMAT_FEATURE_CANVAS_COLOR : 0) |
      ((flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ? GPU_FORMAT_FEATURE_CANVAS_DEPTH : 0) |
      ((flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) ? GPU_FORMAT_FEATURE_BLEND : 0) |
      ((flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ? GPU_FORMAT_FEATURE_FILTER : 0) |
      ((flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) ? GPU_FORMAT_FEATURE_STORAGE : 0) |
      ((flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT) ? GPU_FORMAT_FEATURE_ATOMIC : 0) |
      ((flags & blitMask) == blitMask ? GPU_FORMAT_FEATURE_BLIT : 0);
  }
}

void gpu_get_limits(gpu_limits* limits) {
  VkPhysicalDeviceMaintenance3Properties maintenance3Limits = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES };
  VkPhysicalDeviceMultiviewProperties multiviewLimits = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES, .pNext = &maintenance3Limits };
  VkPhysicalDeviceProperties2 properties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &multiviewLimits };
  VkPhysicalDeviceLimits* deviceLimits = &properties2.properties.limits;
  vkGetPhysicalDeviceProperties2(state.physicalDevice, &properties2);
  limits->textureSize2D = MIN(deviceLimits->maxImageDimension2D, UINT16_MAX);
  limits->textureSize3D = MIN(deviceLimits->maxImageDimension3D, UINT16_MAX);
  limits->textureSizeCube = MIN(deviceLimits->maxImageDimensionCube, UINT16_MAX);
  limits->textureLayers = MIN(deviceLimits->maxImageArrayLayers, UINT16_MAX);
  limits->canvasSize[0] = deviceLimits->maxFramebufferWidth;
  limits->canvasSize[1] = deviceLimits->maxFramebufferHeight;
  limits->canvasViews = multiviewLimits.maxMultiviewViewCount;
  limits->bundleCount = MIN(deviceLimits->maxBoundDescriptorSets, COUNTOF(((gpu_shader*) NULL)->layouts));
  limits->bundleSlots = MIN(maintenance3Limits.maxPerSetDescriptors, COUNTOF(((gpu_bundle_info*) NULL)->bindings));
  limits->uniformBufferRange = deviceLimits->maxUniformBufferRange;
  limits->storageBufferRange = deviceLimits->maxStorageBufferRange;
  limits->uniformBufferAlign = deviceLimits->minUniformBufferOffsetAlignment;
  limits->storageBufferAlign = deviceLimits->minStorageBufferOffsetAlignment;
  limits->vertexAttributes = MIN(deviceLimits->maxVertexInputAttributes, COUNTOF(((gpu_pipeline_info*) NULL)->attributes));
  limits->vertexAttributeOffset = MIN(deviceLimits->maxVertexInputAttributeOffset, UINT8_MAX);
  limits->vertexBuffers = MIN(deviceLimits->maxVertexInputBindings, COUNTOF(((gpu_pipeline_info*) NULL)->buffers));
  limits->vertexBufferStride = MIN(deviceLimits->maxVertexInputBindingStride, UINT16_MAX);
  limits->vertexShaderOutputs = deviceLimits->maxVertexOutputComponents;
  limits->computeCount[0] = deviceLimits->maxComputeWorkGroupCount[0];
  limits->computeCount[1] = deviceLimits->maxComputeWorkGroupCount[1];
  limits->computeCount[2] = deviceLimits->maxComputeWorkGroupCount[2];
  limits->computeGroupSize[0] = deviceLimits->maxComputeWorkGroupSize[0];
  limits->computeGroupSize[1] = deviceLimits->maxComputeWorkGroupSize[1];
  limits->computeGroupSize[2] = deviceLimits->maxComputeWorkGroupSize[2];
  limits->computeGroupVolume = deviceLimits->maxComputeWorkGroupInvocations;
  limits->computeSharedMemory = deviceLimits->maxComputeSharedMemorySize;
  limits->indirectDrawCount = deviceLimits->maxDrawIndirectCount;
  limits->allocationSize = maintenance3Limits.maxMemoryAllocationSize;
  limits->pointSize[0] = deviceLimits->pointSizeRange[0];
  limits->pointSize[1] = deviceLimits->pointSizeRange[1];
  limits->anisotropy = deviceLimits->maxSamplerAnisotropy;
}

void gpu_begin() {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0xf];
  GPU_VK(vkWaitForFences(state.device, 1, &tick->fence, VK_FALSE, ~0ull));
  GPU_VK(vkResetFences(state.device, 1, &tick->fence));
  GPU_VK(vkResetCommandPool(state.device, tick->pool, 0));
  state.tick[GPU]++;
  readquack();
  expunge();

  tick->wait = tick->tell = false;
  state.uploads = tick->commandBuffers[0];
  state.work = tick->commandBuffers[1];
  state.downloads = tick->commandBuffers[2];
  for (uint32_t i = 0; i < COUNTOF(tick->commandBuffers); i++) {
    GPU_VK(vkBeginCommandBuffer(tick->commandBuffers[i], &(VkCommandBufferBeginInfo) {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    }));
  }

  if (state.config.debug) {
    uint32_t queryIndex = (state.tick[CPU] & 0xf) * QUERY_CHUNK;
    vkCmdResetQueryPool(state.uploads, state.queryPool, queryIndex, QUERY_CHUNK);
    state.queryCount = 0;
  }
}

void gpu_flush() {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0xf];

  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = tick->wait ? 1 : 0,
    .pWaitSemaphores = &tick->semaphores.wait,
    .pWaitDstStageMask = (VkPipelineStageFlags[1]) { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
    .commandBufferCount = 3,
    .pCommandBuffers = tick->commandBuffers,
    .signalSemaphoreCount = tick->tell ? 1 : 0,
    .pSignalSemaphores = &tick->semaphores.tell
  };

  for (uint32_t i = 0; i < 3; i++) {
    GPU_VK(vkEndCommandBuffer(tick->commandBuffers[i]));
  }

  GPU_VK(vkQueueSubmit(state.queue, 1, &submit, tick->fence));
  state.uploads = state.work = state.downloads = VK_NULL_HANDLE;
  state.tick[CPU]++;
}

void gpu_render(gpu_canvas* canvas, gpu_batch** batches, uint32_t count) {
  VkRenderPassBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = canvas->handle,
    .framebuffer = canvas->framebuffer,
    .renderArea = canvas->renderArea,
    .clearValueCount = COUNTOF(canvas->clears),
    .pClearValues = canvas->clears
  };

  vkCmdBeginRenderPass(state.work, &beginfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
  execute(batches, count);
  vkCmdEndRenderPass(state.work);
}

void gpu_compute(gpu_batch** batches, uint32_t count) {
  execute(batches, count);
}

void gpu_debug_push(const char* label) {
  if (state.config.debug) {
    vkCmdBeginDebugUtilsLabelEXT(state.work, &(VkDebugUtilsLabelEXT) {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pLabelName = label
    });
  }
}

void gpu_debug_pop() {
  if (state.config.debug) {
    vkCmdEndDebugUtilsLabelEXT(state.work);
  }
}

void gpu_time_write() {
  if (state.config.debug) {
    vkCmdWriteTimestamp(state.work, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, state.queryPool, state.queryCount++);
  }
}

void gpu_time_query(gpu_read_fn* fn, void* userdata) {
  if (!state.config.debug || state.queryCount == 0) {
    return;
  }

  uint32_t size = sizeof(uint64_t) * state.queryCount;
  gpu_mapping mapped = scratch(size);

  uint32_t queryIndex = (state.tick[CPU] & 0xf) * QUERY_CHUNK;
  VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT;
  vkCmdCopyQueryPoolResults(state.downloads, state.queryPool, queryIndex, state.queryCount, mapped.buffer, mapped.offset, sizeof(uint64_t), flags);

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
  memset(buffer, 0, sizeof(*buffer));

  VkBufferUsageFlags usage =
    ((info->usage & GPU_BUFFER_USAGE_VERTEX) ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : 0) |
    ((info->usage & GPU_BUFFER_USAGE_INDEX) ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT : 0) |
    ((info->usage & GPU_BUFFER_USAGE_UNIFORM) ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0) |
    ((info->usage & GPU_BUFFER_USAGE_STORAGE) ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0) |
    ((info->usage & GPU_BUFFER_USAGE_COPY) ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0) |
    ((info->usage & GPU_BUFFER_USAGE_PASTE) ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0);

  VkBufferCreateInfo bufferInfo = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = info->size,
    .usage = usage
  };

  if (vkCreateBuffer(state.device, &bufferInfo, NULL, &buffer->handle)) {
    return false;
  }

  nickname(buffer->handle, VK_OBJECT_TYPE_BUFFER, info->label);

  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(state.device, buffer->handle, &requirements);

  uint32_t memoryType = ~0u;
  for (uint32_t i = 0; i < state.memoryProperties.memoryTypeCount; i++) {
    uint32_t flags = state.memoryProperties.memoryTypes[i].propertyFlags;
    uint32_t mask = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if ((flags & mask) == mask && (requirements.memoryTypeBits & (1 << i))) {
      memoryType = i;
      break;
    }
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

  return true;
}

void gpu_buffer_destroy(gpu_buffer* buffer) {
  if (buffer->handle) condemn(buffer->handle, VK_OBJECT_TYPE_BUFFER);
  if (buffer->memory) condemn(buffer->memory, VK_OBJECT_TYPE_DEVICE_MEMORY);
  memset(buffer, 0, sizeof(*buffer));
}

void* gpu_buffer_map(gpu_buffer* buffer, uint64_t offset, uint64_t size) {
  gpu_mapping mapped = scratch(size);

  VkBufferCopy region = {
    .srcOffset = mapped.offset,
    .dstOffset = offset,
    .size = size
  };

  vkCmdCopyBuffer(state.uploads, mapped.buffer, buffer->handle, 1, &region);

  return mapped.data;
}

void gpu_buffer_read(gpu_buffer* buffer, uint64_t offset, uint64_t size, gpu_read_fn* fn, void* userdata) {
  gpu_mapping mapped = scratch(size);

  VkBufferCopy region = {
    .srcOffset = offset,
    .dstOffset = mapped.offset,
    .size = size
  };

  vkCmdCopyBuffer(state.downloads, buffer->handle, mapped.buffer, 1, &region);

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

void gpu_buffer_copy(gpu_buffer* src, gpu_buffer* dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
  VkBufferCopy region = {
    .srcOffset = srcOffset,
    .dstOffset = dstOffset,
    .size = size
  };

  vkCmdCopyBuffer(state.uploads, src->handle, dst->handle, 1, &region);
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
    case GPU_TEXTURE_TYPE_3D: type = VK_IMAGE_TYPE_3D; break;
    case GPU_TEXTURE_TYPE_CUBE: type = VK_IMAGE_TYPE_2D; flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; break;
    case GPU_TEXTURE_TYPE_ARRAY: type = VK_IMAGE_TYPE_2D; break;
    default: return false;
  }

  texture->format = textureFormats[info->format][info->srgb];
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
    (((info->usage & GPU_TEXTURE_USAGE_CANVAS) && !depth) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0) |
    (((info->usage & GPU_TEXTURE_USAGE_CANVAS) && depth) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0) |
    ((info->usage & GPU_TEXTURE_USAGE_SAMPLE) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0) |
    ((info->usage & GPU_TEXTURE_USAGE_STORAGE) ? VK_IMAGE_USAGE_STORAGE_BIT : 0) |
    ((info->usage & GPU_TEXTURE_USAGE_COPY) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0) |
    ((info->usage & GPU_TEXTURE_USAGE_PASTE) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0);

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

  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(state.device, texture->handle, &requirements);
  uint32_t memoryType = ~0u;
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

  texture->layout = VK_IMAGE_LAYOUT_GENERAL;
  VkPipelineStageFlags src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dst = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = 0,
    .dstAccessMask = 0,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = texture->layout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = texture->handle,
    .subresourceRange = { texture->aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS }
  };

  // If command buffers are recording, record the initial layout transition there, otherwise, submit
  // a tick with a single layout transition.
  if (state.uploads) {
    vkCmdPipelineBarrier(state.uploads, src, dst, 0, 0, NULL, 0, NULL, 1, &barrier);
  } else {
    gpu_begin();
    vkCmdPipelineBarrier(state.uploads, src, dst, 0, 0, NULL, 0, NULL, 1, &barrier);
    gpu_flush();
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
    texture->format = textureFormats[info->format][LINEAR];
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
      .baseMipLevel = info ? info->baseMipmap : 0,
      .levelCount = (info && info->mipmapCount) ? info->mipmapCount : VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = info ? info->baseLayer : 0,
      .layerCount = (info && info->layerCount) ? info->layerCount : VK_REMAINING_ARRAY_LAYERS
    }
  };

  GPU_VK(vkCreateImageView(state.device, &createInfo, NULL, &texture->view));

  return true;
}

void gpu_texture_destroy(gpu_texture* texture) {
  if (texture->handle) condemn(texture->handle, VK_OBJECT_TYPE_IMAGE);
  if (texture->memory) condemn(texture->memory, VK_OBJECT_TYPE_DEVICE_MEMORY);
  if (texture->view) condemn(texture->view, VK_OBJECT_TYPE_IMAGE_VIEW);
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

  vkCmdCopyBufferToImage(state.uploads, mapped.buffer, texture->handle, texture->layout, 1, &region);

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

  vkCmdCopyImageToBuffer(state.downloads, texture->handle, texture->layout, mapped.buffer, 1, &region);

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
    .srcOffset = { srcOffset[0], srcOffset[1], srcArray ? 0 : srcOffset[2] },
    .dstSubresource = {
      .aspectMask = dst->aspect,
      .mipLevel = dstOffset[3],
      .baseArrayLayer = dstArray ? dstOffset[2] : 0,
      .layerCount = dstArray ? size[2] : 1
    },
    .dstOffset = { dstOffset[0], dstOffset[1], dstArray ? 0 : dstOffset[2] },
    .extent = { size[0], size[1], size[2] }
  };

  vkCmdCopyImage(state.uploads, src->handle, src->layout, dst->handle, dst->layout, 1, &region);
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
  if (sampler->handle) condemn(sampler->handle, VK_OBJECT_TYPE_SAMPLER);
  memset(sampler, 0, sizeof(*sampler));
}

// Canvas

size_t gpu_sizeof_canvas() {
  return sizeof(gpu_canvas);
}

bool gpu_canvas_init(gpu_canvas* canvas, gpu_canvas_info* info) {
  static const VkAttachmentLoadOp loadOps[] = {
    [GPU_LOAD_OP_LOAD] = VK_ATTACHMENT_LOAD_OP_LOAD,
    [GPU_LOAD_OP_CLEAR] = VK_ATTACHMENT_LOAD_OP_CLEAR,
    [GPU_LOAD_OP_DISCARD] = VK_ATTACHMENT_LOAD_OP_DONT_CARE
  };

  static const VkAttachmentStoreOp storeOps[] = {
    [GPU_STORE_OP_STORE] = VK_ATTACHMENT_STORE_OP_STORE,
    [GPU_STORE_OP_DISCARD] = VK_ATTACHMENT_STORE_OP_DONT_CARE
  };

  VkAttachmentDescription attachments[9];
  VkImageView images[9];
  uint32_t count = 0;

  struct {
    VkAttachmentReference color[4];
    VkAttachmentReference resolve[4];
    VkAttachmentReference depth;
  } refs;

  for (uint32_t i = 0; i < COUNTOF(info->color) && info->color[i].texture; i++, canvas->colorAttachmentCount++) {
    uint32_t attachment = count++;
    images[attachment] = info->color[i].texture->view;
    attachments[attachment] = (VkAttachmentDescription) {
      .format = info->color[i].texture->format,
      .samples = info->color[i].texture->samples,
      .loadOp = loadOps[info->color[i].load],
      .storeOp = storeOps[info->color[i].store],
      .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
      .finalLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    refs.color[i] = (VkAttachmentReference) { attachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    memcpy(canvas->clears[attachment].color.float32, info->color[i].clear, 4 * sizeof(float));

    if (info->color[i].resolve) {
      attachment = count++;
      images[attachment] = info->color[i].resolve->view;
      attachments[attachment] = (VkAttachmentDescription) {
        .format = info->color[i].resolve->format,
        .samples = info->color[i].resolve->samples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
        .finalLayout = VK_IMAGE_LAYOUT_GENERAL
      };

      refs.resolve[i] = (VkAttachmentReference) { attachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    } else {
      refs.resolve[i] = (VkAttachmentReference) { VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL };
    }
  }

  if (info->depth.texture) {
    uint32_t attachment = count++;
    images[attachment] = info->depth.texture->view;
    attachments[attachment] = (VkAttachmentDescription) {
      .format = info->depth.texture->format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = loadOps[info->depth.load],
      .storeOp = storeOps[info->depth.store],
      .stencilLoadOp = loadOps[info->depth.stencil.load],
      .stencilStoreOp = storeOps[info->depth.stencil.store],
      .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
      .finalLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    refs.depth = (VkAttachmentReference) { attachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    canvas->clears[attachment].depthStencil.depth = info->depth.clear;
    canvas->clears[attachment].depthStencil.stencil = info->depth.stencil.clear;
  }

  VkSubpassDescription subpass = {
    .colorAttachmentCount = canvas->colorAttachmentCount,
    .pColorAttachments = refs.color,
    .pResolveAttachments = refs.resolve,
    .pDepthStencilAttachment = info->depth.texture ? &refs.depth : NULL
  };

  VkRenderPassCreateInfo renderPassInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .pNext = &(VkRenderPassMultiviewCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
      .subpassCount = 1,
      .pViewMasks = (uint32_t[1]) { (1 << info->views) - 1 }
    },
    .attachmentCount = count,
    .pAttachments = attachments,
    .subpassCount = 1,
    .pSubpasses = &subpass
  };

  if (vkCreateRenderPass(state.device, &renderPassInfo, NULL, &canvas->handle)) {
    return false;
  }

  canvas->renderArea = (VkRect2D) { { 0, 0 }, { info->size[0], info->size[1] } };
  canvas->viewport = (VkViewport) { 0.f, 0.f, info->size[0], info->size[1], 0.f, 1.f };

  VkFramebufferCreateInfo framebufferInfo = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass = canvas->handle,
    .attachmentCount = count,
    .pAttachments = images,
    .width = canvas->renderArea.extent.width,
    .height = canvas->renderArea.extent.height,
    .layers = 1
  };

  if (vkCreateFramebuffer(state.device, &framebufferInfo, NULL, &canvas->framebuffer)) {
    vkDestroyRenderPass(state.device, canvas->handle, NULL);
  }


  nickname(canvas, VK_OBJECT_TYPE_RENDER_PASS, info->label);
  canvas->samples = info->color[0].texture ? info->color[0].texture->samples : VK_SAMPLE_COUNT_1_BIT;
  return true;
}

void gpu_canvas_destroy(gpu_canvas* canvas) {
  if (canvas->handle) condemn(canvas->handle, VK_OBJECT_TYPE_RENDER_PASS);
  if (canvas->framebuffer) condemn(canvas->framebuffer, VK_OBJECT_TYPE_FRAMEBUFFER);
  memset(canvas, 0, sizeof(*canvas));
}

// Bundle

size_t gpu_sizeof_bundle() {
  return sizeof(gpu_bundle);
}

bool gpu_bundle_init(gpu_bundle* bundle, gpu_bundle_info* info) {
  VkDescriptorSetLayout layout = (VkDescriptorSetLayout) info->layout.secret;

  if (!layout) {
    if (!createDescriptorSetLayout(&info->layout, &bundle->layout)) {
      return false;
    }

    layout = bundle->layout;
  }

  if (info->immutable) {
    VkDescriptorSetAllocateInfo descriptorSetInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = state.descriptorPool,
      .descriptorSetCount = 1,
      .pSetLayouts = &layout
    };

    if (vkAllocateDescriptorSets(state.device, &descriptorSetInfo, &bundle->handle)) {
      return false;
    }

    VkWriteDescriptorSet writes[COUNTOF(info->bindings)];
    for (uint32_t i = 0; i < info->count; i++) {
      gpu_slot* slot = &info->layout.slots[info->bindings[i].slot];
      writes[i] = (VkWriteDescriptorSet) {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = bundle->handle,
        .dstBinding = info->bindings[i].slot,
        .descriptorCount = 1, // FIXME info->bindings[i].count,
        .descriptorType = descriptorTypes[slot->type][slot->usage & GPU_BINDING_DYNAMIC],
        .pBufferInfo = &(VkDescriptorBufferInfo) { // FIXME multiple fixed-size chunked writes for arrays
          .buffer = info->bindings[i].buffers[0].buffer ? info->bindings[i].buffers[0].buffer->handle : VK_NULL_HANDLE,
          .offset = info->bindings[i].buffers[0].offset,
          .range = info->bindings[i].buffers[0].size ? info->bindings[i].buffers[0].size : VK_WHOLE_SIZE
        },
        .pImageInfo = &(VkDescriptorImageInfo) { // FIXME multiple fixed-size chunked writes for arrays
          .sampler = info->bindings[i].textures[0].sampler ? info->bindings[i].textures[0].sampler->handle : VK_NULL_HANDLE,
          .imageView = info->bindings[i].textures[0].texture ? info->bindings[i].textures[0].texture->view : VK_NULL_HANDLE,
          .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        }
      };
    }

    vkUpdateDescriptorSets(state.device, info->count, writes, 0, NULL);
  }

  bundle->immutable = info->immutable;
  return true;
}

void gpu_bundle_destroy(gpu_bundle* bundle) {
  if (bundle->layout) vkDestroyDescriptorSetLayout(state.device, bundle->layout, NULL);
  if (bundle->handle) condemn(bundle->handle, VK_OBJECT_TYPE_DESCRIPTOR_SET);
  memset(bundle, 0, sizeof(*bundle));
}

// Shader

size_t gpu_sizeof_shader() {
  return sizeof(gpu_shader);
}

bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info) {
  memset(shader, 0, sizeof(*shader));

  if (info->compute.code) {
    loadShader(&info->compute, VK_SHADER_STAGE_COMPUTE_BIT, &shader->handles[0], &shader->pipelineInfo[0], shader->layoutInfo);
  } else {
    loadShader(&info->vertex, VK_SHADER_STAGE_VERTEX_BIT, &shader->handles[0], &shader->pipelineInfo[0], shader->layoutInfo);
    loadShader(&info->fragment, VK_SHADER_STAGE_FRAGMENT_BIT, &shader->handles[1], &shader->pipelineInfo[1], shader->layoutInfo);
  }

  for (uint32_t i = 0; i < info->dynamicBufferCount; i++) {
    gpu_shader_var* var = &info->dynamicBuffers[i];
    gpu_bundle_layout* layout = &shader->layoutInfo[var->group];
    for (uint32_t j = 0; j < layout->count; j++) {
      if (layout->slots[j].index == var->slot) {
        layout->slots[j].usage |= GPU_BINDING_DYNAMIC;
        break;
      }
    }
  }

  uint32_t layoutCount = 0;
  for (uint32_t i = 0; i < COUNTOF(shader->layouts); i++) {
    if (!createDescriptorSetLayout(&shader->layoutInfo[i], &shader->layouts[i])) {
      gpu_shader_destroy(shader);
      return false;
    }

    shader->layoutInfo[i].secret = (uintptr_t) shader->layouts[i];
    layoutCount = shader->layoutInfo[i].count > 0 ? (i + 1) : layoutCount;
  }

  VkPipelineLayoutCreateInfo layoutInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = layoutCount,
    .pSetLayouts = shader->layouts
  };

  GPU_VK(vkCreatePipelineLayout(state.device, &layoutInfo, NULL, &shader->pipelineLayout));

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

bool gpu_shader_get_layout_info(gpu_shader* shader, uint32_t group, gpu_bundle_layout* layout) {
  if (group >= COUNTOF(shader->layouts)) return false;
  *layout = shader->layoutInfo[group];
  return true;
}

// Pipeline

size_t gpu_sizeof_pipeline() {
  return sizeof(gpu_pipeline);
}

bool gpu_pipeline_init(gpu_pipeline* pipeline, gpu_pipeline_info* info) {
  static const VkPrimitiveTopology topologies[] = {
    [GPU_DRAW_POINTS] = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    [GPU_DRAW_LINES] = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    [GPU_DRAW_LINE_STRIP] = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    [GPU_DRAW_TRIANGLES] = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    [GPU_DRAW_TRIANGLE_STRIP] = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
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

  VkPipelineVertexInputStateCreateInfo vertexInput = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 0,
    .pVertexBindingDescriptions = NULL,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions = NULL
  };

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = topologies[info->drawMode],
    .primitiveRestartEnable = info->drawMode == GPU_DRAW_LINE_STRIP || info->drawMode == GPU_DRAW_TRIANGLE_STRIP
  };

  VkPipelineViewportStateCreateInfo viewport = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports = &info->canvas->viewport,
    .scissorCount = 1,
    .pScissors = &info->canvas->renderArea
  };

  VkPipelineRasterizationStateCreateInfo rasterization = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .cullMode = cullModes[info->cullMode],
    .frontFace = frontFaces[info->winding],
    .depthBiasEnable = info->depthOffset != 0.f,
    .depthBiasConstantFactor = info->depthOffset,
    .depthBiasSlopeFactor = info->depthOffsetSloped,
    .lineWidth = 1.f
  };

  VkPipelineMultisampleStateCreateInfo multisample = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = info->canvas->samples,
    .alphaToCoverageEnable = info->alphaToCoverage
  };

  VkPipelineDepthStencilStateCreateInfo depthStencil = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = info->depthTest != GPU_COMPARE_NONE,
    .depthWriteEnable = info->depthWrite,
    .depthCompareOp = compareOps[info->depthTest],
    .stencilTestEnable = info->stencilFront.test != GPU_COMPARE_NONE || info->stencilBack.test != GPU_COMPARE_NONE,
    .front = {
      .failOp = stencilOps[info->stencilFront.fail],
      .passOp = stencilOps[info->stencilFront.pass],
      .depthFailOp = stencilOps[info->stencilFront.depthFail],
      .compareOp = compareOps[info->stencilFront.test],
      .compareMask = info->stencilFront.readMask,
      .writeMask = info->stencilFront.writeMask,
      .reference = info->stencilFront.reference
    },
    .back = {
      .failOp = stencilOps[info->stencilBack.fail],
      .passOp = stencilOps[info->stencilBack.pass],
      .depthFailOp = stencilOps[info->stencilBack.depthFail],
      .compareOp = compareOps[info->stencilBack.test],
      .compareMask = info->stencilBack.readMask,
      .writeMask = info->stencilBack.writeMask,
      .reference = info->stencilBack.reference
    }
  };

  VkPipelineColorBlendAttachmentState blendState = {
    .blendEnable = info->blend.enabled,
    .srcColorBlendFactor = blendFactors[info->blend.color.src],
    .dstColorBlendFactor = blendFactors[info->blend.color.dst],
    .colorBlendOp = blendOps[info->blend.color.op],
    .srcAlphaBlendFactor = blendFactors[info->blend.alpha.src],
    .dstAlphaBlendFactor = blendFactors[info->blend.alpha.dst],
    .alphaBlendOp = blendOps[info->blend.alpha.op],
    .colorWriteMask = info->colorMask
  };

  VkPipelineColorBlendStateCreateInfo colorBlend = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = info->canvas->colorAttachmentCount,
    .pAttachments = (VkPipelineColorBlendAttachmentState[4]) { blendState, blendState, blendState, blendState }
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
    .layout = info->shader->pipelineLayout,
    .renderPass = info->canvas->handle
  };

  if (vkCreateGraphicsPipelines(state.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipeline->handle)) {
    return false;
  }

  nickname(pipeline, VK_OBJECT_TYPE_PIPELINE, info->label);
  return true;
}

void gpu_pipeline_destroy(gpu_pipeline* pipeline) {
  if (pipeline->handle) condemn(pipeline->handle, VK_OBJECT_TYPE_PIPELINE);
}

// Batch

gpu_batch* gpu_batch_begin(gpu_canvas* canvas) {
  gpu_batch_pool* pool = &batches;
  gpu_batch* batch;
  uint32_t index;

  if (pool->tail != ~0u && state.tick[GPU] >= pool->data[pool->tail].tick) {
    index = pool->tail;
    batch = &pool->data[pool->tail];
    pool->tail = batch->next; // TODO what if tail is ~0u now?
  } else {
    GPU_CHECK(pool->count < COUNTOF(pool->data), "GPU batch pool overflow");
    index = pool->count++;
    batch = &pool->data[index];

    VkCommandBufferAllocateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pool->commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
      .commandBufferCount = 1
    };

    GPU_VK(vkAllocateCommandBuffers(state.device, &info, &batch->cmd));

    if (pool->tail == ~0u) {
      pool->tail = index;
    }
  }

  VkCommandBufferBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | (canvas ? VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT : 0),
    .pInheritanceInfo = &(VkCommandBufferInheritanceInfo) {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
      .renderPass = canvas ? canvas->handle : NULL,
      .subpass = 0
    }
  };

  GPU_VK(vkBeginCommandBuffer(batch->cmd, &beginfo));

  if (pool->head != ~0u) {
    pool->data[pool->head].next = index;
  }

  pool->head = index;
  batch->tick = state.tick[CPU];
  batch->next = ~0u;
  return batch;
}

void gpu_batch_end(gpu_batch* batch) {
  GPU_VK(vkEndCommandBuffer(batch->cmd));
}

void gpu_batch_bind_bundle(gpu_batch* batch, gpu_bundle* bundle, uint32_t group, uint32_t* offsets, uint32_t offsetCount) {
  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  VkPipelineLayout layout = VK_NULL_HANDLE; // TODO batch needs to know its pipeline/shader
  vkCmdBindDescriptorSets(batch->cmd, bindPoint, layout, group, 1, &bundle->handle, offsetCount, offsets);
}

void gpu_batch_bind_pipeline(gpu_batch* batch, gpu_pipeline* pipeline) {
  vkCmdBindPipeline(batch->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);
}

void gpu_batch_bind_vertex_buffers(gpu_batch* batch, gpu_buffer** buffers, uint64_t* offsets, uint32_t count) {
  VkBuffer handles[16];
  for (uint32_t i = 0; i < count; i++) {
    handles[i] = buffers[i]->handle;
  }
  vkCmdBindVertexBuffers(batch->cmd, 0, count, handles, offsets);
}

void gpu_batch_bind_index_buffer(gpu_batch* batch, gpu_buffer* buffer, uint64_t offset, gpu_index_type type) {
  static const VkIndexType types[] = {
    [GPU_INDEX_U16] = VK_INDEX_TYPE_UINT16,
    [GPU_INDEX_U32] = VK_INDEX_TYPE_UINT32
  };

  vkCmdBindIndexBuffer(batch->cmd, buffer->handle, offset, types[type]);
}

void gpu_batch_draw(gpu_batch* batch, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) {
  vkCmdDraw(batch->cmd, vertexCount, instanceCount, firstVertex, 0);
}

void gpu_batch_draw_indexed(gpu_batch* batch, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex) {
  vkCmdDrawIndexed(batch->cmd, indexCount, instanceCount, firstIndex, baseVertex, 0);
}

void gpu_batch_draw_indirect(gpu_batch* batch, gpu_buffer* buffer, uint64_t offset, uint32_t drawCount) {
  vkCmdDrawIndirect(batch->cmd, buffer->handle, offset, drawCount, 0);
}

void gpu_batch_draw_indirect_indexed(gpu_batch* batch, gpu_buffer* buffer, uint64_t offset, uint32_t drawCount) {
  vkCmdDrawIndexedIndirect(batch->cmd, buffer->handle, offset, drawCount, 0);
}

void gpu_batch_compute(gpu_batch* batch, gpu_shader* shader, uint32_t x, uint32_t y, uint32_t z) {
  vkCmdDispatch(batch->cmd, x, y, z);
}

void gpu_batch_compute_indirect(gpu_batch* batch, gpu_shader* shader, gpu_buffer* buffer, uint64_t offset) {
  vkCmdDispatchIndirect(batch->cmd, buffer->handle, offset);
}

// Surface

gpu_texture* gpu_surface_acquire() {
  if (!state.swapchain || state.currentBackbuffer != ~0u) {
    return NULL;
  }

  gpu_begin();
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0xf];
  VkSemaphore semaphore = tick->semaphores.wait;
  GPU_VK(vkAcquireNextImageKHR(state.device, state.swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &state.currentBackbuffer));
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

  vkCmdPipelineBarrier(state.work, src, dst, 0, 0, NULL, 0, NULL, 1, &barrier);
  texture->layout = VK_IMAGE_LAYOUT_GENERAL;
  gpu_flush();

  return texture;
}

void gpu_surface_present() {
  if (!state.swapchain || state.currentBackbuffer == ~0u) {
    return;
  }

  gpu_begin();
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0xf];
  VkSemaphore semaphore = tick->semaphores.tell;
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

  vkCmdPipelineBarrier(state.work, src, dst, 0, 0, NULL, 0, NULL, 1, &barrier);
  texture->layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  gpu_flush();

  VkPresentInfoKHR info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &semaphore,
    .swapchainCount = 1,
    .pSwapchains = &state.swapchain,
    .pImageIndices = &state.currentBackbuffer
  };

  GPU_VK(vkQueuePresentKHR(state.queue, &info));
  state.currentBackbuffer = ~0u;
}

// Helpers

static void execute(gpu_batch** batches, uint32_t count) {
  VkCommandBuffer commands[8];
  uint32_t chunk = COUNTOF(commands);
  while (count > 0) {
    chunk = count < chunk ? count : chunk;
    for (uint32_t i = 0; i < chunk; i++) commands[i] = batches[i]->cmd;
    vkCmdExecuteCommands(state.work, chunk, commands);
    batches += chunk;
    count -= chunk;
  }
}

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

static void readquack() {
  gpu_readback_pool* pool = &state.readbacks;
  while (pool->tail != pool->head && state.tick[GPU] >= pool->data[pool->tail & 0x7f].tick) {
    gpu_readback* readback = &pool->data[pool->tail++ & 0x7f];
    readback->fn(readback->data, readback->size, readback->userdata);
  }
}

static void condemn(void* handle, VkObjectType type) {
  gpu_morgue* morgue = &state.morgue;
  gpu_mutex_lock(&morgue->lock);
  GPU_CHECK(morgue->head - morgue->tail != COUNTOF(morgue->data), "GPU morgue overflow"); // TODO emergency morgue flush instead of throw
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
      case VK_OBJECT_TYPE_DESCRIPTOR_SET: vkFreeDescriptorSets(state.device, state.descriptorPool, 1, victim->handle); break;
      case VK_OBJECT_TYPE_PIPELINE: vkDestroyPipeline(state.device, victim->handle, NULL); break;
      default: GPU_THROW("Unreachable"); break;
    }
  }
  gpu_mutex_unlock(&morgue->lock);
}

static bool createDescriptorSetLayout(gpu_bundle_layout* layout, VkDescriptorSetLayout* handle) {
  VkDescriptorSetLayoutBinding bindings[COUNTOF(layout->slots)];

  VkDescriptorSetLayoutCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = layout->count,
    .pBindings = bindings
  };

  for (uint32_t i = 0; i < layout->count; i++) {
    gpu_slot* slot = &layout->slots[i];
    bindings[i] = (VkDescriptorSetLayoutBinding) {
      .binding = slot->index,
      .descriptorType = descriptorTypes[slot->type][slot->usage & GPU_BINDING_DYNAMIC],
      .descriptorCount = slot->count,
      .stageFlags =
        ((slot->usage & GPU_BINDING_VERTEX) ? VK_SHADER_STAGE_VERTEX_BIT : 0) |
        ((slot->usage & GPU_BINDING_FRAGMENT) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0) |
        ((slot->usage & GPU_BINDING_COMPUTE) ? VK_SHADER_STAGE_COMPUTE_BIT : 0)
    };
  }

  return !vkCreateDescriptorSetLayout(state.device, &info, NULL, handle);
}

static bool loadShader(gpu_shader_source* source, VkShaderStageFlagBits stage, VkShaderModule* handle, VkPipelineShaderStageCreateInfo* pipelineInfo, gpu_bundle_layout* layouts) {
  VkShaderModuleCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = source->size,
    .pCode = source->code
  };

  if (vkCreateShaderModule(state.device, &info, NULL, handle)) {
    return false;
  }

  struct {
    uint16_t group;
    uint16_t index;
    uint16_t count;
    gpu_binding_type type;
  } resources[128];
  uint32_t resourceCount = 0;

  // For variable ids, stores the index of its resource
  // For type ids, stores the index of its declaration instruction
  uint32_t cache[8192];
  memset(cache, 0xff, sizeof(cache));

  const uint32_t* words = source->code;
  if (words[0] == 0x07230203) {
    uint32_t wordCount = source->size / sizeof(uint32_t);
    const uint32_t* instruction = words + 5;
    while (instruction < words + wordCount) {
      uint16_t opcode = instruction[0] & 0xffff;
      uint16_t length = instruction[0] >> 16;
      switch (opcode) {
        case 71: { // OpDecorate
          uint32_t target = instruction[1];
          uint32_t decoration = instruction[2];

          // Skip irrelevant decorations, skip out of bounds ids, skip resources that won't fit
          if ((decoration != 33 && decoration != 34) || target >= COUNTOF(cache) || resourceCount > COUNTOF(resources)) {
            break;
          }

          uint32_t resource = cache[target] == ~0u ? (cache[target] = resourceCount++) : cache[target];

          // Decorate the resource
          if (decoration == 33) {
            resources[resource].index = instruction[3];
          } else if (decoration == 34) {
            resources[resource].group = instruction[3];
          }

          break;
        }
        case 19: // OpTypeVoid
        case 20: // OpTypeBool
        case 21: // OpTypeInt
        case 22: // OpTypeFloat
        case 23: // OpTypeVector
        case 24: // OpTypeMatrix
        case 25: // OpTypeImage
        case 26: // OpTypeSampler
        case 27: // OpTypeSampledImage
        case 28: // OpTypeArray
        case 29: // OpTypeRuntimeArray
        case 30: // OpTypeStruct
        case 31: // OpTypeOpaque
        case 32: // OpTypePointer
          cache[instruction[1]] = instruction - words;
          break;
        case 59: { // OpVariable
          uint32_t type = instruction[1];
          uint32_t id = instruction[2];
          uint32_t storageClass = instruction[3];

          // Skip if it wasn't decorated with a group/binding
          if (cache[id] == ~0u) {
            break;
          }

          uint32_t pointerId = type;
          const uint32_t* pointer = words + cache[pointerId];
          uint32_t pointerTypeId = pointer[3];
          const uint32_t* pointerType = words + cache[pointerTypeId];

          // If it's a pointer to an array, set the resource array size and keep going
          if ((pointerType[0] & 0xffff) == 28 /* OpTypeArray */) {
            uint32_t sizeId = pointerType[3];
            const uint32_t* size = words + cache[sizeId];
            if ((size[0] & 0xffff) == 43 /* OpConstant */ || (size[0] & 0xffff) == 50 /* OpSpecConstant */) {
              resources[cache[id]].count = size[3];
            } else {
              return false; // XXX
            }

            pointerTypeId = pointerType[2];
            pointerType = words + cache[pointerTypeId];
          } else {
            resources[cache[id]].count = 1;
          }

          // Use StorageClass to detect uniform/storage buffers
          if (storageClass == 12 /* StorageBuffer */) {
            resources[cache[id]].type = GPU_BINDING_STORAGE_BUFFER;
            break;
          } else if (storageClass == 2 /* Uniform */) {
            resources[cache[id]].type = GPU_BINDING_UNIFORM_BUFFER;
            break;
          }

          // If it's a sampled image, unwrap to get to the image type
          // If it's not an image or a sampled image, then it's weird, fail
          if ((pointerType[0] & 0xffff) == 27 /* OpTypeSampledImage */) {
            pointerTypeId = pointerType[2];
            pointerType = words + cache[pointerTypeId];
          } else if ((pointerType[0] & 0xffff) != 25 /* OpTypeImage */) {
            vkDestroyShaderModule(state.device, *handle, NULL);
            return false; // XXX
          }

          // If it's a buffer (uniform/storage texel buffer) or an input attachment, fail
          if (pointerType[3] == 5 /* DimBuffer */ || pointerType[3] == 6 /* DimSubpassData */) {
            vkDestroyShaderModule(state.device, *handle, NULL);
            return false; // XXX
          }

          // Read the Sampled key to determine if it's a sampled image (1) or a storage image (2)
          if (pointerType[7] == 1) {
            resources[cache[id]].type = GPU_BINDING_SAMPLED_TEXTURE;
          } else if (pointerType[7] == 2) {
            resources[cache[id]].type = GPU_BINDING_STORAGE_TEXTURE;
          } else {
            vkDestroyShaderModule(state.device, *handle, NULL);
            return false; // XXX
          }

          break;
        }
        // If we find a function, we can exit early because the shader's actual code is irrelevant
        case 54: /* OpFunction */
          instruction = words + wordCount;
          break;
        default: break;
      }
      instruction += length;
    }
  }

  uint32_t stageMask = 0;
  switch (stage) {
    case VK_SHADER_STAGE_VERTEX_BIT: stageMask = GPU_BINDING_VERTEX; break;
    case VK_SHADER_STAGE_FRAGMENT_BIT: stageMask = GPU_BINDING_FRAGMENT; break;
    case VK_SHADER_STAGE_COMPUTE_BIT: stageMask = GPU_BINDING_COMPUTE; break;
    default: break;
  }

  // Merge resources into layout info
  for (uint32_t i = 0; i < resourceCount; i++) {
    gpu_bundle_layout* layout = &layouts[resources[i].group];

    bool found = false;
    for (uint32_t j = 0; j < layout->count; j++) {
      if (layout->slots[j].index == resources[i].index) {
        layout->slots[j].usage |= stageMask;
        found = true;
        break;
      }
    }

    if (!found) {
      layout->slots[layout->count++] = (gpu_slot) {
        .type = resources[i].type,
        .index = resources[i].index,
        .count = resources[i].count,
        .usage = stageMask
      };
    }
  }

  *pipelineInfo = (VkPipelineShaderStageCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = stage,
    .module = *handle,
    .pName = source->entry ? source->entry : "main"
  };

  return true;
}

static uint64_t getTextureRegionSize(VkFormat format, uint16_t extent[3]) {
  switch (format) {
    case VK_FORMAT_R8_UNORM:
      return extent[0] * extent[1] * extent[2];
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SFLOAT:
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
    VkDebugUtilsObjectNameInfoEXT info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = type,
      .objectHandle = VOIDP_TO_U64(handle),
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
