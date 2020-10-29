#include "gpu.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define __thread __declspec(thread)
typedef CRITICAL_SECTION gpu_mutex;
static void gpu_mutex_init(gpu_mutex* mutex) { InitializeCriticalSection(mutex); }
static void gpu_mutex_destroy(gpu_mutex* mutex) { DeleteCriticalSection(mutex); }
static void gpu_mutex_lock(gpu_mutex* mutex) { EnterCriticalSection(mutex); }
static void gpu_mutex_unlock(gpu_mutex* mutex) { LeaveCriticalSection(mutex); }
#else
#include <dlfcn.h>
#include <pthread.h>
typedef pthread_mutex_t gpu_mutex;
static void gpu_mutex_init(gpu_mutex* mutex) { pthread_mutex_init(mutex, NULL); }
static void gpu_mutex_destroy(gpu_mutex* mutex) { pthread_mutex_destroy(mutex); }
static void gpu_mutex_lock(gpu_mutex* mutex) { pthread_mutex_lock(mutex); }
static void gpu_mutex_unlock(gpu_mutex* mutex) { pthread_mutex_unlock(mutex); }
#endif

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
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
  VkImageView view;
  VkImageLayout layout;
  VkDeviceMemory memory;
  VkImageAspectFlagBits aspect;
  gpu_texture* source;
  gpu_texture_type type;
  VkFormat format;
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
  struct {
    gpu_texture* texture;
    VkImageLayout layout;
    VkPipelineStageFlags stage;
    VkAccessFlags access;
  } sync[5];
  VkClearValue clears[5];
};

struct gpu_shader {
  VkShaderModule handles[2];
  VkPipelineShaderStageCreateInfo pipelineInfo[2];
  VkDescriptorSetLayout layouts[4];
  VkPipelineLayout pipelineLayout;
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
  VkCommandBuffer commands;
  VkCommandPool pool;
  VkFence fence;
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

// State

static __thread gpu_batch_pool batches;

static struct {
  VkInstance instance;
  VkDevice device;
  VkQueue queue;
  VkCommandBuffer commands;
  VkDebugUtilsMessengerEXT messenger;
  VkPhysicalDeviceMemoryProperties memoryProperties;
  uint32_t queueFamilyIndex;
  uint32_t tick[2];
  gpu_tick ticks[16];
  gpu_morgue morgue;
  gpu_config config;
  void* library;
} state;

// Helpers

static void condemn(void* handle, VkObjectType type);
static void expunge(void);
static bool loadShader(gpu_shader_source* source, VkShaderStageFlagBits stage, VkShaderModule* handle, VkPipelineShaderStageCreateInfo* pipelineInfo);
static void setLayout(gpu_texture* texture, VkImageLayout layout, VkPipelineStageFlags nextStages, VkAccessFlags nextActions);
static void nickname(void* object, VkObjectType type, const char* name);
static VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* context);
static const char* getErrorString(VkResult result);

// Functions that don't require an instance
#define GPU_FOREACH_ANONYMOUS(X)\
  X(vkCreateInstance)

// Functions that require an instance but don't require a device
#define GPU_FOREACH_INSTANCE(X)\
  X(vkDestroyInstance)\
  X(vkCreateDebugUtilsMessengerEXT)\
  X(vkDestroyDebugUtilsMessengerEXT)\
  X(vkEnumeratePhysicalDevices)\
  X(vkGetPhysicalDeviceProperties)\
  X(vkGetPhysicalDeviceFeatures)\
  X(vkGetPhysicalDeviceMemoryProperties)\
  X(vkGetPhysicalDeviceQueueFamilyProperties)\
  X(vkCreateDevice)\
  X(vkDestroyDevice)\
  X(vkGetDeviceQueue)\
  X(vkGetDeviceProcAddr)

