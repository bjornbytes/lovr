#include "gpu.h"
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#define SCRATCHPAD_SIZE (16 * 1024 * 1024)
#define VOIDP_TO_U64(x) (((union { uint64_t u; void* p; }) { .p = x }).u)
#define GPU_THROW(s) if (state.config.callback) { state.config.callback(state.config.context, s, true); }
#define GPU_CHECK(c, s) if (!(c)) { GPU_THROW(s); }
#define GPU_VK(f) do { VkResult r = (f); GPU_CHECK(r >= 0, getErrorString(r)); } while (0)

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
  X(vkCmdCopyBuffer)\
  X(vkCreateImage)\
  X(vkDestroyImage)\
  X(vkGetImageMemoryRequirements)\
  X(vkBindImageMemory)\
  X(vkCmdCopyBufferToImage)\
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
  X(vkCreateGraphicsPipelines)\
  X(vkDestroyPipeline)\
  X(vkCmdBindPipeline)\
  X(vkCmdBindVertexBuffers)\
  X(vkCmdBindIndexBuffer)\
  X(vkCmdDraw)\
  X(vkCmdDrawIndexed)\
  X(vkCmdDrawIndirect)\
  X(vkCmdDrawIndexedIndirect)\
  X(vkCmdDispatch)

// Used to load/declare Vulkan functions without lots of clutter
#define GPU_LOAD_ANONYMOUS(fn) fn = (PFN_##fn) vkGetInstanceProcAddr(NULL, #fn);
#define GPU_LOAD_INSTANCE(fn) fn = (PFN_##fn) vkGetInstanceProcAddr(state.instance, #fn);
#define GPU_LOAD_DEVICE(fn) fn = (PFN_##fn) vkGetDeviceProcAddr(state.device, #fn);
#define GPU_DECLARE(fn) static PFN_##fn fn;

// Declare function pointers
GPU_FOREACH_ANONYMOUS(GPU_DECLARE)
GPU_FOREACH_INSTANCE(GPU_DECLARE)
GPU_FOREACH_DEVICE(GPU_DECLARE)

typedef struct {
  uint16_t frame;
  uint16_t scratchpad;
  uint32_t offset;
} gpu_mapping;

struct gpu_buffer {
  VkBuffer handle;
  VkDeviceMemory memory;
  gpu_mapping mapping;
  uint64_t targetOffset;
  uint64_t targetSize;
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

typedef struct {
  VkObjectType type;
  void* handle;
} gpu_ref;

typedef struct {
  gpu_ref* data;
  size_t capacity;
  size_t length;
} gpu_freelist;

typedef struct {
  VkBuffer buffer;
  VkDeviceMemory memory;
  void* data;
} gpu_scratchpad;

typedef struct {
  gpu_scratchpad* list;
  uint16_t count;
  uint16_t current;
  uint32_t cursor;
} gpu_pool;

typedef struct {
  uint32_t index;
  VkFence fence;
  VkCommandBuffer commandBuffer;
  gpu_freelist freelist;
  gpu_pool pool;
} gpu_frame;

static struct {
  gpu_config config;
  gpu_features features;
  void* library;
  VkInstance instance;
  VkDebugUtilsMessengerEXT messenger;
  VkPhysicalDeviceMemoryProperties memoryProperties;
  VkMemoryRequirements scratchpadMemoryRequirements;
  uint32_t scratchpadMemoryType;
  uint32_t queueFamilyIndex;
  VkDevice device;
  VkQueue queue;
  VkCommandPool commandPool;
  gpu_frame frames[2];
  gpu_frame* frame;
  gpu_pipeline* pipeline;
} state;

static void condemn(void* handle, VkObjectType type);
static void purge(gpu_frame* frame);
static bool loadShader(gpu_shader_source* source, VkShaderStageFlagBits stage, VkShaderModule* handle, VkPipelineShaderStageCreateInfo* pipelineInfo);
static void setLayout(gpu_texture* texture, VkImageLayout layout, VkPipelineStageFlags nextStages, VkAccessFlags nextActions);
static void nickname(void* object, VkObjectType type, const char* name);
static VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* context);
static const char* getErrorString(VkResult result);

