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

// State

static struct {
  void* library;
  gpu_config config;
  VkInstance instance;
  VkPhysicalDevice adapter;
  VkDevice device;
  VkQueue queue;
  uint32_t queueFamilyIndex;
  uint32_t memoryTypeCount;
  uint32_t memoryTypes[32];
  VkDebugUtilsMessengerEXT messenger;
} state;

// Helpers

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#define VK(f, s) if (!vcheck(f, s))
#define CHECK(c, s) if (!check(c, s))

static VkBool32 relay(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userdata);
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

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(state.adapter, &memoryProperties);
    state.memoryTypeCount = memoryProperties.memoryTypeCount;
    for (uint32_t i = 0; i < state.memoryTypeCount; i++) {
      state.memoryTypes[i] = memoryProperties.memoryTypes[i].propertyFlags;
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
      enable->largePoints = supports->largePoints;

      // Optional features (currently always enabled when supported)
      config->features->astc = enable->textureCompressionASTC_LDR = supports->textureCompressionASTC_LDR;
      config->features->bptc = enable->textureCompressionBC = supports->textureCompressionBC;
      config->features->wireframe = enable->fillModeNonSolid = supports->fillModeNonSolid;
      config->features->depthClamp = enable->depthClamp = supports->depthClamp;
      config->features->clipDistance = enable->shaderClipDistance = supports->shaderClipDistance;
      config->features->cullDistance = enable->shaderCullDistance = supports->shaderCullDistance;
      config->features->indirectDrawFirstInstance = enable->drawIndirectFirstInstance = supports->drawIndirectFirstInstance;
      config->features->float64 = enable->shaderFloat64 = supports->shaderFloat64;
      config->features->int64 = enable->shaderInt64 = supports->shaderInt64;
      config->features->int16 = enable->shaderInt16 = supports->shaderInt16;

      bool dynamicIndexing =
        supports->shaderUniformBufferArrayDynamicIndexing &&
        supports->shaderSampledImageArrayDynamicIndexing &&
        supports->shaderStorageBufferArrayDynamicIndexing &&
        supports->shaderStorageImageArrayDynamicIndexing;

      if (dynamicIndexing) {
        enable->shaderUniformBufferArrayDynamicIndexing = true;
        enable->shaderSampledImageArrayDynamicIndexing = true;
        enable->shaderStorageBufferArrayDynamicIndexing = true;
        enable->shaderStorageImageArrayDynamicIndexing = true;
        config->features->dynamicIndexing = true;
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

  return true;
}

void gpu_destroy(void) {
  if (state.device) vkDeviceWaitIdle(state.device);
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

// Helpers

static VkBool32 relay(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userdata) {
  if (state.config.callback) {
    bool severe = severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    state.config.callback(state.config.userdata, data->pMessage, severe);
  }
  return VK_FALSE;
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