// Functions that require a device
#define GPU_FOREACH_DEVICE(X)\
  X(vkSetDebugUtilsObjectNameEXT)\
  X(vkQueueSubmit)\
  X(vkDeviceWaitIdle)\
  X(vkCreateCommandPool)\
  X(vkDestroyCommandPool)\
  X(vkResetCommandPool)\
  X(vkAllocateCommandBuffers)\
  X(vkFreeCommandBuffers)\
  X(vkBeginCommandBuffer)\
  X(vkEndCommandBuffer)\
  X(vkCreateFence)\
  X(vkDestroyFence)\
  X(vkWaitForFences)\
  X(vkResetFences)\
  X(vkCmdPipelineBarrier)\
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
  X(vkUnmapMemory)\
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
  X(vkCreatePipelineLayout)\
  X(vkDestroyPipelineLayout)\
  X(vkCreateGraphicsPipelines)\
  X(vkDestroyPipeline)\
  X(vkCmdBindPipeline)\
  X(vkCmdBindVertexBuffers)\
  X(vkCmdBindIndexBuffer)\
  X(vkCmdDraw)\
  X(vkCmdDrawIndexed)\
  X(vkCmdDrawIndirect)\
  X(vkCmdDrawIndexedIndirect)\
  X(vkCmdDispatch)\
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
#ifdef _WIN32
  state.library = LoadLibraryA("vulkan-1.dll");
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) GetProcAddress(state.library, "vkGetInstanceProcAddr");
#else
  state.library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym(state.library, "vkGetInstanceProcAddr");
#endif
  GPU_FOREACH_ANONYMOUS(GPU_LOAD_ANONYMOUS);

  { // Instance
    const char* layers[] = {
      "VK_LAYER_KHRONOS_validation"
    };

    const char* extensions[] = {
      "VK_EXT_debug_utils"
    };

    VkInstanceCreateInfo instanceInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &(VkApplicationInfo) {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_MAKE_VERSION(1, 1, 0)
      },
      .enabledLayerCount = state.config.debug ? COUNTOF(layers) : 0,
      .ppEnabledLayerNames = layers,
      .enabledExtensionCount = state.config.debug ? COUNTOF(extensions) : 0,
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
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType =
          VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback
      };

      if (vkCreateDebugUtilsMessengerEXT(state.instance, &messengerInfo, NULL, &state.messenger)) {
        gpu_destroy();
        return false;
      }
    }
  }

  { // Device
    uint32_t deviceCount = 1;
    VkPhysicalDevice physicalDevice;
    if (vkEnumeratePhysicalDevices(state.instance, &deviceCount, &physicalDevice) < 0) {
      gpu_destroy();
      return false;
    }

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    VkQueueFamilyProperties queueFamilies[4];
    uint32_t queueFamilyCount = COUNTOF(queueFamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

    state.queueFamilyIndex = ~0u;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
      uint32_t flags = queueFamilies[i].queueFlags;
      if ((flags & VK_QUEUE_GRAPHICS_BIT) && (flags & VK_QUEUE_COMPUTE_BIT)) {
        state.queueFamilyIndex = i;
        break;
      }
    }

    if (state.queueFamilyIndex == ~0u) {
      gpu_destroy();
      return false;
    }

    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &state.memoryProperties);

    VkDeviceQueueCreateInfo queueInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = state.queueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = &(float) { 1.f }
    };

    VkDeviceCreateInfo deviceInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &(VkPhysicalDeviceFeatures2) {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &(VkPhysicalDeviceMultiviewFeatures) {
          .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
          .multiview = VK_TRUE
        },
        .features = {
          .fullDrawIndexUint32 = VK_TRUE,
          .multiDrawIndirect = VK_TRUE,
          .shaderSampledImageArrayDynamicIndexing = VK_TRUE
        }
      },
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queueInfo
    };

    if (vkCreateDevice(physicalDevice, &deviceInfo, NULL, &state.device)) {
      gpu_destroy();
      return false;
    }

    vkGetDeviceQueue(state.device, state.queueFamilyIndex, 0, &state.queue);

    GPU_FOREACH_DEVICE(GPU_LOAD_DEVICE);

    // Frames
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
        .commandBufferCount = 1
      };

      if (vkAllocateCommandBuffers(state.device, &commandBufferInfo, &state.ticks[i].commands)) {
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
  }

  gpu_mutex_init(&state.morgue.lock);
  state.tick[CPU] = COUNTOF(state.ticks);
  state.tick[GPU] = ~0u;
  return true;
}

