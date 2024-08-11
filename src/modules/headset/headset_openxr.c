#include "headset/headset.h"
#include "data/blob.h"
#include "data/image.h"
#include "data/modelData.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "core/maf.h"
#include "core/os.h"
#include "util.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(_WIN32)
 #define XR_USE_PLATFORM_WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <unknwn.h>
 #include <windows.h>
 #define XR_FOREACH_PLATFORM(X) X(xrConvertWin32PerformanceCounterToTimeKHR)
#else
 #if defined(__ANDROID__)
  #define XR_USE_PLATFORM_ANDROID
  void* os_get_java_vm(void);
  void* os_get_jni_context(void);
  #include <jni.h>
 #endif
 #include <time.h>
 #define XR_USE_TIMESPEC
 #define XR_FOREACH_PLATFORM(X) X(xrConvertTimespecTimeToTimeKHR)
#endif

#ifdef LOVR_VK
#define XR_USE_GRAPHICS_API_VULKAN
uintptr_t gpu_vk_get_instance(void);
uintptr_t gpu_vk_get_physical_device(void);
uintptr_t gpu_vk_get_device(void);
uintptr_t gpu_vk_get_queue(uint32_t* queueFamilyIndex, uint32_t* queueIndex);
#include <vulkan/vulkan.h>
#endif

#define XR_NO_PROTOTYPES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#define XR(f, s) xrthrow(f, s)
#define XR_INIT(f, s) if (!xrwarn(f, s)) return openxr_destroy(), false;
#define SESSION_ACTIVE(s) (s >= XR_SESSION_STATE_READY && s <= XR_SESSION_STATE_FOCUSED)
#define MAX_IMAGES 4
#define MAX_HAND_JOINTS 27

#define XR_FOREACH(X)\
  X(xrDestroyInstance)\
  X(xrGetInstanceProperties)\
  X(xrPollEvent)\
  X(xrResultToString)\
  X(xrGetSystem)\
  X(xrGetSystemProperties)\
  X(xrCreateVulkanInstanceKHR)\
  X(xrGetVulkanGraphicsDevice2KHR)\
  X(xrCreateVulkanDeviceKHR)\
  X(xrCreateSession)\
  X(xrDestroySession)\
  X(xrEnumerateReferenceSpaces)\
  X(xrCreateReferenceSpace)\
  X(xrGetReferenceSpaceBoundsRect)\
  X(xrCreateActionSpace)\
  X(xrLocateSpace)\
  X(xrDestroySpace)\
  X(xrEnumerateViewConfigurations)\
  X(xrEnumerateViewConfigurationViews)\
  X(xrEnumerateEnvironmentBlendModes)\
  X(xrEnumerateSwapchainFormats)\
  X(xrCreateSwapchain)\
  X(xrDestroySwapchain)\
  X(xrEnumerateSwapchainImages)\
  X(xrAcquireSwapchainImage)\
  X(xrWaitSwapchainImage)\
  X(xrReleaseSwapchainImage)\
  X(xrBeginSession)\
  X(xrEndSession)\
  X(xrWaitFrame)\
  X(xrBeginFrame)\
  X(xrEndFrame)\
  X(xrLocateViews)\
  X(xrStringToPath)\
  X(xrCreateActionSet)\
  X(xrDestroyActionSet)\
  X(xrCreateAction)\
  X(xrDestroyAction)\
  X(xrSuggestInteractionProfileBindings)\
  X(xrAttachSessionActionSets)\
  X(xrGetActionStateBoolean)\
  X(xrGetActionStateFloat)\
  X(xrGetActionStatePose)\
  X(xrSyncActions)\
  X(xrApplyHapticFeedback)\
  X(xrStopHapticFeedback)\
  X(xrCreateHandTrackerEXT)\
  X(xrDestroyHandTrackerEXT)\
  X(xrLocateHandJointsEXT)\
  X(xrGetHandMeshFB)\
  X(xrGetControllerModelKeyMSFT)\
  X(xrLoadControllerModelMSFT)\
  X(xrGetControllerModelPropertiesMSFT)\
  X(xrGetControllerModelStateMSFT)\
  X(xrGetDisplayRefreshRateFB)\
  X(xrEnumerateDisplayRefreshRatesFB)\
  X(xrRequestDisplayRefreshRateFB)\
  X(xrQuerySystemTrackedKeyboardFB)\
  X(xrCreateKeyboardSpaceFB)\
  X(xrCreatePassthroughFB)\
  X(xrDestroyPassthroughFB)\
  X(xrPassthroughStartFB)\
  X(xrPassthroughPauseFB)\
  X(xrCreatePassthroughLayerFB)\
  X(xrDestroyPassthroughLayerFB)

#define XR_DECLARE(fn) static PFN_##fn fn;
#define XR_LOAD(fn) xrGetInstanceProcAddr(state.instance, #fn, (PFN_xrVoidFunction*) &fn);
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function);
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char* layerName, uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrExtensionProperties* properties);
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo* createInfo, XrInstance* instance);
XR_FOREACH(XR_DECLARE)
XR_FOREACH_PLATFORM(XR_DECLARE)

enum {
  ACTION_PINCH_POSE,
  ACTION_POKE_POSE,
  ACTION_GRIP_POSE,
  ACTION_POINTER_POSE,
  ACTION_TRACKER_POSE,
  ACTION_GAZE_POSE,
  ACTION_TRIGGER_DOWN,
  ACTION_TRIGGER_TOUCH,
  ACTION_TRIGGER_AXIS,
  ACTION_TRACKPAD_DOWN,
  ACTION_TRACKPAD_TOUCH,
  ACTION_TRACKPAD_X,
  ACTION_TRACKPAD_Y,
  ACTION_THUMBSTICK_DOWN,
  ACTION_THUMBSTICK_TOUCH,
  ACTION_THUMBSTICK_X,
  ACTION_THUMBSTICK_Y,
  ACTION_MENU_DOWN,
  ACTION_MENU_TOUCH,
  ACTION_GRIP_DOWN,
  ACTION_GRIP_TOUCH,
  ACTION_GRIP_AXIS,
  ACTION_A_DOWN,
  ACTION_A_TOUCH,
  ACTION_B_DOWN,
  ACTION_B_TOUCH,
  ACTION_X_DOWN,
  ACTION_X_TOUCH,
  ACTION_Y_DOWN,
  ACTION_Y_TOUCH,
  ACTION_THUMBREST_TOUCH,
  ACTION_VIBRATE,
  MAX_ACTIONS
};

typedef struct {
  XrSwapchain handle;
  uint32_t textureIndex;
  uint32_t textureCount;
  Texture* textures[MAX_IMAGES];
  bool acquired;
} Swapchain;

struct Layer {
  uint32_t ref;
  uint32_t width;
  uint32_t height;
  Swapchain swapchain;
  XrCompositionLayerQuad info;
  XrCompositionLayerDepthTestFB depthTest;
  XrCompositionLayerSettingsFB settings;
  Pass* pass;
};

enum { COLOR, DEPTH };

static struct {
  HeadsetConfig config;
  XrInstance instance;
  XrSystemId system;
  XrSession session;
  XrSessionState sessionState;
  XrSpace referenceSpace;
  float* refreshRates;
  uint32_t refreshRateCount;
  XrEnvironmentBlendMode* blendModes;
  XrEnvironmentBlendMode blendMode;
  uint32_t blendModeCount;
  XrSpace spaces[MAX_DEVICES];
  TextureFormat depthFormat;
  Pass* pass;
  Swapchain swapchains[2];
  XrCompositionLayerProjection layer;
  XrCompositionLayerProjectionView layerViews[2];
  XrCompositionLayerDepthInfoKHR depthInfo[2];
  XrCompositionLayerPassthroughFB passthroughLayer;
  Layer* layers[MAX_LAYERS];
  uint32_t layerCount;
  XrFrameState frameState;
  XrTime lastDisplayTime;
  XrTime epoch;
  uint32_t width;
  uint32_t height;
  float clipNear;
  float clipFar;
  bool waited;
  bool began;
  XrActionSet actionSet;
  XrAction actions[MAX_ACTIONS];
  XrPath actionFilters[MAX_DEVICES];
  XrHandTrackerEXT handTrackers[2];
  XrControllerModelKeyMSFT controllerModelKeys[2];
  XrPassthroughFB passthrough;
  XrPassthroughLayerFB passthroughLayerHandle;
  bool passthroughActive;
  bool mounted;
  struct {
    bool controllerModel;
    bool depth;
    bool gaze;
    bool handInteraction;
    bool handTracking;
    bool handTrackingAim;
    bool handTrackingElbow;
    bool handTrackingMesh;
    bool headless;
    bool keyboardTracking;
    bool layerDepthTest;
    bool layerSettings;
    bool localFloor;
    bool ml2Controller;
    bool overlay;
    bool picoController;
    bool presence;
    bool questPassthrough;
    bool refreshRate;
    bool viveTrackers;
  } features;
} state;

static bool xrwarn(XrResult result, const char* message) {
  if (XR_SUCCEEDED(result)) return true;
  char errorCode[XR_MAX_RESULT_STRING_SIZE];
  if (state.instance && XR_SUCCEEDED(xrResultToString(state.instance, result, errorCode))) {
    lovrLog(LOG_WARN, "XR", "OpenXR failed to start: %s (%s)", message, errorCode);
  } else {
    lovrLog(LOG_WARN, "XR", "OpenXR failed to start: %s (%d)", message, result);
  }
  return false;
}

static bool xrthrow(XrResult result, const char* message) {
  if (XR_SUCCEEDED(result)) return true;
  char errorCode[XR_MAX_RESULT_STRING_SIZE];
  if (state.instance && XR_SUCCEEDED(xrResultToString(state.instance, result, errorCode))) {
    lovrThrow("OpenXR Error: %s (%s)", message, errorCode);
  } else {
    lovrThrow("OpenXR Error: %s (%d)", message, result);
  }
  return false;
}

static bool hasExtension(XrExtensionProperties* extensions, uint32_t count, const char* extension) {
  for (uint32_t i = 0; i < count; i++) {
    if (!strcmp(extensions[i].extensionName, extension)) {
      return true;
    }
  }
  return false;
}

static XrTime getCurrentXrTime(void) {
  XrTime time;
#ifdef _WIN32
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  XR(xrConvertWin32PerformanceCounterToTimeKHR(state.instance, &t, &time), "Failed to get time");
#else
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  XR(xrConvertTimespecTimeToTimeKHR(state.instance, &t, &time), "Failed to get time");
#endif
  return time;
}

static bool openxr_getDriverName(char* name, size_t length);
static void createReferenceSpace(XrTime time) {
  XrReferenceSpaceCreateInfo info = {
    .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
    .poseInReferenceSpace = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } }
  };

  // Reference space doesn't need to be recreated for seated experiences (those always use local
  // space), or when local-floor is supported.  Otherwise, vertical offset must be re-measured.
  if (state.referenceSpace && (state.features.localFloor || state.config.seated)) {
    return;
  }

  if (state.features.localFloor) {
    info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
  } else if (state.config.seated) {
    info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  } else if (state.spaces[DEVICE_FLOOR]) {
    XrSpace local;
    info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    XR(xrCreateReferenceSpace(state.session, &info, &local), "Failed to create local space");

    XrSpaceLocation location = { .type = XR_TYPE_SPACE_LOCATION };
    XR(xrLocateSpace(state.spaces[DEVICE_FLOOR], local, time, &location), "Failed to locate space");
    XR(xrDestroySpace(local), "Failed to destroy local space");

    if (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
      info.poseInReferenceSpace.position.y = location.pose.position.y;
    } else {
      info.poseInReferenceSpace.position.y = -1.7f;
    }
  } else {
    info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    info.poseInReferenceSpace.position.y = -1.7f;
  }

  if (state.referenceSpace) {
    XR(xrDestroySpace(state.referenceSpace), "Failed to destroy reference space");
  }

  XR(xrCreateReferenceSpace(state.session, &info, &state.referenceSpace), "Failed to create reference space");
}

static XrAction getPoseActionForDevice(Device device) {
  switch (device) {
    case DEVICE_HEAD:
      return XR_NULL_HANDLE; // Uses reference space
    case DEVICE_HAND_LEFT:
    case DEVICE_HAND_RIGHT:
    case DEVICE_HAND_LEFT_GRIP:
    case DEVICE_HAND_RIGHT_GRIP:
      return state.actions[ACTION_GRIP_POSE];
    case DEVICE_HAND_LEFT_PINCH:
    case DEVICE_HAND_RIGHT_PINCH:
      return state.features.handInteraction ? state.actions[ACTION_PINCH_POSE] : XR_NULL_HANDLE;
    case DEVICE_HAND_LEFT_POKE:
    case DEVICE_HAND_RIGHT_POKE:
      return state.features.handInteraction ? state.actions[ACTION_POKE_POSE] : XR_NULL_HANDLE;
    case DEVICE_HAND_LEFT_POINT:
    case DEVICE_HAND_RIGHT_POINT:
      return state.actions[ACTION_POINTER_POSE];
    case DEVICE_ELBOW_LEFT:
    case DEVICE_ELBOW_RIGHT:
    case DEVICE_SHOULDER_LEFT:
    case DEVICE_SHOULDER_RIGHT:
    case DEVICE_CHEST:
    case DEVICE_WAIST:
    case DEVICE_KNEE_LEFT:
    case DEVICE_KNEE_RIGHT:
    case DEVICE_FOOT_LEFT:
    case DEVICE_FOOT_RIGHT:
    case DEVICE_CAMERA:
    case DEVICE_KEYBOARD:
      return state.features.viveTrackers ? state.actions[ACTION_TRACKER_POSE] : XR_NULL_HANDLE;
    case DEVICE_EYE_GAZE:
      return state.actions[ACTION_GAZE_POSE];
    default:
      return XR_NULL_HANDLE;
  }
}

// Hand trackers are created lazily because on some implementations xrCreateHandTrackerEXT will
// return XR_ERROR_FEATURE_UNSUPPORTED if called too early.
static XrHandTrackerEXT getHandTracker(Device device) {
  if (!state.features.handTracking || (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT)) {
    return XR_NULL_HANDLE;
  }

  XrHandTrackerEXT* tracker = &state.handTrackers[device == DEVICE_HAND_RIGHT];

  if (!*tracker) {
    XrHandTrackerCreateInfoEXT info = {
      .type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
      .handJointSet = state.features.handTrackingElbow ?
        XR_HAND_JOINT_SET_HAND_WITH_FOREARM_ULTRALEAP :
        XR_HAND_JOINT_SET_DEFAULT_EXT,
      .hand = device == DEVICE_HAND_RIGHT ? XR_HAND_RIGHT_EXT : XR_HAND_LEFT_EXT
    };

    if (XR_FAILED(xrCreateHandTrackerEXT(state.session, &info, tracker))) {
      return XR_NULL_HANDLE;
    }
  }

  return *tracker;
}

// Controller model keys are created lazily because the runtime is allowed to
// return XR_NULL_CONTROLLER_MODEL_KEY_MSFT until it is ready.
static XrControllerModelKeyMSFT getControllerModelKey(Device device) {
  if (!state.features.controllerModel || (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT)) {
    return XR_NULL_CONTROLLER_MODEL_KEY_MSFT;
  }

  XrControllerModelKeyMSFT* modelKey = &state.controllerModelKeys[device == DEVICE_HAND_RIGHT];

  if (!*modelKey) {
    XrControllerModelKeyStateMSFT modelKeyState = {
      .type = XR_TYPE_CONTROLLER_MODEL_KEY_STATE_MSFT,
    };

    if (XR_FAILED(xrGetControllerModelKeyMSFT(state.session, state.actionFilters[device], &modelKeyState))) {
      return XR_NULL_CONTROLLER_MODEL_KEY_MSFT;
    }

    *modelKey = modelKeyState.modelKey;
  }

  return *modelKey;
}