static uint8_t* gpu_map(uint64_t size, gpu_mapping* mapping) {
  gpu_pool* pool = &state.frame->pool;
  gpu_scratchpad* scratchpad = &pool->list[pool->current];

  if (pool->count == 0 || pool->cursor + size > SCRATCHPAD_SIZE) {
    GPU_CHECK(size <= SCRATCHPAD_SIZE, "Tried to map too much memory");

    pool->cursor = 0;
    pool->current = pool->count++;
    pool->list = realloc(pool->list, pool->count * sizeof(gpu_scratchpad));
    GPU_CHECK(pool->list, "Out of memory");
    scratchpad = &pool->list[pool->current];

    // Create buffer
    VkBufferCreateInfo bufferInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = SCRATCHPAD_SIZE,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };

    GPU_VK(vkCreateBuffer(state.device, &bufferInfo, NULL, &scratchpad->buffer));

    // Get memory requirements, once
    if (state.scratchpadMemoryRequirements.size == 0) {
      state.scratchpadMemoryType = ~0u;
      vkGetBufferMemoryRequirements(state.device, scratchpad->buffer, &state.scratchpadMemoryRequirements);
      for (uint32_t i = 0; i < state.memoryProperties.memoryTypeCount; i++) {
        uint32_t flags = state.memoryProperties.memoryTypes[i].propertyFlags;
        uint32_t mask = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if ((flags & mask) == mask && (state.scratchpadMemoryRequirements.memoryTypeBits & (1 << i))) {
          state.scratchpadMemoryType = i;
          break;
        }
      }

      if (state.scratchpadMemoryType == ~0u) {
        vkDestroyBuffer(state.device, scratchpad->buffer, NULL);
        return NULL; // PANIC
      }
    }

    // Allocate, bind
    VkMemoryAllocateInfo memoryInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = state.scratchpadMemoryRequirements.size,
      .memoryTypeIndex = state.scratchpadMemoryType
    };

    if (vkAllocateMemory(state.device, &memoryInfo, NULL, &scratchpad->memory)) {
      vkDestroyBuffer(state.device, scratchpad->buffer, NULL);
      GPU_THROW("Out of memory");
    }

    if (vkBindBufferMemory(state.device, scratchpad->buffer, scratchpad->memory, 0)) {
      vkDestroyBuffer(state.device, scratchpad->buffer, NULL);
      vkFreeMemory(state.device, scratchpad->memory, NULL);
      GPU_THROW("Out of memory");
    }

    if (vkMapMemory(state.device, scratchpad->memory, 0, VK_WHOLE_SIZE, 0, &scratchpad->data)) {
      vkDestroyBuffer(state.device, scratchpad->buffer, NULL);
      vkFreeMemory(state.device, scratchpad->memory, NULL);
      GPU_THROW("Out of memory");
    }
  }

  mapping->frame = state.frame->index;
  mapping->offset = pool->cursor;
  mapping->scratchpad = pool->current;
  return (uint8_t*) scratchpad->data + pool->cursor;
}

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
      "VK_LAYER_LUNARG_object_tracker",
      "VK_LAYER_LUNARG_core_validation",
      "VK_LAYER_LUNARG_parameter_validation"
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
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType =
          VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback,
        .pUserData = state.config.context
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

    state.queueFamilyIndex = ~0u;
    VkQueueFamilyProperties queueFamilies[4];
    uint32_t queueFamilyCount = COUNTOF(queueFamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

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

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    state.features.anisotropy = features.samplerAnisotropy;
    state.features.astc = features.textureCompressionASTC_LDR;
    state.features.dxt = features.textureCompressionBC;

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

    VkCommandPoolCreateInfo commandPoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = state.queueFamilyIndex
    };

    if (vkCreateCommandPool(state.device, &commandPoolInfo, NULL, &state.commandPool)) {
      gpu_destroy();
      return false;
    }
  }

  { // Frame state
    state.frame = &state.frames[0];

    for (uint32_t i = 0; i < COUNTOF(state.frames); i++) {
      state.frames[i].index = i;

      VkCommandBufferAllocateInfo commandBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = state.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
      };

      if (vkAllocateCommandBuffers(state.device, &commandBufferInfo, &state.frames[i].commandBuffer)) {
        gpu_destroy();
        return false;
      }

      VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
      };

      if (vkCreateFence(state.device, &fenceInfo, NULL, &state.frames[i].fence)) {
        gpu_destroy();
        return false;
      }
    }
  }

  return true;
}