void gpu_destroy(void) {
  if (state.device) vkDeviceWaitIdle(state.device);
  state.tick[GPU] = state.tick[CPU];
  expunge();
  for (uint32_t i = 0; i < COUNTOF(state.ticks); i++) {
    gpu_tick* tick = &state.ticks[i];
    if (tick->fence) vkDestroyFence(state.device, tick->fence, NULL);
    if (tick->pool) vkDestroyCommandPool(state.device, tick->pool, NULL);
  }
  if (state.device) vkDestroyDevice(state.device, NULL);
  if (state.messenger) vkDestroyDebugUtilsMessengerEXT(state.instance, state.messenger, NULL);
  if (state.instance) vkDestroyInstance(state.instance, NULL);
#ifdef _WIN32
  FreeLibrary(state.library);
#else
  dlclose(state.library);
#endif
  gpu_mutex_destroy(&state.morgue.lock);
  memset(&state, 0, sizeof(state));
}

void gpu_thread_init() {
  gpu_batch_pool* pool = &batches;

  if (pool->commandPool) {
    return;
  }

  VkCommandPoolCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
    .queueFamilyIndex = state.queueFamilyIndex
  };

  GPU_VK(vkCreateCommandPool(state.device, &info, NULL, &pool->commandPool));
  pool->head = ~0u;
  pool->tail = ~0u;
}

void gpu_thread_destroy() {
  vkDeviceWaitIdle(state.device);
  gpu_batch_pool* pool = &batches;
  vkDestroyCommandPool(state.device, pool->commandPool, NULL);
  memset(pool, 0, sizeof(*pool));
}

void gpu_prepare() {
  gpu_tick* tick = &state.ticks[state.tick[CPU] & 0xf];
  GPU_VK(vkWaitForFences(state.device, 1, &tick->fence, VK_FALSE, ~0ull));
  GPU_VK(vkResetFences(state.device, 1, &tick->fence));
  GPU_VK(vkResetCommandPool(state.device, tick->pool, 0));
  state.tick[GPU]++;
  expunge();

  state.commands = tick->commands;
  GPU_VK(vkBeginCommandBuffer(state.commands, &(VkCommandBufferBeginInfo) {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  }));
}

void gpu_submit() {
  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pCommandBuffers = &state.commands,
    .commandBufferCount = 1
  };

  GPU_VK(vkEndCommandBuffer(state.commands));
  GPU_VK(vkQueueSubmit(state.queue, 1, &submit, state.ticks[state.tick[CPU]++ & 0xf].fence));
  state.commands = VK_NULL_HANDLE;
}

void gpu_pass_begin(gpu_canvas* canvas) {
  for (uint32_t i = 0; i < COUNTOF(canvas->sync) && canvas->sync[i].texture; i++) {
    setLayout(canvas->sync[i].texture, canvas->sync[i].layout, canvas->sync[i].stage, canvas->sync[i].access);
  }

  VkRenderPassBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = canvas->handle,
    .framebuffer = canvas->framebuffer,
    .renderArea = canvas->renderArea,
    .clearValueCount = COUNTOF(canvas->clears),
    .pClearValues = canvas->clears
  };

  vkCmdBeginRenderPass(state.commands, &beginfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
}

void gpu_pass_end() {
  vkCmdEndRenderPass(state.commands);
}