static void swapchain_init(Swapchain* swapchain, uint32_t width, uint32_t height, bool stereo, bool depth) {
  XrSwapchainCreateInfo info = {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .width = width,
    .height = height,
    .sampleCount = 1,
    .faceCount = 1,
    .arraySize = 1 << stereo,
    .mipCount = 1
  };

  if (depth) {
    info.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    switch (state.depthFormat) {
      case FORMAT_D24: info.format = VK_FORMAT_X8_D24_UNORM_PACK32; break;
      case FORMAT_D32F: info.format = VK_FORMAT_D32_SFLOAT; break;
      case FORMAT_D24S8: info.format = VK_FORMAT_D24_UNORM_S8_UINT; break;
      case FORMAT_D32FS8: info.format = VK_FORMAT_D32_SFLOAT_S8_UINT; break;
      default: lovrUnreachable();
    }
  } else {
    info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    info.format = VK_FORMAT_R8G8B8A8_SRGB;
  }

  XR(xrCreateSwapchain(state.session, &info, &swapchain->handle), "Failed to create swapchain");

#ifdef LOVR_VK
  XrSwapchainImageVulkanKHR images[MAX_IMAGES];
  for (uint32_t i = 0; i < MAX_IMAGES; i++) {
    images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
    images[i].next = NULL;
  }
#else
#error "Unsupported graphics backend"
#endif

  XR(xrEnumerateSwapchainImages(swapchain->handle, MAX_IMAGES, &swapchain->textureCount, (XrSwapchainImageBaseHeader*) images), "Failed to query swapchain images");

  for (uint32_t i = 0; i < swapchain->textureCount; i++) {
    swapchain->textures[i] = lovrTextureCreate(&(TextureInfo) {
      .type = stereo ? TEXTURE_ARRAY : TEXTURE_2D,
      .format = depth ? state.depthFormat : FORMAT_RGBA8,
      .srgb = !depth,
      .width = width,
      .height = height,
      .layers = 1 << stereo,
      .mipmaps = 1,
      .usage = TEXTURE_RENDER | (depth ? 0 : TEXTURE_SAMPLE),
      .handle = (uintptr_t) images[i].image,
      .label = "OpenXR Swapchain",
      .xr = true
    });
  }
}

static void swapchain_destroy(Swapchain* swapchain) {
  if (!swapchain->handle) return;
  for (uint32_t i = 0; i < swapchain->textureCount; i++) {
    lovrRelease(swapchain->textures[i], lovrTextureDestroy);
  }
  xrDestroySwapchain(swapchain->handle);
  swapchain->handle = XR_NULL_HANDLE;
}

static Texture* swapchain_acquire(Swapchain* swapchain) {
  if (!swapchain->acquired) {
    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .timeout = XR_INFINITE_DURATION };
    XR(xrAcquireSwapchainImage(swapchain->handle, NULL, &swapchain->textureIndex), "Failed to acquire swapchain image");
    XR(xrWaitSwapchainImage(swapchain->handle, &waitInfo), "Failed to wait on swapchain image");
    swapchain->acquired = true;
  }

  return swapchain->textures[swapchain->textureIndex];
}

static void swapchain_release(Swapchain* swapchain) {
  if (swapchain->handle && swapchain->acquired) {
    XR(xrReleaseSwapchainImage(swapchain->handle, NULL), "Failed to release swapchain image");
    swapchain->acquired = false;
  }
}

static void openxr_getVulkanPhysicalDevice(void* instance, uintptr_t physicalDevice) {
  XrVulkanGraphicsDeviceGetInfoKHR info = {
    .type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
    .systemId = state.system,
    .vulkanInstance = (VkInstance) instance
  };

  XR(xrGetVulkanGraphicsDevice2KHR(state.instance, &info, (VkPhysicalDevice*) physicalDevice), "Failed to get Vulkan graphics device");
}

static uint32_t openxr_createVulkanInstance(void* instanceCreateInfo, void* allocator, uintptr_t instance, void* getInstanceProcAddr) {
  XrVulkanInstanceCreateInfoKHR info = {
    .type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
    .systemId = state.system,
    .pfnGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) getInstanceProcAddr,
    .vulkanCreateInfo = instanceCreateInfo,
    .vulkanAllocator = allocator
  };

  VkResult result;
  XR(xrCreateVulkanInstanceKHR(state.instance, &info, (VkInstance*) instance, &result), "Failed to create Vulkan instance");
  return result;
}

static uint32_t openxr_createVulkanDevice(void* instance, void* deviceCreateInfo, void* allocator, uintptr_t device, void* getInstanceProcAddr) {
  XrVulkanDeviceCreateInfoKHR info = {
    .type = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
    .systemId = state.system,
    .pfnGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) getInstanceProcAddr,
    .vulkanPhysicalDevice = (VkPhysicalDevice) gpu_vk_get_physical_device(),
    .vulkanCreateInfo = deviceCreateInfo,
    .vulkanAllocator = allocator
  };

  VkResult result;
  XR(xrCreateVulkanDeviceKHR(state.instance, &info, (VkDevice*) device, &result), "Failed to create Vulkan device");
  return result;
}

static uintptr_t openxr_getOpenXRInstanceHandle(void) {
  return (uintptr_t) state.instance;
}

static uintptr_t openxr_getOpenXRSessionHandle(void) {
  return (uintptr_t) state.session;
}

static void openxr_destroy();
static void openxr_setClipDistance(float clipNear, float clipFar);

static bool openxr_init(HeadsetConfig* config) {
  state.config = *config;

  // Loader
#if defined(__ANDROID__)
  static PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
  XR_LOAD(xrInitializeLoaderKHR);
  if (!xrInitializeLoaderKHR) {
    return false;
  }

  XrLoaderInitInfoAndroidKHR loaderInfo = {
    .type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,
    .applicationVM = os_get_java_vm(),
    .applicationContext = os_get_jni_context()
  };

  if (XR_FAILED(xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*) &loaderInfo))) {
    return false;
  }
#elif defined(__linux__) || defined(__APPLE__)
  setenv("XR_LOADER_DEBUG", "none", 0);
#elif defined(_WIN32)
  if (GetEnvironmentVariable("XR_LOADER_DEBUG", NULL, 0) == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
    SetEnvironmentVariable("XR_LOADER_DEBUG", "none");
  }