void gpu_destroy(void) {
  if (state.device) vkDeviceWaitIdle(state.device);
  for (uint32_t i = 0; i < COUNTOF(state.frames); i++) {
    gpu_frame* frame = &state.frames[i];

    purge(frame);
    free(frame->freelist.data);

    if (frame->fence) vkDestroyFence(state.device, frame->fence, NULL);
    if (frame->commandBuffer) vkFreeCommandBuffers(state.device, state.commandPool, 1, &frame->commandBuffer);

    for (uint32_t j = 0; j < frame->pool.count; j++) {
      gpu_scratchpad* scratchpad = &frame->pool.list[j];
      vkDestroyBuffer(state.device, scratchpad->buffer, NULL);
      vkUnmapMemory(state.device, scratchpad->memory);
      vkFreeMemory(state.device, scratchpad->memory, NULL);
    }

    free(frame->pool.list);
  }
  if (state.commandPool) vkDestroyCommandPool(state.device, state.commandPool, NULL);
  if (state.device) vkDestroyDevice(state.device, NULL);
  if (state.messenger) vkDestroyDebugUtilsMessengerEXT(state.instance, state.messenger, NULL);
  if (state.instance) vkDestroyInstance(state.instance, NULL);
#ifdef _WIN32
  FreeLibrary(state.library);
#else
  dlclose(state.library);
#endif
  memset(&state, 0, sizeof(state));
}

void gpu_frame_wait(void) {
  GPU_VK(vkWaitForFences(state.device, 1, &state.frame->fence, VK_FALSE, ~0ull));
  GPU_VK(vkResetFences(state.device, 1, &state.frame->fence));
  state.frame->pool.current = 0;
  state.frame->pool.cursor = 0;
  purge(state.frame);

  VkCommandBufferBeginInfo beginfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };

  GPU_VK(vkBeginCommandBuffer(state.frame->commandBuffer, &beginfo));
}

void gpu_frame_finish(void) {
  GPU_VK(vkEndCommandBuffer(state.frame->commandBuffer));

  VkSubmitInfo submitInfo = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pCommandBuffers = &state.frame->commandBuffer,
    .commandBufferCount = 1
  };

  GPU_VK(vkQueueSubmit(state.queue, 1, &submitInfo, state.frame->fence));

  state.frame = &state.frames[(state.frame->index + 1) % COUNTOF(state.frames)];
}

void gpu_render_begin(gpu_canvas* canvas) {
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

  vkCmdBeginRenderPass(state.frame->commandBuffer, &beginfo, VK_SUBPASS_CONTENTS_INLINE);
}

void gpu_render_finish(void) {
  vkCmdEndRenderPass(state.frame->commandBuffer);
}

void gpu_set_pipeline(gpu_pipeline* pipeline) {
  if (state.pipeline != pipeline) {
    vkCmdBindPipeline(state.frame->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);
    state.pipeline = pipeline;
  }
}

void gpu_set_vertex_buffers(gpu_buffer** buffers, uint64_t* offsets, uint32_t count) {
  VkBuffer handles[16];
  for (uint32_t i = 0; i < count; i++) {
    handles[i] = buffers[i]->handle;
  }
  vkCmdBindVertexBuffers(state.frame->commandBuffer, 0, count, handles, offsets);
}

void gpu_set_index_buffer(gpu_buffer* buffer, uint64_t offset) {
  vkCmdBindIndexBuffer(state.frame->commandBuffer, buffer->handle, offset, state.pipeline->indexType);
}

void gpu_draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) {
  vkCmdDraw(state.frame->commandBuffer, vertexCount, instanceCount, firstVertex, 0);
}

void gpu_draw_indexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex) {
  vkCmdDrawIndexed(state.frame->commandBuffer, indexCount, instanceCount, firstIndex, baseVertex, 0);
}

void gpu_draw_indirect(gpu_buffer* buffer, uint64_t offset, uint32_t drawCount) {
  vkCmdDrawIndirect(state.frame->commandBuffer, buffer->handle, offset, drawCount, 0);
}