void gpu_execute(gpu_batch** batches, uint32_t count) {
  VkCommandBuffer commands[8];
  uint32_t chunk = COUNTOF(commands);
  for (uint32_t i = 0; i < count; i += chunk) {
    for (uint32_t j = 0; j < chunk && i + j < count; j++) {
      commands[j] = batches[i + j]->cmd;
    }
    vkCmdExecuteCommands(state.commands, count, commands);
  }
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
    ((info->usage & GPU_BUFFER_USAGE_COMPUTE) ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0) |
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
  return NULL;
}

void gpu_buffer_read(gpu_buffer* buffer, uint64_t offset, uint64_t size, gpu_read_fn* fn, void* userdata) {
  //
}

void gpu_buffer_copy(gpu_buffer* src, gpu_buffer* dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
  VkBufferCopy region = {
    .srcOffset = srcOffset,
    .dstOffset = dstOffset,
    .size = size
  };

  vkCmdCopyBuffer(state.commands, src->handle, dst->handle, 1, &region);
}

// Texture

static const struct { VkFormat format; VkImageAspectFlagBits aspect; } textureInfo[] = {
  [GPU_TEXTURE_FORMAT_RGBA8] = { VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT },
  [GPU_TEXTURE_FORMAT_RGBA4] = { VK_FORMAT_R4G4B4A4_UNORM_PACK16, VK_IMAGE_ASPECT_COLOR_BIT },
  [GPU_TEXTURE_FORMAT_R16F] = { VK_FORMAT_R16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT },
  [GPU_TEXTURE_FORMAT_RG16F] = { VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT },
  [GPU_TEXTURE_FORMAT_RGBA16F] = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT },
  [GPU_TEXTURE_FORMAT_R32F] = { VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT },
  [GPU_TEXTURE_FORMAT_RG32F] = { VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT },
  [GPU_TEXTURE_FORMAT_RGBA32F] = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT },
  [GPU_TEXTURE_FORMAT_RGB10A2] = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_ASPECT_COLOR_BIT },
  [GPU_TEXTURE_FORMAT_RG11B10F] = { VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_ASPECT_COLOR_BIT },
  [GPU_TEXTURE_FORMAT_D16] = { VK_FORMAT_D16_UNORM, VK_IMAGE_ASPECT_DEPTH_BIT },
  [GPU_TEXTURE_FORMAT_D32F] = { VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT },
  [GPU_TEXTURE_FORMAT_D24S8] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT }
};

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
    case GPU_TEXTURE_TYPE_ARRAY: type = VK_IMAGE_TYPE_3D; flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT; break;
    default: return false;
  }

  texture->format = textureInfo[info->format].format;
  texture->aspect = textureInfo[info->format].aspect;

  bool depth = texture->aspect & VK_IMAGE_ASPECT_DEPTH_BIT;
  VkImageUsageFlags usage =
    (((info->usage & GPU_TEXTURE_USAGE_CANVAS) && !depth) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0) |
    (((info->usage & GPU_TEXTURE_USAGE_CANVAS) && depth) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0) |
    ((info->usage & GPU_TEXTURE_USAGE_SAMPLE) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0) |
    ((info->usage & GPU_TEXTURE_USAGE_COMPUTE) ? VK_IMAGE_USAGE_STORAGE_BIT : 0) |
    ((info->usage & GPU_TEXTURE_USAGE_COPY) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0) |
    ((info->usage & GPU_TEXTURE_USAGE_PASTE) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0);

  VkImageCreateInfo imageInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = flags,
    .imageType = type,
    .format = texture->format,
    .extent.width = info->size[0],
    .extent.height = info->size[1],
    .extent.depth = info->size[2],
    .mipLevels = info->mipmaps,
    .arrayLayers = info->layers,
    .samples = VK_SAMPLE_COUNT_1_BIT,
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

  texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;

  if (!gpu_texture_init_view(texture, &(gpu_texture_view_info) { .source = texture })) {
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
    texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;
    texture->source = info->source;
    texture->type = info ? info->type : info->source->type;
    texture->format = textureInfo[info->format].format;
    texture->aspect = info->source->aspect;
  }

  VkImageViewType type;
  switch (texture->type) {
    case GPU_TEXTURE_TYPE_2D: type = VK_IMAGE_VIEW_TYPE_2D; break;
    case GPU_TEXTURE_TYPE_3D: type = VK_IMAGE_VIEW_TYPE_3D; break;
    case GPU_TEXTURE_TYPE_CUBE: type = VK_IMAGE_VIEW_TYPE_CUBE; break;
    case GPU_TEXTURE_TYPE_ARRAY: type = VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
    default: return false;
  }

  VkImageViewCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = info->source->handle,
    .viewType = type,
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