#endif

  { // Instance
    uint32_t extensionCount;
    XrResult result = xrEnumerateInstanceExtensionProperties(NULL, 0, &extensionCount, NULL);

    if (result == XR_ERROR_RUNTIME_UNAVAILABLE) {
      return openxr_destroy(), false;
    } else {
      XR_INIT(result, "Failed to query extensions");
    }

    XrExtensionProperties* extensionProperties = lovrCalloc(extensionCount * sizeof(*extensionProperties));
    for (uint32_t i = 0; i < extensionCount; i++) extensionProperties[i].type = XR_TYPE_EXTENSION_PROPERTIES;
    xrEnumerateInstanceExtensionProperties(NULL, extensionCount, &extensionCount, extensionProperties);

    // Extensions with feature == NULL must be present.  The enable flag can be used to
    // conditionally enable extensions based on config, platform, etc.
    struct { const char* name; bool* feature; bool enable; } extensions[] = {
#ifdef LOVR_VK
      { "XR_KHR_vulkan_enable2", NULL, true },
#endif
#ifdef __ANDROID__
      { "XR_KHR_android_create_instance", NULL, true },
#endif
      { "XR_KHR_composition_layer_depth", &state.features.depth, config->submitDepth },
#ifdef _WIN32
      { "XR_KHR_win32_convert_performance_counter_time", NULL, true },
#else
      { "XR_KHR_convert_timespec_time", NULL, true },
#endif
      { "XR_EXT_eye_gaze_interaction", &state.features.gaze, true },
      { "XR_EXT_hand_interaction", &state.features.handInteraction, true },
      { "XR_EXT_hand_tracking", &state.features.handTracking, true },
      { "XR_EXT_local_floor", &state.features.localFloor, true },
      { "XR_EXT_user_presence", &state.features.presence, true },
      { "XR_BD_controller_interaction", &state.features.picoController, true },
      { "XR_FB_composition_layer_depth_test", &state.features.layerDepthTest, true },
      { "XR_FB_composition_layer_settings", &state.features.layerSettings, true },
      { "XR_FB_display_refresh_rate", &state.features.refreshRate, true },
      { "XR_FB_hand_tracking_aim", &state.features.handTrackingAim, true },
      { "XR_FB_hand_tracking_mesh", &state.features.handTrackingMesh, true },
      { "XR_FB_keyboard_tracking", &state.features.keyboardTracking, true },
      { "XR_FB_passthrough", &state.features.questPassthrough, true },
      { "XR_ML_ml2_controller_interaction", &state.features.ml2Controller, true },
      { "XR_MND_headless", &state.features.headless, true },
      { "XR_MSFT_controller_model", &state.features.controllerModel, true },
      { "XR_ULTRALEAP_hand_tracking_forearm", &state.features.handTrackingElbow, true },
      { "XR_EXTX_overlay", &state.features.overlay, config->overlay },
      { "XR_HTCX_vive_tracker_interaction", &state.features.viveTrackers, true }
    };

    uint32_t enabledExtensionCount = 0;
    const char* enabledExtensionNames[COUNTOF(extensions)];
    for (uint32_t i = 0; i < COUNTOF(extensions); i++) {
      if (!extensions[i].enable) continue;
      if (!extensions[i].feature || hasExtension(extensionProperties, extensionCount, extensions[i].name)) {
        enabledExtensionNames[enabledExtensionCount++] = extensions[i].name;
        if (extensions[i].feature) *extensions[i].feature = true;
      }
    }

    lovrFree(extensionProperties);

#ifdef __ANDROID__
    XrInstanceCreateInfoAndroidKHR androidInfo = {
      .type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR,
      .applicationVM = os_get_java_vm(),
      .applicationActivity = os_get_jni_context(),
      .next = NULL
    };
#endif

    XrInstanceCreateInfo info = {
      .type = XR_TYPE_INSTANCE_CREATE_INFO,
#ifdef __ANDROID__
      .next = &androidInfo,
#endif
      .applicationInfo.engineName = "LÖVR",
      .applicationInfo.engineVersion = (LOVR_VERSION_MAJOR << 24) + (LOVR_VERSION_MINOR << 16) + LOVR_VERSION_PATCH,
      .applicationInfo.applicationName = "LÖVR",
      .applicationInfo.applicationVersion = 0,
      .applicationInfo.apiVersion = XR_API_VERSION_1_0,
      .enabledExtensionCount = enabledExtensionCount,
      .enabledExtensionNames = enabledExtensionNames
    };

    XR_INIT(xrCreateInstance(&info, &state.instance), "Failed to create instance");
    XR_FOREACH(XR_LOAD)
    XR_FOREACH_PLATFORM(XR_LOAD)
  }

  { // System
    XrSystemGetInfo info = {
      .type = XR_TYPE_SYSTEM_GET_INFO,
      .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY
    };

    XR_INIT(xrGetSystem(state.instance, &info, &state.system), "Failed to query system");

    XrSystemEyeGazeInteractionPropertiesEXT eyeGazeProperties = { .type = XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT };
    XrSystemHandTrackingPropertiesEXT handTrackingProperties = { .type = XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT };
    XrSystemKeyboardTrackingPropertiesFB keyboardTrackingProperties = { .type = XR_TYPE_SYSTEM_KEYBOARD_TRACKING_PROPERTIES_FB };
    XrSystemUserPresencePropertiesEXT presenceProperties = { .type = XR_TYPE_SYSTEM_USER_PRESENCE_PROPERTIES_EXT };
    XrSystemPassthroughProperties2FB passthroughProperties = { .type = XR_TYPE_SYSTEM_PASSTHROUGH_PROPERTIES2_FB };
    XrSystemProperties properties = { .type = XR_TYPE_SYSTEM_PROPERTIES };

    if (state.features.gaze) {
      eyeGazeProperties.next = properties.next;
      properties.next = &eyeGazeProperties;
    }

    if (state.features.handTracking) {
      handTrackingProperties.next = properties.next;
      properties.next = &handTrackingProperties;
    }

    if (state.features.keyboardTracking) {
      keyboardTrackingProperties.next = properties.next;
      properties.next = &keyboardTrackingProperties;
    }

    if (state.features.presence) {
      presenceProperties.next = properties.next;
      properties.next = &presenceProperties;
    }

    if (state.features.questPassthrough) {
      passthroughProperties.next = properties.next;
      properties.next = &passthroughProperties;
    }

    XR_INIT(xrGetSystemProperties(state.instance, state.system, &properties), "Failed to query system properties");
    state.features.gaze = eyeGazeProperties.supportsEyeGazeInteraction;
    state.features.handTracking = handTrackingProperties.supportsHandTracking;
    state.features.keyboardTracking = keyboardTrackingProperties.supportsKeyboardTracking;
    state.features.presence = presenceProperties.supportsUserPresence;
    state.features.questPassthrough = passthroughProperties.capabilities & XR_PASSTHROUGH_CAPABILITY_BIT_FB;

    uint32_t viewConfigurationCount;
    XrViewConfigurationType viewConfigurations[2];
    XR_INIT(xrEnumerateViewConfigurations(state.instance, state.system, 2, &viewConfigurationCount, viewConfigurations), "Failed to query view configurations");

    uint32_t viewCount;
    XrViewConfigurationView views[2] = { [0].type = XR_TYPE_VIEW_CONFIGURATION_VIEW, [1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW };
    XR_INIT(xrEnumerateViewConfigurationViews(state.instance, state.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, NULL), "Failed to query view configurations");
    XR_INIT(xrEnumerateViewConfigurationViews(state.instance, state.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewCount, views), "Failed to query view configurations");

    if ( // Only 2 views are supported, and since they're rendered together they must be identical
      viewCount != 2 ||
      views[0].recommendedSwapchainSampleCount != views[1].recommendedSwapchainSampleCount ||
      views[0].recommendedImageRectWidth != views[1].recommendedImageRectWidth ||
      views[0].recommendedImageRectHeight != views[1].recommendedImageRectHeight
    ) {
      openxr_destroy();
      return false;
    }

    state.width = MIN(views[0].recommendedImageRectWidth * config->supersample, views[0].maxImageRectWidth);
    state.height = MIN(views[0].recommendedImageRectHeight * config->supersample, views[0].maxImageRectHeight);

    // Blend modes
    XR_INIT(xrEnumerateEnvironmentBlendModes(state.instance, state.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &state.blendModeCount, NULL), "Failed to query blend modes");
    state.blendModes = lovrMalloc(state.blendModeCount * sizeof(XrEnvironmentBlendMode));
    XR_INIT(xrEnumerateEnvironmentBlendModes(state.instance, state.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, state.blendModeCount, &state.blendModeCount, state.blendModes), "Failed to query blend modes");
    state.blendMode = state.blendModes[0];
  }

  { // Actions
    XrActionSetCreateInfo info = {
      .type = XR_TYPE_ACTION_SET_CREATE_INFO,
      .localizedActionSetName = "Default",
      .actionSetName = "default"
    };

    XR_INIT(xrCreateActionSet(state.instance, &info, &state.actionSet), "Failed to create action set");

    // Subaction paths, for filtering actions by device
    XR_INIT(xrStringToPath(state.instance, "/user/hand/left", &state.actionFilters[DEVICE_HAND_LEFT]), "Failed to create path");
    XR_INIT(xrStringToPath(state.instance, "/user/hand/right", &state.actionFilters[DEVICE_HAND_RIGHT]), "Failed to create path");

    state.actionFilters[DEVICE_HAND_LEFT_GRIP] = state.actionFilters[DEVICE_HAND_LEFT];
    state.actionFilters[DEVICE_HAND_LEFT_POINT] = state.actionFilters[DEVICE_HAND_LEFT];
    state.actionFilters[DEVICE_HAND_LEFT_PINCH] = state.actionFilters[DEVICE_HAND_LEFT];
    state.actionFilters[DEVICE_HAND_LEFT_POKE] = state.actionFilters[DEVICE_HAND_LEFT];
    state.actionFilters[DEVICE_HAND_RIGHT_GRIP] = state.actionFilters[DEVICE_HAND_RIGHT];
    state.actionFilters[DEVICE_HAND_RIGHT_POINT] = state.actionFilters[DEVICE_HAND_RIGHT];
    state.actionFilters[DEVICE_HAND_RIGHT_PINCH] = state.actionFilters[DEVICE_HAND_RIGHT];
    state.actionFilters[DEVICE_HAND_RIGHT_POKE] = state.actionFilters[DEVICE_HAND_RIGHT];

    if (state.features.viveTrackers) {
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/left_elbow", &state.actionFilters[DEVICE_ELBOW_LEFT]), "Failed to create path");
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/right_elbow", &state.actionFilters[DEVICE_ELBOW_RIGHT]), "Failed to create path");
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/left_shoulder", &state.actionFilters[DEVICE_SHOULDER_LEFT]), "Failed to create path");
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/right_shoulder", &state.actionFilters[DEVICE_SHOULDER_RIGHT]), "Failed to create path");
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/chest", &state.actionFilters[DEVICE_CHEST]), "Failed to create path");
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/waist", &state.actionFilters[DEVICE_WAIST]), "Failed to create path");
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/left_knee", &state.actionFilters[DEVICE_KNEE_LEFT]), "Failed to create path");
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/right_knee", &state.actionFilters[DEVICE_KNEE_RIGHT]), "Failed to create path");
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/left_foot", &state.actionFilters[DEVICE_FOOT_LEFT]), "Failed to create path");
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/right_foot", &state.actionFilters[DEVICE_FOOT_RIGHT]), "Failed to create path");
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/camera", &state.actionFilters[DEVICE_CAMERA]), "Failed to create path");
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/keyboard", &state.actionFilters[DEVICE_KEYBOARD]), "Failed to create path");
    }

    XrPath hands[] = {
      state.actionFilters[DEVICE_HAND_LEFT],
      state.actionFilters[DEVICE_HAND_RIGHT]
    };

    XrPath trackers[] = {
      state.actionFilters[DEVICE_ELBOW_LEFT],
      state.actionFilters[DEVICE_ELBOW_RIGHT],
      state.actionFilters[DEVICE_SHOULDER_LEFT],
      state.actionFilters[DEVICE_SHOULDER_RIGHT],
      state.actionFilters[DEVICE_CHEST],
      state.actionFilters[DEVICE_WAIST],
      state.actionFilters[DEVICE_KNEE_LEFT],
      state.actionFilters[DEVICE_KNEE_RIGHT],
      state.actionFilters[DEVICE_FOOT_LEFT],
      state.actionFilters[DEVICE_FOOT_RIGHT],
      state.actionFilters[DEVICE_CAMERA],
      state.actionFilters[DEVICE_KEYBOARD]
    };

    XrActionCreateInfo actionInfo[] = {
      { 0, NULL, "pinch_pose",       XR_ACTION_TYPE_POSE_INPUT,       2, hands, "Pinch Pose" },
      { 0, NULL, "poke_pose",        XR_ACTION_TYPE_POSE_INPUT,       2, hands, "Poke Pose" },
      { 0, NULL, "grip_pose",        XR_ACTION_TYPE_POSE_INPUT,       2, hands, "Grip Pose" },
      { 0, NULL, "pointer_pose",     XR_ACTION_TYPE_POSE_INPUT,       2, hands, "Pointer Pose" },
      { 0, NULL, "tracker_pose",     XR_ACTION_TYPE_POSE_INPUT,       12, trackers, "Tracker Pose" },
      { 0, NULL, "gaze_pose",        XR_ACTION_TYPE_POSE_INPUT,       0, NULL, "Gaze Pose" },
      { 0, NULL, "trigger_down",     XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Trigger Down" },
      { 0, NULL, "trigger_touch",    XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Trigger Touch" },
      { 0, NULL, "trigger_axis" ,    XR_ACTION_TYPE_FLOAT_INPUT,      2, hands, "Trigger Axis" },
      { 0, NULL, "trackpad_down" ,   XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Trackpad Down" },
      { 0, NULL, "trackpad_touch",   XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Trackpad Touch" },
      { 0, NULL, "trackpad_x",       XR_ACTION_TYPE_FLOAT_INPUT,      2, hands, "Trackpad X" },
      { 0, NULL, "trackpad_y",       XR_ACTION_TYPE_FLOAT_INPUT,      2, hands, "Trackpad Y" },
      { 0, NULL, "thumbstick_down",  XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Thumbstick Down" },
      { 0, NULL, "thumbstick_touch", XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Thumbstick Touch" },
      { 0, NULL, "thumbstick_x",     XR_ACTION_TYPE_FLOAT_INPUT,      2, hands, "Thumbstick X" },
      { 0, NULL, "thumbstick_y",     XR_ACTION_TYPE_FLOAT_INPUT,      2, hands, "Thumbstick Y" },
      { 0, NULL, "menu_down",        XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Menu Down" },
      { 0, NULL, "menu_touch",       XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Menu Touch" },
      { 0, NULL, "grip_down",        XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Grip Down" },
      { 0, NULL, "grip_touch",       XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Grip Touch" },
      { 0, NULL, "grip_axis",        XR_ACTION_TYPE_FLOAT_INPUT,      2, hands, "Grip Axis" },
      { 0, NULL, "a_down",           XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "A Down" },
      { 0, NULL, "a_touch",          XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "A Touch" },
      { 0, NULL, "b_down",           XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "B Down" },
      { 0, NULL, "b_touch",          XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "B Touch" },
      { 0, NULL, "x_down",           XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "X Down" },
      { 0, NULL, "x_touch",          XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "X Touch" },
      { 0, NULL, "y_down",           XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Y Down" },
      { 0, NULL, "y_touch",          XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Y Touch" },
      { 0, NULL, "thumbrest_touch",  XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Thumbrest Touch" },
      { 0, NULL, "vibrate",          XR_ACTION_TYPE_VIBRATION_OUTPUT, 2, hands, "Vibrate" }
    };

    static_assert(COUNTOF(actionInfo) == MAX_ACTIONS, "Unbalanced action table!");

    if (!state.features.viveTrackers) {
      actionInfo[ACTION_TRACKER_POSE].countSubactionPaths = 0;
    }

    if (!state.features.gaze) {
      actionInfo[ACTION_GAZE_POSE].countSubactionPaths = 0;
    }

    for (uint32_t i = 0; i < MAX_ACTIONS; i++) {
      actionInfo[i].type = XR_TYPE_ACTION_CREATE_INFO;
      XR_INIT(xrCreateAction(state.actionSet, &actionInfo[i], &state.actions[i]), "Failed to create action");
    }

    enum {
      PROFILE_SIMPLE,
      PROFILE_VIVE,
      PROFILE_TOUCH,
      PROFILE_GO,
      PROFILE_INDEX,
      PROFILE_WMR,
      PROFILE_ML2,
      PROFILE_PICO_NEO3,
      PROFILE_PICO4,
      PROFILE_TRACKER,
      PROFILE_GAZE,
      MAX_PROFILES
    };

    const char* interactionProfilePaths[] = {
      [PROFILE_SIMPLE] = "/interaction_profiles/khr/simple_controller",
      [PROFILE_VIVE] = "/interaction_profiles/htc/vive_controller",
      [PROFILE_TOUCH] = "/interaction_profiles/oculus/touch_controller",
      [PROFILE_GO] = "/interaction_profiles/oculus/go_controller",
      [PROFILE_INDEX] = "/interaction_profiles/valve/index_controller",
      [PROFILE_WMR] = "/interaction_profiles/microsoft/motion_controller",
      [PROFILE_ML2] = "/interaction_profiles/ml/ml2_controller",
      [PROFILE_PICO_NEO3] = "/interaction_profiles/bytedance/pico_neo3_controller",
      [PROFILE_PICO4] = "/interaction_profiles/bytedance/pico4_controller",
      [PROFILE_TRACKER] = "/interaction_profiles/htc/vive_tracker_htcx",
      [PROFILE_GAZE] = "/interaction_profiles/ext/eye_gaze_interaction"
    };

    typedef struct {
      int action;
      const char* path;
    } Binding;

    Binding* bindings[] = {
      [PROFILE_SIMPLE] = (Binding[]) {
        { ACTION_PINCH_POSE, "/user/hand/left/pinch_ext/pose" },
        { ACTION_PINCH_POSE, "/user/hand/right/pinch_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/left/poke_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/right/poke_ext/pose" },
        { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
        { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
        { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
        { ACTION_TRIGGER_DOWN, "/user/hand/left/input/select/click" },
        { ACTION_TRIGGER_DOWN, "/user/hand/right/input/select/click" },
        { ACTION_MENU_DOWN, "/user/hand/left/input/menu/click" },
        { ACTION_MENU_DOWN, "/user/hand/right/input/menu/click" },
        { ACTION_VIBRATE, "/user/hand/left/output/haptic" },
        { ACTION_VIBRATE, "/user/hand/right/output/haptic" },
        { 0, NULL }
      },
      [PROFILE_VIVE] = (Binding[]) {
        { ACTION_PINCH_POSE, "/user/hand/left/pinch_ext/pose" },
        { ACTION_PINCH_POSE, "/user/hand/right/pinch_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/left/poke_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/right/poke_ext/pose" },
        { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
        { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
        { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
        { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/click" },
        { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/click" },
        { ACTION_TRIGGER_AXIS, "/user/hand/left/input/trigger/value" },
        { ACTION_TRIGGER_AXIS, "/user/hand/right/input/trigger/value" },
        { ACTION_TRACKPAD_DOWN, "/user/hand/left/input/trackpad/click" },
        { ACTION_TRACKPAD_DOWN, "/user/hand/right/input/trackpad/click" },
        { ACTION_TRACKPAD_TOUCH, "/user/hand/left/input/trackpad/touch" },
        { ACTION_TRACKPAD_TOUCH, "/user/hand/right/input/trackpad/touch" },
        { ACTION_TRACKPAD_X, "/user/hand/left/input/trackpad/x" },
        { ACTION_TRACKPAD_X, "/user/hand/right/input/trackpad/x" },
        { ACTION_TRACKPAD_Y, "/user/hand/left/input/trackpad/y" },
        { ACTION_TRACKPAD_Y, "/user/hand/right/input/trackpad/y" },
        { ACTION_MENU_DOWN, "/user/hand/left/input/menu/click" },
        { ACTION_MENU_DOWN, "/user/hand/right/input/menu/click" },
        { ACTION_GRIP_DOWN, "/user/hand/left/input/squeeze/click" },
        { ACTION_GRIP_DOWN, "/user/hand/right/input/squeeze/click" },
        { ACTION_VIBRATE, "/user/hand/left/output/haptic" },
        { ACTION_VIBRATE, "/user/hand/right/output/haptic" },
        { 0, NULL }
      },
      [PROFILE_TOUCH] = (Binding[]) {
        { ACTION_PINCH_POSE, "/user/hand/left/pinch_ext/pose" },
        { ACTION_PINCH_POSE, "/user/hand/right/pinch_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/left/poke_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/right/poke_ext/pose" },
        { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
        { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
        { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
        { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/value" },
        { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/value" },
        { ACTION_TRIGGER_TOUCH, "/user/hand/left/input/trigger/touch" },
        { ACTION_TRIGGER_TOUCH, "/user/hand/right/input/trigger/touch" },
        { ACTION_TRIGGER_AXIS, "/user/hand/left/input/trigger/value" },
        { ACTION_TRIGGER_AXIS, "/user/hand/right/input/trigger/value" },
        { ACTION_THUMBSTICK_DOWN, "/user/hand/left/input/thumbstick/click" },
        { ACTION_THUMBSTICK_DOWN, "/user/hand/right/input/thumbstick/click" },
        { ACTION_THUMBSTICK_TOUCH, "/user/hand/left/input/thumbstick/touch" },
        { ACTION_THUMBSTICK_TOUCH, "/user/hand/right/input/thumbstick/touch" },
        { ACTION_THUMBSTICK_X, "/user/hand/left/input/thumbstick/x" },
        { ACTION_THUMBSTICK_X, "/user/hand/right/input/thumbstick/x" },
        { ACTION_THUMBSTICK_Y, "/user/hand/left/input/thumbstick/y" },
        { ACTION_THUMBSTICK_Y, "/user/hand/right/input/thumbstick/y" },
        { ACTION_MENU_DOWN, "/user/hand/left/input/menu/click" },
        { ACTION_MENU_DOWN, "/user/hand/right/input/system/click" },
        { ACTION_GRIP_DOWN, "/user/hand/left/input/squeeze/value" },
        { ACTION_GRIP_DOWN, "/user/hand/right/input/squeeze/value" },
        { ACTION_GRIP_AXIS, "/user/hand/left/input/squeeze/value" },
        { ACTION_GRIP_AXIS, "/user/hand/right/input/squeeze/value" },
        { ACTION_A_DOWN, "/user/hand/right/input/a/click" },
        { ACTION_A_TOUCH, "/user/hand/right/input/a/touch" },
        { ACTION_B_DOWN, "/user/hand/right/input/b/click" },
        { ACTION_B_TOUCH, "/user/hand/right/input/b/touch" },
        { ACTION_X_DOWN, "/user/hand/left/input/x/click" },
        { ACTION_X_TOUCH, "/user/hand/left/input/x/touch" },
        { ACTION_Y_DOWN, "/user/hand/left/input/y/click" },
        { ACTION_Y_TOUCH, "/user/hand/left/input/y/touch" },
        { ACTION_THUMBREST_TOUCH, "/user/hand/left/input/thumbrest/touch" },
        { ACTION_THUMBREST_TOUCH, "/user/hand/right/input/thumbrest/touch" },
        { ACTION_VIBRATE, "/user/hand/left/output/haptic" },
        { ACTION_VIBRATE, "/user/hand/right/output/haptic" },
        { 0, NULL }
      },
      [PROFILE_GO] = (Binding[]) {
        { ACTION_PINCH_POSE, "/user/hand/left/pinch_ext/pose" },
        { ACTION_PINCH_POSE, "/user/hand/right/pinch_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/left/poke_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/right/poke_ext/pose" },
        { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
        { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
        { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
        { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/click" },
        { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/click" },
        { ACTION_TRACKPAD_DOWN, "/user/hand/left/input/trackpad/click" },
        { ACTION_TRACKPAD_DOWN, "/user/hand/right/input/trackpad/click" },
        { ACTION_TRACKPAD_TOUCH, "/user/hand/left/input/trackpad/touch" },
        { ACTION_TRACKPAD_TOUCH, "/user/hand/right/input/trackpad/touch" },
        { ACTION_TRACKPAD_X, "/user/hand/left/input/trackpad/x" },
        { ACTION_TRACKPAD_X, "/user/hand/right/input/trackpad/x" },
        { ACTION_TRACKPAD_Y, "/user/hand/left/input/trackpad/y" },
        { ACTION_TRACKPAD_Y, "/user/hand/right/input/trackpad/y" },
        { 0, NULL }
      },
      [PROFILE_INDEX] = (Binding[]) {
        { ACTION_PINCH_POSE, "/user/hand/left/pinch_ext/pose" },
        { ACTION_PINCH_POSE, "/user/hand/right/pinch_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/left/poke_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/right/poke_ext/pose" },
        { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
        { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
        { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
        { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/click" },
        { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/click" },
        { ACTION_TRIGGER_TOUCH, "/user/hand/left/input/trigger/touch" },
        { ACTION_TRIGGER_TOUCH, "/user/hand/right/input/trigger/touch" },
        { ACTION_TRIGGER_AXIS, "/user/hand/left/input/trigger/value" },
        { ACTION_TRIGGER_AXIS, "/user/hand/right/input/trigger/value" },
        { ACTION_TRACKPAD_DOWN, "/user/hand/left/input/trackpad/force" },
        { ACTION_TRACKPAD_DOWN, "/user/hand/right/input/trackpad/force" },
        { ACTION_TRACKPAD_TOUCH, "/user/hand/left/input/trackpad/touch" },
        { ACTION_TRACKPAD_TOUCH, "/user/hand/right/input/trackpad/touch" },
        { ACTION_TRACKPAD_X, "/user/hand/left/input/trackpad/x" },
        { ACTION_TRACKPAD_X, "/user/hand/right/input/trackpad/x" },
        { ACTION_TRACKPAD_Y, "/user/hand/left/input/trackpad/y" },
        { ACTION_TRACKPAD_Y, "/user/hand/right/input/trackpad/y" },
        { ACTION_THUMBSTICK_DOWN, "/user/hand/left/input/thumbstick/click" },
        { ACTION_THUMBSTICK_DOWN, "/user/hand/right/input/thumbstick/click" },
        { ACTION_THUMBSTICK_TOUCH, "/user/hand/left/input/thumbstick/touch" },
        { ACTION_THUMBSTICK_TOUCH, "/user/hand/right/input/thumbstick/touch" },
        { ACTION_THUMBSTICK_X, "/user/hand/left/input/thumbstick/x" },
        { ACTION_THUMBSTICK_X, "/user/hand/right/input/thumbstick/x" },
        { ACTION_THUMBSTICK_Y, "/user/hand/left/input/thumbstick/y" },
        { ACTION_THUMBSTICK_Y, "/user/hand/right/input/thumbstick/y" },
        { ACTION_GRIP_DOWN, "/user/hand/left/input/squeeze/force" },
        { ACTION_GRIP_DOWN, "/user/hand/right/input/squeeze/force" },
        { ACTION_GRIP_TOUCH, "/user/hand/left/input/squeeze/value" },
        { ACTION_GRIP_TOUCH, "/user/hand/right/input/squeeze/value" },
        { ACTION_GRIP_AXIS, "/user/hand/left/input/squeeze/force" },
        { ACTION_GRIP_AXIS, "/user/hand/right/input/squeeze/force" },
        { ACTION_A_DOWN, "/user/hand/left/input/a/click" },
        { ACTION_A_DOWN, "/user/hand/right/input/a/click" },
        { ACTION_A_TOUCH, "/user/hand/left/input/a/touch" },
        { ACTION_A_TOUCH, "/user/hand/right/input/a/touch" },
        { ACTION_B_DOWN, "/user/hand/left/input/b/click" },
        { ACTION_B_DOWN, "/user/hand/right/input/b/click" },
        { ACTION_B_TOUCH, "/user/hand/left/input/b/touch" },
        { ACTION_B_TOUCH, "/user/hand/right/input/b/touch" },
        { ACTION_VIBRATE, "/user/hand/left/output/haptic" },
        { ACTION_VIBRATE, "/user/hand/right/output/haptic" },
        { 0, NULL }
      },
      [PROFILE_WMR] = (Binding[]) {
        { ACTION_PINCH_POSE, "/user/hand/left/pinch_ext/pose" },
        { ACTION_PINCH_POSE, "/user/hand/right/pinch_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/left/poke_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/right/poke_ext/pose" },
        { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
        { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
        { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
        { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/value" },
        { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/value" },
        { ACTION_TRIGGER_AXIS, "/user/hand/left/input/trigger/value" },
        { ACTION_TRIGGER_AXIS, "/user/hand/right/input/trigger/value" },
        { ACTION_TRACKPAD_DOWN, "/user/hand/left/input/trackpad/click" },
        { ACTION_TRACKPAD_DOWN, "/user/hand/right/input/trackpad/click" },
        { ACTION_TRACKPAD_TOUCH, "/user/hand/left/input/trackpad/touch" },
        { ACTION_TRACKPAD_TOUCH, "/user/hand/right/input/trackpad/touch" },
        { ACTION_TRACKPAD_X, "/user/hand/left/input/trackpad/x" },
        { ACTION_TRACKPAD_X, "/user/hand/right/input/trackpad/x" },
        { ACTION_TRACKPAD_Y, "/user/hand/left/input/trackpad/y" },
        { ACTION_TRACKPAD_Y, "/user/hand/right/input/trackpad/y" },
        { ACTION_THUMBSTICK_DOWN, "/user/hand/left/input/thumbstick/click" },
        { ACTION_THUMBSTICK_DOWN, "/user/hand/right/input/thumbstick/click" },
        { ACTION_THUMBSTICK_X, "/user/hand/left/input/thumbstick/x" },
        { ACTION_THUMBSTICK_X, "/user/hand/right/input/thumbstick/x" },
        { ACTION_THUMBSTICK_Y, "/user/hand/left/input/thumbstick/y" },
        { ACTION_THUMBSTICK_Y, "/user/hand/right/input/thumbstick/y" },
        { ACTION_MENU_DOWN, "/user/hand/left/input/menu/click" },
        { ACTION_MENU_DOWN, "/user/hand/right/input/menu/click" },
        { ACTION_GRIP_DOWN, "/user/hand/left/input/squeeze/click" },
        { ACTION_GRIP_DOWN, "/user/hand/right/input/squeeze/click" },
        { ACTION_GRIP_AXIS, "/user/hand/left/input/squeeze/click" },
        { ACTION_GRIP_AXIS, "/user/hand/right/input/squeeze/click" },
        { ACTION_VIBRATE, "/user/hand/left/output/haptic" },
        { ACTION_VIBRATE, "/user/hand/right/output/haptic" },
        { 0, NULL }
      },
      [PROFILE_ML2] = (Binding[]) {
        { ACTION_PINCH_POSE, "/user/hand/left/pinch_ext/pose" },
        { ACTION_PINCH_POSE, "/user/hand/right/pinch_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/left/poke_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/right/poke_ext/pose" },
        { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
        { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
        { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
        { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/click" },
        { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/click" },
        { ACTION_TRIGGER_AXIS, "/user/hand/left/input/trigger/value" },
        { ACTION_TRIGGER_AXIS, "/user/hand/right/input/trigger/value" },
        { ACTION_TRACKPAD_DOWN, "/user/hand/left/input/trackpad/click" },
        { ACTION_TRACKPAD_DOWN, "/user/hand/right/input/trackpad/click" },
        { ACTION_TRACKPAD_TOUCH, "/user/hand/left/input/trackpad/touch" },
        { ACTION_TRACKPAD_TOUCH, "/user/hand/right/input/trackpad/touch" },
        { ACTION_TRACKPAD_X, "/user/hand/left/input/trackpad/x" },
        { ACTION_TRACKPAD_X, "/user/hand/right/input/trackpad/x" },
        { ACTION_TRACKPAD_Y, "/user/hand/left/input/trackpad/y" },
        { ACTION_TRACKPAD_Y, "/user/hand/right/input/trackpad/y" },
        { ACTION_MENU_DOWN, "/user/hand/left/input/menu/click" },
        { ACTION_MENU_DOWN, "/user/hand/right/input/menu/click" },
        { ACTION_GRIP_DOWN, "/user/hand/left/input/shoulder/click" },
        { ACTION_GRIP_DOWN, "/user/hand/right/input/shoulder/click" },
        { ACTION_VIBRATE, "/user/hand/left/output/haptic" },
        { ACTION_VIBRATE, "/user/hand/right/output/haptic" },
        { 0, NULL }
      },
      [PROFILE_PICO_NEO3] = (Binding[]) {
        { ACTION_PINCH_POSE, "/user/hand/left/pinch_ext/pose" },
        { ACTION_PINCH_POSE, "/user/hand/right/pinch_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/left/poke_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/right/poke_ext/pose" },
        { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
        { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
        { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
        { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/click" },
        { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/click" },
        { ACTION_TRIGGER_TOUCH, "/user/hand/left/input/trigger/touch" },
        { ACTION_TRIGGER_TOUCH, "/user/hand/right/input/trigger/touch" },
        { ACTION_TRIGGER_AXIS, "/user/hand/left/input/trigger/value" },
        { ACTION_TRIGGER_AXIS, "/user/hand/right/input/trigger/value" },
        { ACTION_THUMBSTICK_DOWN, "/user/hand/left/input/thumbstick/click" },
        { ACTION_THUMBSTICK_DOWN, "/user/hand/right/input/thumbstick/click" },
        { ACTION_THUMBSTICK_TOUCH, "/user/hand/left/input/thumbstick/touch" },
        { ACTION_THUMBSTICK_TOUCH, "/user/hand/right/input/thumbstick/touch" },
        { ACTION_THUMBSTICK_X, "/user/hand/left/input/thumbstick/x" },
        { ACTION_THUMBSTICK_X, "/user/hand/right/input/thumbstick/x" },
        { ACTION_THUMBSTICK_Y, "/user/hand/left/input/thumbstick/y" },
        { ACTION_THUMBSTICK_Y, "/user/hand/right/input/thumbstick/y" },
        { ACTION_MENU_DOWN, "/user/hand/left/input/menu/click" },
        { ACTION_MENU_DOWN, "/user/hand/right/input/menu/click" },
        { ACTION_GRIP_DOWN, "/user/hand/left/input/squeeze/click" },
        { ACTION_GRIP_DOWN, "/user/hand/right/input/squeeze/click" },
        { ACTION_GRIP_AXIS, "/user/hand/left/input/squeeze/value" },
        { ACTION_GRIP_AXIS, "/user/hand/right/input/squeeze/value" },
        { ACTION_A_DOWN, "/user/hand/right/input/a/click" },
        { ACTION_A_TOUCH, "/user/hand/right/input/a/touch" },
        { ACTION_B_DOWN, "/user/hand/right/input/b/click" },
        { ACTION_B_TOUCH, "/user/hand/right/input/b/touch" },
        { ACTION_X_DOWN, "/user/hand/left/input/x/click" },
        { ACTION_X_TOUCH, "/user/hand/left/input/x/touch" },
        { ACTION_Y_DOWN, "/user/hand/left/input/y/click" },
        { ACTION_Y_TOUCH, "/user/hand/left/input/y/touch" },
        { ACTION_VIBRATE, "/user/hand/left/output/haptic" },
        { ACTION_VIBRATE, "/user/hand/right/output/haptic" },
        { 0, NULL }
      },
      [PROFILE_PICO4] = (Binding[]) {
        { ACTION_PINCH_POSE, "/user/hand/left/pinch_ext/pose" },
        { ACTION_PINCH_POSE, "/user/hand/right/pinch_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/left/poke_ext/pose" },
        { ACTION_POKE_POSE, "/user/hand/right/poke_ext/pose" },
        { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
        { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
        { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
        { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/value" },
        { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/value" },
        { ACTION_TRIGGER_TOUCH, "/user/hand/left/input/trigger/touch" },
        { ACTION_TRIGGER_TOUCH, "/user/hand/right/input/trigger/touch" },
        { ACTION_TRIGGER_AXIS, "/user/hand/left/input/trigger/value" },
        { ACTION_TRIGGER_AXIS, "/user/hand/right/input/trigger/value" },
        { ACTION_THUMBSTICK_DOWN, "/user/hand/left/input/thumbstick/click" },
        { ACTION_THUMBSTICK_DOWN, "/user/hand/right/input/thumbstick/click" },
        { ACTION_THUMBSTICK_TOUCH, "/user/hand/left/input/thumbstick/touch" },
        { ACTION_THUMBSTICK_TOUCH, "/user/hand/right/input/thumbstick/touch" },
        { ACTION_THUMBSTICK_X, "/user/hand/left/input/thumbstick/x" },
        { ACTION_THUMBSTICK_X, "/user/hand/right/input/thumbstick/x" },
        { ACTION_THUMBSTICK_Y, "/user/hand/left/input/thumbstick/y" },
        { ACTION_THUMBSTICK_Y, "/user/hand/right/input/thumbstick/y" },
        { ACTION_MENU_DOWN, "/user/hand/left/input/menu/click" },
        { ACTION_MENU_DOWN, "/user/hand/right/input/system/click" },
        { ACTION_GRIP_DOWN, "/user/hand/left/input/squeeze/click" },
        { ACTION_GRIP_DOWN, "/user/hand/right/input/squeeze/click" },
        { ACTION_GRIP_AXIS, "/user/hand/left/input/squeeze/value" },
        { ACTION_GRIP_AXIS, "/user/hand/right/input/squeeze/value" },
        { ACTION_A_DOWN, "/user/hand/right/input/a/click" },
        { ACTION_A_TOUCH, "/user/hand/right/input/a/touch" },
        { ACTION_B_DOWN, "/user/hand/right/input/b/click" },
        { ACTION_B_TOUCH, "/user/hand/right/input/b/touch" },
        { ACTION_X_DOWN, "/user/hand/left/input/x/click" },
        { ACTION_X_TOUCH, "/user/hand/left/input/x/touch" },
        { ACTION_Y_DOWN, "/user/hand/left/input/y/click" },
        { ACTION_Y_TOUCH, "/user/hand/left/input/y/touch" },
        { ACTION_THUMBREST_TOUCH, "/user/hand/left/input/thumbrest/touch" },
        { ACTION_THUMBREST_TOUCH, "/user/hand/right/input/thumbrest/touch" },
        { ACTION_VIBRATE, "/user/hand/left/output/haptic" },
        { ACTION_VIBRATE, "/user/hand/right/output/haptic" },
        { 0, NULL }
      },
      [PROFILE_TRACKER] = (Binding[]) {
        { ACTION_TRACKER_POSE, "/user/vive_tracker_htcx/role/left_elbow/input/grip/pose" },
        { ACTION_TRACKER_POSE, "/user/vive_tracker_htcx/role/right_elbow/input/grip/pose" },
        { ACTION_TRACKER_POSE, "/user/vive_tracker_htcx/role/left_shoulder/input/grip/pose" },
        { ACTION_TRACKER_POSE, "/user/vive_tracker_htcx/role/right_shoulder/input/grip/pose" },
        { ACTION_TRACKER_POSE, "/user/vive_tracker_htcx/role/chest/input/grip/pose" },
        { ACTION_TRACKER_POSE, "/user/vive_tracker_htcx/role/waist/input/grip/pose" },
        { ACTION_TRACKER_POSE, "/user/vive_tracker_htcx/role/left_knee/input/grip/pose" },
        { ACTION_TRACKER_POSE, "/user/vive_tracker_htcx/role/right_knee/input/grip/pose" },
        { ACTION_TRACKER_POSE, "/user/vive_tracker_htcx/role/left_foot/input/grip/pose" },
        { ACTION_TRACKER_POSE, "/user/vive_tracker_htcx/role/right_foot/input/grip/pose" },
        { ACTION_TRACKER_POSE, "/user/vive_tracker_htcx/role/camera/input/grip/pose" },
        { ACTION_TRACKER_POSE, "/user/vive_tracker_htcx/role/keyboard/input/grip/pose" },
        { 0, NULL }
      },
      [PROFILE_GAZE] = (Binding[]) {
        { ACTION_GAZE_POSE, "/user/eyes_ext/input/gaze_ext/pose" },
        { 0, NULL }
      }
    };

    // Don't suggest bindings for unsupported input profiles
    if (!state.features.ml2Controller) {
      bindings[PROFILE_ML2][0].path = NULL;
    }

    if (!state.features.picoController) {
      bindings[PROFILE_PICO_NEO3][0].path = NULL;
      bindings[PROFILE_PICO4][0].path = NULL;
    }

    if (!state.features.viveTrackers) {
      bindings[PROFILE_TRACKER][0].path = NULL;
    }

    if (!state.features.gaze) {
      bindings[PROFILE_GAZE][0].path = NULL;
    }

    // For this to work, pinch/poke need to be the first paths in the interaction profile
    if (!state.features.handInteraction) {
      bindings[PROFILE_SIMPLE] += 4;
      bindings[PROFILE_VIVE] += 4;
      bindings[PROFILE_TOUCH] += 4;
      bindings[PROFILE_GO] += 4;
      bindings[PROFILE_INDEX] += 4;
      bindings[PROFILE_WMR] += 4;
      if (state.features.ml2Controller) bindings[PROFILE_ML2] += 4;
      if (state.features.picoController) bindings[PROFILE_PICO_NEO3] += 4;
      if (state.features.picoController) bindings[PROFILE_PICO4] += 4;
    }

    XrPath path;
    XrActionSuggestedBinding suggestedBindings[64];
    for (uint32_t i = 0, count = 0; i < MAX_PROFILES; i++, count = 0) {
      for (uint32_t j = 0; bindings[i][j].path; j++, count++) {
        XR_INIT(xrStringToPath(state.instance, bindings[i][j].path, &path), "Failed to create path");
        suggestedBindings[j].action = state.actions[bindings[i][j].action];
        suggestedBindings[j].binding = path;
      }

      if (count > 0) {
        XR_INIT(xrStringToPath(state.instance, interactionProfilePaths[i], &path), "Failed to create path");
        XrResult result = (xrSuggestInteractionProfileBindings(state.instance, &(XrInteractionProfileSuggestedBinding) {
          .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
          .interactionProfile = path,
          .countSuggestedBindings = count,
          .suggestedBindings = suggestedBindings
        }));

        if (XR_FAILED(result)) {
          lovrLog(LOG_WARN, "XR", "Failed to suggest input bindings for %s", interactionProfilePaths[i]);
        }
      }
    }
  }

  openxr_setClipDistance(.01f, 0.f);
  state.frameState.type = XR_TYPE_FRAME_STATE;
  return true;
}

static void openxr_start(void) {
#ifdef LOVR_DISABLE_GRAPHICS
  bool hasGraphics = false;
#else
  bool hasGraphics = lovrGraphicsIsInitialized();
#endif

  { // Session
    XrSessionCreateInfo info = {
      .type = XR_TYPE_SESSION_CREATE_INFO,
      .systemId = state.system
    };

#if !defined(LOVR_DISABLE_GRAPHICS) && defined(LOVR_VK)
    XrGraphicsBindingVulkanKHR graphicsBinding = {
      .type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR,
      .next = info.next
    };

    XrGraphicsRequirementsVulkanKHR requirements = {
      .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR
    };

    if (hasGraphics) {
      PFN_xrGetVulkanGraphicsRequirements2KHR xrGetVulkanGraphicsRequirements2KHR;
      XR_LOAD(xrGetVulkanGraphicsRequirements2KHR);

      XR(xrGetVulkanGraphicsRequirements2KHR(state.instance, state.system, &requirements), "Failed to query Vulkan graphics requirements");
      if (XR_VERSION_MAJOR(requirements.minApiVersionSupported) > 1 || XR_VERSION_MINOR(requirements.minApiVersionSupported) > 1) {
        lovrThrow("OpenXR Vulkan version not supported");
      }

      graphicsBinding.instance = (VkInstance) gpu_vk_get_instance();
      graphicsBinding.physicalDevice = (VkPhysicalDevice) gpu_vk_get_physical_device();
      graphicsBinding.device = (VkDevice) gpu_vk_get_device();
      gpu_vk_get_queue(&graphicsBinding.queueFamilyIndex, &graphicsBinding.queueIndex);
      info.next = &graphicsBinding;
    }
#endif

    lovrAssert(hasGraphics || state.features.headless, "Graphics module is not available, and headless headset is not supported");

#ifdef XR_EXTX_overlay
    XrSessionCreateInfoOverlayEXTX overlayInfo = {
      .type = XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX,
      .next = info.next,
      .sessionLayersPlacement = state.config.overlayOrder
    };

    if (state.features.overlay) {
      info.next = &overlayInfo;
    }
#endif

    XrSessionActionSetsAttachInfo attachInfo = {
      .type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
      .countActionSets = 1,
      .actionSets = &state.actionSet
    };

    XR(xrCreateSession(state.instance, &info, &state.session), "Failed to create session");
    XR(xrAttachSessionActionSets(state.session, &attachInfo), "Failed to attach action sets");
  }

  { // Spaaace
    XrReferenceSpaceCreateInfo referenceSpaceInfo = {
      .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
      .poseInReferenceSpace = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } }
    };

    // Head
    referenceSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    XR(xrCreateReferenceSpace(state.session, &referenceSpaceInfo, &state.spaces[DEVICE_HEAD]), "Failed to create head space");

    // Floor (may not be supported, which is okay)
    referenceSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    if (XR_FAILED(xrCreateReferenceSpace(state.session, &referenceSpaceInfo, &state.spaces[DEVICE_FLOOR]))) {
      state.spaces[DEVICE_FLOOR] = XR_NULL_HANDLE;
    }

    createReferenceSpace(getCurrentXrTime());

    // Action spaces
    XrActionSpaceCreateInfo actionSpaceInfo = {
      .type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
      .poseInActionSpace = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } }
    };

    for (uint32_t i = 0; i < MAX_DEVICES; i++) {
      actionSpaceInfo.action = getPoseActionForDevice(i);
      actionSpaceInfo.subactionPath = state.actionFilters[i];

      if (!actionSpaceInfo.action) {
        continue;
      }

      XR(xrCreateActionSpace(state.session, &actionSpaceInfo, &state.spaces[i]), "Failed to create action space");
    }
  }

  // Swapchain
  if (hasGraphics) {
    state.depthFormat = state.config.stencil ? FORMAT_D32FS8 : FORMAT_D32F;

    if (!lovrGraphicsGetFormatSupport(state.depthFormat, TEXTURE_FEATURE_RENDER)) {
      state.depthFormat = state.config.stencil ? FORMAT_D24S8 : FORMAT_D24;
    }

    state.pass = lovrPassCreate("Headset");

#ifdef LOVR_VK
    int64_t nativeColorFormat = VK_FORMAT_R8G8B8A8_SRGB;
    int64_t nativeDepthFormat;

    switch (state.depthFormat) {
      case FORMAT_D24: nativeDepthFormat = VK_FORMAT_X8_D24_UNORM_PACK32; break;
      case FORMAT_D32F: nativeDepthFormat = VK_FORMAT_D32_SFLOAT; break;
      case FORMAT_D24S8: nativeDepthFormat = VK_FORMAT_D24_UNORM_S8_UINT; break;
      case FORMAT_D32FS8: nativeDepthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT; break;
      default: lovrUnreachable();
    }
#endif

    int64_t formats[128];
    uint32_t formatCount;
    XR(xrEnumerateSwapchainFormats(state.session, COUNTOF(formats), &formatCount, formats), "Failed to query swapchain formats");

    bool supportsColor = false;
    bool supportsDepth = false;

    for (uint32_t i = 0; i < formatCount && (!supportsColor || !supportsDepth); i++) {
      if (formats[i] == nativeColorFormat) {
        supportsColor = true;
      } else if (formats[i] == nativeDepthFormat) {
        supportsDepth = true;
      }
    }

    lovrAssert(supportsColor, "This VR runtime does not support sRGB rgba8 textures");
    swapchain_init(&state.swapchains[COLOR], state.width, state.height, true, false);

    GraphicsFeatures features;
    lovrGraphicsGetFeatures(&features);
    if (state.features.depth && supportsDepth && features.depthResolve) {
      swapchain_init(&state.swapchains[DEPTH], state.width, state.height, true, true);
    } else {
      state.features.depth = false;
    }

    // Pre-init composition layer
    state.layer = (XrCompositionLayerProjection) {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
      .viewCount = 2,
      .views = state.layerViews
    };

    // Pre-init composition layer views
    state.layerViews[0] = (XrCompositionLayerProjectionView) {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
      .subImage = { state.swapchains[COLOR].handle, { { 0, 0 }, { state.width, state.height } }, 0 }
    };

    state.layerViews[1] = (XrCompositionLayerProjectionView) {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
      .subImage = { state.swapchains[COLOR].handle, { { 0, 0 }, { state.width, state.height } }, 1 }
    };

    if (state.features.depth) {
      for (uint32_t i = 0; i < 2; i++) {
        state.layerViews[i].next = &state.depthInfo[i];
        state.depthInfo[i] = (XrCompositionLayerDepthInfoKHR) {
          .type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR,
          .subImage.swapchain = state.swapchains[DEPTH].handle,
          .subImage.imageRect = state.layerViews[i].subImage.imageRect,
          .subImage.imageArrayIndex = i,
          .minDepth = 0.f,
          .maxDepth = 1.f
        };
      }
    }
  }

  if (state.features.keyboardTracking) {
    XrKeyboardTrackingQueryFB queryInfo = {
      .type = XR_TYPE_KEYBOARD_TRACKING_QUERY_FB,
      .flags = XR_KEYBOARD_TRACKING_QUERY_LOCAL_BIT_FB
    };

    XrKeyboardTrackingDescriptionFB keyboard;
    XrResult result = xrQuerySystemTrackedKeyboardFB(state.session, &queryInfo, &keyboard);

    if (result == XR_SUCCESS) {
      XrKeyboardSpaceCreateInfoFB spaceInfo = {
        .type = XR_TYPE_KEYBOARD_SPACE_CREATE_INFO_FB,
        .trackedKeyboardId = keyboard.trackedKeyboardId
      };

      xrCreateKeyboardSpaceFB(state.session, &spaceInfo, &state.spaces[DEVICE_KEYBOARD]);
    } else {
      state.features.keyboardTracking = false;
    }
  }

  if (state.features.refreshRate) {
    XR(xrEnumerateDisplayRefreshRatesFB(state.session, 0, &state.refreshRateCount, NULL), "Failed to query refresh rates");
    state.refreshRates = lovrMalloc(state.refreshRateCount * sizeof(float));
    XR(xrEnumerateDisplayRefreshRatesFB(state.session, state.refreshRateCount, &state.refreshRateCount, state.refreshRates), "Failed to query refresh rates");
  }
}

static void openxr_stop(void) {
  if (!state.session) {
    return;
  }

  for (uint32_t i = 0; i < state.layerCount; i++) {
    lovrRelease(state.layers[i], lovrLayerDestroy);
  }

  swapchain_destroy(&state.swapchains[0]);
  swapchain_destroy(&state.swapchains[1]);
  lovrRelease(state.pass, lovrPassDestroy);

  if (state.handTrackers[0]) xrDestroyHandTrackerEXT(state.handTrackers[0]);
  if (state.handTrackers[1]) xrDestroyHandTrackerEXT(state.handTrackers[1]);

  if (state.passthrough) xrDestroyPassthroughFB(state.passthrough);
  if (state.passthroughLayerHandle) xrDestroyPassthroughLayerFB(state.passthroughLayerHandle);

  for (size_t i = 0; i < MAX_DEVICES; i++) {
    if (state.spaces[i]) {
      xrDestroySpace(state.spaces[i]);
    }
  }

  if (state.referenceSpace) xrDestroySpace(state.referenceSpace);
  if (state.session) xrDestroySession(state.session);
  state.session = NULL;
}

static void openxr_destroy(void) {
  openxr_stop();

  if (state.actionSet) xrDestroyActionSet(state.actionSet);
  if (state.instance) xrDestroyInstance(state.instance);
  memset(&state, 0, sizeof(state));
}

static bool openxr_getDriverName(char* name, size_t length) {
  XrInstanceProperties properties = { .type = XR_TYPE_INSTANCE_PROPERTIES };
  if (XR_FAILED(xrGetInstanceProperties(state.instance, &properties))) return false;
  strncpy(name, properties.runtimeName, length - 1);
  name[length - 1] = '\0';
  return true;
}

static bool openxr_getName(char* name, size_t length) {
  XrSystemProperties properties = { .type = XR_TYPE_SYSTEM_PROPERTIES };
  if (XR_FAILED(xrGetSystemProperties(state.instance, state.system, &properties))) return false;
  strncpy(name, properties.systemName, length - 1);
  name[length - 1] = '\0';
  return true;
}

static bool openxr_isSeated(void) {
  return state.config.seated;
}

static void openxr_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  *width = state.width;
  *height = state.height;
}

static float openxr_getRefreshRate(void) {
  if (!state.features.refreshRate) return 0.f;
  float refreshRate;
  XR(xrGetDisplayRefreshRateFB(state.session, &refreshRate), "Failed to query refresh rate");
  return refreshRate;
}

static bool openxr_setRefreshRate(float refreshRate) {
  if (!state.features.refreshRate) return false;
  XrResult result = xrRequestDisplayRefreshRateFB(state.session, refreshRate);
  if (result == XR_ERROR_DISPLAY_REFRESH_RATE_UNSUPPORTED_FB) return false;
  XR(result, "Failed to set refresh rate");
  return true;
}

static const float* openxr_getRefreshRates(uint32_t* count) {
  *count = state.refreshRateCount;
  return state.refreshRates;
}

static XrEnvironmentBlendMode convertPassthroughMode(PassthroughMode mode) {
  switch (mode) {
    case PASSTHROUGH_OPAQUE: return XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    case PASSTHROUGH_BLEND: return XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
    case PASSTHROUGH_ADD: return XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
    default: lovrUnreachable();
  }
}

static PassthroughMode openxr_getPassthrough(void) {
  switch (state.blendMode) {
    case XR_ENVIRONMENT_BLEND_MODE_OPAQUE: return PASSTHROUGH_OPAQUE;
    case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND: return PASSTHROUGH_BLEND;
    case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE: return PASSTHROUGH_ADD;
    default: lovrUnreachable();
  }
}

static bool openxr_setPassthrough(PassthroughMode mode) {
  if (state.features.questPassthrough) {
    if (mode == PASSTHROUGH_ADD) {
      return false;
    }

    if (!state.passthrough) {
      XrPassthroughCreateInfoFB info = { .type = XR_TYPE_PASSTHROUGH_CREATE_INFO_FB };

      if (XR_FAILED(xrCreatePassthroughFB(state.session, &info, &state.passthrough))) {
        return false;
      }

      XrPassthroughLayerCreateInfoFB layerInfo = {
        .type = XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB,
        .passthrough = state.passthrough,
        .purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB,
        .flags = XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB
      };

      if (XR_FAILED(xrCreatePassthroughLayerFB(state.session, &layerInfo, &state.passthroughLayerHandle))) {
        xrDestroyPassthroughFB(state.passthrough);
        state.passthrough = NULL;
        return false;
      }

      state.passthroughLayer = (XrCompositionLayerPassthroughFB) {
        .type = XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB,
        .layerHandle = state.passthroughLayerHandle
      };
    }

    bool enable = mode == PASSTHROUGH_BLEND || mode == PASSTHROUGH_TRANSPARENT;

    if (state.passthroughActive == enable) {
      return true;
    }

    if (enable) {
      if (XR_SUCCEEDED(xrPassthroughStartFB(state.passthrough))) {
        state.passthroughActive = true;
        return true;
      }
    } else {
      if (XR_SUCCEEDED(xrPassthroughPauseFB(state.passthrough))) {
        state.passthroughActive = false;
        return true;
      }
    }

    return false;
  }

  if (mode == PASSTHROUGH_DEFAULT) {
    state.blendMode = state.blendModes[0];
    return true;
  } else if (mode == PASSTHROUGH_TRANSPARENT) {
    for (uint32_t i = 0; i < state.blendModeCount; i++) {
      switch (state.blendModes[i]) {
        case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
        case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
          state.blendMode = state.blendModes[i];
          return true;
        default: continue;
      }
    }
  } else {
    XrEnvironmentBlendMode blendMode = convertPassthroughMode(mode);
    for (uint32_t i = 0; i < state.blendModeCount; i++) {
      if (state.blendModes[i] == blendMode) {
        state.blendMode = state.blendModes[i];
        return true;
      }
    }
  }

  return false;
}

static bool openxr_isPassthroughSupported(PassthroughMode mode) {
  if (state.features.questPassthrough && mode == PASSTHROUGH_BLEND) {
    return true;
  }

  XrEnvironmentBlendMode blendMode = convertPassthroughMode(mode);
  for (uint32_t i = 0; i < state.blendModeCount; i++) {
    if (state.blendModes[i] == blendMode) {
      return true;
    }
  }

  return false;
}

static double openxr_getDisplayTime(void) {
  return (state.frameState.predictedDisplayTime - state.epoch) / 1e9;
}

static double openxr_getDeltaTime(void) {
  return (state.frameState.predictedDisplayTime - state.lastDisplayTime) / 1e9;
}

static XrViewStateFlags getViews(XrView views[2], uint32_t* count) {
  if (state.frameState.predictedDisplayTime <= 0) {
    return 0;
  }

  XrViewLocateInfo viewLocateInfo = {
    .type = XR_TYPE_VIEW_LOCATE_INFO,
    .viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
    .displayTime = state.frameState.predictedDisplayTime,
    .space = state.referenceSpace
  };

  for (uint32_t i = 0; i < 2; i++) {
    views[i].type = XR_TYPE_VIEW;
    views[i].next = NULL;
  }

  XrViewState viewState = { .type = XR_TYPE_VIEW_STATE };
  XR(xrLocateViews(state.session, &viewLocateInfo, &viewState, 2, count, views), "Failed to locate views");
  return viewState.viewStateFlags;
}

static uint32_t openxr_getViewCount(void) {
  return 2;
}

static bool openxr_getViewPose(uint32_t view, float* position, float* orientation) {
  uint32_t count;
  XrView views[2];
  XrViewStateFlags flags = getViews(views, &count);

  if (view >= count || !flags) {
    return false;
  }

  if (flags & XR_VIEW_STATE_POSITION_VALID_BIT) {
    memcpy(position, &views[view].pose.position.x, 3 * sizeof(float));
  } else {
    memset(position, 0, 3 * sizeof(float));
  }

  if (flags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
    memcpy(orientation, &views[view].pose.orientation.x, 4 * sizeof(float));
  } else {
    memset(orientation, 0, 4 * sizeof(float));
  }

  return true;
}

static bool openxr_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  uint32_t count;
  XrView views[2];
  XrViewStateFlags flags = getViews(views, &count);

  if (view >= count || !flags) {
    return false;
  }

  *left = -views[view].fov.angleLeft;
  *right = views[view].fov.angleRight;
  *up = views[view].fov.angleUp;
  *down = -views[view].fov.angleDown;
  return true;
}

static void openxr_getClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void openxr_setClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static void openxr_getBoundsDimensions(float* width, float* depth) {
  XrExtent2Df bounds;
  if (XR_SUCCEEDED(xrGetReferenceSpaceBoundsRect(state.session, XR_REFERENCE_SPACE_TYPE_STAGE, &bounds))) {
    *width = bounds.width;
    *depth = bounds.height;
  } else {
    *width = 0.f;
    *depth = 0.f;
  }
}

static const float* openxr_getBoundsGeometry(uint32_t* count) {
  *count = 0;
  return NULL;
}

static bool openxr_getPose(Device device, float* position, float* orientation) {
  if (state.frameState.predictedDisplayTime <= 0) {
    return false;
  }

  XrAction action = getPoseActionForDevice(device);
  XrActionStatePose poseState = { .type = XR_TYPE_ACTION_STATE_POSE };

  // If there's a pose action for this device, see if the action is active before locating its space
  // (because Oculus runtimes had a bug that forced checking the action before locating the space)
  if (action) {
    XrActionStateGetInfo info = {
      .type = XR_TYPE_ACTION_STATE_GET_INFO,
      .action = action,
      .subactionPath = state.actionFilters[device]
    };

    XR(xrGetActionStatePose(state.session, &info, &poseState), "Failed to get pose");
  }

  // If there's no space to locate, or the pose action isn't active, fall back to alternative
  // methods, e.g. hand tracking can sometimes be used for grip/aim/elbow devices
  if (!state.spaces[device] || (action && !poseState.isActive)) {
    bool point = false;
    bool elbow = false;

    if (state.features.handTrackingAim && (device == DEVICE_HAND_LEFT_POINT || device == DEVICE_HAND_RIGHT_POINT)) {
      device = DEVICE_HAND_LEFT + (device == DEVICE_HAND_RIGHT_POINT);
      point = true;
    }

    if (state.features.handTrackingElbow && (device == DEVICE_ELBOW_LEFT || device == DEVICE_ELBOW_RIGHT)) {
      device = DEVICE_HAND_LEFT + (device == DEVICE_ELBOW_RIGHT);
      elbow = true;
    }

    XrHandTrackerEXT tracker = getHandTracker(device);

    if (!tracker) {
      return false;
    }

    XrHandJointsLocateInfoEXT info = {
      .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
      .baseSpace = state.referenceSpace,
      .time = state.frameState.predictedDisplayTime
    };

    XrHandJointLocationEXT joints[MAX_HAND_JOINTS];
    XrHandJointLocationsEXT hand = {
      .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
      .jointCount = 26 + state.features.handTrackingElbow,
      .jointLocations = joints
    };

    XrHandTrackingAimStateFB aimState = {
      .type = XR_TYPE_HAND_TRACKING_AIM_STATE_FB
    };

    if (point) {
      hand.next = &aimState;
    }

    if (XR_FAILED(xrLocateHandJointsEXT(tracker, &info, &hand)) || !hand.isActive) {
      return false;
    }

    XrPosef* pose;
    if (point) {
      pose = &aimState.aimPose;
    } else if (elbow) {
      pose = &joints[XR_HAND_FOREARM_JOINT_ELBOW_ULTRALEAP].pose;
    } else {
      pose = &joints[XR_HAND_JOINT_WRIST_EXT].pose;
    }

    memcpy(orientation, &pose->orientation, 4 * sizeof(float));
    memcpy(position, &pose->position, 3 * sizeof(float));
    return true;
  }

  XrSpaceLocation location = { .type = XR_TYPE_SPACE_LOCATION };
  xrLocateSpace(state.spaces[device], state.referenceSpace, state.frameState.predictedDisplayTime, &location);
  memcpy(orientation, &location.pose.orientation, 4 * sizeof(float));
  memcpy(position, &location.pose.position, 3 * sizeof(float));
  return location.locationFlags & (XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
}

static bool openxr_getVelocity(Device device, float* linearVelocity, float* angularVelocity) {
  if (!state.spaces[device] || state.frameState.predictedDisplayTime <= 0) {
    return false;
  }

  XrSpaceVelocity velocity = { .type = XR_TYPE_SPACE_VELOCITY };
  XrSpaceLocation location = { .type = XR_TYPE_SPACE_LOCATION, .next = &velocity };
  xrLocateSpace(state.spaces[device], state.referenceSpace, state.frameState.predictedDisplayTime, &location);
  memcpy(linearVelocity, &velocity.linearVelocity, 3 * sizeof(float));
  memcpy(angularVelocity, &velocity.angularVelocity, 3 * sizeof(float));
  return velocity.velocityFlags & (XR_SPACE_VELOCITY_LINEAR_VALID_BIT | XR_SPACE_VELOCITY_ANGULAR_VALID_BIT);
}

static XrPath getInputActionFilter(Device device) {
  return (device == DEVICE_HAND_LEFT || device == DEVICE_HAND_RIGHT) ? state.actionFilters[device] : XR_NULL_PATH;
}

static bool getButtonState(Device device, DeviceButton button, bool* value, bool* changed, bool touch) {
  XrActionStateGetInfo info = {
    .type = XR_TYPE_ACTION_STATE_GET_INFO,
    .subactionPath = getInputActionFilter(device)
  };

  if (info.subactionPath == XR_NULL_PATH) {
    return false;
  }

  switch (button) {
    case BUTTON_TRIGGER: info.action = state.actions[ACTION_TRIGGER_DOWN + touch]; break;
    case BUTTON_THUMBREST: info.action = touch ? state.actions[ACTION_THUMBREST_TOUCH] : XR_NULL_HANDLE; break;
    case BUTTON_THUMBSTICK: info.action = state.actions[ACTION_THUMBSTICK_DOWN + touch]; break;
    case BUTTON_TOUCHPAD: info.action = state.actions[ACTION_TRACKPAD_DOWN + touch]; break;
    case BUTTON_MENU: info.action = state.actions[ACTION_MENU_DOWN + touch]; break;
    case BUTTON_GRIP: info.action = state.actions[ACTION_GRIP_DOWN + touch]; break;
    case BUTTON_A: info.action = state.actions[ACTION_A_DOWN + touch]; break;
    case BUTTON_B: info.action = state.actions[ACTION_B_DOWN + touch]; break;
    case BUTTON_X: info.action = state.actions[ACTION_X_DOWN + touch]; break;
    case BUTTON_Y: info.action = state.actions[ACTION_Y_DOWN + touch]; break;
    default: return false;
  }

  if (!info.action) {
    return false;
  }

  XrActionStateBoolean actionState = { .type = XR_TYPE_ACTION_STATE_BOOLEAN };
  XR(xrGetActionStateBoolean(state.session, &info, &actionState), "Failed to read button input");
  *value = actionState.currentState;
  *changed = actionState.changedSinceLastSync;
  return actionState.isActive;
}

static bool openxr_isDown(Device device, DeviceButton button, bool* down, bool* changed) {
  return getButtonState(device, button, down, changed, false);
}

static bool openxr_isTouched(Device device, DeviceButton button, bool* touched) {
  bool unused;
  return getButtonState(device, button, touched, &unused, true);
}

static bool getFloatAction(uint32_t action, XrPath filter, float* value) {
  XrActionStateGetInfo info = {
    .type = XR_TYPE_ACTION_STATE_GET_INFO,
    .action = state.actions[action],
    .subactionPath = filter
  };

  XrActionStateFloat actionState = { .type = XR_TYPE_ACTION_STATE_FLOAT };
  XR(xrGetActionStateFloat(state.session, &info, &actionState), "Failed to read axis input");
  *value = actionState.currentState;
  return actionState.isActive;
}

static bool openxr_getAxis(Device device, DeviceAxis axis, float* value) {
  XrPath filter = getInputActionFilter(device);

  if (filter == XR_NULL_PATH) {
    return false;
  }

  switch (axis) {
    case AXIS_TRIGGER:
      if (getFloatAction(ACTION_TRIGGER_AXIS, filter, &value[0])) {
        return true;
      }

      // FB extension for pinch
      if (!state.features.handTrackingAim) {
        return false;
      }

      XrHandTrackerEXT tracker = getHandTracker(device);

      if (!tracker) {
        return false;
      }

      XrHandJointsLocateInfoEXT info = {
        .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
        .baseSpace = state.referenceSpace,
        .time = state.frameState.predictedDisplayTime
      };

      XrHandTrackingAimStateFB aimState = {
        .type = XR_TYPE_HAND_TRACKING_AIM_STATE_FB
      };

      XrHandJointLocationEXT joints[MAX_HAND_JOINTS];
      XrHandJointLocationsEXT hand = {
        .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
        .next = &aimState,
        .jointCount = 26 + state.features.handTrackingElbow,
        .jointLocations = joints
      };

      if (XR_FAILED(xrLocateHandJointsEXT(tracker, &info, &hand))) {
        return false;
      }

      *value = aimState.pinchStrengthIndex;
      return true;
    case AXIS_THUMBSTICK: return getFloatAction(ACTION_THUMBSTICK_X, filter, &value[0]) && getFloatAction(ACTION_THUMBSTICK_Y, filter, &value[1]);
    case AXIS_TOUCHPAD: return getFloatAction(ACTION_TRACKPAD_X, filter, &value[0]) && getFloatAction(ACTION_TRACKPAD_Y, filter, &value[1]);
    case AXIS_GRIP: return getFloatAction(ACTION_GRIP_AXIS, filter, &value[0]);
    default: return false;
  }
}

static bool openxr_getSkeleton(Device device, float* poses) {
  XrHandTrackerEXT tracker = getHandTracker(device);

  if (!tracker || state.frameState.predictedDisplayTime <= 0) {
    return false;
  }

  XrHandJointsLocateInfoEXT info = {
    .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
    .baseSpace = state.referenceSpace,
    .time = state.frameState.predictedDisplayTime
  };

  XrHandJointLocationEXT joints[MAX_HAND_JOINTS];
  XrHandJointLocationsEXT hand = {
    .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
    .jointCount = 26 + state.features.handTrackingElbow,
    .jointLocations = joints
  };

  if (XR_FAILED(xrLocateHandJointsEXT(tracker, &info, &hand)) || !hand.isActive) {
    return false;
  }

  float* pose = poses;
  for (uint32_t i = 0; i < HAND_JOINT_COUNT; i++) {
    memcpy(pose, &joints[i].pose.position.x, 3 * sizeof(float));
    pose[3] = joints[i].radius;
    memcpy(pose + 4, &joints[i].pose.orientation.x, 4 * sizeof(float));
    pose += 8;
  }

  return true;
}

static bool openxr_vibrate(Device device, float power, float duration, float frequency) {
  XrHapticActionInfo info = {
    .type = XR_TYPE_HAPTIC_ACTION_INFO,
    .action = state.actions[ACTION_VIBRATE],
    .subactionPath = getInputActionFilter(device)
  };

  if (info.subactionPath == XR_NULL_PATH) {
    return false;
  }

  XrHapticVibration vibration = {
    .type = XR_TYPE_HAPTIC_VIBRATION,
    .duration = (XrDuration) (duration * 1e9f + .5f),
    .frequency = frequency,
    .amplitude = power
  };

  XR(xrApplyHapticFeedback(state.session, &info, (XrHapticBaseHeader*) &vibration), "Failed to apply haptic feedback");
  return true;
}

static void openxr_stopVibration(Device device) {
  XrHapticActionInfo info = {
    .type = XR_TYPE_HAPTIC_ACTION_INFO,
    .action = state.actions[ACTION_VIBRATE],
    .subactionPath = getInputActionFilter(device)
  };

  if (info.subactionPath == XR_NULL_PATH) {
    return;
  }

  XR(xrStopHapticFeedback(state.session, &info), "Failed to stop haptic feedback");
}

static ModelData* openxr_newModelDataFB(XrHandTrackerEXT tracker, bool animated) {
  if (!state.features.handTrackingMesh) {
    return NULL;
  }

  // First, figure out how much data there is
  XrHandTrackingMeshFB mesh = { .type = XR_TYPE_HAND_TRACKING_MESH_FB };
  XrResult result = xrGetHandMeshFB(tracker, &mesh);

  if (XR_FAILED(result)) {
    return NULL;
  }

  uint32_t jointCount = mesh.jointCapacityInput = mesh.jointCountOutput;
  uint32_t vertexCount = mesh.vertexCapacityInput = mesh.vertexCountOutput;
  uint32_t indexCount = mesh.indexCapacityInput = mesh.indexCountOutput;

  // Sum all the sizes to get the total amount of memory required
  size_t sizes[10];
  size_t totalSize = 0;
  size_t alignment = 8;
  totalSize += sizes[0] = ALIGN(jointCount * sizeof(XrPosef), alignment);
  totalSize += sizes[1] = ALIGN(jointCount * sizeof(float), alignment);
  totalSize += sizes[2] = ALIGN(jointCount * sizeof(XrHandJointEXT), alignment);
  totalSize += sizes[3] = ALIGN(vertexCount * sizeof(XrVector3f), alignment);
  totalSize += sizes[4] = ALIGN(vertexCount * sizeof(XrVector3f), alignment);
  totalSize += sizes[5] = ALIGN(vertexCount * sizeof(XrVector2f), alignment);
  totalSize += sizes[6] = ALIGN(vertexCount * sizeof(XrVector4sFB), alignment);
  totalSize += sizes[7] = ALIGN(vertexCount * sizeof(XrVector4f), alignment);
  totalSize += sizes[8] = ALIGN(indexCount * sizeof(int16_t), alignment);
  totalSize += sizes[9] = ALIGN(jointCount * 16 * sizeof(float), alignment);

  // Allocate
  char* meshData = lovrMalloc(totalSize);

  // Write offseted pointers to the mesh struct, to be filled in by the second call
  size_t offset = 0;
  mesh.jointBindPoses = (XrPosef*) (meshData + offset), offset += sizes[0];
  mesh.jointRadii = (float*) (meshData + offset), offset += sizes[1];
  mesh.jointParents = (XrHandJointEXT*) (meshData + offset), offset += sizes[2];
  mesh.vertexPositions = (XrVector3f*) (meshData + offset), offset += sizes[3];
  mesh.vertexNormals = (XrVector3f*) (meshData + offset), offset += sizes[4];
  mesh.vertexUVs = (XrVector2f*) (meshData + offset), offset += sizes[5];
  mesh.vertexBlendIndices = (XrVector4sFB*) (meshData + offset), offset += sizes[6];
  mesh.vertexBlendWeights = (XrVector4f*) (meshData + offset), offset += sizes[7];
  mesh.indices = (int16_t*) (meshData + offset), offset += sizes[8];
  float* inverseBindMatrices = (float*) (meshData + offset); offset += sizes[9];
  lovrAssert(offset == totalSize, "Unreachable!");

  // Populate the data
  result = xrGetHandMeshFB(tracker, &mesh);
  if (XR_FAILED(result)) {
    lovrFree(meshData);
    return NULL;
  }

  ModelData* model = lovrCalloc(sizeof(ModelData));
  model->ref = 1;
  model->blobCount = 1;
  model->bufferCount = 6;
  model->attributeCount = 6;
  model->primitiveCount = 1;
  model->skinCount = 1;
  model->jointCount = jointCount;
  model->childCount = jointCount + 1;
  model->nodeCount = 2 + jointCount;
  lovrModelDataAllocate(model);

  model->metadata = lovrMalloc(sizeof(XrHandTrackerEXT));
  *((XrHandTrackerEXT*)model->metadata) = tracker;
  model->metadataSize = sizeof(XrHandTrackerEXT);
  model->metadataType = META_HANDTRACKING_FB;

  model->blobs[0] = lovrBlobCreate(meshData, totalSize, "Hand Mesh Data");

  model->buffers[0] = (ModelBuffer) {
    .offset = (char*) mesh.vertexPositions - (char*) meshData,
    .data = (char*) mesh.vertexPositions,
    .size = sizeof(mesh.vertexPositions[0]) * vertexCount,
    .stride = sizeof(mesh.vertexPositions[0])
  };

  model->buffers[1] = (ModelBuffer) {
    .offset = (char*) mesh.vertexNormals - (char*) meshData,
    .data = (char*) mesh.vertexNormals,
    .size = sizeof(mesh.vertexNormals[0]) * vertexCount,
    .stride = sizeof(mesh.vertexNormals[0])
  };

  model->buffers[2] = (ModelBuffer) {
    .offset = (char*) mesh.vertexUVs - (char*) meshData,
    .data = (char*) mesh.vertexUVs,
    .size = sizeof(mesh.vertexUVs[0]) * vertexCount,
    .stride = sizeof(mesh.vertexUVs[0])
  };

  model->buffers[3] = (ModelBuffer) {
    .offset = (char*) mesh.vertexBlendIndices - (char*) meshData,
    .data = (char*) mesh.vertexBlendIndices,
    .size = sizeof(mesh.vertexBlendIndices[0]) * vertexCount,
    .stride = sizeof(mesh.vertexBlendIndices[0])
  };

  model->buffers[4] = (ModelBuffer) {
    .offset = (char*) mesh.vertexBlendWeights - (char*) meshData,
    .data = (char*) mesh.vertexBlendWeights,
    .size = sizeof(mesh.vertexBlendWeights[0]) * vertexCount,
    .stride = sizeof(mesh.vertexBlendWeights[0])
  };

  model->buffers[5] = (ModelBuffer) {
    .offset = (char*) mesh.indices - (char*) meshData,
    .data = (char*) mesh.indices,
    .size = sizeof(mesh.indices[0]) * indexCount,
    .stride = sizeof(mesh.indices[0])
  };

  model->attributes[0] = (ModelAttribute) { .buffer = 0, .type = F32, .components = 3, .count = vertexCount };
  model->attributes[1] = (ModelAttribute) { .buffer = 1, .type = F32, .components = 3 };
  model->attributes[2] = (ModelAttribute) { .buffer = 2, .type = F32, .components = 2 };
  model->attributes[3] = (ModelAttribute) { .buffer = 3, .type = I16, .components = 4 };
  model->attributes[4] = (ModelAttribute) { .buffer = 4, .type = F32, .components = 4 };
  model->attributes[5] = (ModelAttribute) { .buffer = 5, .type = U16, .count = indexCount };

  model->primitives[0] = (ModelPrimitive) {
    .mode = DRAW_TRIANGLE_LIST,
    .attributes = {
      [ATTR_POSITION] = &model->attributes[0],
      [ATTR_NORMAL] = &model->attributes[1],
      [ATTR_UV] = &model->attributes[2],
      [ATTR_JOINTS] = &model->attributes[3],
      [ATTR_WEIGHTS] = &model->attributes[4]
    },
    .indices = &model->attributes[5],
    .material = ~0u
  };

  // The nodes in the Model correspond directly to the joints in the skin, for convenience
  uint32_t* children = model->children;
  model->skins[0].joints = model->joints;
  model->skins[0].jointCount = model->jointCount;
  model->skins[0].inverseBindMatrices = inverseBindMatrices;
  for (uint32_t i = 0; i < model->jointCount; i++) {
    model->joints[i] = i;

    // Joint node
    model->nodes[i] = (ModelNode) {
      .transform.translation = { 0.f, 0.f, 0.f },
      .transform.rotation = { 0.f, 0.f, 0.f, 1.f },
      .transform.scale = { 1.f, 1.f, 1.f },
      .skin = ~0u
    };

    // Inverse bind matrix
    XrPosef* pose = &mesh.jointBindPoses[i];
    float* inverseBindMatrix = inverseBindMatrices + 16 * i;
    mat4_fromPose(inverseBindMatrix, &pose->position.x, &pose->orientation.x);
    mat4_invert(inverseBindMatrix);

    // Add child bones by looking for any bones that have a parent of the current bone.
    // This is somewhat slow; use the fact that bones are sorted to reduce the work a bit.
    model->nodes[i].childCount = 0;
    model->nodes[i].children = children;
    for (uint32_t j = i + 1; j < jointCount; j++) {
      if (mesh.jointParents[j] == i) {
        model->nodes[i].children[model->nodes[i].childCount++] = j;
        children++;
      }
    }
  }

  // Add a node that holds the skinned mesh
  model->nodes[model->jointCount] = (ModelNode) {
    .transform.translation = { 0.f, 0.f, 0.f },
    .transform.rotation = { 0.f, 0.f, 0.f, 1.f },
    .transform.scale = { 1.f, 1.f, 1.f },
    .primitiveIndex = 0,
    .primitiveCount = 1,
    .skin = 0
  };

  // The root node has the mesh node and root joint as children
  model->rootNode = model->jointCount + 1;
  model->nodes[model->rootNode] = (ModelNode) {
    .hasMatrix = true,
    .transform = { MAT4_IDENTITY },
    .childCount = 2,
    .children = children,
    .skin = ~0u
  };

  // Add the children to the root node
  *children++ = XR_HAND_JOINT_WRIST_EXT;
  *children++ = model->jointCount;

  lovrModelDataFinalize(model);

  return model;
}

typedef struct {
  XrControllerModelKeyMSFT modelKey;
  uint32_t* nodeIndices;
} MetadataControllerMSFT;

static ModelData* openxr_newModelDataMSFT(XrControllerModelKeyMSFT modelKey, bool animated) {
  uint32_t size;
  if (XR_FAILED(xrLoadControllerModelMSFT(state.session, modelKey, 0, &size, NULL))) {
    return NULL;
  }

  unsigned char* modelData = lovrMalloc(size);

  if (XR_FAILED(xrLoadControllerModelMSFT(state.session, modelKey, size, &size, modelData))) {
    lovrFree(modelData);
    return NULL;
  }

  Blob* blob = lovrBlobCreate(modelData, size, "Controller Model Data");
  ModelData* model = lovrModelDataCreate(blob, NULL);
  lovrRelease(blob, lovrBlobDestroy);

  XrControllerModelNodePropertiesMSFT nodeProperties[16];
  for (uint32_t i = 0; i < COUNTOF(nodeProperties); i++) {
    nodeProperties[i].type = XR_TYPE_CONTROLLER_MODEL_NODE_PROPERTIES_MSFT;
    nodeProperties[i].next = 0;
  }
  XrControllerModelPropertiesMSFT properties = {
    .type = XR_TYPE_CONTROLLER_MODEL_PROPERTIES_MSFT,
    .nodeCapacityInput = COUNTOF(nodeProperties),
    .nodeProperties = nodeProperties,
  };

  if (XR_FAILED(xrGetControllerModelPropertiesMSFT(state.session, modelKey, &properties))) {
    return false;
  }

  lovrFree(model->metadata);
  model->metadataType = META_CONTROLLER_MSFT;
  model->metadataSize = sizeof(MetadataControllerMSFT) + sizeof(uint32_t) * properties.nodeCountOutput;
  model->metadata = lovrMalloc(model->metadataSize);

  MetadataControllerMSFT* metadata = model->metadata;
  metadata->modelKey = modelKey;
  metadata->nodeIndices = (uint32_t*)((char*) model->metadata + sizeof(MetadataControllerMSFT));

  for (uint32_t i = 0; i < properties.nodeCountOutput; i++) {
    const char* name = nodeProperties[i].nodeName;
    uint64_t nodeIndex = map_get(model->nodeMap, hash64(name, strlen(name)));
    lovrCheck(nodeIndex != MAP_NIL, "ModelData has no node named '%s'", name);
    metadata->nodeIndices[i] = nodeIndex;
  }

  return model;
}

static ModelData* openxr_newModelData(Device device, bool animated) {
  XrHandTrackerEXT tracker;
  if ((tracker = getHandTracker(device))) {
    return openxr_newModelDataFB(tracker, animated);
  }

  XrControllerModelKeyMSFT modelKey;
  if ((modelKey = getControllerModelKey(device))) {
    return openxr_newModelDataMSFT(modelKey, animated);
  }

  return NULL;
}

static bool openxr_animateFB(Model* model, const ModelInfo* info) {
  XrHandTrackerEXT tracker = *(XrHandTrackerEXT*) info->data->metadata;
  Device device = tracker == state.handTrackers[0] ? DEVICE_HAND_LEFT : DEVICE_HAND_RIGHT;

  XrHandJointsLocateInfoEXT locateInfo = {
    .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
    .baseSpace = state.spaces[device],
    .time = state.frameState.predictedDisplayTime
  };

  XrHandJointLocationEXT joints[MAX_HAND_JOINTS];
  XrHandJointLocationsEXT hand = {
    .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
    .jointCount = 26 + state.features.handTrackingElbow,
    .jointLocations = joints
  };

  if (XR_FAILED(xrLocateHandJointsEXT(tracker, &locateInfo, &hand)) || !hand.isActive) {
    return false;
  }

  lovrModelResetNodeTransforms(model);

  // This is kinda brittle, ideally we would use the jointParents from the actual mesh object
  uint32_t jointParents[HAND_JOINT_COUNT] = {
    XR_HAND_JOINT_WRIST_EXT,
    ~0u,
    XR_HAND_JOINT_WRIST_EXT,
    XR_HAND_JOINT_THUMB_METACARPAL_EXT,
    XR_HAND_JOINT_THUMB_PROXIMAL_EXT,
    XR_HAND_JOINT_THUMB_DISTAL_EXT,
    XR_HAND_JOINT_WRIST_EXT,
    XR_HAND_JOINT_INDEX_METACARPAL_EXT,
    XR_HAND_JOINT_INDEX_PROXIMAL_EXT,
    XR_HAND_JOINT_INDEX_INTERMEDIATE_EXT,
    XR_HAND_JOINT_INDEX_DISTAL_EXT,
    XR_HAND_JOINT_WRIST_EXT,
    XR_HAND_JOINT_MIDDLE_METACARPAL_EXT,
    XR_HAND_JOINT_MIDDLE_PROXIMAL_EXT,
    XR_HAND_JOINT_MIDDLE_INTERMEDIATE_EXT,
    XR_HAND_JOINT_MIDDLE_DISTAL_EXT,
    XR_HAND_JOINT_WRIST_EXT,
    XR_HAND_JOINT_RING_METACARPAL_EXT,
    XR_HAND_JOINT_RING_PROXIMAL_EXT,
    XR_HAND_JOINT_RING_INTERMEDIATE_EXT,
    XR_HAND_JOINT_RING_DISTAL_EXT,
    XR_HAND_JOINT_WRIST_EXT,
    XR_HAND_JOINT_LITTLE_METACARPAL_EXT,
    XR_HAND_JOINT_LITTLE_PROXIMAL_EXT,
    XR_HAND_JOINT_LITTLE_INTERMEDIATE_EXT,
    XR_HAND_JOINT_LITTLE_DISTAL_EXT
  };

  float position[3], orientation[4], scale[3] = { 1.f, 1.f, 1.f };
  for (uint32_t i = 0; i < HAND_JOINT_COUNT; i++) {
    if (jointParents[i] == ~0u) {
      XrPosef* pose = &joints[i].pose;
      lovrModelSetNodeTransform(model, i, &pose->position.x, scale, &pose->orientation.x, 1.f);
    } else {
      XrPosef* parent = &joints[jointParents[i]].pose;
      XrPosef* pose = &joints[i].pose;

      // Convert global pose to parent-local pose (premultiply with inverse of parent pose)
      // TODO there should be maf for this
      vec3_init(position, &pose->position.x);
      vec3_sub(position, &parent->position.x);

      quat_init(orientation, &parent->orientation.x);
      quat_conjugate(orientation);

      quat_rotate(orientation, position);
      quat_mul(orientation, orientation, &pose->orientation.x);

      lovrModelSetNodeTransform(model, i, position, scale, orientation, 1.f);
    }
  }

  return true;
}

static bool openxr_animateMSFT(Model* model, const ModelInfo* info) {
  MetadataControllerMSFT* metadata = info->data->metadata;

  XrControllerModelNodeStateMSFT nodeStates[16];
  for (uint32_t i = 0; i < COUNTOF(nodeStates); i++) {
    nodeStates[i].type = XR_TYPE_CONTROLLER_MODEL_NODE_STATE_MSFT;
    nodeStates[i].next = 0;
  }
  XrControllerModelStateMSFT modelState = {
    .type = XR_TYPE_CONTROLLER_MODEL_STATE_MSFT,
    .nodeCapacityInput = COUNTOF(nodeStates),
    .nodeStates = nodeStates,
  };

  if (XR_FAILED(xrGetControllerModelStateMSFT(state.session, metadata->modelKey, &modelState))) {
    return false;
  }

  for (uint32_t i = 0; i < modelState.nodeCountOutput; i++) {
    float position[3], rotation[4];
    vec3_init(position, (vec3)&nodeStates[i].nodePose.position);
    quat_init(rotation, (quat)&nodeStates[i].nodePose.orientation);
    lovrModelSetNodeTransform(model, metadata->nodeIndices[i], position, NULL, rotation, 1);
  }

  return false;
}

static bool openxr_animate(Model* model) {
  const ModelInfo* info = lovrModelGetInfo(model);

  switch (info->data->metadataType) {
    case META_HANDTRACKING_FB: return openxr_animateFB(model, info);
    case META_CONTROLLER_MSFT: return openxr_animateMSFT(model, info);
    default: return false;
  }
}

static Layer* openxr_newLayer(uint32_t width, uint32_t height) {
  Layer* layer = lovrCalloc(sizeof(Layer));
  layer->ref = 1;
  layer->width = width;
  layer->height = height;
  swapchain_init(&layer->swapchain, width, height, false, false);
  swapchain_acquire(&layer->swapchain); // Avoid submission of un-acquired swapchain
  layer->info.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
  layer->info.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  layer->info.layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
  layer->info.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
  layer->info.subImage.swapchain = layer->swapchain.handle;
  layer->info.subImage.imageRect.extent.width = width;
  layer->info.subImage.imageRect.extent.height = height;
  layer->info.pose.orientation.w = 1.f;
  layer->info.size.width = 1.f;
  layer->info.size.height = 1.f;
  if (state.features.layerDepthTest) {
    layer->depthTest.type = XR_TYPE_COMPOSITION_LAYER_DEPTH_TEST_FB;
    layer->depthTest.next = layer->info.next;
    layer->depthTest.depthMask = XR_TRUE;
    layer->depthTest.compareOp = XR_COMPARE_OP_LESS_OR_EQUAL_FB;
    layer->info.next = &layer->depthTest;
  }
  if (state.features.layerSettings) {
    layer->settings.type = XR_TYPE_COMPOSITION_LAYER_SETTINGS_FB;
    layer->settings.next = layer->info.next;
    layer->info.next = &layer->settings;
  }
  layer->pass = lovrPassCreate(NULL);
  return layer;
}

static void openxr_destroyLayer(void* ref) {
  Layer* layer = ref;
  swapchain_destroy(&layer->swapchain);
  lovrRelease(layer->pass, lovrPassDestroy);
  lovrFree(layer);
}

static Layer** openxr_getLayers(uint32_t* count) {
  *count = state.layerCount;
  return state.layers;
}

static void openxr_setLayers(Layer** layers, uint32_t count) {
  lovrCheck(count <= MAX_LAYERS, "Too many layers");

  for (uint32_t i = 0; i < state.layerCount; i++) {
    lovrRelease(state.layers[i], lovrLayerDestroy);
  }

  state.layerCount = count;
  for (uint32_t i = 0; i < count; i++) {
    lovrRetain(layers[i]);
    state.layers[i] = layers[i];
  }
}

static void openxr_getLayerPose(Layer* layer, float position[3], float orientation[4]) {
  memcpy(position, &layer->info.pose.position.x, 3 * sizeof(float));
  memcpy(orientation, &layer->info.pose.orientation.x, 4 * sizeof(float));
}

static void openxr_setLayerPose(Layer* layer, float position[3], float orientation[4]) {
  memcpy(&layer->info.pose.position.x, position, 3 * sizeof(float));
  memcpy(&layer->info.pose.orientation.x, orientation, 4 * sizeof(float));
}

static void openxr_getLayerSize(Layer* layer, float* width, float* height) {
  *width = layer->info.size.width;
  *height = layer->info.size.height;
}

static void openxr_setLayerSize(Layer* layer, float width, float height) {
  layer->info.size.width = width;
  layer->info.size.height = height;
}

static ViewMask openxr_getLayerViewMask(Layer* layer) {
  return (ViewMask) layer->info.eyeVisibility;
}

static void openxr_setLayerViewMask(Layer* layer, ViewMask mask) {
  layer->info.eyeVisibility = (XrEyeVisibility) mask;
}

static void openxr_getLayerViewport(Layer* layer, int32_t* viewport) {
  viewport[0] = layer->info.subImage.imageRect.offset.x;
  viewport[1] = layer->info.subImage.imageRect.offset.y;
  viewport[2] = layer->info.subImage.imageRect.extent.width;
  viewport[3] = layer->info.subImage.imageRect.extent.height;
}

static void openxr_setLayerViewport(Layer* layer, int32_t* viewport) {
  layer->info.subImage.imageRect.offset.x = viewport[0];
  layer->info.subImage.imageRect.offset.y = viewport[1];
  layer->info.subImage.imageRect.extent.width = viewport[2] ? viewport[2] : layer->width - viewport[0];
  layer->info.subImage.imageRect.extent.height = viewport[3] ? viewport[3] : layer->height - viewport[1];
}

static bool openxr_getLayerFlag(Layer* layer, LayerFlag flag) {
  switch (flag) {
    case LAYER_SUPERSAMPLE: return layer->settings.layerFlags & XR_COMPOSITION_LAYER_SETTINGS_QUALITY_SUPER_SAMPLING_BIT_FB;
    case LAYER_SHARPEN: return layer->settings.layerFlags & XR_COMPOSITION_LAYER_SETTINGS_QUALITY_SHARPENING_BIT_FB;
    default: lovrUnreachable();
  }
}

static void openxr_setLayerFlag(Layer* layer, LayerFlag flag, bool enable) {
  XrCompositionLayerSettingsFlagsFB bit;

  switch (flag) {
    case LAYER_SUPERSAMPLE: bit = XR_COMPOSITION_LAYER_SETTINGS_QUALITY_SUPER_SAMPLING_BIT_FB; break;
    case LAYER_SHARPEN: bit = XR_COMPOSITION_LAYER_SETTINGS_QUALITY_SHARPENING_BIT_FB; break;
    default: lovrUnreachable();
  }

  if (enable) {
    layer->settings.layerFlags |= bit;
  } else {
    layer->settings.layerFlags &= ~bit;
  }
}

static Texture* openxr_getLayerTexture(Layer* layer) {
  return swapchain_acquire(&layer->swapchain);
}

static Pass* openxr_getLayerPass(Layer* layer) {
  Texture* textures[4] = { openxr_getLayerTexture(layer) };
  lovrPassSetCanvas(layer->pass, textures, NULL, state.depthFormat, state.config.antialias ? 4 : 1);

  float background[4][4];
  LoadAction loads[4] = { LOAD_CLEAR };
  lovrGraphicsGetBackgroundColor(background[0]);
  lovrPassSetClear(layer->pass, loads, background, LOAD_CLEAR, 0.f);

  float viewMatrix[16] = MAT4_IDENTITY;
  lovrPassSetViewMatrix(layer->pass, 0, viewMatrix);

  float projection[16];
  mat4_orthographic(projection, 0, layer->width, 0, layer->height, -1.f, 1.f);
  lovrPassSetProjection(layer->pass, 0, projection);

  return layer->pass;
}

static Texture* openxr_getTexture(void) {
  if (!SESSION_ACTIVE(state.sessionState)) {
    return NULL;
  }

  if (!state.began) {
    XrFrameBeginInfo beginfo = { .type = XR_TYPE_FRAME_BEGIN_INFO };
    XR(xrBeginFrame(state.session, &beginfo), "Failed to begin headset rendering");
    state.began = true;
  }

  if (!state.frameState.shouldRender) {
    return NULL;
  }

  return swapchain_acquire(&state.swapchains[COLOR]);
}

static Texture* openxr_getDepthTexture(void) {
  if (!SESSION_ACTIVE(state.sessionState) || !state.features.depth) {
    return NULL;
  }

  if (!state.began) {
    XrFrameBeginInfo beginfo = { .type = XR_TYPE_FRAME_BEGIN_INFO };
    XR(xrBeginFrame(state.session, &beginfo), "Failed to begin headset rendering");
    state.began = true;
  }

  if (!state.frameState.shouldRender) {
    return NULL;
  }

  return swapchain_acquire(&state.swapchains[DEPTH]);
}

static Pass* openxr_getPass(void) {
  if (state.began) {
    return state.frameState.shouldRender ? state.pass : NULL;
  }

  Texture* textures[4] = { openxr_getTexture() };
  Texture* depth = openxr_getDepthTexture();

  if (!textures[0]) {
    return NULL;
  }

  lovrPassSetCanvas(state.pass, textures, depth, state.depthFormat, state.config.antialias ? 4 : 1);

  float background[4][4];
  LoadAction loads[4] = { LOAD_CLEAR };
  lovrGraphicsGetBackgroundColor(background[0]);
  lovrPassSetClear(state.pass, loads, background, LOAD_CLEAR, 0.f);

  uint32_t count;
  XrView views[2];
  XrViewStateFlags flags = getViews(views, &count);

  for (uint32_t i = 0; i < count; i++) {
    state.layerViews[i].pose = views[i].pose;
    state.layerViews[i].fov = views[i].fov;

    float viewMatrix[16];
    float projection[16];

    if (flags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
      mat4_fromQuat(viewMatrix, &views[i].pose.orientation.x);
    } else {
      mat4_identity(viewMatrix);
    }

    if (flags & XR_VIEW_STATE_POSITION_VALID_BIT) {
      memcpy(viewMatrix + 12, &views[i].pose.position.x, 3 * sizeof(float));
    }

    mat4_invert(viewMatrix);
    lovrPassSetViewMatrix(state.pass, i, viewMatrix);

    if (flags != 0) {
      XrFovf* fov = &views[i].fov;
      mat4_fov(projection, -fov->angleLeft, fov->angleRight, fov->angleUp, -fov->angleDown, state.clipNear, state.clipFar);
      lovrPassSetProjection(state.pass, i, projection);
    }
  }

  return state.pass;
}

static void openxr_submit(void) {
  if (!SESSION_ACTIVE(state.sessionState)) {
    state.waited = false;
    return;
  }

  if (!state.began) {
    XrFrameBeginInfo beginfo = { .type = XR_TYPE_FRAME_BEGIN_INFO };
    XR(xrBeginFrame(state.session, &beginfo), "Failed to begin headset rendering");
    state.began = true;
  }

  XrCompositionLayerBaseHeader const* layers[MAX_LAYERS + 2];

  XrCompositionLayerDepthTestFB depthTestInfo = {
    .type = XR_TYPE_COMPOSITION_LAYER_DEPTH_TEST_FB,
    .depthMask = XR_TRUE,
    .compareOp = XR_COMPARE_OP_LESS_OR_EQUAL_FB
  };

  XrFrameEndInfo info = {
    .type = XR_TYPE_FRAME_END_INFO,
    .displayTime = state.frameState.predictedDisplayTime,
    .environmentBlendMode = state.blendMode,
    .layers = layers
  };

  if (state.frameState.shouldRender) {
    swapchain_release(&state.swapchains[COLOR]);
    swapchain_release(&state.swapchains[DEPTH]);

    if (state.passthroughActive) {
      layers[info.layerCount++] = (const XrCompositionLayerBaseHeader*) &state.passthroughLayer;
    }

    state.layer.next = NULL;

    if (state.features.layerDepthTest && state.features.depth && state.layerCount > 0) {
      depthTestInfo.next = state.layer.next;
      state.layer.next = &depthTestInfo;
    }

    if (state.features.depth) {
      if (state.clipFar == 0.f) {
        state.depthInfo[0].nearZ = state.depthInfo[1].nearZ = +INFINITY;
        state.depthInfo[0].farZ = state.depthInfo[1].farZ = state.clipNear;
      } else {
        state.depthInfo[0].nearZ = state.depthInfo[1].nearZ = state.clipNear;
        state.depthInfo[0].farZ = state.depthInfo[1].farZ = state.clipFar;
      }
    }

    if (state.features.overlay || state.passthroughActive || state.blendMode != XR_ENVIRONMENT_BLEND_MODE_OPAQUE) {
      state.layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    } else {
      state.layer.layerFlags = 0;
    }

    state.layer.space = state.referenceSpace;

    layers[info.layerCount++] = (const XrCompositionLayerBaseHeader*) &state.layer;

    for (uint32_t i = 0; i < state.layerCount; i++) {
      layers[info.layerCount++] = (const XrCompositionLayerBaseHeader*) &state.layers[i]->info;
      state.layers[i]->info.space = state.referenceSpace;
      swapchain_release(&state.layers[i]->swapchain);
    }
  }

  XR(xrEndFrame(state.session, &info), "Failed to submit layers");
  state.began = false;
  state.waited = false;
}

static bool openxr_isVisible(void) {
  return state.sessionState >= XR_SESSION_STATE_VISIBLE;
}

static bool openxr_isFocused(void) {
  return state.sessionState == XR_SESSION_STATE_FOCUSED;
}

static bool openxr_isMounted(void) {
  return state.features.presence ? state.mounted : true;
}

static double openxr_update(void) {
  if (state.waited) return openxr_getDeltaTime();

  XrEventDataBuffer e; // Not using designated initializers here to avoid an implicit 4k zero
  e.type = XR_TYPE_EVENT_DATA_BUFFER;
  e.next = NULL;

  while (xrPollEvent(state.instance, &e) == XR_SUCCESS) {
    switch (e.type) {
      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
        XrEventDataSessionStateChanged* event = (XrEventDataSessionStateChanged*) &e;

        switch (event->state) {
          case XR_SESSION_STATE_READY:
            XR(xrBeginSession(state.session, &(XrSessionBeginInfo) {
              .type = XR_TYPE_SESSION_BEGIN_INFO,
              .primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO
            }), "Failed to begin session");
            break;

          case XR_SESSION_STATE_STOPPING:
            XR(xrEndSession(state.session), "Failed to end session");
            state.mounted = false;
            break;

          case XR_SESSION_STATE_EXITING:
          case XR_SESSION_STATE_LOSS_PENDING:
            lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit.exitCode = 0 });
            break;

          default: break;
        }

        bool wasVisible = state.sessionState >= XR_SESSION_STATE_VISIBLE;
        bool isVisible = event->state >= XR_SESSION_STATE_VISIBLE;
        if (wasVisible != isVisible) {
          lovrEventPush((Event) { .type = EVENT_VISIBLE, .data.boolean.value = isVisible });
        }

        bool wasFocused = state.sessionState == XR_SESSION_STATE_FOCUSED;
        bool isFocused = event->state == XR_SESSION_STATE_FOCUSED;
        if (wasFocused != isFocused) {
          lovrEventPush((Event) { .type = EVENT_FOCUS, .data.boolean.value = isFocused });
        }

        state.sessionState = event->state;
        break;
      }
      case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
        XrEventDataReferenceSpaceChangePending* event = (XrEventDataReferenceSpaceChangePending*) &e;
        if (event->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL) {
          createReferenceSpace(event->changeTime);
          lovrEventPush((Event) { .type = EVENT_RECENTER });
        }
        break;
      }
      case XR_TYPE_EVENT_DATA_USER_PRESENCE_CHANGED_EXT: {
        XrEventDataUserPresenceChangedEXT* event = (XrEventDataUserPresenceChangedEXT*) &e;
        state.mounted = event->isUserPresent;
        lovrEventPush((Event) { .type = EVENT_MOUNT, .data.boolean.value = state.mounted });
        break;
      }
      default: break;
    }
    e.type = XR_TYPE_EVENT_DATA_BUFFER;
  }

  if (SESSION_ACTIVE(state.sessionState)) {
    state.lastDisplayTime = state.frameState.predictedDisplayTime;
    XR(xrWaitFrame(state.session, NULL, &state.frameState), "Failed to wait for next frame");
    state.waited = true;

    if (state.epoch == 0) {
      state.epoch = state.frameState.predictedDisplayTime - state.frameState.predictedDisplayPeriod;
      state.lastDisplayTime = state.epoch;
    }

    XrActiveActionSet activeSets[] = {
      { state.actionSet, XR_NULL_PATH }
    };

    XrActionsSyncInfo syncInfo = {
      .type = XR_TYPE_ACTIONS_SYNC_INFO,
      .countActiveActionSets = COUNTOF(activeSets),
      .activeActionSets = activeSets
    };

    XR(xrSyncActions(state.session, &syncInfo), "Failed to sync actions");
  }

  // Throttle when session is idle (but not too much, a desktop window might be rendering stuff)
  if (state.sessionState == XR_SESSION_STATE_IDLE) {
    os_sleep(.001);
  }

  return openxr_getDeltaTime();
}

HeadsetInterface lovrHeadsetOpenXRDriver = {
  .driverType = DRIVER_OPENXR,
  .getVulkanPhysicalDevice = openxr_getVulkanPhysicalDevice,
  .createVulkanInstance = openxr_createVulkanInstance,
  .createVulkanDevice = openxr_createVulkanDevice,
  .getOpenXRInstanceHandle = openxr_getOpenXRInstanceHandle,
  .getOpenXRSessionHandle = openxr_getOpenXRSessionHandle,
  .init = openxr_init,
  .start = openxr_start,
  .stop = openxr_stop,
  .destroy = openxr_destroy,
  .getDriverName = openxr_getDriverName,
  .getName = openxr_getName,
  .isSeated = openxr_isSeated,
  .getDisplayDimensions = openxr_getDisplayDimensions,
  .getRefreshRate = openxr_getRefreshRate,
  .setRefreshRate = openxr_setRefreshRate,
  .getRefreshRates = openxr_getRefreshRates,
  .getPassthrough = openxr_getPassthrough,
  .setPassthrough = openxr_setPassthrough,
  .isPassthroughSupported = openxr_isPassthroughSupported,
  .getDisplayTime = openxr_getDisplayTime,
  .getDeltaTime = openxr_getDeltaTime,
  .getViewCount = openxr_getViewCount,
  .getViewPose = openxr_getViewPose,
  .getViewAngles = openxr_getViewAngles,
  .getClipDistance = openxr_getClipDistance,
  .setClipDistance = openxr_setClipDistance,
  .getBoundsDimensions = openxr_getBoundsDimensions,
  .getBoundsGeometry = openxr_getBoundsGeometry,
  .getPose = openxr_getPose,
  .getVelocity = openxr_getVelocity,
  .isDown = openxr_isDown,
  .isTouched = openxr_isTouched,
  .getAxis = openxr_getAxis,
  .getSkeleton = openxr_getSkeleton,
  .vibrate = openxr_vibrate,
  .stopVibration = openxr_stopVibration,
  .newModelData = openxr_newModelData,
  .animate = openxr_animate,
  .newLayer = openxr_newLayer,
  .destroyLayer = openxr_destroyLayer,
  .getLayers = openxr_getLayers,
  .setLayers = openxr_setLayers,
  .getLayerPose = openxr_getLayerPose,
  .setLayerPose = openxr_setLayerPose,
  .getLayerSize = openxr_getLayerSize,
  .setLayerSize = openxr_setLayerSize,
  .getLayerViewMask = openxr_getLayerViewMask,
  .setLayerViewMask = openxr_setLayerViewMask,
  .getLayerViewport = openxr_getLayerViewport,
  .setLayerViewport = openxr_setLayerViewport,
  .getLayerFlag = openxr_getLayerFlag,
  .setLayerFlag = openxr_setLayerFlag,
  .getLayerTexture = openxr_getLayerTexture,
  .getLayerPass = openxr_getLayerPass,
  .getTexture = openxr_getTexture,
  .getPass = openxr_getPass,
  .submit = openxr_submit,
  .isVisible = openxr_isVisible,
  .isFocused = openxr_isFocused,
  .isMounted = openxr_isMounted,
  .update = openxr_update
};