void gpu_draw_indirect_indexed(gpu_buffer* buffer, uint64_t offset, uint32_t drawCount) {
  vkCmdDrawIndexedIndirect(state.frame->commandBuffer, buffer->handle, offset, drawCount, 0);
}

void gpu_compute(gpu_shader* shader, uint32_t x, uint32_t y, uint32_t z) {
  // TODO bind compute pipeline
  vkCmdDispatch(state.frame->commandBuffer, x, y, z);
}

void gpu_get_features(gpu_features* features) {
  *features = state.features;
}

void gpu_get_limits(gpu_limits* limits) {
  //
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

uint8_t* gpu_buffer_map(gpu_buffer* buffer, uint64_t offset, uint64_t size) {
  buffer->targetOffset = offset;
  buffer->targetSize = size;
  return gpu_map(size, &buffer->mapping);
}

void gpu_buffer_unmap(gpu_buffer* buffer) {
  if (buffer->mapping.frame != state.frame->index) {
    return;
  }

  VkBuffer source = state.frame->pool.list[buffer->mapping.scratchpad].buffer;
  VkBuffer destination = buffer->handle;

  VkBufferCopy copy = {
    .srcOffset = buffer->mapping.offset,
    .dstOffset = buffer->targetOffset,
    .size = buffer->targetSize
  };

  vkCmdCopyBuffer(state.frame->commandBuffer, source, destination, 1, &copy);
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

void gpu_texture_paste(gpu_texture* texture, uint8_t* data, uint64_t size, uint16_t offset[4], uint16_t extent[4], uint16_t mip) {
  if (size > SCRATCHPAD_SIZE) {
    // TODO loop or big boi staging buffer
    return;
  }

  gpu_mapping mapping;
  uint8_t* scratch = gpu_map(size, &mapping);
  memcpy(data, scratch, size);

  VkBuffer source = state.frames[mapping.frame].pool.list[mapping.scratchpad].buffer;
  VkImage destination = texture->handle;

  VkBufferImageCopy copy = {
    .bufferOffset = mapping.offset,
    .imageSubresource.aspectMask = texture->aspect,
    .imageSubresource.mipLevel = mip,
    .imageSubresource.baseArrayLayer = offset[3],
    .imageSubresource.layerCount = extent[3],
    .imageOffset = { offset[0], offset[1], offset[2] },
    .imageExtent = { extent[0], extent[1], extent[2] }
  };

  setLayout(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

  vkCmdCopyBufferToImage(state.frame->commandBuffer, source, destination, texture->layout, 1, &copy);
}

// Sampler

/*
size_t gpu_sizeof_sampler() {
  return sizeof(gpu_sampler);
}

bool gpu_sampler_init(gpu_sampler* sampler, gpu_sampler_info* info) {
  VkSamplerCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = info->mag == GPU_FILTER_NEAREST ? VK_FILTER_NEAREST : VK_FILTER_LINEAR,
    .minFilter = info->min == GPU_FILTER_NEAREST ? VK_FILTER_NEAREST : VK_FILTER_LINEAR,
    .mipmapMode = info->mip == GPU_FILTER_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = convertWrap(info->wrapu),
    .addressModeV = convertWrap(info->wrapv),
    .addressModeW = convertWrap(info->wrapw),
    .anisotropyEnable = info->anisotropy > 0.f,
    .maxAnisotropy = info->anisotropy
  };

  GPU_VK(vkCreateSampler(state.device, &createInfo, NULL, &sampler->handle));

  nickname(sampler, VK_OBJECT_TYPE_SAMPLER, info->label);

  return true;
}

void gpu_sampler_destroy(gpu_sampler* sampler) {
  if (sampler->handle) condemn(sampler->handle, VK_OBJECT_TYPE_SAMPLER);
  memset(sampler, 0, sizeof(*sampler));
}
*/

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

  return true;
}

void gpu_shader_destroy(gpu_shader* shader) {
  if (shader->handles[0]) condemn(shader->handles[0], VK_OBJECT_TYPE_SHADER_MODULE);
  if (shader->handles[1]) condemn(shader->handles[1], VK_OBJECT_TYPE_SHADER_MODULE);
  memset(shader, 0, sizeof(*shader));
}

// Pipeline

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

  static const VkIndexType types[] = {
    [GPU_INDEX_U16] = VK_INDEX_TYPE_UINT16,
    [GPU_INDEX_U32] = VK_INDEX_TYPE_UINT32
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
    .primitiveRestartEnable = true
  };

  VkPipelineViewportStateCreateInfo viewport = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports = &info->canvas->viewport
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
    .renderPass = info->canvas->handle
  };

  if (vkCreateGraphicsPipelines(state.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipeline->handle)) {
    return false;
  }

  nickname(pipeline, VK_OBJECT_TYPE_PIPELINE, info->label);
  pipeline->indexType = types[info->indexStride];
  return true;
}