void* gpu_texture_map(gpu_texture* texture, uint16_t offset[4], uint16_t size[3]) {
  return NULL;
}

void gpu_texture_read(gpu_texture* texture, uint16_t offset[4], uint16_t size[3], gpu_read_fn* fn, void* userdata) {
  //
}

void gpu_texture_copy(gpu_texture* src, gpu_texture* dst, uint16_t srcOffset[4], uint16_t dstOffset[4], uint16_t size[3]) {
  // TODO is it better to transition to transfer-optimal layouts?
  // TODO could have a more fine-grained image barrier on only the slices/levels being touched
  setLayout(src, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
  setLayout(dst, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

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

  vkCmdCopyImage(state.commands, src->handle, src->layout, dst->handle, dst->layout, 1, &region);
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

  VkAttachmentDescription attachments[5];
  VkAttachmentReference references[5];
  VkImageView imageViews[5];
  canvas->colorAttachmentCount = 0;
  uint32_t totalCount = 0;

  for (uint32_t i = 0; i < COUNTOF(info->color) && info->color[i].texture; i++, canvas->colorAttachmentCount++, totalCount++) {
    attachments[i] = (VkAttachmentDescription) {
      .format = info->color[i].texture->format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = loadOps[info->color[i].load],
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    references[i] = (VkAttachmentReference) { i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    imageViews[i] = info->color[i].texture->view;

    memcpy(canvas->clears[i].color.float32, info->color[i].clear, 4 * sizeof(float));
    canvas->sync[i].texture = info->color[i].texture;;
    canvas->sync[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    canvas->sync[i].stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    canvas->sync[i].access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    canvas->sync[i].access |= info->color[i].load ? VK_ACCESS_COLOR_ATTACHMENT_READ_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  }

  if (info->depth.texture) {
    uint32_t i = totalCount++;

    attachments[i] = (VkAttachmentDescription) {
      .format = info->depth.texture->format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = loadOps[info->depth.load],
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = loadOps[info->depth.stencil.load],
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
      .finalLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    references[i] = (VkAttachmentReference) { i, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    imageViews[i] = info->depth.texture->view;

    canvas->clears[i].depthStencil.depth = info->depth.clear;
    canvas->clears[i].depthStencil.stencil = info->depth.stencil.clear;
    canvas->sync[i].texture = info->depth.texture;
    canvas->sync[i].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    canvas->sync[i].stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    canvas->sync[i].access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    canvas->sync[i].access |= (info->depth.load || info->depth.stencil.load) ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT : 0;
  }

  VkSubpassDescription subpass = {
    .colorAttachmentCount = canvas->colorAttachmentCount,
    .pColorAttachments = references,
    .pDepthStencilAttachment = info->depth.texture ? &references[canvas->colorAttachmentCount] : NULL
  };

  VkRenderPassCreateInfo renderPassInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = totalCount,
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
    .attachmentCount = totalCount,
    .pAttachments = imageViews,
    .width = canvas->renderArea.extent.width,
    .height = canvas->renderArea.extent.height,
    .layers = 1
  };

  if (vkCreateFramebuffer(state.device, &framebufferInfo, NULL, &canvas->framebuffer)) {
    vkDestroyRenderPass(state.device, canvas->handle, NULL);
  }

  nickname(canvas, VK_OBJECT_TYPE_RENDER_PASS, info->label);

  return false;
}

void gpu_canvas_destroy(gpu_canvas* canvas) {
  if (canvas->handle) condemn(canvas->handle, VK_OBJECT_TYPE_RENDER_PASS);
  if (canvas->framebuffer) condemn(canvas->framebuffer, VK_OBJECT_TYPE_FRAMEBUFFER);
  memset(canvas, 0, sizeof(*canvas));
}

// Shader

size_t gpu_sizeof_shader() {
  return sizeof(gpu_shader);
}

bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info) {
  memset(shader, 0, sizeof(*shader));

  if (info->compute.code) {
    loadShader(&info->compute, VK_SHADER_STAGE_COMPUTE_BIT, &shader->handles[0], &shader->pipelineInfo[0]);
  } else {
    loadShader(&info->vertex, VK_SHADER_STAGE_VERTEX_BIT, &shader->handles[0], &shader->pipelineInfo[0]);
    loadShader(&info->fragment, VK_SHADER_STAGE_FRAGMENT_BIT, &shader->handles[1], &shader->pipelineInfo[1]);
  }

  VkPipelineLayoutCreateInfo layoutInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
  };

  GPU_VK(vkCreatePipelineLayout(state.device, &layoutInfo, NULL, &shader->pipelineLayout));

  return true;
}

void gpu_shader_destroy(gpu_shader* shader) {
  // The spec says it's safe to destroy shaders while still in use
  if (shader->handles[0]) vkDestroyShaderModule(state.device, shader->handles[0], NULL);
  if (shader->handles[1]) vkDestroyShaderModule(state.device, shader->handles[1], NULL);
  vkDestroyPipelineLayout(state.device, shader->pipelineLayout, NULL);
  memset(shader, 0, sizeof(*shader));
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
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
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

void gpu_batch_bind(gpu_batch* batch) {
  //
}

void gpu_batch_set_pipeline(gpu_batch* batch, gpu_pipeline* pipeline) {
  vkCmdBindPipeline(batch->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);
}

void gpu_batch_set_vertex_buffers(gpu_batch* batch, gpu_buffer** buffers, uint64_t* offsets, uint32_t count) {
  VkBuffer handles[16];
  for (uint32_t i = 0; i < count; i++) {
    handles[i] = buffers[i]->handle;
  }
  vkCmdBindVertexBuffers(batch->cmd, 0, count, handles, offsets);
}

void gpu_batch_set_index_buffer(gpu_batch* batch, gpu_buffer* buffer, uint64_t offset, gpu_index_type type) {
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

// Helpers

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
      case VK_OBJECT_TYPE_PIPELINE: vkDestroyPipeline(state.device, victim->handle, NULL); break;
      default: GPU_THROW("Unreachable"); break;
    }
  }
  gpu_mutex_unlock(&morgue->lock);
}

static bool loadShader(gpu_shader_source* source, VkShaderStageFlagBits stage, VkShaderModule* handle, VkPipelineShaderStageCreateInfo* pipelineInfo) {
  VkShaderModuleCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = source->size,
    .pCode = source->code
  };

  if (vkCreateShaderModule(state.device, &info, NULL, handle)) {
    return false;
  }

  *pipelineInfo = (VkPipelineShaderStageCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = stage,
    .module = *handle,
    .pName = source->entry ? source->entry : "main"
  };

  return true;
}

static void setLayout(gpu_texture* texture, VkImageLayout layout, VkPipelineStageFlags nextStages, VkAccessFlags nextActions) {
  if (texture->layout == layout) {
    return;
  }

  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = 0,
    .dstAccessMask = nextActions,
    .oldLayout = texture->layout,
    .newLayout = layout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = texture->handle,
    .subresourceRange = { texture->aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS }
  };

  // TODO Wait for nothing, but could we opportunistically sync with other pending writes?  Or is that weird
  VkPipelineStageFlags waitFor = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  vkCmdPipelineBarrier(state.commands, waitFor, nextStages, 0, 0, NULL, 0, NULL, 1, &barrier);
  texture->layout = layout;
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