void gpu_pipeline_destroy(gpu_pipeline* pipeline) {
  if (pipeline->handle) condemn(pipeline->handle, VK_OBJECT_TYPE_PIPELINE);
}

// Helpers

// Condemns an object, marking it for deletion (objects can't be destroyed while the GPU is still
// using them).  Condemned objects are purged in gpu_begin_frame after waiting on a fence.  We have
// to manage the freelist memory ourselves because the user could immediately free memory after
// destroying a resource.
// TODO currently there is a bug where condemning a resource outside of begin/end frame will
// immediately purge it on the next call to begin_frame (it should somehow get added to the previous
// frame's freelist, ugh, maybe advance frame index in begin_frame instead of end_frame)
// TODO even though this is fairly lightweight it might be worth doing some object tracking to see
// if you actually need to delay the destruction, and try to destroy it immediately when possible.
// Because Lua objects are GC'd we probably already have our own delay and could get away with
// skipping the freelist a lot of the time.  Also you might need to track object access anyway for
// barriers, so this could wombo combo with the condemnation.  It might be complicated though if you
// have to track access across multiple frames.
static void condemn(void* handle, VkObjectType type) {
  gpu_freelist* freelist = &state.frame->freelist;

  if (freelist->length >= freelist->capacity) {
    freelist->capacity = freelist->capacity ? (freelist->capacity * 2) : 1;
    freelist->data = realloc(freelist->data, freelist->capacity * sizeof(*freelist->data));
    GPU_CHECK(freelist->data, "Out of memory");
  }

  freelist->data[freelist->length++] = (gpu_ref) { type, handle };
}

static void purge(gpu_frame* frame) {
  for (size_t i = 0; i < frame->freelist.length; i++) {
    gpu_ref* ref = &frame->freelist.data[i];
    switch (ref->type) {
      case VK_OBJECT_TYPE_BUFFER: vkDestroyBuffer(state.device, ref->handle, NULL); break;
      case VK_OBJECT_TYPE_IMAGE: vkDestroyImage(state.device, ref->handle, NULL); break;
      case VK_OBJECT_TYPE_DEVICE_MEMORY: vkFreeMemory(state.device, ref->handle, NULL); break;
      case VK_OBJECT_TYPE_IMAGE_VIEW: vkDestroyImageView(state.device, ref->handle, NULL); break;
      case VK_OBJECT_TYPE_SAMPLER: vkDestroySampler(state.device, ref->handle, NULL); break;
      case VK_OBJECT_TYPE_RENDER_PASS: vkDestroyRenderPass(state.device, ref->handle, NULL); break;
      case VK_OBJECT_TYPE_FRAMEBUFFER: vkDestroyFramebuffer(state.device, ref->handle, NULL); break;
      case VK_OBJECT_TYPE_PIPELINE: vkDestroyPipeline(state.device, ref->handle, NULL); break;
      default: GPU_THROW("Unreachable"); break;
    }
  }
  frame->freelist.length = 0;
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
    .pName = "main",
    .pSpecializationInfo = NULL
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
  vkCmdPipelineBarrier(state.frame->commandBuffer, waitFor, nextStages, 0, 0, NULL, 0, NULL, 1, &barrier);
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

static VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT flags, const VkDebugUtilsMessengerCallbackDataEXT* data, void* context) {
  if (state.config.callback) {
    bool severe = severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    state.config.callback(state.config.context, data->pMessage, severe);
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
