#include "headset/headset.h"
#include "data/blob.h"
#include "data/image.h"
#include "data/modelData.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "math/math.h"
#include "system/system.h"
#include "timer/timer.h"
#include "core/maf.h"
#include "core/os.h"
#include "util.h"
#include <stdatomic.h>
#include <threads.h>
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
 #include <time.h>
 #define XR_USE_TIMESPEC
 #if defined(__ANDROID__)
  #define XR_USE_PLATFORM_ANDROID
  void* os_get_java_vm(void);
  void* os_get_jni_context(void);
  #include <jni.h>
  #include <unistd.h>
  #define XR_FOREACH_PLATFORM(X)\
    X(xrConvertTimespecTimeToTimeKHR)\
    X(xrSetAndroidApplicationThreadKHR)
 #else
  #define XR_FOREACH_PLATFORM(X) X(xrConvertTimespecTimeToTimeKHR)
 #endif
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

#define XR(f, s) do { XrResult r = f; if (XR_FAILED(r)) { xrthrow(r, s); return 0; } } while(0)
#define XRG(f, s, j) do { XrResult r = f; if (XR_FAILED(r)) { xrthrow(r, s); goto j; } } while(0)
#define SESSION_RUNNING(s) (s >= XR_SESSION_STATE_READY && s <= XR_SESSION_STATE_FOCUSED)
#define MAX_IMAGES 4
#define MAX_HAND_JOINTS 27

#define XR_FOREACH(X)\
  X(xrDestroyInstance)\
  X(xrGetInstanceProperties)\
  X(xrCreateDebugUtilsMessengerEXT)\
  X(xrDestroyDebugUtilsMessengerEXT)\
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
  X(xrUpdateSwapchainFB)\
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
  X(xrGetCurrentInteractionProfile)\
  X(xrAttachSessionActionSets)\
  X(xrGetActionStateBoolean)\
  X(xrGetActionStateFloat)\
  X(xrGetActionStateVector2f)\
  X(xrGetActionStatePose)\
  X(xrSyncActions)\
  X(xrApplyHapticFeedback)\
  X(xrStopHapticFeedback)\
  X(xrGetVisibilityMaskKHR)\
  X(xrCreateHandTrackerEXT)\
  X(xrDestroyHandTrackerEXT)\
  X(xrLocateHandJointsEXT)\
  X(xrCreateRenderModelEXT)\
  X(xrDestroyRenderModelEXT)\
  X(xrGetRenderModelPropertiesEXT)\
  X(xrCreateRenderModelSpaceEXT)\
  X(xrCreateRenderModelAssetEXT)\
  X(xrDestroyRenderModelAssetEXT)\
  X(xrGetRenderModelAssetPropertiesEXT)\
  X(xrGetRenderModelAssetDataEXT)\
  X(xrGetRenderModelStateEXT)\
  X(xrEnumerateInteractionRenderModelIdsEXT)\
  X(xrCreateBodyTrackerBD)\
  X(xrDestroyBodyTrackerBD)\
  X(xrLocateBodyJointsBD)\
  X(xrGetHandMeshFB)\
  X(xrGetDisplayRefreshRateFB)\
  X(xrEnumerateDisplayRefreshRatesFB)\
  X(xrRequestDisplayRefreshRateFB)\
  X(xrQuerySystemTrackedKeyboardFB)\
  X(xrCreateKeyboardSpaceFB)\
  X(xrCreateFoveationProfileFB)\
  X(xrDestroyFoveationProfileFB)\
  X(xrCreatePassthroughFB)\
  X(xrDestroyPassthroughFB)\
  X(xrPassthroughStartFB)\
  X(xrPassthroughPauseFB)\
  X(xrCreatePassthroughLayerFB)\
  X(xrDestroyPassthroughLayerFB)\
  X(xrGetPassthroughPreferencesMETA)

#define XR_DECLARE(fn) static PFN_##fn fn;
#define XR_LOAD(fn) xrGetInstanceProcAddr(state.instance, #fn, (PFN_xrVoidFunction*) &fn);
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function);
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateApiLayerProperties(uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrApiLayerProperties* properties);
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char* layerName, uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrExtensionProperties* properties);
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo* createInfo, XrInstance* instance);
XR_FOREACH(XR_DECLARE)
XR_FOREACH_PLATFORM(XR_DECLARE)

enum {
  SWAPCHAIN_COLOR,
  SWAPCHAIN_DEPTH,
  SWAPCHAIN_BACKGROUND
};

enum {
  VIEW = (1 << 0),
  STEREO = (1 << 1),
  DEPTH = (1 << 2),
  CUBE = (1 << 3),
  STATIC = (1 << 4),
  FOVEATED = (1 << 5)
};

typedef struct {
  XrSwapchain handle;
  uint32_t textureIndex;
  uint32_t textureCount;
  Texture* textures[MAX_IMAGES];
  Texture* foveationTextures[MAX_IMAGES];
  bool immutable;
  bool acquired;
} Swapchain;

struct Layer {
  atomic_uint ref;
  LayerInfo info;
  Swapchain swapchain;
  Device origin;
  float curve;
  Pass* pass;
  union {
    XrCompositionLayerBaseHeader header;
    XrCompositionLayerQuad quad;
    XrCompositionLayerCylinderKHR cylinder;
  };
  XrCompositionLayerColorScaleBiasKHR color;
  XrCompositionLayerDepthTestFB depthTest;
  XrCompositionLayerSettingsFB settings;
};

typedef struct {
  XrRenderModelEXT handle;
  XrRenderModelPropertiesEXT properties;
  XrRenderModelNodeStateEXT* nodeStates;
  uint32_t* nodes;
  XrSpace space;
} RenderModel;

typedef struct {
  float poses[MAX_DEVICES][7];
  uint32_t lastButtons[MAX_DEVICES];
  uint32_t buttons[MAX_DEVICES];
  bool initialized;
} Simulator;

enum {
  ACTION_NONE,
  ACTION_GRIP_POSE,
  ACTION_POINTER_POSE,
  ACTION_PINCH_POSE,
  ACTION_POKE_POSE,
  ACTION_PALM_POSE,
  ACTION_TRACKER_POSE,
  ACTION_STYLUS_POSE,
  ACTION_GAZE_POSE,
  ACTION_TRIGGER_DOWN,
  ACTION_TRIGGER_TOUCH,
  ACTION_TRIGGER_AXIS,
  ACTION_TRACKPAD_DOWN,
  ACTION_TRACKPAD_TOUCH,
  ACTION_TRACKPAD_AXIS,
  ACTION_THUMBSTICK_DOWN,
  ACTION_THUMBSTICK_TOUCH,
  ACTION_THUMBSTICK_AXIS,
  ACTION_THUMBREST_TOUCH,
  ACTION_THUMBREST_AXIS,
  ACTION_THUMBTAP_DOWN,
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
  ACTION_DPAD_UP_DOWN,
  ACTION_DPAD_UP_TOUCH,
  ACTION_DPAD_DOWN_DOWN,
  ACTION_DPAD_DOWN_TOUCH,
  ACTION_DPAD_LEFT_DOWN,
  ACTION_DPAD_LEFT_TOUCH,
  ACTION_DPAD_RIGHT_DOWN,
  ACTION_DPAD_RIGHT_TOUCH,
  ACTION_BUMPER_DOWN,
  ACTION_BUMPER_TOUCH,
  ACTION_NIB_DOWN,
  ACTION_NIB_FORCE,
  ACTION_HAND_VIBRATE,
  ACTION_STYLUS_VIBRATE,
  MAX_ACTIONS
};

static atomic_uint ref;

static struct {
  HeadsetConfig config;
  Simulator simulator;
  XrInstance instance;
  XrSystemId system;
  XrViewConfigurationType viewConfiguration;
  uint32_t viewCount;
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
  Swapchain swapchains[3];
  XrCompositionLayerProjection layer;
  XrCompositionLayerProjectionView layerViews[4];
  XrCompositionLayerDepthInfoKHR depthInfo[4];
  XrCompositionLayerPassthroughFB passthroughLayer;
  union {
    XrCompositionLayerBaseHeader header;
    XrCompositionLayerCubeKHR cube;
    XrCompositionLayerEquirectKHR equirect;
    XrCompositionLayerEquirect2KHR equirect2;
  } background;
  Layer* layers[MAX_LAYERS];
  uint32_t layerCount;
  bool showMainLayer;
  Mesh* mask;
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
  XrBodyTrackerBD bodyTracker;
  XrRenderModelIdEXT* modelKeys;
  RenderModel* models;
  uint32_t modelCount;
  mtx_t modelLock;
  FoveationLevel foveationLevel;
  bool foveationDynamic;
  XrPassthroughFB passthrough;
  XrPassthroughLayerFB passthroughLayerHandle;
  bool passthroughActive;
  bool mounted;
  XrDebugUtilsMessengerEXT messenger;
  struct {
    bool battery;
    bool debug;
    bool depth;
    bool foveatedInset;
    bool foveation;
    bool foveationConfig;
    bool foveationVulkan;
    bool frameController;
    bool gaze;
    bool genericController;
    bool handInteraction;
    bool handTracking;
    bool handTrackingDataSource;
    bool handTrackingElbow;
    bool handTrackingMesh;
    bool handTrackingMotionRange;
    bool bodyTracking;
    bool headless;
    bool interactionRenderModel;
    bool keyboardTracking;
    bool layerAutoFilter;
    bool layerColor;
    bool layerCube;
    bool layerCurve;
    bool layerDepthTest;
    bool layerEquirect;
    bool layerEquirect2;
    bool layerSettings;
    bool localFloor;
    bool microgestures;
    bool ml2Controller;
    bool mxInk;
    bool overlay;
    bool palmPose;
    bool passthroughPreferences;
    bool picoController;
    bool presence;
    bool questPassthrough;
    bool renderModel;
    bool swapchainUpdate;
    bool refreshRate;
    bool threadHint;
    bool touchPro;
    bool uuid;
    bool visibilityMask;
    bool viveTrackers;
  } extensions;
} state;

// Helpers

static bool lovrSwapchainInit(Swapchain* swapchain, uint32_t width, uint32_t height, uint32_t flags);
static void lovrSwapchainDestroy(Swapchain* swapchain);
static Texture* lovrSwapchainAcquire(Swapchain* swapchain);
static bool lovrSwapchainRelease(Swapchain* swapchain);

static void disconnect(void);
static void xrthrow(XrResult result, const char* symbol);
static XrBool32 onMessage(XrDebugUtilsMessageSeverityFlagsEXT severity, XrDebugUtilsMessageTypeFlagsEXT type, const XrDebugUtilsMessengerCallbackDataEXT* data, void* userdata);
static bool hasExtension(XrExtensionProperties* extensions, uint32_t count, const char* extension);
static XrTime getCurrentXrTime(void);
static bool createReferenceSpace(XrTime time);
static XrAction getPoseActionForDevice(Device device);
static XrHandTrackerEXT getHandTracker(Device device);
static XrBodyTrackerBD getBodyTracker(void);
static bool loadControllerModels(void);
static bool loadVisibilityMask(void);

// Entry

bool lovrHeadsetInit(HeadsetConfig* config) {
  if (!lovrModuleAcquire(&ref)) {
    lovrFree(config->extensions);
    return true;
  }

  state.config = *config;

  if (!state.simulator.initialized) {
    if (!state.config.seated) {
      state.simulator.poses[DEVICE_HEAD][1] = 1.7f;
    }

    // Normalize simulator quaternions
    for (uint32_t i = 0; i < MAX_DEVICES; i++) {
      quat_identity(state.simulator.poses[i] + 3);
    }

    state.simulator.initialized = true;
  }

  lovrHeadsetSetClipDistance(.01f, 0.f);

  lovrModuleReady(&ref);
  return true;
}

void lovrHeadsetDestroy(void) {
  if (!lovrModuleRelease(&ref)) return;
  disconnect();
  lovrFree(state.config.extensions);
  Simulator simulator = state.simulator; // Keep simulator state between restarts, for convenience
  memset(&state, 0, sizeof(state));
  state.simulator = simulator;
  lovrModuleReset(&ref);
}

bool lovrHeadsetConnect(void) {
  if (state.system) {
    return true;
  }

  HeadsetConfig* config = &state.config;

  XrResult result;

  // Loader

#if defined(__ANDROID__)
  static PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
  XR_LOAD(xrInitializeLoaderKHR);
  lovrAssert(xrInitializeLoaderKHR, "Failed to initialize loader");

  XrLoaderInitInfoAndroidKHR loaderInfo = {
    .type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,
    .applicationVM = os_get_java_vm(),
    .applicationContext = os_get_jni_context()
  };

  if (XR_FAILED(xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*) &loaderInfo))) {
    return true;
  }
#elif defined(__linux__) || defined(__APPLE__)
  if (!config->debug) {
    setenv("XR_LOADER_DEBUG", "none", 0);
  }
#elif defined(_WIN32)
  if (!config->debug && GetEnvironmentVariable("XR_LOADER_DEBUG", NULL, 0) == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
    SetEnvironmentVariable("XR_LOADER_DEBUG", "none");
  }
#endif

  // Extensions

  uint32_t extensionCount = 0;
  XR(xrEnumerateInstanceExtensionProperties(NULL, 0, &extensionCount, NULL), "xrEnumerateInstanceExtensionProperties");

  XrExtensionProperties* extensionProperties = lovrCalloc(extensionCount * sizeof(*extensionProperties));
  for (uint32_t i = 0; i < extensionCount; i++) extensionProperties[i].type = XR_TYPE_EXTENSION_PROPERTIES;
  xrEnumerateInstanceExtensionProperties(NULL, extensionCount, &extensionCount, extensionProperties);

  // Extensions with feature == NULL must be present.  The enable flag can be used to
  // conditionally enable extensions based on config, platform, etc.
  struct { const char* name; bool* feature; bool enable; } extensions[] = {
#ifdef __ANDROID__
    { "XR_KHR_android_create_instance", NULL, true },
    { "XR_KHR_android_thread_settings", &state.extensions.threadHint, true },
#endif
    { "XR_KHR_composition_layer_color_scale_bias", &state.extensions.layerColor, true },
    { "XR_KHR_composition_layer_cylinder", &state.extensions.layerCurve, true },
    { "XR_KHR_composition_layer_cube", &state.extensions.layerCube, true },
    { "XR_KHR_composition_layer_depth", &state.extensions.depth, config->submitDepth },
    { "XR_KHR_composition_layer_equirect", &state.extensions.layerEquirect, true },
    { "XR_KHR_composition_layer_equirect2", &state.extensions.layerEquirect2, true },
#ifndef _WIN32
    { "XR_KHR_convert_timespec_time", NULL, true },
#endif
    { "XR_KHR_generic_controller", &state.extensions.genericController, true },
    { "XR_KHR_visibility_mask", &state.extensions.visibilityMask, config->mask },
#ifdef LOVR_VK
    { "XR_KHR_vulkan_enable2", NULL, true },
#endif
#ifdef _WIN32
    { "XR_KHR_win32_convert_performance_counter_time", NULL, true },
#endif
    { "XR_EXT_debug_utils", &state.extensions.debug, true },
    { "XR_EXT_eye_gaze_interaction", &state.extensions.gaze, true },
    { "XR_EXT_hand_interaction", &state.extensions.handInteraction, true },
    { "XR_EXT_hand_joints_motion_range", &state.extensions.handTrackingMotionRange, true },
    { "XR_EXT_hand_tracking", &state.extensions.handTracking, true },
    { "XR_EXT_hand_tracking_data_source", &state.extensions.handTrackingDataSource, true },
    { "XR_EXT_interaction_profile_battery_state_display", &state.extensions.battery, true },
    { "XR_EXT_interaction_render_model", &state.extensions.interactionRenderModel, true },
    { "XR_EXT_local_floor", &state.extensions.localFloor, true },
    { "XR_EXT_palm_pose", &state.extensions.palmPose, true },
    { "XR_EXT_render_model", &state.extensions.renderModel, true },
    { "XR_EXT_user_presence", &state.extensions.presence, true },
    { "XR_EXT_uuid", &state.extensions.uuid, true },
    { "XR_BD_body_tracking", &state.extensions.bodyTracking, true },
    { "XR_BD_controller_interaction", &state.extensions.picoController, true },
    { "XR_FB_composition_layer_depth_test", &state.extensions.layerDepthTest, true },
    { "XR_FB_composition_layer_settings", &state.extensions.layerSettings, true },
    { "XR_FB_display_refresh_rate", &state.extensions.refreshRate, true },
    { "XR_FB_foveation", &state.extensions.foveation, true },
    { "XR_FB_foveation_configuration", &state.extensions.foveationConfig, true },
    { "XR_FB_foveation_vulkan", &state.extensions.foveationVulkan, true },
    { "XR_FB_hand_tracking_mesh", &state.extensions.handTrackingMesh, true },
    { "XR_FB_keyboard_tracking", &state.extensions.keyboardTracking, true },
    { "XR_FB_passthrough", &state.extensions.questPassthrough, true },
    { "XR_FB_swapchain_update_state", &state.extensions.swapchainUpdate, true },
    { "XR_FB_touch_controller_pro", &state.extensions.touchPro, true },
    { "XR_LOGITECH_mx_ink_stylus_interaction", &state.extensions.mxInk, true },
    { "XR_META_automatic_layer_filter", &state.extensions.layerAutoFilter, true },
    { "XR_META_hand_tracking_microgestures", &state.extensions.microgestures, true },
    { "XR_META_passthrough_preferences", &state.extensions.passthroughPreferences, true },
    { "XR_ML_ml2_controller_interaction", &state.extensions.ml2Controller, true },
    { "XR_MND_headless", &state.extensions.headless, true },
    { "XR_ULTRALEAP_hand_tracking_forearm", &state.extensions.handTrackingElbow, true },
    { "XR_VALVE_frame_controller_interaction", &state.extensions.frameController, true },
    { "XR_VARJO_quad_views", &state.extensions.foveatedInset, true },
    { "XR_EXTX_overlay", &state.extensions.overlay, config->overlay },
    { "XR_HTCX_vive_tracker_interaction", &state.extensions.viveTrackers, true }
  };

  uint32_t enabledExtensionCount = 0;
  const char** enabledExtensionNames = lovrMalloc((COUNTOF(extensions) + state.config.extensionCount) * sizeof(char*));

  for (uint32_t i = 0; i < COUNTOF(extensions); i++) {
    if (!extensions[i].enable) continue;
    if (!extensions[i].feature || hasExtension(extensionProperties, extensionCount, extensions[i].name)) {
      enabledExtensionNames[enabledExtensionCount++] = extensions[i].name;
      if (extensions[i].feature) *extensions[i].feature = true;
    }
  }

  // Custom user-requested extensions
  const char* extension = state.config.extensions;
  for (uint32_t i = 0; i < state.config.extensionCount; i++) {
    if (hasExtension(extensionProperties, extensionCount, extension)) {
      enabledExtensionNames[enabledExtensionCount++] = extension;
    }
    extension += strlen(extension) + 1;
  }

  lovrFree(extensionProperties);

  // Instance

#ifdef __ANDROID__
  XrInstanceCreateInfoAndroidKHR androidInfo = {
    .type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR,
    .applicationVM = os_get_java_vm(),
    .applicationActivity = os_get_jni_context(),
    .next = NULL
  };
#endif

  XrInstanceCreateInfo instanceInfo = {
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

  XR(xrCreateInstance(&instanceInfo, &state.instance), "xrCreateInstance");
  lovrFree(enabledExtensionNames);

  XR_FOREACH(XR_LOAD)
  XR_FOREACH_PLATFORM(XR_LOAD)

  if (state.extensions.debug) {
    XrDebugUtilsMessengerCreateInfoEXT messengerInfo = {
      .type = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverities =
        (config->debug ? XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT : 0) |
        (config->debug ? XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT : 0 ) |
        XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageTypes =
        XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        (config->debug ? XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT : 0) |
        XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT,
      .userCallback = onMessage
    };

    xrCreateDebugUtilsMessengerEXT(state.instance, &messengerInfo, &state.messenger);
  }

  // System

  XrSystemGetInfo systemInfo = {
    .type = XR_TYPE_SYSTEM_GET_INFO,
    .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY
  };

  XRG(xrGetSystem(state.instance, &systemInfo, &state.system), "xrGetSystem", fail);

  XrSystemEyeGazeInteractionPropertiesEXT eyeGazeProperties = { .type = XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT };
  XrSystemHandTrackingPropertiesEXT handTrackingProperties = { .type = XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT };
  XrSystemBodyTrackingPropertiesBD bodyTrackingProperties = { .type = XR_TYPE_SYSTEM_BODY_TRACKING_PROPERTIES_BD };
  XrSystemKeyboardTrackingPropertiesFB keyboardTrackingProperties = { .type = XR_TYPE_SYSTEM_KEYBOARD_TRACKING_PROPERTIES_FB };
  XrSystemUserPresencePropertiesEXT presenceProperties = { .type = XR_TYPE_SYSTEM_USER_PRESENCE_PROPERTIES_EXT };
  XrSystemPassthroughProperties2FB passthroughProperties = { .type = XR_TYPE_SYSTEM_PASSTHROUGH_PROPERTIES2_FB };
  XrSystemProperties properties = { .type = XR_TYPE_SYSTEM_PROPERTIES };

  if (state.extensions.gaze) {
    eyeGazeProperties.next = properties.next;
    properties.next = &eyeGazeProperties;
  }

  if (state.extensions.handTracking) {
    handTrackingProperties.next = properties.next;
    properties.next = &handTrackingProperties;
  }

  if (state.extensions.bodyTracking) {
    bodyTrackingProperties.next = properties.next;
    properties.next = &bodyTrackingProperties;
  }

  if (state.extensions.keyboardTracking) {
    keyboardTrackingProperties.next = properties.next;
    properties.next = &keyboardTrackingProperties;
  }

  if (state.extensions.presence) {
    presenceProperties.next = properties.next;
    properties.next = &presenceProperties;
  }

  if (state.extensions.questPassthrough) {
    passthroughProperties.next = properties.next;
    properties.next = &passthroughProperties;
  }

  XRG(xrGetSystemProperties(state.instance, state.system, &properties), "xrGetSystemProperties", fail);
  state.extensions.gaze = eyeGazeProperties.supportsEyeGazeInteraction;
  state.extensions.handTracking = handTrackingProperties.supportsHandTracking;
  state.extensions.bodyTracking = bodyTrackingProperties.supportsBodyTracking;
  state.extensions.keyboardTracking = keyboardTrackingProperties.supportsKeyboardTracking;
  state.extensions.presence = presenceProperties.supportsUserPresence;
  state.extensions.questPassthrough = passthroughProperties.capabilities & XR_PASSTHROUGH_CAPABILITY_BIT_FB;

  // View Configuration

  XrViewConfigurationType supportedViewConfigurations[] = {
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET,
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO
  };

  uint32_t viewConfigurationCount;
  XrViewConfigurationType viewConfigurations[4];
  XRG(xrEnumerateViewConfigurations(state.instance, state.system, 4, &viewConfigurationCount, viewConfigurations), "xrEnumerateViewConfigurations", fail);

  for (uint32_t i = 0; !state.viewConfiguration && i < COUNTOF(supportedViewConfigurations); i++) {
    for (uint32_t j = 0; j < viewConfigurationCount; j++) {
      if (viewConfigurations[j] == supportedViewConfigurations[i]) {
        state.viewConfiguration = supportedViewConfigurations[i];
        break;
      }
    }
  }

  lovrAssertGoto(fail, state.viewConfiguration, "No supported view configuration available");

  XrViewConfigurationView views[4] = {
    [0].type = XR_TYPE_VIEW_CONFIGURATION_VIEW,
    [1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW,
    [2].type = XR_TYPE_VIEW_CONFIGURATION_VIEW,
    [3].type = XR_TYPE_VIEW_CONFIGURATION_VIEW
  };

  XRG(xrEnumerateViewConfigurationViews(state.instance, state.system, state.viewConfiguration, 0, &state.viewCount, NULL), "xrEnumerateViewConfigurationViews", fail);
  XRG(xrEnumerateViewConfigurationViews(state.instance, state.system, state.viewConfiguration, COUNTOF(views), &state.viewCount, views), "xrEnumerateViewConfigurationViews", fail);

  uint32_t maxWidth = ~0u;
  uint32_t maxHeight = ~0u;
  uint32_t recommendedWidth = 0;
  uint32_t recommendedHeight = 0;

  for (uint32_t i = 0; i < state.viewCount; i++) {
    maxWidth = MIN(maxWidth, views[i].maxImageRectWidth);
    maxHeight = MIN(maxHeight, views[i].maxImageRectHeight);
    recommendedWidth = MAX(recommendedWidth, views[i].recommendedImageRectWidth);
    recommendedHeight = MAX(recommendedHeight, views[i].recommendedImageRectHeight);
  }

  state.width = MIN(recommendedWidth * config->supersample, maxWidth);
  state.height = MIN(recommendedHeight * config->supersample, maxHeight);

  // Blend Modes

  XRG(xrEnumerateEnvironmentBlendModes(state.instance, state.system, state.viewConfiguration, 0, &state.blendModeCount, NULL), "xrEnumerateEnvironmentBlendModes", fail);
  state.blendModes = lovrMalloc(state.blendModeCount * sizeof(XrEnvironmentBlendMode));
  XRG(xrEnumerateEnvironmentBlendModes(state.instance, state.system, state.viewConfiguration, state.blendModeCount, &state.blendModeCount, state.blendModes), "xrEnumerateEnvironmentBlendModes", fail);
  state.blendMode = state.blendModes[0];

  // Actions

  XrActionSetCreateInfo actionSetInfo = {
    .type = XR_TYPE_ACTION_SET_CREATE_INFO,
    .localizedActionSetName = "Default",
    .actionSetName = "default"
  };

  XRG(xrCreateActionSet(state.instance, &actionSetInfo, &state.actionSet), "xrCreateActionSet", fail);

  // Subaction paths, for filtering actions by device
  XRG(xrStringToPath(state.instance, "/user/hand/left", &state.actionFilters[DEVICE_HAND_LEFT]), "xrStringToPath", fail);
  XRG(xrStringToPath(state.instance, "/user/hand/right", &state.actionFilters[DEVICE_HAND_RIGHT]), "xrStringToPath", fail);

  state.actionFilters[DEVICE_HAND_LEFT_GRIP] = state.actionFilters[DEVICE_HAND_LEFT];
  state.actionFilters[DEVICE_HAND_LEFT_POINT] = state.actionFilters[DEVICE_HAND_LEFT];
  state.actionFilters[DEVICE_HAND_LEFT_PINCH] = state.actionFilters[DEVICE_HAND_LEFT];
  state.actionFilters[DEVICE_HAND_LEFT_POKE] = state.actionFilters[DEVICE_HAND_LEFT];
  state.actionFilters[DEVICE_HAND_LEFT_PALM] = state.actionFilters[DEVICE_HAND_LEFT];
  state.actionFilters[DEVICE_HAND_RIGHT_GRIP] = state.actionFilters[DEVICE_HAND_RIGHT];
  state.actionFilters[DEVICE_HAND_RIGHT_POINT] = state.actionFilters[DEVICE_HAND_RIGHT];
  state.actionFilters[DEVICE_HAND_RIGHT_PINCH] = state.actionFilters[DEVICE_HAND_RIGHT];
  state.actionFilters[DEVICE_HAND_RIGHT_POKE] = state.actionFilters[DEVICE_HAND_RIGHT];
  state.actionFilters[DEVICE_HAND_RIGHT_PALM] = state.actionFilters[DEVICE_HAND_RIGHT];

  if (state.extensions.viveTrackers) {
    XRG(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/left_elbow", &state.actionFilters[DEVICE_ELBOW_LEFT]), "xrStringToPath", fail);
    XRG(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/right_elbow", &state.actionFilters[DEVICE_ELBOW_RIGHT]), "xrStringToPath", fail);
    XRG(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/left_shoulder", &state.actionFilters[DEVICE_SHOULDER_LEFT]), "xrStringToPath", fail);
    XRG(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/right_shoulder", &state.actionFilters[DEVICE_SHOULDER_RIGHT]), "xrStringToPath", fail);
    XRG(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/chest", &state.actionFilters[DEVICE_CHEST]), "xrStringToPath", fail);
    XRG(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/waist", &state.actionFilters[DEVICE_WAIST]), "xrStringToPath", fail);
    XRG(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/left_knee", &state.actionFilters[DEVICE_KNEE_LEFT]), "xrStringToPath", fail);
    XRG(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/right_knee", &state.actionFilters[DEVICE_KNEE_RIGHT]), "xrStringToPath", fail);
    XRG(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/left_foot", &state.actionFilters[DEVICE_FOOT_LEFT]), "xrStringToPath", fail);
    XRG(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/right_foot", &state.actionFilters[DEVICE_FOOT_RIGHT]), "xrStringToPath", fail);
    XRG(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/camera", &state.actionFilters[DEVICE_CAMERA]), "xrStringToPath", fail);
    XRG(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/keyboard", &state.actionFilters[DEVICE_KEYBOARD]), "xrStringToPath", fail);
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
    { 0 },
    { 0, NULL, "grip_pose",        XR_ACTION_TYPE_POSE_INPUT,       2, hands, "Grip Pose" },
    { 0, NULL, "pointer_pose",     XR_ACTION_TYPE_POSE_INPUT,       2, hands, "Pointer Pose" },
    { 0, NULL, "pinch_pose",       XR_ACTION_TYPE_POSE_INPUT,       2, hands, "Pinch Pose" },
    { 0, NULL, "poke_pose",        XR_ACTION_TYPE_POSE_INPUT,       2, hands, "Poke Pose" },
    { 0, NULL, "palm_pose",        XR_ACTION_TYPE_POSE_INPUT,       2, hands, "Palm Pose" },
    { 0, NULL, "tracker_pose",     XR_ACTION_TYPE_POSE_INPUT,       12, trackers, "Tracker Pose" },
    { 0, NULL, "stylus_pose",      XR_ACTION_TYPE_POSE_INPUT,       0, NULL, "Stylus Pose" },
    { 0, NULL, "gaze_pose",        XR_ACTION_TYPE_POSE_INPUT,       0, NULL, "Gaze Pose" },
    { 0, NULL, "trigger_down",     XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Trigger Down" },
    { 0, NULL, "trigger_touch",    XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Trigger Touch" },
    { 0, NULL, "trigger_axis" ,    XR_ACTION_TYPE_FLOAT_INPUT,      2, hands, "Trigger Axis" },
    { 0, NULL, "trackpad_down" ,   XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Trackpad Down" },
    { 0, NULL, "trackpad_touch",   XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Trackpad Touch" },
    { 0, NULL, "trackpad_axis",    XR_ACTION_TYPE_VECTOR2F_INPUT,   2, hands, "Trackpad Axis" },
    { 0, NULL, "thumbstick_down",  XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Thumbstick Down" },
    { 0, NULL, "thumbstick_touch", XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Thumbstick Touch" },
    { 0, NULL, "thumbstick_axis" , XR_ACTION_TYPE_VECTOR2F_INPUT,   2, hands, "Thumbstick Axis" },
    { 0, NULL, "thumbrest_touch",  XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Thumbrest Touch" },
    { 0, NULL, "thumbrest_axis",   XR_ACTION_TYPE_FLOAT_INPUT,      2, hands, "Thumbtap Down" },
    { 0, NULL, "thumbtap_down",    XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Thumbrest Axis" },
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
    { 0, NULL, "dpad_up_down",     XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "DPad Up Down" },
    { 0, NULL, "dpad_up_touch",    XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "DPad Up Touch" },
    { 0, NULL, "dpad_down_down",   XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "DPad Down Down" },
    { 0, NULL, "dpad_down_touch",  XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "DPad Down Touch" },
    { 0, NULL, "dpad_left_down",   XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "DPad Left Down" },
    { 0, NULL, "dpad_left_touch",  XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "DPad Left Touch" },
    { 0, NULL, "dpad_right_down",  XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "DPad Right Down" },
    { 0, NULL, "dpad_right_touch", XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "DPad Right Touch" },
    { 0, NULL, "bumper_down",      XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Bumper Down" },
    { 0, NULL, "bumper_touch",     XR_ACTION_TYPE_BOOLEAN_INPUT,    2, hands, "Bumper Touch" },
    { 0, NULL, "nib_down",         XR_ACTION_TYPE_BOOLEAN_INPUT,    0, NULL, "Nib Down" },
    { 0, NULL, "nib_force",        XR_ACTION_TYPE_FLOAT_INPUT,      0, NULL, "Nib Force" },
    { 0, NULL, "vibrate",          XR_ACTION_TYPE_VIBRATION_OUTPUT, 2, hands, "Vibrate" },
    { 0, NULL, "stylus_vibrate",   XR_ACTION_TYPE_VIBRATION_OUTPUT, 0, NULL, "Stylus Vibrate" }
  };

  static_assert(COUNTOF(actionInfo) == MAX_ACTIONS, "Unbalanced action table!");

  if (!state.extensions.viveTrackers) {
    actionInfo[ACTION_TRACKER_POSE].countSubactionPaths = 0;
  }

  if (!state.extensions.gaze) {
    actionInfo[ACTION_GAZE_POSE].countSubactionPaths = 0;
  }

  for (uint32_t i = 0; i < MAX_ACTIONS; i++) {
    if (i == ACTION_NONE) continue;
    actionInfo[i].type = XR_TYPE_ACTION_CREATE_INFO;
    XRG(xrCreateAction(state.actionSet, &actionInfo[i], &state.actions[i]), "xrCreateAction", fail);
  }

  enum {
    PROFILE_SIMPLE,
    PROFILE_GENERIC,
    PROFILE_VIVE,
    PROFILE_TOUCH,
    PROFILE_TOUCH_PRO,
    PROFILE_GO,
    PROFILE_INDEX,
    PROFILE_WMR,
    PROFILE_ML2,
    PROFILE_PICO_NEO3,
    PROFILE_PICO4,
    PROFILE_TRACKER,
    PROFILE_MX_INK,
    PROFILE_GAZE,
    PROFILE_HAND,
    PROFILE_FRAME,
    MAX_PROFILES
  };

  const char* interactionProfilePaths[] = {
    [PROFILE_SIMPLE] = "/interaction_profiles/khr/simple_controller",
    [PROFILE_GENERIC] = "/interaction_profiles/khr/generic_controller",
    [PROFILE_VIVE] = "/interaction_profiles/htc/vive_controller",
    [PROFILE_TOUCH] = "/interaction_profiles/oculus/touch_controller",
    [PROFILE_TOUCH_PRO] = "/interaction_profiles/facebook/touch_controller_pro",
    [PROFILE_GO] = "/interaction_profiles/oculus/go_controller",
    [PROFILE_INDEX] = "/interaction_profiles/valve/index_controller",
    [PROFILE_WMR] = "/interaction_profiles/microsoft/motion_controller",
    [PROFILE_ML2] = "/interaction_profiles/ml/ml2_controller",
    [PROFILE_PICO_NEO3] = "/interaction_profiles/bytedance/pico_neo3_controller",
    [PROFILE_PICO4] = "/interaction_profiles/bytedance/pico4_controller",
    [PROFILE_TRACKER] = "/interaction_profiles/htc/vive_tracker_htcx",
    [PROFILE_MX_INK] = "/interaction_profiles/logitech/mx_ink_stylus_logitech",
    [PROFILE_GAZE] = "/interaction_profiles/ext/eye_gaze_interaction",
    [PROFILE_HAND] = "/interaction_profiles/ext/hand_interaction_ext",
    [PROFILE_FRAME] = "/interaction_profiles/valve/frame_controller"
  };

  typedef struct {
    int action;
    const char* path;
  } Binding;

  Binding* bindings[] = {
    [PROFILE_SIMPLE] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
      { ACTION_TRIGGER_DOWN, "/user/hand/left/input/select/click" },
      { ACTION_TRIGGER_DOWN, "/user/hand/right/input/select/click" },
      { ACTION_MENU_DOWN, "/user/hand/left/input/menu/click" },
      { ACTION_MENU_DOWN, "/user/hand/right/input/menu/click" },
      { ACTION_HAND_VIBRATE, "/user/hand/left/output/haptic" },
      { ACTION_HAND_VIBRATE, "/user/hand/right/output/haptic" },
      { 0, NULL }
    },
    [PROFILE_GENERIC] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
      { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/value" },
      { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/value" },
      { ACTION_TRIGGER_AXIS, "/user/hand/left/input/trigger/value" },
      { ACTION_TRIGGER_AXIS, "/user/hand/right/input/trigger/value" },
      { ACTION_THUMBSTICK_DOWN, "/user/hand/left/input/thumbstick/click" },
      { ACTION_THUMBSTICK_DOWN, "/user/hand/right/input/thumbstick/click" },
      { ACTION_THUMBSTICK_AXIS, "/user/hand/left/input/thumbstick" },
      { ACTION_THUMBSTICK_AXIS, "/user/hand/right/input/thumbstick" },
      { ACTION_GRIP_DOWN, "/user/hand/left/input/squeeze/value" },
      { ACTION_GRIP_DOWN, "/user/hand/right/input/squeeze/value" },
      { ACTION_GRIP_AXIS, "/user/hand/left/input/squeeze/value" },
      { ACTION_GRIP_AXIS, "/user/hand/right/input/squeeze/value" },
      { ACTION_A_DOWN, "/user/hand/left/input/primary/click" },
      { ACTION_A_DOWN, "/user/hand/right/input/primary/click" },
      { ACTION_B_DOWN, "/user/hand/left/input/secondary/click" },
      { ACTION_B_DOWN, "/user/hand/right/input/secondary/click" },
      { ACTION_HAND_VIBRATE, "/user/hand/left/output/haptic" },
      { ACTION_HAND_VIBRATE, "/user/hand/right/output/haptic" },
      { 0, NULL }
    },
    [PROFILE_VIVE] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
      { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/click" },
      { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/click" },
      { ACTION_TRIGGER_AXIS, "/user/hand/left/input/trigger/value" },
      { ACTION_TRIGGER_AXIS, "/user/hand/right/input/trigger/value" },
      { ACTION_TRACKPAD_DOWN, "/user/hand/left/input/trackpad/click" },
      { ACTION_TRACKPAD_DOWN, "/user/hand/right/input/trackpad/click" },
      { ACTION_TRACKPAD_TOUCH, "/user/hand/left/input/trackpad/touch" },
      { ACTION_TRACKPAD_TOUCH, "/user/hand/right/input/trackpad/touch" },
      { ACTION_TRACKPAD_AXIS, "/user/hand/left/input/trackpad" },
      { ACTION_TRACKPAD_AXIS, "/user/hand/right/input/trackpad" },
      { ACTION_MENU_DOWN, "/user/hand/left/input/menu/click" },
      { ACTION_MENU_DOWN, "/user/hand/right/input/menu/click" },
      { ACTION_GRIP_DOWN, "/user/hand/left/input/squeeze/click" },
      { ACTION_GRIP_DOWN, "/user/hand/right/input/squeeze/click" },
      { ACTION_HAND_VIBRATE, "/user/hand/left/output/haptic" },
      { ACTION_HAND_VIBRATE, "/user/hand/right/output/haptic" },
      { 0, NULL }
    },
    [PROFILE_TOUCH] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
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
      { ACTION_THUMBSTICK_AXIS, "/user/hand/left/input/thumbstick" },
      { ACTION_THUMBSTICK_AXIS, "/user/hand/right/input/thumbstick" },
      { ACTION_THUMBREST_TOUCH, "/user/hand/left/input/thumbrest/touch" },
      { ACTION_THUMBREST_TOUCH, "/user/hand/right/input/thumbrest/touch" },
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
      { ACTION_HAND_VIBRATE, "/user/hand/left/output/haptic" },
      { ACTION_HAND_VIBRATE, "/user/hand/right/output/haptic" },
      { 0, NULL }
    },
    [PROFILE_TOUCH_PRO] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
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
      { ACTION_THUMBSTICK_AXIS, "/user/hand/left/input/thumbstick" },
      { ACTION_THUMBSTICK_AXIS, "/user/hand/right/input/thumbstick" },
      { ACTION_THUMBREST_TOUCH, "/user/hand/left/input/thumbrest/touch" },
      { ACTION_THUMBREST_TOUCH, "/user/hand/right/input/thumbrest/touch" },
      { ACTION_THUMBREST_AXIS, "/user/hand/left/input/thumbrest/force" },
      { ACTION_THUMBREST_AXIS, "/user/hand/right/input/thumbrest/force" },
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
      { ACTION_NIB_DOWN, "/user/hand/left/input/stylus_fb/force" },
      { ACTION_NIB_DOWN, "/user/hand/right/input/stylus_fb/force" },
      { ACTION_NIB_FORCE, "/user/hand/left/input/stylus_fb/force" },
      { ACTION_NIB_FORCE, "/user/hand/right/input/stylus_fb/force" },
      { ACTION_HAND_VIBRATE, "/user/hand/left/output/haptic" },
      { ACTION_HAND_VIBRATE, "/user/hand/right/output/haptic" },
      { 0, NULL }
    },
    [PROFILE_GO] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
      { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/click" },
      { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/click" },
      { ACTION_TRACKPAD_DOWN, "/user/hand/left/input/trackpad/click" },
      { ACTION_TRACKPAD_DOWN, "/user/hand/right/input/trackpad/click" },
      { ACTION_TRACKPAD_TOUCH, "/user/hand/left/input/trackpad/touch" },
      { ACTION_TRACKPAD_TOUCH, "/user/hand/right/input/trackpad/touch" },
      { ACTION_TRACKPAD_AXIS, "/user/hand/left/input/trackpad" },
      { ACTION_TRACKPAD_AXIS, "/user/hand/right/input/trackpad" },
      { 0, NULL }
    },
    [PROFILE_INDEX] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
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
      { ACTION_TRACKPAD_AXIS, "/user/hand/left/input/trackpad" },
      { ACTION_TRACKPAD_AXIS, "/user/hand/right/input/trackpad" },
      { ACTION_THUMBSTICK_DOWN, "/user/hand/left/input/thumbstick/click" },
      { ACTION_THUMBSTICK_DOWN, "/user/hand/right/input/thumbstick/click" },
      { ACTION_THUMBSTICK_TOUCH, "/user/hand/left/input/thumbstick/touch" },
      { ACTION_THUMBSTICK_TOUCH, "/user/hand/right/input/thumbstick/touch" },
      { ACTION_THUMBSTICK_AXIS, "/user/hand/left/input/thumbstick" },
      { ACTION_THUMBSTICK_AXIS, "/user/hand/right/input/thumbstick" },
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
      { ACTION_HAND_VIBRATE, "/user/hand/left/output/haptic" },
      { ACTION_HAND_VIBRATE, "/user/hand/right/output/haptic" },
      { 0, NULL }
    },
    [PROFILE_WMR] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
      { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/value" },
      { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/value" },
      { ACTION_TRIGGER_AXIS, "/user/hand/left/input/trigger/value" },
      { ACTION_TRIGGER_AXIS, "/user/hand/right/input/trigger/value" },
      { ACTION_TRACKPAD_DOWN, "/user/hand/left/input/trackpad/click" },
      { ACTION_TRACKPAD_DOWN, "/user/hand/right/input/trackpad/click" },
      { ACTION_TRACKPAD_TOUCH, "/user/hand/left/input/trackpad/touch" },
      { ACTION_TRACKPAD_TOUCH, "/user/hand/right/input/trackpad/touch" },
      { ACTION_TRACKPAD_AXIS, "/user/hand/left/input/trackpad" },
      { ACTION_TRACKPAD_AXIS, "/user/hand/right/input/trackpad" },
      { ACTION_THUMBSTICK_DOWN, "/user/hand/left/input/thumbstick/click" },
      { ACTION_THUMBSTICK_DOWN, "/user/hand/right/input/thumbstick/click" },
      { ACTION_THUMBSTICK_AXIS, "/user/hand/left/input/thumbstick" },
      { ACTION_THUMBSTICK_AXIS, "/user/hand/right/input/thumbstick" },
      { ACTION_MENU_DOWN, "/user/hand/left/input/menu/click" },
      { ACTION_MENU_DOWN, "/user/hand/right/input/menu/click" },
      { ACTION_GRIP_DOWN, "/user/hand/left/input/squeeze/click" },
      { ACTION_GRIP_DOWN, "/user/hand/right/input/squeeze/click" },
      { ACTION_GRIP_AXIS, "/user/hand/left/input/squeeze/click" },
      { ACTION_GRIP_AXIS, "/user/hand/right/input/squeeze/click" },
      { ACTION_HAND_VIBRATE, "/user/hand/left/output/haptic" },
      { ACTION_HAND_VIBRATE, "/user/hand/right/output/haptic" },
      { 0, NULL }
    },
    [PROFILE_ML2] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
      { ACTION_TRIGGER_DOWN, "/user/hand/left/input/trigger/click" },
      { ACTION_TRIGGER_DOWN, "/user/hand/right/input/trigger/click" },
      { ACTION_TRIGGER_AXIS, "/user/hand/left/input/trigger/value" },
      { ACTION_TRIGGER_AXIS, "/user/hand/right/input/trigger/value" },
      { ACTION_TRACKPAD_DOWN, "/user/hand/left/input/trackpad/click" },
      { ACTION_TRACKPAD_DOWN, "/user/hand/right/input/trackpad/click" },
      { ACTION_TRACKPAD_TOUCH, "/user/hand/left/input/trackpad/touch" },
      { ACTION_TRACKPAD_TOUCH, "/user/hand/right/input/trackpad/touch" },
      { ACTION_TRACKPAD_AXIS, "/user/hand/left/input/trackpad" },
      { ACTION_TRACKPAD_AXIS, "/user/hand/right/input/trackpad" },
      { ACTION_MENU_DOWN, "/user/hand/left/input/menu/click" },
      { ACTION_MENU_DOWN, "/user/hand/right/input/menu/click" },
      { ACTION_GRIP_DOWN, "/user/hand/left/input/shoulder/click" },
      { ACTION_GRIP_DOWN, "/user/hand/right/input/shoulder/click" },
      { ACTION_HAND_VIBRATE, "/user/hand/left/output/haptic" },
      { ACTION_HAND_VIBRATE, "/user/hand/right/output/haptic" },
      { 0, NULL }
    },
    [PROFILE_PICO_NEO3] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
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
      { ACTION_THUMBSTICK_AXIS, "/user/hand/left/input/thumbstick" },
      { ACTION_THUMBSTICK_AXIS, "/user/hand/right/input/thumbstick" },
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
      { ACTION_HAND_VIBRATE, "/user/hand/left/output/haptic" },
      { ACTION_HAND_VIBRATE, "/user/hand/right/output/haptic" },
      { 0, NULL }
    },
    [PROFILE_PICO4] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
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
      { ACTION_THUMBSTICK_AXIS, "/user/hand/left/input/thumbstick" },
      { ACTION_THUMBSTICK_AXIS, "/user/hand/right/input/thumbstick" },
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
      { ACTION_HAND_VIBRATE, "/user/hand/left/output/haptic" },
      { ACTION_HAND_VIBRATE, "/user/hand/right/output/haptic" },
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
    [PROFILE_MX_INK] = (Binding[]) {
      { ACTION_STYLUS_POSE, "/user/hand/left/input/tip_logitech/pose" },
      { ACTION_STYLUS_POSE, "/user/hand/right/input/tip_logitech/pose" },
      { ACTION_GRIP_DOWN, "/user/hand/left/input/cluster_middle_logitech/force" },
      { ACTION_GRIP_DOWN, "/user/hand/right/input/cluster_middle_logitech/force" },
      { ACTION_GRIP_AXIS, "/user/hand/left/input/cluster_middle_logitech/force" },
      { ACTION_GRIP_AXIS, "/user/hand/right/input/cluster_middle_logitech/force" },
      { ACTION_A_DOWN, "/user/hand/left/input/cluster_front_logitech/click" },
      { ACTION_A_DOWN, "/user/hand/right/input/cluster_front_logitech/click" },
      { ACTION_B_DOWN, "/user/hand/left/input/cluster_back_logitech/click" },
      { ACTION_B_DOWN, "/user/hand/right/input/cluster_back_logitech/click" },
      { ACTION_NIB_DOWN, "/user/hand/left/input/tip_logitech/force" },
      { ACTION_NIB_DOWN, "/user/hand/right/input/tip_logitech/force" },
      { ACTION_NIB_FORCE, "/user/hand/left/input/tip_logitech/force" },
      { ACTION_NIB_FORCE, "/user/hand/right/input/tip_logitech/force" },
      { ACTION_STYLUS_VIBRATE, "/user/hand/left/output/haptic" },
      { ACTION_STYLUS_VIBRATE, "/user/hand/right/output/haptic" },
      { 0, NULL }
    },
    [PROFILE_GAZE] = (Binding[]) {
      { ACTION_GAZE_POSE, "/user/eyes_ext/input/gaze_ext/pose" },
      { 0, NULL }
    },
    [PROFILE_HAND] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
      { ACTION_TRIGGER_DOWN, "/user/hand/left/input/pinch_ext/value" },
      { ACTION_TRIGGER_DOWN, "/user/hand/right/input/pinch_ext/value" },
      { ACTION_TRIGGER_AXIS, "/user/hand/left/input/pinch_ext/value" },
      { ACTION_TRIGGER_AXIS, "/user/hand/right/input/pinch_ext/value" },
      { ACTION_GRIP_DOWN, "/user/hand/left/input/grasp_ext/value" },
      { ACTION_GRIP_DOWN, "/user/hand/right/input/grasp_ext/value" },
      { ACTION_GRIP_AXIS, "/user/hand/left/input/grasp_ext/value" },
      { ACTION_GRIP_AXIS, "/user/hand/right/input/grasp_ext/value" },
      { ACTION_DPAD_UP_DOWN, "/user/hand/left/input/swipe_forward_meta/click" },
      { ACTION_DPAD_UP_DOWN, "/user/hand/right/input/swipe_forward_meta/click" },
      { ACTION_DPAD_DOWN_DOWN, "/user/hand/left/input/swipe_backward_meta/click" },
      { ACTION_DPAD_DOWN_DOWN, "/user/hand/right/input/swipe_backward_meta/click" },
      { ACTION_DPAD_LEFT_DOWN, "/user/hand/left/input/swipe_left_meta/click" },
      { ACTION_DPAD_LEFT_DOWN, "/user/hand/right/input/swipe_left_meta/click" },
      { ACTION_DPAD_RIGHT_DOWN, "/user/hand/left/input/swipe_right_meta/click" },
      { ACTION_DPAD_RIGHT_DOWN, "/user/hand/right/input/swipe_right_meta/click" },
      { ACTION_THUMBTAP_DOWN, "/user/hand/left/input/tap_thumb_meta/click" },
      { ACTION_THUMBTAP_DOWN, "/user/hand/right/input/tap_thumb_meta/click" },
      { 0, NULL }
    },
    [PROFILE_FRAME] = (Binding[]) {
      { ACTION_GRIP_POSE, "/user/hand/left/input/grip/pose" },
      { ACTION_GRIP_POSE, "/user/hand/right/input/grip/pose" },
      { ACTION_POINTER_POSE, "/user/hand/left/input/aim/pose" },
      { ACTION_POINTER_POSE, "/user/hand/right/input/aim/pose" },
      { ACTION_PINCH_POSE, "/user/hand/left/input/pinch_ext/pose" },
      { ACTION_PINCH_POSE, "/user/hand/right/input/pinch_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/left/input/poke_ext/pose" },
      { ACTION_POKE_POSE, "/user/hand/right/input/poke_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/left/input/palm_ext/pose" },
      { ACTION_PALM_POSE, "/user/hand/right/input/palm_ext/pose" },
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
      { ACTION_THUMBSTICK_AXIS, "/user/hand/left/input/thumbstick" },
      { ACTION_THUMBSTICK_AXIS, "/user/hand/right/input/thumbstick" },
      { ACTION_MENU_DOWN, "/user/hand/left/input/view/click" },
      { ACTION_MENU_DOWN, "/user/hand/right/input/menu/click" },
      { ACTION_MENU_TOUCH, "/user/hand/left/input/view/touch" },
      { ACTION_MENU_TOUCH, "/user/hand/right/input/menu/touch" },
      { ACTION_GRIP_DOWN, "/user/hand/left/input/squeeze/click" },
      { ACTION_GRIP_DOWN, "/user/hand/right/input/squeeze/click" },
      { ACTION_GRIP_TOUCH, "/user/hand/left/input/squeeze/touch" },
      { ACTION_GRIP_TOUCH, "/user/hand/right/input/squeeze/touch" },
      { ACTION_GRIP_AXIS, "/user/hand/left/input/squeeze/value" },
      { ACTION_GRIP_AXIS, "/user/hand/right/input/squeeze/value" },
      { ACTION_A_DOWN, "/user/hand/right/input/a/click" },
      { ACTION_A_TOUCH, "/user/hand/right/input/a/touch" },
      { ACTION_B_DOWN, "/user/hand/right/input/b/click" },
      { ACTION_B_TOUCH, "/user/hand/right/input/b/touch" },
      { ACTION_X_DOWN, "/user/hand/right/input/x/click" },
      { ACTION_X_TOUCH, "/user/hand/right/input/x/touch" },
      { ACTION_Y_DOWN, "/user/hand/right/input/y/click" },
      { ACTION_Y_TOUCH, "/user/hand/right/input/y/touch" },
      { ACTION_DPAD_UP_DOWN, "/user/hand/left/input/dpad_up/click" },
      { ACTION_DPAD_UP_TOUCH, "/user/hand/left/input/dpad_up/touch" },
      { ACTION_DPAD_DOWN_DOWN, "/user/hand/left/input/dpad_down/click" },
      { ACTION_DPAD_DOWN_TOUCH, "/user/hand/left/input/dpad_down/touch" },
      { ACTION_DPAD_LEFT_DOWN, "/user/hand/left/input/dpad_left/click" },
      { ACTION_DPAD_LEFT_TOUCH, "/user/hand/left/input/dpad_left/touch" },
      { ACTION_DPAD_RIGHT_DOWN, "/user/hand/left/input/dpad_right/click" },
      { ACTION_DPAD_RIGHT_TOUCH, "/user/hand/left/input/dpad_right/touch" },
      { ACTION_BUMPER_DOWN, "/user/hand/left/input/bumper/click" },
      { ACTION_BUMPER_DOWN, "/user/hand/right/input/bumper/click" },
      { ACTION_BUMPER_TOUCH, "/user/hand/left/input/bumper/touch" },
      { ACTION_BUMPER_TOUCH, "/user/hand/right/input/bumper/touch" },
      { ACTION_HAND_VIBRATE, "/user/hand/left/output/haptic" },
      { ACTION_HAND_VIBRATE, "/user/hand/right/output/haptic" },
      { 0, NULL }
    }
  };

  uint32_t bindingCount[MAX_PROFILES] = { 0 };

  for (uint32_t i = 0; i < MAX_PROFILES; i++) {
    for (uint32_t j = 0; bindings[i][j].path; j++) {
      bindingCount[i]++;
    }
  }

  // Don't suggest bindings for unsupported input profiles

  if (!state.extensions.genericController) {
    bindingCount[PROFILE_GENERIC] = 0;
  }

  if (!state.extensions.ml2Controller) {
    bindingCount[PROFILE_ML2] = 0;
  }

  if (!state.extensions.picoController) {
    bindingCount[PROFILE_PICO_NEO3] = 0;
    bindingCount[PROFILE_PICO4] = 0;
  }

  if (!state.extensions.viveTrackers) {
    bindingCount[PROFILE_TRACKER] = 0;
  }

  if (!state.extensions.mxInk) {
    bindingCount[PROFILE_MX_INK] = 0;
  }

  if (!state.extensions.gaze) {
    bindingCount[PROFILE_GAZE] = 0;
  }

  if (!state.extensions.handInteraction) {
    bindingCount[PROFILE_HAND] = 0;
  }

  if (!state.extensions.touchPro) {
    bindingCount[PROFILE_TOUCH_PRO] = 0;
  }

  if (!state.extensions.frameController) {
    bindingCount[PROFILE_FRAME] = 0;
  }

  // Remove bindings for unsupported extensions

  #define REMOVE_BINDINGS(bindings, length, index, count)\
    if (index < length - count) memmove(&bindings[index], &bindings[index + count], (length - index - count) * sizeof(Binding));

  if (!state.extensions.handInteraction) {
    for (uint32_t i = 0; i < MAX_PROFILES; i++) {
      for (uint32_t j = 0; j < bindingCount[i]; j++) {
        if (bindings[i][j].action == ACTION_PINCH_POSE || bindings[i][j].action == ACTION_POKE_POSE) {
          REMOVE_BINDINGS(bindings[i], bindingCount[i], j, 2);
          bindingCount[i] -= 2;
          i--;
          break;
        }
      }
    }
  }

  if (!state.extensions.palmPose) {
    for (uint32_t i = 0; i < MAX_PROFILES; i++) {
      for (uint32_t j = 0; j < bindingCount[i]; j++) {
        if (bindings[i][j].action == ACTION_PALM_POSE) {
          REMOVE_BINDINGS(bindings[i], bindingCount[i], j, 2);
          bindingCount[i] -= 2;
          break;
        }
      }
    }
  }

  if (!state.extensions.microgestures) {
    for (uint32_t i = 0; i < bindingCount[PROFILE_HAND]; i++) {
      if (bindings[PROFILE_HAND][i].action == ACTION_DPAD_UP_DOWN) {
        REMOVE_BINDINGS(bindings[PROFILE_HAND], bindingCount[PROFILE_HAND], i, 10);
        bindingCount[PROFILE_HAND] -= 10;
        break;
      }
    }
  }

  XrPath path;
  XrActionSuggestedBinding suggestedBindings[64];
  for (uint32_t i = 0; i < MAX_PROFILES; i++) {
    if (bindingCount[i] == 0) continue;

    for (uint32_t j = 0; j < bindingCount[i]; j++) {
      XRG(xrStringToPath(state.instance, bindings[i][j].path, &path), "xrStringToPath", fail);
      suggestedBindings[j].action = state.actions[bindings[i][j].action];
      suggestedBindings[j].binding = path;
    }

    XRG(xrStringToPath(state.instance, interactionProfilePaths[i], &path), "xrStringToPath", fail);

    result = (xrSuggestInteractionProfileBindings(state.instance, &(XrInteractionProfileSuggestedBinding) {
      .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
      .interactionProfile = path,
      .countSuggestedBindings = bindingCount[i],
      .suggestedBindings = suggestedBindings
    }));

    if (XR_FAILED(result)) {
      lovrLog(LOG_WARN, "XR", "Failed to suggest input bindings for %s", interactionProfilePaths[i]);
    }
  }

  state.frameState.type = XR_TYPE_FRAME_STATE;
  return true;
fail:
  disconnect();
  return false;
}

bool lovrHeadsetIsConnected(void) {
  return state.system;
}

bool lovrHeadsetGetName(char* name, size_t length) {
  if (!state.system) return false;
  XrSystemProperties properties = { .type = XR_TYPE_SYSTEM_PROPERTIES };
  if (XR_FAILED(xrGetSystemProperties(state.instance, state.system, &properties))) return false;
  strncpy(name, properties.systemName, length - 1);
  name[length - 1] = '\0';
  return true;
}

bool lovrHeadsetGetDriver(char* name, size_t length) {
  if (!state.system) return false;
  XrInstanceProperties properties = { .type = XR_TYPE_INSTANCE_PROPERTIES };
  if (XR_FAILED(xrGetInstanceProperties(state.instance, &properties))) return false;
  strncpy(name, properties.runtimeName, length - 1);
  name[length - 1] = '\0';
  return true;
}

void lovrHeadsetGetFeatures(HeadsetFeatures* features) {
  features->overlay = state.extensions.overlay;
  features->battery = state.extensions.battery;
  features->proximity = state.extensions.presence;
  features->passthrough = lovrHeadsetIsPassthroughSupported(PASSTHROUGH_BLEND) || lovrHeadsetIsPassthroughSupported(PASSTHROUGH_ADD);
  features->refreshRate = state.extensions.refreshRate;
  features->depthSubmission = state.extensions.depth;
  features->eyeTracking = state.extensions.gaze;
  features->handTracking = state.extensions.handTracking;
  features->handTrackingElbow = state.extensions.handTrackingElbow;
  features->bodyTracking = state.extensions.bodyTracking;
  features->keyboardTracking = state.extensions.keyboardTracking;
  features->viveTrackers = state.extensions.viveTrackers;
  features->handModel = state.extensions.handTrackingMesh;
  features->controllerModel = state.extensions.renderModel;
  features->controllerSkeleton = state.extensions.handTrackingDataSource && state.extensions.handTrackingMotionRange;
  features->cubeBackground = state.extensions.layerCube;
  features->equirectBackground = state.extensions.layerEquirect || state.extensions.layerEquirect2;
  features->layerColor = state.extensions.layerColor;
  features->layerCurve = state.extensions.layerCurve;
  features->layerDepthTest = state.extensions.layerDepthTest;
  features->layerFilter = state.extensions.layerSettings && state.extensions.layerAutoFilter;
}

bool lovrHeadsetIsSeated(void) {
  return state.config.seated;
}

bool lovrHeadsetStart(void) {
  if (!lovrHeadsetIsConnected()) {
    return lovrSetError("not connected");
  }

  if (state.session) {
    return lovrSetError("already started");
  }

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

      XR(xrGetVulkanGraphicsRequirements2KHR(state.instance, state.system, &requirements), "xrGetVulkanGraphicsRequirements2KHR");
      if (XR_VERSION_MAJOR(requirements.minApiVersionSupported) > 1 || XR_VERSION_MINOR(requirements.minApiVersionSupported) > 1) {
        return lovrSetError("OpenXR Vulkan version not supported");
      }

      graphicsBinding.instance = (VkInstance) gpu_vk_get_instance();
      graphicsBinding.physicalDevice = (VkPhysicalDevice) gpu_vk_get_physical_device();
      graphicsBinding.device = (VkDevice) gpu_vk_get_device();
      gpu_vk_get_queue(&graphicsBinding.queueFamilyIndex, &graphicsBinding.queueIndex);
      info.next = &graphicsBinding;
    }
#endif

    lovrAssert(hasGraphics || state.extensions.headless, "Graphics module is not available, and headless headset is not supported");

#ifdef XR_EXTX_overlay
    XrSessionCreateInfoOverlayEXTX overlayInfo = {
      .type = XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX,
      .next = info.next,
      .sessionLayersPlacement = state.config.overlayOrder
    };

    if (state.extensions.overlay) {
      info.next = &overlayInfo;
    }
#endif

    XrSessionActionSetsAttachInfo attachInfo = {
      .type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
      .countActionSets = 1,
      .actionSets = &state.actionSet
    };

    XR(xrCreateSession(state.instance, &info, &state.session), "xrCreateSession");
    XRG(xrAttachSessionActionSets(state.session, &attachInfo), "xrAttachSessionActionSets", stop);

#ifdef __ANDROID__
    if (state.extensions.threadHint) {
      XRG(xrSetAndroidApplicationThreadKHR(state.session, XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR, gettid()), "xrSetAndroidApplicationThreadKHR", stop);
    }
#endif
  }

  { // Spaaace
    XrReferenceSpaceCreateInfo referenceSpaceInfo = {
      .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
      .poseInReferenceSpace = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } }
    };

    // Head
    referenceSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    XRG(xrCreateReferenceSpace(state.session, &referenceSpaceInfo, &state.spaces[DEVICE_HEAD]), "xrCreateReferenceSpace", stop);

    // Floor (may not be supported, which is okay)
    referenceSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    if (XR_FAILED(xrCreateReferenceSpace(state.session, &referenceSpaceInfo, &state.spaces[DEVICE_FLOOR]))) {
      state.spaces[DEVICE_FLOOR] = XR_NULL_HANDLE;
    }

    if (!createReferenceSpace(getCurrentXrTime())) {
      goto stop;
    }

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

      XRG(xrCreateActionSpace(state.session, &actionSpaceInfo, &state.spaces[i]), "xrCreateActionSpace", stop);
    }
  }

  // Swapchain
  if (hasGraphics) {
    state.depthFormat = state.config.stencil ? FORMAT_D32FS8 : FORMAT_D32F;

    if (!lovrGraphicsGetFormatSupport(state.depthFormat, TEXTURE_FEATURE_RENDER)) {
      state.depthFormat = state.config.stencil ? FORMAT_D24S8 : FORMAT_D24;
    }

    state.pass = lovrPassCreate("Headset");

    if (!state.pass) {
      goto stop;
    }

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
    XRG(xrEnumerateSwapchainFormats(state.session, COUNTOF(formats), &formatCount, formats), "xrEnumerateSwapchainFormats", stop);

    bool supportsColor = false;
    bool supportsDepth = false;

    for (uint32_t i = 0; i < formatCount && (!supportsColor || !supportsDepth); i++) {
      if (formats[i] == nativeColorFormat) {
        supportsColor = true;
      } else if (formats[i] == nativeDepthFormat) {
        supportsDepth = true;
      }
    }

    lovrAssertGoto(stop, supportsColor, "This VR runtime does not support sRGB rgba8 textures");
    if (!lovrSwapchainInit(&state.swapchains[SWAPCHAIN_COLOR], state.width, state.height, VIEW | FOVEATED)) {
      goto stop;
    }

    GraphicsFeatures features;
    lovrGraphicsGetFeatures(&features);
    if (state.extensions.depth && supportsDepth && features.depthResolve) {
      if (!lovrSwapchainInit(&state.swapchains[SWAPCHAIN_DEPTH], state.width, state.height, VIEW | DEPTH)) {
        goto stop;
      }
    } else {
      state.extensions.depth = false;
    }

    // Pre-init composition layer
    state.layer = (XrCompositionLayerProjection) {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
      .viewCount = state.viewCount,
      .views = state.layerViews
    };

    // Pre-init composition layer views
    for (uint32_t i = 0; i < state.viewCount; i++) {
      state.layerViews[i] = (XrCompositionLayerProjectionView) {
        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
        .subImage = { state.swapchains[SWAPCHAIN_COLOR].handle, { { 0, 0 }, { state.width, state.height } }, i }
      };
    }

    if (state.extensions.depth) {
      for (uint32_t i = 0; i < state.viewCount; i++) {
        state.layerViews[i].next = &state.depthInfo[i];
        state.depthInfo[i] = (XrCompositionLayerDepthInfoKHR) {
          .type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR,
          .subImage.swapchain = state.swapchains[SWAPCHAIN_DEPTH].handle,
          .subImage.imageRect = state.layerViews[i].subImage.imageRect,
          .subImage.imageArrayIndex = i,
          .minDepth = 0.f,
          .maxDepth = 1.f
        };
      }
    }
  }

  if (state.extensions.keyboardTracking) {
    XrKeyboardTrackingQueryFB queryInfo = {
      .type = XR_TYPE_KEYBOARD_TRACKING_QUERY_FB,
      .flags = XR_KEYBOARD_TRACKING_QUERY_LOCAL_BIT_FB
    };

    XrKeyboardTrackingDescriptionFB keyboard;
    XrResult result = xrQuerySystemTrackedKeyboardFB(state.session, &queryInfo, &keyboard);

    if (result == XR_SUCCESS && (keyboard.flags & XR_KEYBOARD_TRACKING_EXISTS_BIT_FB)) {
      XrKeyboardSpaceCreateInfoFB spaceInfo = {
        .type = XR_TYPE_KEYBOARD_SPACE_CREATE_INFO_FB,
        .trackedKeyboardId = keyboard.trackedKeyboardId
      };

      XRG(xrCreateKeyboardSpaceFB(state.session, &spaceInfo, &state.spaces[DEVICE_KEYBOARD]), "xrCreateKeyboardSpaceFB", stop);
    } else {
      state.extensions.keyboardTracking = false;
    }
  }

  // On Quest, ask for the default passthrough mode at startup (will check preference and enable
  // passthrough if needed)
  if (state.extensions.passthroughPreferences && state.extensions.questPassthrough) {
    lovrHeadsetSetPassthrough(PASSTHROUGH_DEFAULT);
  }

  if (state.extensions.refreshRate) {
    XRG(xrEnumerateDisplayRefreshRatesFB(state.session, 0, &state.refreshRateCount, NULL), "xrEnumerateDisplayRefreshRatesFB", stop);
    state.refreshRates = lovrMalloc(state.refreshRateCount * sizeof(float));
    XRG(xrEnumerateDisplayRefreshRatesFB(state.session, state.refreshRateCount, &state.refreshRateCount, state.refreshRates), "xrEnumerateDisplayRefreshRatesFB", stop);
  }

  if (state.extensions.visibilityMask && !loadVisibilityMask()) {
    lovrLog(LOG_WARN, "XR", "Failed to load headset mask: %s", lovrGetError());
  }

  if (state.extensions.handTrackingMesh && !state.extensions.renderModel) {
    state.modelCount = 2;
    state.modelKeys = lovrMalloc(state.modelCount * sizeof(XrRenderModelIdEXT));
    state.modelKeys[0] = 1;
    state.modelKeys[1] = 2;
    state.models = lovrCalloc(state.modelCount * sizeof(XrRenderModelIdEXT));
    lovrEventPush((Event) { .type = EVENT_MODELSCHANGED });
  }

  if (state.extensions.renderModel) {
    mtx_init(&state.modelLock, mtx_plain);
  }

  if (state.extensions.bodyTracking) {
    XrBodyTrackerCreateInfoBD info = {
      .type = XR_TYPE_BODY_TRACKER_CREATE_INFO_BD,
      .jointSet = XR_BODY_JOINT_SET_FULL_BODY_JOINTS_BD
    };

    if (XR_FAILED(xrCreateBodyTrackerBD(state.session, &info, &state.bodyTracker))) {
      lovrLog(LOG_WARN, "XR", "Failed to create body tracker");
      state.bodyTracker = XR_NULL_HANDLE;
    }
  }

  state.showMainLayer = true;
  return true;

stop:
  lovrHeadsetStop();
  return false;
}

void lovrHeadsetStop(void) {
  if (!state.session) {
    return;
  }

  state.began = false;
  state.waited = false;
  state.mounted = false;
  state.frameState.predictedDisplayTime = 0;
  state.frameState.predictedDisplayPeriod = 0;
  state.frameState.shouldRender = XR_FALSE;
  state.lastDisplayTime = 0;
  state.epoch = 0;

  lovrFree(state.refreshRates);
  state.refreshRateCount = 0;
  state.refreshRates = NULL;

  for (uint32_t i = 0; i < state.layerCount; i++) {
    lovrRelease(state.layers[i], lovrLayerDestroy);
    state.layers[i] = NULL;
  }
  state.layerCount = 0;

  lovrSwapchainDestroy(&state.swapchains[0]);
  lovrSwapchainDestroy(&state.swapchains[1]);
  lovrRelease(state.pass, lovrPassDestroy);
  state.pass = NULL;

  lovrRelease(state.mask, lovrMeshDestroy);
  state.mask = NULL;

  if (state.extensions.renderModel) {
    mtx_destroy(&state.modelLock);
    for (uint32_t i = 0; i < state.modelCount; i++) {
      if (state.models[i].handle) xrDestroyRenderModelEXT(state.models[i].handle);
      if (state.models[i].space) xrDestroySpace(state.models[i].space);
      lovrFree(state.models[i].nodeStates);
      lovrFree(state.models[i].nodes);
    }
  }
  lovrFree(state.modelKeys);
  lovrFree(state.models);
  state.modelKeys = NULL;
  state.models = NULL;

  if (state.handTrackers[0]) xrDestroyHandTrackerEXT(state.handTrackers[0]);
  if (state.handTrackers[1]) xrDestroyHandTrackerEXT(state.handTrackers[1]);

  if (state.bodyTracker) xrDestroyBodyTrackerBD(state.bodyTracker);

  if (state.passthrough) xrDestroyPassthroughFB(state.passthrough);
  if (state.passthroughLayerHandle) xrDestroyPassthroughLayerFB(state.passthroughLayerHandle);
  state.passthroughActive = false;

  for (size_t i = 0; i < MAX_DEVICES; i++) {
    if (state.spaces[i]) {
      xrDestroySpace(state.spaces[i]);
      state.spaces[i] = XR_NULL_HANDLE;
    }
  }

  if (state.referenceSpace) xrDestroySpace(state.referenceSpace);
  state.referenceSpace = XR_NULL_HANDLE;

  if (state.session) xrDestroySession(state.session);
  state.sessionState = XR_SESSION_STATE_UNKNOWN;
  state.session = XR_NULL_HANDLE;
}

bool lovrHeadsetIsActive(void) {
  return state.session;
}

bool lovrHeadsetIsVisible(void) {
  return state.sessionState >= XR_SESSION_STATE_VISIBLE;
}

bool lovrHeadsetIsFocused(void) {
  return state.sessionState == XR_SESSION_STATE_FOCUSED;
}

bool lovrHeadsetIsMounted(void) {
  return state.extensions.presence ? state.mounted : true;
}

bool lovrHeadsetPollEvents(void) {
  if (!state.session) return true;

  XrEventDataBuffer e; // Not using designated initializers here to avoid an implicit 4k zero
  e.type = XR_TYPE_EVENT_DATA_BUFFER;
  e.next = NULL;

  bool visibilityMaskDirty = false;

  while (xrPollEvent(state.instance, &e) == XR_SUCCESS) {
    switch (e.type) {
      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
        XrEventDataSessionStateChanged* event = (XrEventDataSessionStateChanged*) &e;

        switch (event->state) {
          case XR_SESSION_STATE_READY:
            XR(xrBeginSession(state.session, &(XrSessionBeginInfo) {
              .type = XR_TYPE_SESSION_BEGIN_INFO,
              .primaryViewConfigurationType = state.viewConfiguration
            }), "xrBeginSession");
            break;

          case XR_SESSION_STATE_STOPPING:
            XR(xrEndSession(state.session), "xrEndSession");
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
          lovrEventPush((Event) { .type = EVENT_VISIBLE, .data.visible.visible = isVisible });
        }

        bool wasFocused = state.sessionState == XR_SESSION_STATE_FOCUSED;
        bool isFocused = event->state == XR_SESSION_STATE_FOCUSED;
        if (wasFocused != isFocused) {
          lovrEventPush((Event) {
            .type = EVENT_FOCUS,
            .data.focus.focused = isFocused,
            .data.focus.display = DISPLAY_HEADSET
          });
        }

        state.sessionState = event->state;
        break;
      }
      case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
        XrEventDataReferenceSpaceChangePending* event = (XrEventDataReferenceSpaceChangePending*) &e;
        if (event->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL) {
          if (!createReferenceSpace(event->changeTime)) {
            return false;
          }
          lovrEventPush((Event) { .type = EVENT_RECENTER });
        }
        break;
      }
      case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR:
        visibilityMaskDirty = true;
        break;
      case XR_TYPE_EVENT_DATA_USER_PRESENCE_CHANGED_EXT: {
        XrEventDataUserPresenceChangedEXT* event = (XrEventDataUserPresenceChangedEXT*) &e;
        state.mounted = event->isUserPresent;
        lovrEventPush((Event) { .type = EVENT_MOUNT, .data.mount.mounted = state.mounted });
        break;
      }
      case XR_TYPE_EVENT_DATA_INTERACTION_RENDER_MODELS_CHANGED_EXT: {
        if (!loadControllerModels()) {
          return false;
        }
        lovrEventPush((Event) { .type = EVENT_MODELSCHANGED });
        break;
      }
      default: break;
    }
    e.type = XR_TYPE_EVENT_DATA_BUFFER;
  }

  if (SESSION_RUNNING(state.sessionState) && visibilityMaskDirty && !loadVisibilityMask()) {
    lovrLog(LOG_WARN, "XR", "Failed to load headset mask: %s", lovrGetError());
  }

  return true;
}

bool lovrHeadsetUpdate(void) {
  if (!state.session) {
    memcpy(state.simulator.lastButtons, state.simulator.buttons, sizeof(state.simulator.buttons));
    return true;
  }

  if (state.waited) {
    return true;
  }

  if (SESSION_RUNNING(state.sessionState)) {
    state.lastDisplayTime = state.frameState.predictedDisplayTime;
    XR(xrWaitFrame(state.session, NULL, &state.frameState), "xrWaitFrame");
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

    XR(xrSyncActions(state.session, &syncInfo), "xrSyncActions");
  }

  // Throttle when session is idle (but not too much, a desktop window might be rendering stuff)
  if (state.sessionState == XR_SESSION_STATE_IDLE) {
    os_sleep(.001);
  }

  return true;
}

void lovrHeadsetGetDisplayDimensions(uint32_t* width, uint32_t* height) {
  if (state.session) {
    *width = state.width;
    *height = state.height;
  } else {
    lovrSystemGetWindowSize(width, height);
    *width *= state.config.supersample;
    *height *= state.config.supersample;
  }
}

float lovrHeadsetGetRefreshRate(void) {
  float refreshRate;
  if (state.session && state.extensions.refreshRate && XR_SUCCEEDED(xrGetDisplayRefreshRateFB(state.session, &refreshRate))) {
    return refreshRate;
  }
  return 0.f;
}

bool lovrHeadsetSetRefreshRate(float refreshRate) {
  if (!state.extensions.refreshRate || !state.session) return false;
  return XR_SUCCEEDED(xrRequestDisplayRefreshRateFB(state.session, refreshRate));
}

float* lovrHeadsetGetRefreshRates(uint32_t* count) {
  *count = state.refreshRateCount;
  return state.refreshRates;
}

void lovrHeadsetGetFoveation(FoveationLevel* level, bool* dynamic) {
  *level = state.foveationLevel;
  *dynamic = state.foveationDynamic;
}

bool lovrHeadsetSetFoveation(FoveationLevel level, bool dynamic) {
  if (!state.session || !state.extensions.foveation) {
    return level == FOVEATION_NONE;
  }

  if (state.foveationLevel == level && state.foveationDynamic == dynamic) {
    return true;
  }

  XrFoveationLevelProfileCreateInfoFB profileInfo = {
    .type = XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB
  };

  switch (level) {
    case FOVEATION_NONE: profileInfo.level = XR_FOVEATION_LEVEL_NONE_FB; break;
    case FOVEATION_LOW: profileInfo.level = XR_FOVEATION_LEVEL_LOW_FB; break;
    case FOVEATION_MEDIUM: profileInfo.level = XR_FOVEATION_LEVEL_MEDIUM_FB; break;
    case FOVEATION_HIGH: profileInfo.level = XR_FOVEATION_LEVEL_HIGH_FB; break;
    default: break;
  }

  if (dynamic) {
    profileInfo.dynamic = XR_FOVEATION_DYNAMIC_LEVEL_ENABLED_FB;
  } else {
    profileInfo.dynamic = XR_FOVEATION_DYNAMIC_DISABLED_FB;
  }

  XrFoveationProfileCreateInfoFB info = {
    .type = XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB,
    .next = &profileInfo
  };

  XrFoveationProfileFB profile;
  if (XR_FAILED(xrCreateFoveationProfileFB(state.session, &info, &profile))) {
    return false;
  }

  XrSwapchainStateFoveationFB foveationState = {
    .type = XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB,
    .profile = profile
  };

  if (XR_FAILED(xrUpdateSwapchainFB(state.swapchains[SWAPCHAIN_COLOR].handle, (XrSwapchainStateBaseHeaderFB*) &foveationState))) {
    return false;
  }

  xrDestroyFoveationProfileFB(profile);

  state.foveationLevel = level;
  state.foveationDynamic = dynamic;

  return true;
}

static XrEnvironmentBlendMode convertPassthroughMode(PassthroughMode mode) {
  switch (mode) {
    case PASSTHROUGH_OPAQUE: return XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    case PASSTHROUGH_BLEND: return XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
    case PASSTHROUGH_ADD: return XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
    default: lovrUnreachable();
  }
}

PassthroughMode lovrHeadsetGetPassthrough(void) {
  if (!state.session) {
    return PASSTHROUGH_OPAQUE;
  }

  if (state.extensions.questPassthrough) {
    return state.passthroughActive ? PASSTHROUGH_BLEND : PASSTHROUGH_OPAQUE;
  }

  switch (state.blendMode) {
    case XR_ENVIRONMENT_BLEND_MODE_OPAQUE: return PASSTHROUGH_OPAQUE;
    case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND: return PASSTHROUGH_BLEND;
    case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE: return PASSTHROUGH_ADD;
    default: lovrUnreachable();
  }
}

bool lovrHeadsetSetPassthrough(PassthroughMode mode) {
  if (!state.session) return false;

  if (state.extensions.questPassthrough) {
    if (mode == PASSTHROUGH_ADD) {
      return false;
    }

    if (mode == PASSTHROUGH_DEFAULT && state.extensions.passthroughPreferences) {
      XrPassthroughPreferencesMETA preferences = {
        .type = XR_TYPE_PASSTHROUGH_PREFERENCES_META
      };

      xrGetPassthroughPreferencesMETA(state.session, &preferences);

      if (preferences.flags & XR_PASSTHROUGH_PREFERENCE_DEFAULT_TO_ACTIVE_BIT_META) {
        mode = PASSTHROUGH_BLEND;
      } else {
        mode = PASSTHROUGH_OPAQUE;
      }
    }

    bool enable = mode == PASSTHROUGH_BLEND || mode == PASSTHROUGH_TRANSPARENT;

    if (state.passthroughActive == enable) {
      return true;
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

bool lovrHeadsetIsPassthroughSupported(PassthroughMode mode) {
  if (state.extensions.questPassthrough && mode == PASSTHROUGH_BLEND) {
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

static XrViewStateFlags getViews(XrView views[4], uint32_t* count) {
  if (state.frameState.predictedDisplayTime <= 0) {
    return 0;
  }

  XrViewLocateInfo viewLocateInfo = {
    .type = XR_TYPE_VIEW_LOCATE_INFO,
    .viewConfigurationType = state.viewConfiguration,
    .displayTime = state.frameState.predictedDisplayTime,
    .space = state.referenceSpace
  };

  for (uint32_t i = 0; i < 4; i++) {
    views[i].type = XR_TYPE_VIEW;
    views[i].next = NULL;
  }

  XrViewState viewState = { .type = XR_TYPE_VIEW_STATE };
  if (XR_FAILED(xrLocateViews(state.session, &viewLocateInfo, &viewState, state.viewCount, count, views))) {
    return 0;
  }

  return viewState.viewStateFlags;
}

uint32_t lovrHeadsetGetViewCount(void) {
  return state.session ? state.viewCount : 1;
}

bool lovrHeadsetGetViewPose(uint32_t view, float* position, float* orientation) {
  if (!state.session) {
    vec3_init(position, state.simulator.poses[DEVICE_HEAD]);
    quat_init(orientation, state.simulator.poses[DEVICE_HEAD] + 3);
    return view == 0;
  }

  uint32_t count;
  XrView views[4];
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
    quat_identity(orientation);
  }

  return true;
}

bool lovrHeadsetGetViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  if (!state.session) {
    uint32_t width, height;
    lovrHeadsetGetDisplayDimensions(&width, &height);
    float aspect = (float) width / height;
    float fov = .7f;
    *left = atanf(tanf(fov) * aspect);
    *right = atanf(tanf(fov) * aspect);
    *up = fov;
    *down = fov;
    return view == 0;
  }

  uint32_t count;
  XrView views[4];
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

void lovrHeadsetGetClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

void lovrHeadsetSetClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

void lovrHeadsetGetBoundsDimensions(float* width, float* depth) {
  XrExtent2Df bounds;
  if (state.session && XR_SUCCEEDED(xrGetReferenceSpaceBoundsRect(state.session, XR_REFERENCE_SPACE_TYPE_STAGE, &bounds))) {
    *width = bounds.width;
    *depth = bounds.height;
  } else {
    *width = 0.f;
    *depth = 0.f;
  }
}

bool lovrHeadsetGetPose(Device device, float* position, float* orientation) {
  if (!state.session) {
    vec3_init(position, state.simulator.poses[device] + 0);
    quat_init(orientation, state.simulator.poses[device] + 3);
    return true;
  }

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

    if (XR_FAILED(xrGetActionStatePose(state.session, &info, &poseState))) {
      return false;
    }
  }

  // If there's no space, or the pose action isn't active, fall back to hand tracking for some devices
  if (!state.spaces[device] || (action && !poseState.isActive)) {
    if (state.extensions.handTrackingElbow && (device == DEVICE_ELBOW_LEFT || device == DEVICE_ELBOW_RIGHT)) {
      XrHandTrackerEXT tracker = getHandTracker(device == DEVICE_ELBOW_LEFT ? DEVICE_HAND_LEFT : DEVICE_HAND_RIGHT);

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
        .jointCount = 26 + state.extensions.handTrackingElbow,
        .jointLocations = joints
      };

      if (XR_FAILED(xrLocateHandJointsEXT(tracker, &info, &hand))) {
        return false;
      }

      XrPosef* pose;
      pose = &joints[XR_HAND_FOREARM_JOINT_ELBOW_ULTRALEAP].pose;
      memcpy(orientation, &pose->orientation, 4 * sizeof(float));
      memcpy(position, &pose->position, 3 * sizeof(float));
      return hand.isActive;
    }

    return false;
  }

  XrSpaceLocation location = { .type = XR_TYPE_SPACE_LOCATION };
  xrLocateSpace(state.spaces[device], state.referenceSpace, state.frameState.predictedDisplayTime, &location);
  memcpy(orientation, &location.pose.orientation, 4 * sizeof(float));
  memcpy(position, &location.pose.position, 3 * sizeof(float));
  return location.locationFlags & (XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
}

bool lovrHeadsetGetVelocity(Device device, float* linearVelocity, float* angularVelocity) {
  if (!state.session || !state.spaces[device] || state.frameState.predictedDisplayTime <= 0) {
    return false;
  }

  XrSpaceVelocity velocity = { .type = XR_TYPE_SPACE_VELOCITY };
  XrSpaceLocation location = { .type = XR_TYPE_SPACE_LOCATION, .next = &velocity };
  xrLocateSpace(state.spaces[device], state.referenceSpace, state.frameState.predictedDisplayTime, &location);
  memcpy(linearVelocity, &velocity.linearVelocity, 3 * sizeof(float));
  memcpy(angularVelocity, &velocity.angularVelocity, 3 * sizeof(float));
  return velocity.velocityFlags & (XR_SPACE_VELOCITY_LINEAR_VALID_BIT | XR_SPACE_VELOCITY_ANGULAR_VALID_BIT);
}

bool lovrHeadsetIsDown(Device device, DeviceButton button, bool* down, bool* changed) {
  if (!state.session) {
    uint32_t mask = 1u << button;
    *down = state.simulator.buttons[device] & mask;
    *changed = (state.simulator.lastButtons[device] & mask) ^ (state.simulator.buttons[device] & mask);
    return true;
  }

  static const uint8_t actions[MAX_DEVICES][MAX_BUTTONS] = {
    [DEVICE_HAND_LEFT] = {
      [BUTTON_TRIGGER] = ACTION_TRIGGER_DOWN,
      [BUTTON_THUMBSTICK] = ACTION_THUMBSTICK_DOWN,
      [BUTTON_THUMBTAP] = ACTION_THUMBTAP_DOWN,
      [BUTTON_TOUCHPAD] = ACTION_TRACKPAD_DOWN,
      [BUTTON_MENU] = ACTION_MENU_DOWN,
      [BUTTON_GRIP] = ACTION_GRIP_DOWN,
      [BUTTON_A] = ACTION_A_DOWN,
      [BUTTON_B] = ACTION_B_DOWN,
      [BUTTON_X] = ACTION_X_DOWN,
      [BUTTON_Y] = ACTION_Y_DOWN,
      [BUTTON_DPAD_UP] = ACTION_DPAD_UP_DOWN,
      [BUTTON_DPAD_DOWN] = ACTION_DPAD_DOWN_DOWN,
      [BUTTON_DPAD_LEFT] = ACTION_DPAD_LEFT_DOWN,
      [BUTTON_DPAD_RIGHT] = ACTION_DPAD_RIGHT_DOWN,
      [BUTTON_BUMPER] = ACTION_BUMPER_DOWN,
      [BUTTON_NIB] = ACTION_NIB_DOWN
    },
    [DEVICE_HAND_RIGHT] = {
      [BUTTON_TRIGGER] = ACTION_TRIGGER_DOWN,
      [BUTTON_THUMBSTICK] = ACTION_THUMBSTICK_DOWN,
      [BUTTON_THUMBTAP] = ACTION_THUMBTAP_DOWN,
      [BUTTON_TOUCHPAD] = ACTION_TRACKPAD_DOWN,
      [BUTTON_MENU] = ACTION_MENU_DOWN,
      [BUTTON_GRIP] = ACTION_GRIP_DOWN,
      [BUTTON_A] = ACTION_A_DOWN,
      [BUTTON_B] = ACTION_B_DOWN,
      [BUTTON_X] = ACTION_X_DOWN,
      [BUTTON_Y] = ACTION_Y_DOWN,
      [BUTTON_DPAD_UP] = ACTION_DPAD_UP_DOWN,
      [BUTTON_DPAD_DOWN] = ACTION_DPAD_DOWN_DOWN,
      [BUTTON_DPAD_LEFT] = ACTION_DPAD_LEFT_DOWN,
      [BUTTON_DPAD_RIGHT] = ACTION_DPAD_RIGHT_DOWN,
      [BUTTON_BUMPER] = ACTION_BUMPER_DOWN,
      [BUTTON_NIB] = ACTION_NIB_DOWN
    },
    [DEVICE_STYLUS] = {
      [BUTTON_GRIP] = ACTION_GRIP_DOWN,
      [BUTTON_A] = ACTION_A_DOWN,
      [BUTTON_B] = ACTION_B_DOWN,
      [BUTTON_NIB] = ACTION_NIB_DOWN
    }
  };

  XrActionStateGetInfo info = {
    .type = XR_TYPE_ACTION_STATE_GET_INFO,
    .action = state.actions[actions[device][button]],
    .subactionPath = state.actionFilters[device]
  };

  if (!info.action) {
    return false;
  }

  XrActionStateBoolean actionState = { .type = XR_TYPE_ACTION_STATE_BOOLEAN };
  if (XR_FAILED(xrGetActionStateBoolean(state.session, &info, &actionState))) {
    return false;
  }

  *down = actionState.currentState;
  *changed = actionState.changedSinceLastSync;
  return actionState.isActive;
}

bool lovrHeadsetIsTouched(Device device, DeviceButton button, bool* touched) {
  if (!state.session) return false;

  static const uint8_t actions[MAX_DEVICES][MAX_BUTTONS] = {
    [DEVICE_HAND_LEFT] = {
      [BUTTON_TRIGGER] = ACTION_TRIGGER_TOUCH,
      [BUTTON_THUMBSTICK] = ACTION_THUMBSTICK_TOUCH,
      [BUTTON_THUMBREST] = ACTION_THUMBREST_TOUCH,
      [BUTTON_TOUCHPAD] = ACTION_TRACKPAD_TOUCH,
      [BUTTON_MENU] = ACTION_MENU_TOUCH,
      [BUTTON_GRIP] = ACTION_GRIP_TOUCH,
      [BUTTON_A] = ACTION_A_TOUCH,
      [BUTTON_B] = ACTION_B_TOUCH,
      [BUTTON_X] = ACTION_X_TOUCH,
      [BUTTON_Y] = ACTION_Y_TOUCH,
      [BUTTON_DPAD_UP] = ACTION_DPAD_UP_TOUCH,
      [BUTTON_DPAD_DOWN] = ACTION_DPAD_DOWN_TOUCH,
      [BUTTON_DPAD_LEFT] = ACTION_DPAD_LEFT_TOUCH,
      [BUTTON_DPAD_RIGHT] = ACTION_DPAD_RIGHT_TOUCH,
      [BUTTON_BUMPER] = ACTION_BUMPER_TOUCH
    },
    [DEVICE_HAND_RIGHT] = {
      [BUTTON_TRIGGER] = ACTION_TRIGGER_TOUCH,
      [BUTTON_THUMBSTICK] = ACTION_THUMBSTICK_TOUCH,
      [BUTTON_THUMBREST] = ACTION_THUMBREST_TOUCH,
      [BUTTON_TOUCHPAD] = ACTION_TRACKPAD_TOUCH,
      [BUTTON_MENU] = ACTION_MENU_TOUCH,
      [BUTTON_GRIP] = ACTION_GRIP_TOUCH,
      [BUTTON_A] = ACTION_A_TOUCH,
      [BUTTON_B] = ACTION_B_TOUCH,
      [BUTTON_X] = ACTION_X_TOUCH,
      [BUTTON_Y] = ACTION_Y_TOUCH,
      [BUTTON_DPAD_UP] = ACTION_DPAD_UP_TOUCH,
      [BUTTON_DPAD_DOWN] = ACTION_DPAD_DOWN_TOUCH,
      [BUTTON_DPAD_LEFT] = ACTION_DPAD_LEFT_TOUCH,
      [BUTTON_DPAD_RIGHT] = ACTION_DPAD_RIGHT_TOUCH,
      [BUTTON_BUMPER] = ACTION_BUMPER_TOUCH
    }
  };

  XrActionStateGetInfo info = {
    .type = XR_TYPE_ACTION_STATE_GET_INFO,
    .action = state.actions[actions[device][button]],
    .subactionPath = state.actionFilters[device]
  };

  if (!info.action) {
    return false;
  }

  XrActionStateBoolean actionState = { .type = XR_TYPE_ACTION_STATE_BOOLEAN };
  if (XR_FAILED(xrGetActionStateBoolean(state.session, &info, &actionState))) {
    return false;
  }

  *touched = actionState.currentState;
  return actionState.isActive;
}

bool lovrHeadsetGetAxis(Device device, DeviceAxis axis, float* value) {
  if (!state.session) return false;

  static const uint8_t actions[MAX_DEVICES][MAX_AXES] = {
    [DEVICE_HAND_LEFT] = {
      [AXIS_TRIGGER] = ACTION_TRIGGER_AXIS,
      [AXIS_THUMBSTICK] = ACTION_THUMBSTICK_AXIS,
      [AXIS_THUMBREST] = ACTION_THUMBREST_AXIS,
      [AXIS_TOUCHPAD] = ACTION_TRACKPAD_AXIS,
      [AXIS_GRIP] = ACTION_GRIP_AXIS,
      [AXIS_NIB] = ACTION_NIB_FORCE
    },
    [DEVICE_HAND_RIGHT] = {
      [AXIS_TRIGGER] = ACTION_TRIGGER_AXIS,
      [AXIS_THUMBSTICK] = ACTION_THUMBSTICK_AXIS,
      [AXIS_THUMBREST] = ACTION_THUMBREST_AXIS,
      [AXIS_TOUCHPAD] = ACTION_TRACKPAD_AXIS,
      [AXIS_GRIP] = ACTION_GRIP_AXIS,
      [AXIS_NIB] = ACTION_NIB_FORCE
    },
    [DEVICE_STYLUS] = {
      [AXIS_GRIP] = ACTION_GRIP_AXIS,
      [AXIS_NIB] = ACTION_NIB_FORCE
    }
  };

  if (!actions[device][axis]) {
    return false;
  }

  XrActionStateGetInfo info = {
    .type = XR_TYPE_ACTION_STATE_GET_INFO,
    .action = state.actions[actions[device][axis]],
    .subactionPath = state.actionFilters[device]
  };

  if (axis == AXIS_THUMBSTICK || axis == AXIS_TOUCHPAD) {
    XrActionStateVector2f actionState = { .type = XR_TYPE_ACTION_STATE_VECTOR2F };
    if (XR_FAILED(xrGetActionStateVector2f(state.session, &info, &actionState))) {
      return false;
    }

    value[0] = actionState.currentState.x;
    value[1] = actionState.currentState.y;
    return actionState.isActive;
  } else {
    XrActionStateFloat actionState = { .type = XR_TYPE_ACTION_STATE_FLOAT };

    if (XR_FAILED(xrGetActionStateFloat(state.session, &info, &actionState))) {
      return false;
    }

    *value = actionState.currentState;
    return actionState.isActive;
  }
}

bool lovrHeadsetGetSkeleton(Device device, float* poses, SkeletonSource* source) {
  if (device == DEVICE_BODY) {
    XrBodyTrackerBD tracker = getBodyTracker();

    if (!tracker || state.frameState.predictedDisplayTime <= 0) {
      return false;
    }

    XrBodyJointsLocateInfoBD locateInfo = {
      .type = XR_TYPE_BODY_JOINTS_LOCATE_INFO_BD,
      .baseSpace = state.referenceSpace,
      .time = state.frameState.predictedDisplayTime
    };

    XrBodyJointLocationBD joints[XR_BODY_JOINT_COUNT_BD];
    XrBodyJointLocationsBD locations = {
      .type = XR_TYPE_BODY_JOINT_LOCATIONS_BD,
      .jointLocationCount = XR_BODY_JOINT_COUNT_BD,
      .jointLocations = joints
    };

    if (XR_FAILED(xrLocateBodyJointsBD(tracker, &locateInfo, &locations))) {
      return false;
    }

    float* pose = poses;
    for (uint32_t i = 0; i < BODY_JOINT_COUNT; i++) {
      memset(pose, 0, 8 * sizeof(float));

      if (joints[i].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
        memcpy(pose, &joints[i].pose.position.x, 3 * sizeof(float));
      }
      if (joints[i].locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
        memcpy(pose + 4, &joints[i].pose.orientation.x, 4 * sizeof(float));
      }

      pose += 8;
    }

    if (source) {
      *source = SOURCE_UNKNOWN;
    }

    return true;
  }

  XrHandTrackerEXT tracker = getHandTracker(device);

  if (!tracker || state.frameState.predictedDisplayTime <= 0) {
    return false;
  }

  XrHandJointsLocateInfoEXT info = {
    .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
    .baseSpace = state.referenceSpace,
    .time = state.frameState.predictedDisplayTime
  };

  XrHandJointsMotionRangeInfoEXT motionRange = {
    .type = XR_TYPE_HAND_JOINTS_MOTION_RANGE_INFO_EXT,
    .handJointsMotionRange = state.config.controllerSkeleton == SKELETON_CONTROLLER ?
      XR_HAND_JOINTS_MOTION_RANGE_CONFORMING_TO_CONTROLLER_EXT :
      XR_HAND_JOINTS_MOTION_RANGE_UNOBSTRUCTED_EXT
  };

  if (state.extensions.handTrackingMotionRange) {
    motionRange.next = info.next;
    info.next = &motionRange;
  }

  XrHandJointLocationEXT joints[MAX_HAND_JOINTS];
  XrHandJointLocationsEXT hand = {
    .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
    .jointCount = 26 + state.extensions.handTrackingElbow,
    .jointLocations = joints
  };

  XrHandTrackingDataSourceStateEXT sourceState = {
    .type = XR_TYPE_HAND_TRACKING_DATA_SOURCE_STATE_EXT
  };

  if (state.extensions.handTrackingDataSource) {
    sourceState.next = hand.next;
    hand.next = &sourceState;
  }

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

  if (state.extensions.handTrackingDataSource) {
    *source = sourceState.dataSource == XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT ? SOURCE_CONTROLLER : SOURCE_HAND;
  } else {
    *source = SOURCE_UNKNOWN;
  }

  return true;
}

bool lovrHeadsetGetBattery(Device device, float* level, bool* charging) {
  if (!state.extensions.battery) {
    return false;
  }

  XrBatteryStateDisplayEXT battery = {
    .type = XR_TYPE_BATTERY_STATE_DISPLAY_EXT
  };

  XrInteractionProfileState profile = {
    .type = XR_TYPE_INTERACTION_PROFILE_STATE,
    .next = &battery
  };

  XrPath path = state.actionFilters[device];

  if (XR_FAILED(xrGetCurrentInteractionProfile(state.session, path, &profile))) {
    return false;
  }

  if (~battery.stateFlags & (XR_BATTERY_STATE_DISPLAY_STATE_VALID_BIT_EXT | XR_BATTERY_STATE_DISPLAY_STATE_NO_BATTERY_BIT_EXT)) {
    return false;
  }

  *level = battery.batteryLevel;
  *charging = !!(battery.stateFlags & XR_BATTERY_STATE_DISPLAY_STATE_CHARGING_BIT_EXT);
  return true;
}

bool lovrHeadsetVibrate(Device device, float power, float duration, float frequency) {
  static const uint8_t actions[MAX_DEVICES] = {
    [DEVICE_HAND_LEFT] = ACTION_HAND_VIBRATE,
    [DEVICE_HAND_RIGHT] = ACTION_HAND_VIBRATE,
    [DEVICE_STYLUS] = ACTION_STYLUS_VIBRATE
  };

  if (!state.session || !actions[device]) {
    return false;
  }

  XrHapticActionInfo info = {
    .type = XR_TYPE_HAPTIC_ACTION_INFO,
    .action = state.actions[actions[device]],
    .subactionPath = state.actionFilters[device]
  };

  XrHapticVibration vibration = {
    .type = XR_TYPE_HAPTIC_VIBRATION,
    .duration = (XrDuration) (duration * 1e9f + .5f),
    .frequency = frequency,
    .amplitude = power
  };

  return XR_SUCCEEDED(xrApplyHapticFeedback(state.session, &info, (XrHapticBaseHeader*) &vibration));
}

void lovrHeadsetStopVibration(Device device) {
  static const uint8_t actions[MAX_DEVICES] = {
    [DEVICE_HAND_LEFT] = ACTION_HAND_VIBRATE,
    [DEVICE_HAND_RIGHT] = ACTION_HAND_VIBRATE,
    [DEVICE_STYLUS] = ACTION_STYLUS_VIBRATE
  };

  if (!state.session || !actions[device]) {
    return;
  }

  XrHapticActionInfo info = {
    .type = XR_TYPE_HAPTIC_ACTION_INFO,
    .action = state.actions[actions[device]],
    .subactionPath = state.actionFilters[device]
  };

  xrStopHapticFeedback(state.session, &info);
}

uint64_t* lovrHeadsetGetModelKeys(uint32_t* count) {
  *count = state.modelCount;
  return state.modelKeys;
}

static ModelData* newModelDataEXT(uint64_t key) {
  XrUuidEXT cacheId;
  uint32_t nodeCount;
  bool found = false;

  mtx_lock(&state.modelLock);
  for (uint32_t i = 0; i < state.modelCount; i++) {
    if (state.modelKeys[i] == key) {
      cacheId = state.models[i].properties.cacheId;
      nodeCount = state.models[i].properties.animatableNodeCount;
      found = true;
      break;
    }
  }
  mtx_unlock(&state.modelLock);

  if (!found) {
    return NULL;
  }

  XrRenderModelAssetCreateInfoEXT assetInfo = {
    .type = XR_TYPE_RENDER_MODEL_ASSET_CREATE_INFO_EXT,
    .cacheId = cacheId
  };

  XrRenderModelAssetEXT asset;
  XR(xrCreateRenderModelAssetEXT(state.session, &assetInfo, &asset), "xrCreateRenderModelAssetEXT");

  XrRenderModelAssetPropertiesGetInfoEXT propertyInfo = { .type = XR_TYPE_RENDER_MODEL_ASSET_PROPERTIES_GET_INFO_EXT };
  XrRenderModelAssetNodePropertiesEXT* nodeProperties = lovrMalloc(nodeCount * sizeof(XrRenderModelAssetNodePropertiesEXT));

  XrRenderModelAssetPropertiesEXT assetProperties = {
    .type = XR_TYPE_RENDER_MODEL_ASSET_PROPERTIES_EXT,
    .nodePropertyCount = nodeCount,
    .nodeProperties = nodeProperties
  };

  XrResult result = xrGetRenderModelAssetPropertiesEXT(asset, &propertyInfo, &assetProperties);

  if (XR_FAILED(result)) {
    xrthrow(result, "xrGetRenderModelAssetPropertiesEXT");
    xrDestroyRenderModelAssetEXT(asset);
    lovrFree(nodeProperties);
    return NULL;
  }

  XrRenderModelAssetDataGetInfoEXT dataInfo = { .type = XR_TYPE_RENDER_MODEL_ASSET_DATA_GET_INFO_EXT };
  XrRenderModelAssetDataEXT data = { .type = XR_TYPE_RENDER_MODEL_ASSET_DATA_EXT };
  XR(xrGetRenderModelAssetDataEXT(asset, &dataInfo, &data), "xrGetRenderModelAssetDataEXT");

  data.bufferCapacityInput = data.bufferCountOutput;
  data.buffer = lovrMalloc(data.bufferCountOutput);

  XR(xrGetRenderModelAssetDataEXT(asset, &dataInfo, &data), "xrGetRenderModelAssetDataEXT");
  Blob* blob = lovrBlobCreate(data.buffer, data.bufferCountOutput, "Headset Render Model Data");
  xrDestroyRenderModelAssetEXT(asset);

  ModelData* modelData = lovrModelDataCreate(blob, NULL);
  lovrRelease(blob, lovrBlobDestroy);

  if (!modelData) {
    lovrFree(nodeProperties);
    return NULL;
  }

  modelData->meta.id = key;

  // Fill out node lookup
  mtx_lock(&state.modelLock);
  for (uint32_t i = 0; i < state.modelCount; i++) {
    if (state.modelKeys[i] == key) {
      for (uint32_t n = 0; n < nodeCount; n++) {
        const char* name = nodeProperties[n].uniqueName;
        uint32_t hash = (uint32_t) hash64(name, strlen(name));
        for (uint32_t m = 0; m < modelData->meta.nodeCount; m++) {
          if (modelData->meta.nodeLookup[m] == hash) {
            state.models[i].nodes[n] = m;
            break;
          }
        }
      }
      break;
    }
  }
  lovrFree(nodeProperties);
  mtx_unlock(&state.modelLock);

  return modelData;
}

static ModelData* newModelDataFB(uint64_t key) {
  Device device = key == 1 ? DEVICE_HAND_LEFT : DEVICE_HAND_RIGHT;
  XrHandTrackerEXT tracker = getHandTracker(device);

  if (!tracker) {
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
  size_t sizes[9];
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
  if (offset != totalSize) lovrUnreachable();

  // Populate the data
  result = xrGetHandMeshFB(tracker, &mesh);
  if (XR_FAILED(result)) {
    lovrFree(meshData);
    return NULL;
  }

  ModelData* model = lovrCalloc(sizeof(ModelData));
  model->ref = 1;
  model->meta.id = key;
  model->meta.meshCount = 1;
  model->meta.partCount = 1;
  model->meta.skinCount = 1;
  model->meta.nodeCount = 2 + jointCount;
  model->meta.jointCount = jointCount;
  model->meta.vertexCount = vertexCount;
  model->meta.indexCount = indexCount;
  model->meta.skinnedVertexCount = vertexCount;
  model->meta.animatedVertexCount = vertexCount;
  model->meta.indexSize = 2;
  lovrModelDataAllocate(model);

  XrVector3f* positions = mesh.vertexPositions;
  XrVector3f* normals = mesh.vertexNormals;
  XrVector2f* uvs = mesh.vertexUVs;

  for (uint32_t i = 0; i < vertexCount; i++) {
    model->vertices[i] = (ModelVertex) {
      .position = { positions[i].x, positions[i].y, positions[i].z },
      .normal =
        ((((uint32_t) (int32_t) (normals[i].x * 511.f)) & 0x3ff) <<  0) |
        ((((uint32_t) (int32_t) (normals[i].y * 511.f)) & 0x3ff) << 10) |
        ((((uint32_t) (int32_t) (normals[i].z * 511.f)) & 0x3ff) << 20),
      .uv = { uvs[i].x, uvs[i].y },
      .color = { 0xff, 0xff, 0xff, 0xff }
    };
  }

  XrVector4sFB* joints = mesh.vertexBlendIndices;
  XrVector4f* weights = mesh.vertexBlendWeights;

  for (uint32_t i = 0; i < vertexCount; i++) {
    model->skinData[i] = (SkinData) {
      .joints = { (uint8_t) joints[i].x, (uint8_t) joints[i].y, (uint8_t) joints[i].z, (uint8_t) joints[i].w },
      .weights = { weights[i].x * 255.f + .5f, weights[i].y * 255.f + .5f, weights[i].z * 255.f + .5f, weights[i].w * 255.f + .5f },
    };
  }

  ModelMetadata* meta = &model->meta;

  memcpy(model->indices, mesh.indices, meta->indexCount * meta->indexSize);

  meta->meshes[0].parts = meta->parts;
  meta->meshes[0].partCount = 1;
  meta->meshes[0].vertexOffset = 0;
  meta->meshes[0].vertexCount = vertexCount;
  meta->meshes[0].indexOffset = 0;
  meta->meshes[0].indexCount = indexCount;
  meta->meshes[0].skinDataOffset = 0;

  meta->parts[0].start = 0;
  meta->parts[0].count = indexCount;
  meta->parts[0].baseVertex = 0;

  meta->skins[0] = (ModelSkin) {
    .jointCount = meta->jointCount,
    .joints = meta->joints,
    .inverseBindMatrices = meta->inverseBindMatrices
  };

  // The nodes in the Model correspond directly to the joints in the skin, for convenience
  for (uint32_t i = 0; i < meta->jointCount; i++) {
    meta->joints[i] = i;

    uint32_t parent = mesh.jointParents[i];

    // Joint node
    meta->nodes[i] = (ModelNode) {
      .transform.translation = { 0.f, 0.f, 0.f },
      .transform.rotation = { 0.f, 0.f, 0.f, 1.f },
      .transform.scale = { 1.f, 1.f, 1.f },
      .child = ~0u,
      .sibling = ~0u,
      .parent = parent < meta->jointCount ? parent : ~0u,
      .mesh = ~0u,
      .skin = ~0u
    };

    if (parent < meta->jointCount) {
      meta->nodes[i].sibling = meta->nodes[parent].child;
      meta->nodes[parent].child = i;
    }

    // Inverse bind matrix
    XrPosef* pose = &mesh.jointBindPoses[i];
    float* inverseBindMatrix = meta->inverseBindMatrices + 16 * i;
    mat4_fromPose(inverseBindMatrix, &pose->position.x, &pose->orientation.x);
    mat4_invert(inverseBindMatrix);
  }

  // Add a node that holds the skinned mesh
  meta->nodes[meta->jointCount] = (ModelNode) {
    .transform.translation = { 0.f, 0.f, 0.f },
    .transform.rotation = { 0.f, 0.f, 0.f, 1.f },
    .transform.scale = { 1.f, 1.f, 1.f },
    .child = ~0u,
    .sibling = ~0u,
    .parent = ~0u,
    .mesh = 0,
    .skin = 0
  };

  // The root node has the mesh node and root joint as children
  meta->rootNode = meta->jointCount + 1;
  meta->nodes[meta->rootNode] = (ModelNode) {
    .hasMatrix = true,
    .transform = { MAT4_IDENTITY },
    .child = ~0u,
    .sibling = ~0u,
    .parent = ~0u,
    .mesh = ~0u,
    .skin = ~0u
  };

  // Add the 2 children to the root node
  meta->nodes[meta->rootNode].child = XR_HAND_JOINT_WRIST_EXT;
  meta->nodes[XR_HAND_JOINT_WRIST_EXT].sibling = meta->jointCount;
  meta->nodes[XR_HAND_JOINT_WRIST_EXT].parent = meta->rootNode;
  meta->nodes[meta->jointCount].parent = meta->rootNode;

  lovrModelDataFinalize(model);

  return model;
}

ModelData* lovrHeadsetNewModelData(uint64_t key) {
  if (state.extensions.renderModel) {
    return newModelDataEXT(key);
  } else if (state.extensions.handTrackingMesh) {
    return newModelDataFB(key);
  } else {
    return NULL;
  }
}

bool lovrHeadsetGetModelPose(Model* model, float* position, float* orientation) {
  if (state.extensions.renderModel) {
    uint64_t key = lovrModelGetMetadata(model)->id;

    mtx_lock(&state.modelLock);

    for (uint32_t i = 0; i < state.modelCount; i++) {
      if (state.modelKeys[i] != key) {
        continue;
      }

      RenderModel* renderModel = &state.models[i];
      XrSpaceLocation location = { .type = XR_TYPE_SPACE_LOCATION };
      xrLocateSpace(renderModel->space, state.referenceSpace, state.frameState.predictedDisplayTime, &location);
      memcpy(orientation, &location.pose.orientation, 4 * sizeof(float));
      memcpy(position, &location.pose.position, 3 * sizeof(float));
      mtx_unlock(&state.modelLock);
      return location.locationFlags & (XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
    }

    mtx_unlock(&state.modelLock);
    return false;
  } else if (state.extensions.handTrackingMesh) {
    Device device = lovrModelGetMetadata(model)->id == 1 ? DEVICE_HAND_LEFT : DEVICE_HAND_RIGHT;
    return lovrHeadsetGetPose(device, position, orientation);
  }

  return false;
}

static bool animateEXT(Model* model) {
  uint64_t key = lovrModelGetMetadata(model)->id;

  mtx_lock(&state.modelLock);

  for (uint32_t i = 0; i < state.modelCount; i++) {
    if (state.modelKeys[i] != key) {
      continue;
    }

    RenderModel* renderModel = &state.models[i];

    XrRenderModelStateGetInfoEXT request = {
      .type = XR_TYPE_RENDER_MODEL_STATE_GET_INFO_EXT,
      .displayTime = state.frameState.predictedDisplayTime
    };

    XrRenderModelStateEXT modelState = {
      .type = XR_TYPE_RENDER_MODEL_STATE_EXT,
      .nodeStateCount = renderModel->properties.animatableNodeCount,
      .nodeStates = renderModel->nodeStates
    };

    XRG(xrGetRenderModelStateEXT(renderModel->handle, &request, &modelState), "xrGetRenderModelStateEXT", fail);

    lovrModelResetNodeTransforms(model);

    for (uint32_t n = 0; n < modelState.nodeStateCount; n++) {
      XrRenderModelNodeStateEXT nodeState = renderModel->nodeStates[n];

      if (renderModel->nodes[n] == ~0u) {
        continue;
      }

      float position[3], orientation[4];
      vec3_init(position, &nodeState.nodePose.position.x);
      quat_init(orientation, &nodeState.nodePose.orientation.x);
      lovrModelSetNodeTransform(model, renderModel->nodes[n], position, NULL, orientation, ORIGIN_PARENT);
      lovrModelSetNodeVisible(model, renderModel->nodes[n], nodeState.isVisible);
    }

    mtx_unlock(&state.modelLock);
    return true;
  }

fail:
  mtx_unlock(&state.modelLock);
  return false;
}

static bool animateFB(Model* model) {
  Device device = lovrModelGetMetadata(model)->id == 1 ? DEVICE_HAND_LEFT : DEVICE_HAND_RIGHT;
  XrHandTrackerEXT tracker = state.handTrackers[device == DEVICE_HAND_RIGHT];

  XrHandJointsLocateInfoEXT locateInfo = {
    .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
    .baseSpace = state.spaces[device],
    .time = state.frameState.predictedDisplayTime
  };

  XrHandJointLocationEXT joints[MAX_HAND_JOINTS];
  XrHandJointLocationsEXT hand = {
    .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
    .jointCount = 26 + state.extensions.handTrackingElbow,
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

bool lovrHeadsetAnimate(Model* model) {
  if (state.extensions.renderModel) {
    return animateEXT(model);
  } else if (state.extensions.handTrackingMesh) {
    return animateFB(model);
  }

  return false;
}

Texture* lovrHeadsetSetBackground(uint32_t width, uint32_t height, uint32_t layers) {
  Swapchain* swapchain = &state.swapchains[SWAPCHAIN_BACKGROUND];

  if (width == 0 && height == 0) {
    lovrSwapchainDestroy(swapchain);
    memset(swapchain, 0, sizeof(Swapchain));
    return NULL;
  }

  lovrCheck(state.extensions.layerCube || layers != 6, "This headset does not support cubemap backgrounds");
  lovrCheck(state.extensions.layerEquirect || state.extensions.layerEquirect2 || layers != 1, "This headset does not support equirectangular backgrounds");

  if (!lovrSwapchainInit(swapchain, width, height, STATIC | (layers == 6 ? CUBE : 0))) {
    return NULL;
  }

  if (!lovrSwapchainAcquire(swapchain)) {
    lovrSwapchainDestroy(swapchain);
    memset(swapchain, 0, sizeof(Swapchain));
    return NULL;
  }

  if (layers == 6) {
    state.background.cube = (XrCompositionLayerCubeKHR) {
      .type = XR_TYPE_COMPOSITION_LAYER_CUBE_KHR,
      .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
      .swapchain = swapchain->handle,
      .orientation.w = 1.f
    };
  } else if (state.extensions.layerEquirect2) {
    state.background.equirect2 = (XrCompositionLayerEquirect2KHR) {
      .type = XR_TYPE_COMPOSITION_LAYER_EQUIRECT2_KHR,
      .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
      .subImage = { swapchain->handle, { { 0, 0 }, { width, height } }, 0 },
      .pose.orientation.w = 1.f,
      .centralHorizontalAngle = 2.f * (float) M_PI,
      .upperVerticalAngle = (float) M_PI * .5f,
      .lowerVerticalAngle = (float) -M_PI * .5f
    };
  } else {
    state.background.equirect = (XrCompositionLayerEquirectKHR) {
      .type = XR_TYPE_COMPOSITION_LAYER_EQUIRECT_KHR,
      .eyeVisibility = XR_EYE_VISIBILITY_BOTH,
      .subImage = { swapchain->handle, { { 0, 0 }, { width, height } }, 0 },
      .pose.orientation.w = 1.f,
      .scale = { 1.f, 1.f }
    };
  }

  return state.swapchains[SWAPCHAIN_BACKGROUND].textures[0];
}

Layer** lovrHeadsetGetLayers(uint32_t* count, bool* main) {
  *count = state.layerCount;
  *main = state.showMainLayer;
  return state.layers;
}

bool lovrHeadsetSetLayers(Layer** layers, uint32_t count, bool main) {
  uint32_t total = 0;

  for (uint32_t i = 0; i < count; i++) {
    total += layers[i]->info.stereo ? 2 : 1;
  }

  lovrCheck(total <= MAX_LAYERS, "Too many layers");

  for (uint32_t i = 0; i < state.layerCount; i++) {
    lovrRelease(state.layers[i], lovrLayerDestroy);
  }

  state.layerCount = count;
  for (uint32_t i = 0; i < count; i++) {
    lovrRetain(layers[i]);
    state.layers[i] = layers[i];
  }

  state.showMainLayer = main;

  return true;
}

bool lovrHeadsetGetTexture(Texture** texture) {
  if (!state.session) {
    return lovrGraphicsGetWindowTexture(texture);
  }

  if (!SESSION_RUNNING(state.sessionState)) {
    *texture = NULL;
    return true;
  }

  if (!state.began) {
    XrFrameBeginInfo beginfo = { .type = XR_TYPE_FRAME_BEGIN_INFO };
    XR(xrBeginFrame(state.session, &beginfo), "xrBeginFrame");
    state.began = true;
  }

  if (!state.frameState.shouldRender) {
    *texture = NULL;
    return true;
  }

  *texture = lovrSwapchainAcquire(&state.swapchains[SWAPCHAIN_COLOR]);
  return *texture != NULL;
}

bool lovrHeadsetGetDepthTexture(Texture** texture) {
  if (!SESSION_RUNNING(state.sessionState) || !state.extensions.depth) {
    *texture = NULL;
    return true;
  }

  if (!state.began) {
    XrFrameBeginInfo beginfo = { .type = XR_TYPE_FRAME_BEGIN_INFO };
    XR(xrBeginFrame(state.session, &beginfo), "xrBeginFrame");
    state.began = true;
  }

  if (!state.frameState.shouldRender) {
    *texture = NULL;
    return true;
  }

  *texture = lovrSwapchainAcquire(&state.swapchains[SWAPCHAIN_DEPTH]);
  return *texture != NULL;
}

bool lovrHeadsetGetPass(Pass** pass) {
  if (!state.session) {
    if (!lovrGraphicsGetWindowPass(pass)) {
      return false;
    }

    if (*pass == NULL) {
      return true;
    }

    float position[3], orientation[4], viewMatrix[16];
    lovrHeadsetGetViewPose(0, position, orientation);
    mat4_fromPose(viewMatrix, position, orientation);
    mat4_invert(viewMatrix);

    float left, right, up, down, projection[16];
    lovrHeadsetGetViewAngles(0, &left, &right, &up, &down);
    mat4_fov(projection, left, right, up, down, state.clipNear, state.clipFar);

    lovrPassSetViewMatrix(*pass, 0, viewMatrix);
    lovrPassSetProjection(*pass, 0, projection);
    return true;
  }

  if (state.began) {
    *pass = state.frameState.shouldRender ? state.pass : NULL;
    return true;
  }

  Canvas canvas = {
    .depthFormat = state.depthFormat,
    .samples = state.config.antialias ? 4 : 1
  };

  if (!lovrHeadsetGetTexture(&canvas.color[0].texture) || !lovrHeadsetGetDepthTexture(&canvas.depth.texture)) {
    return false;
  }

  if (!canvas.color[0].texture) {
    *pass = NULL;
    return true;
  }

  canvas.foveation = state.foveationLevel ? state.swapchains[SWAPCHAIN_COLOR].foveationTextures[state.swapchains[SWAPCHAIN_COLOR].textureIndex] : NULL;

  if (!lovrPassSetCanvas(state.pass, &canvas)) {
    return false;
  }

  float background[4][4];
  LoadAction loads[4] = { LOAD_CLEAR };
  lovrGraphicsGetBackgroundColor(background[0]);
  lovrPassSetClear(state.pass, loads, background, LOAD_CLEAR, 0.f);

  uint32_t count;
  XrView views[4];
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

  if (state.extensions.visibilityMask && state.mask) {
    Shader* shader = lovrGraphicsGetDefaultShader(SHADER_MASK);
    lovrPassPush(state.pass, STACK_STATE);
    lovrPassSetShader(state.pass, shader);
    lovrPassSetColor(state.pass, (float[4]) { 0.f, 0.f, 0.f, 1.f });
    if (!shader || !lovrPassDrawMesh(state.pass, state.mask, NULL, 1)) {
      lovrLog(LOG_WARN, "XR", "Failed to draw headset mask: %s", lovrGetError());
      lovrRelease(state.mask, lovrMeshDestroy);
      state.mask = NULL;
    }
    lovrPassPop(state.pass, STACK_STATE);
  }

  *pass = state.pass;
  return true;
}

bool lovrHeadsetSubmit(void) {
  if (!SESSION_RUNNING(state.sessionState)) {
    state.waited = false;
    return true;
  }

  if (!state.began) {
    XrFrameBeginInfo beginfo = { .type = XR_TYPE_FRAME_BEGIN_INFO };
    XR(xrBeginFrame(state.session, &beginfo), "xrBeginFrame");
    state.began = true;
  }

  XrCompositionLayerBaseHeader const* layers[MAX_LAYERS + 3];

  union {
    XrCompositionLayerQuad quad;
    XrCompositionLayerCylinderKHR cylinder;
  } stereoLayers[MAX_LAYERS];

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
    lovrSwapchainRelease(&state.swapchains[SWAPCHAIN_COLOR]);
    lovrSwapchainRelease(&state.swapchains[SWAPCHAIN_DEPTH]);

    // Passthrough layer
    if (state.passthroughActive) {
      layers[info.layerCount++] = (const XrCompositionLayerBaseHeader*) &state.passthroughLayer;
    }

    // Background layer
    if (state.swapchains[SWAPCHAIN_BACKGROUND].handle) {
      layers[info.layerCount++] = (const XrCompositionLayerBaseHeader*) &state.background.header;
      state.background.header.space = state.referenceSpace;
      lovrSwapchainRelease(&state.swapchains[SWAPCHAIN_BACKGROUND]);
    }

    // Main layer
    if (state.showMainLayer) {
      state.layer.next = NULL;

      if (state.extensions.layerDepthTest && state.extensions.depth && state.layerCount > 0) {
        depthTestInfo.next = state.layer.next;
        state.layer.next = &depthTestInfo;
      }

      if (state.extensions.depth) {
        for (uint32_t i = 0; i < state.viewCount; i++) {
          if (state.clipFar == 0.f) {
            state.depthInfo[i].nearZ = +INFINITY;
            state.depthInfo[i].farZ = state.clipNear;
          } else {
            state.depthInfo[i].nearZ = state.clipNear;
            state.depthInfo[i].farZ = state.clipFar;
          }
        }
      }

      if (state.extensions.overlay || state.passthroughActive || state.blendMode != XR_ENVIRONMENT_BLEND_MODE_OPAQUE || state.layerCount > 0) {
        state.layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
      } else {
        state.layer.layerFlags = 0;
      }

      state.layer.space = state.referenceSpace;

      layers[info.layerCount++] = (const XrCompositionLayerBaseHeader*) &state.layer;
    }

    // Quad layers
    for (uint32_t i = 0; i < state.layerCount; i++) {
      Layer* layer = state.layers[i];

      layers[info.layerCount++] = (const XrCompositionLayerBaseHeader*) &layer->header;
      layer->header.space = layer->origin >= MAX_DEVICES ? state.referenceSpace : state.spaces[layer->origin];
      lovrSwapchainRelease(&layer->swapchain);

      // Stereo layers require 2 composition layers (gr?).  We make a temporary copy of the layer's
      // data and change it to show up in the right eye with the second texture array layer.
      if (layer->info.stereo) {
        layers[info.layerCount++] = (const XrCompositionLayerBaseHeader*) &stereoLayers[i];

        if (layer->curve == 0.f) {
          stereoLayers[i].quad = layer->quad;
          stereoLayers[i].quad.eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
          stereoLayers[i].quad.subImage.imageArrayIndex = 1;
        } else {
          stereoLayers[i].cylinder = layer->cylinder;
          stereoLayers[i].cylinder.eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
          stereoLayers[i].cylinder.subImage.imageArrayIndex = 1;
        }
      }
    }
  }

  XR(xrEndFrame(state.session, &info), "xrEndFrame");
  state.began = false;
  state.waited = false;
  return true;
}

void lovrHeadsetSetPose(Device device, float* position, float* orientation) {
  vec3_init(state.simulator.poses[device] + 0, position);
  quat_init(state.simulator.poses[device] + 3, orientation);
}

void lovrHeadsetSetButton(Device device, DeviceButton button, bool down) {
  state.simulator.buttons[device] &= ~(1u << button);
  state.simulator.buttons[device] |= down << button;
}

// Layer

Layer* lovrLayerCreate(const LayerInfo* info) {
  lovrAssert(state.session, "A headset session must be active to create a Layer");

  Layer* layer = lovrCalloc(sizeof(Layer));
  layer->ref = 1;
  layer->info = *info;
  layer->origin = ~0u;

  uint32_t flags = (info->stereo ? STEREO : 0) | (info->immutable ? STATIC : 0);

  if (!lovrSwapchainInit(&layer->swapchain, info->width, info->height, flags)) {
    lovrLayerDestroy(layer);
    return NULL;
  }

  layer->quad = (XrCompositionLayerQuad) {
    .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
    .layerFlags = info->transparent ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0,
    .eyeVisibility = info->stereo ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_BOTH,
    .subImage = { layer->swapchain.handle, { { 0, 0 }, { info->width, info->height } }, 0 },
    .pose.orientation.w = 1.f,
    .size = { 1.f, 1.f }
  };

  if (state.extensions.layerColor) {
    layer->color.type = XR_TYPE_COMPOSITION_LAYER_COLOR_SCALE_BIAS_KHR;
    layer->color.next = layer->header.next;
    layer->color.colorScale.r = 1.f;
    layer->color.colorScale.g = 1.f;
    layer->color.colorScale.b = 1.f;
    layer->color.colorScale.a = 1.f;
    layer->header.next = &layer->color;
  }

  if (state.extensions.layerDepthTest) {
    layer->depthTest.type = XR_TYPE_COMPOSITION_LAYER_DEPTH_TEST_FB;
    layer->depthTest.next = layer->header.next;
    layer->depthTest.depthMask = XR_TRUE;
    layer->depthTest.compareOp = XR_COMPARE_OP_LESS_OR_EQUAL_FB;
    layer->header.next = &layer->depthTest;
  }

  if (info->filter && state.extensions.layerSettings && state.extensions.layerAutoFilter) {
    layer->settings.type = XR_TYPE_COMPOSITION_LAYER_SETTINGS_FB;
    layer->settings.next = layer->header.next;
    layer->settings.layerFlags |= XR_COMPOSITION_LAYER_SETTINGS_NORMAL_SUPER_SAMPLING_BIT_FB;
    layer->settings.layerFlags |= XR_COMPOSITION_LAYER_SETTINGS_QUALITY_SUPER_SAMPLING_BIT_FB;
    layer->settings.layerFlags |= XR_COMPOSITION_LAYER_SETTINGS_NORMAL_SHARPENING_BIT_FB;
    layer->settings.layerFlags |= XR_COMPOSITION_LAYER_SETTINGS_QUALITY_SHARPENING_BIT_FB;
    layer->settings.layerFlags |= XR_COMPOSITION_LAYER_SETTINGS_AUTO_LAYER_FILTER_BIT_META;
    layer->header.next = &layer->settings;
  }

  // Avoid submission of un-acquired swapchain
  if (!lovrSwapchainAcquire(&layer->swapchain)) {
    lovrLayerDestroy(layer);
    return NULL;
  }

  return layer;
}

void lovrLayerDestroy(void* ref) {
  Layer* layer = ref;
  lovrSwapchainDestroy(&layer->swapchain);
  lovrRelease(layer->pass, lovrPassDestroy);
  lovrFree(layer);
}

Device lovrLayerGetOrigin(Layer* layer) {
  return layer->origin;
}

void lovrLayerSetOrigin(Layer* layer, Device device) {
  layer->origin = device;
}

void lovrLayerGetPose(Layer* layer, float* position, float* orientation) {
  if (layer->curve == 0.f) {
    memcpy(position, &layer->quad.pose.position.x, 3 * sizeof(float));
    memcpy(orientation, &layer->quad.pose.orientation.x, 4 * sizeof(float));
  } else {
    memcpy(position, &layer->cylinder.pose.position, 3 * sizeof(float));
    memcpy(orientation, &layer->cylinder.pose.orientation.x, 4 * sizeof(float));
    float direction[3] = { 0.f, 0.f, -1.f };
    quat_rotate(orientation, direction);
    vec3_scale(direction, layer->cylinder.radius);
    vec3_add(position, direction);
  }
}

void lovrLayerSetPose(Layer* layer, float* position, float* orientation) {
  if (layer->curve == 0.f) {
    memcpy(&layer->quad.pose.position.x, position, 3 * sizeof(float));
    memcpy(&layer->quad.pose.orientation.x, orientation, 4 * sizeof(float));
  } else {
    memcpy(&layer->cylinder.pose.position.x, position, 3 * sizeof(float));
    memcpy(&layer->cylinder.pose.orientation.x, orientation, 4 * sizeof(float));
    float direction[3] = { 0.f, 0.f, 1.f };
    quat_rotate(orientation, direction);
    vec3_scale(direction, layer->cylinder.radius);
    vec3_add(&layer->cylinder.pose.position.x, direction);
  }
}

void lovrLayerGetDimensions(Layer* layer, float* width, float* height) {
  if (layer->curve == 0.f) {
    *width = layer->quad.size.width;
    *height = layer->quad.size.height;
  } else {
    *width = layer->cylinder.radius * layer->cylinder.centralAngle;
    *height = layer->cylinder.radius * layer->cylinder.centralAngle / layer->cylinder.aspectRatio;
  }
}

void lovrLayerSetDimensions(Layer* layer, float width, float height) {
  if (layer->curve == 0.f) {
    layer->quad.size.width = width;
    layer->quad.size.height = height;
  } else {
    layer->cylinder.centralAngle = width / layer->cylinder.radius;
    layer->cylinder.aspectRatio = width / height;
  }
}

float lovrLayerGetCurve(Layer* layer) {
  return layer->curve;
}

bool lovrLayerSetCurve(Layer* layer, float curve) {
  if (!state.extensions.layerCurve) return true;
  if (curve < 1e-3) curve = 0.f;

  XrPosef quadPose;
  lovrLayerGetPose(layer, &quadPose.position.x, &quadPose.orientation.x);

  float width, height;
  lovrLayerGetDimensions(layer, &width, &height);

  bool wasCylinder = layer->curve > 0.f;
  layer->curve = curve;

  if (curve > 0.f) {
    if (!wasCylinder) {
      layer->cylinder = (XrCompositionLayerCylinderKHR) {
        .type = XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR,
        .layerFlags = layer->quad.layerFlags,
        .eyeVisibility = layer->quad.eyeVisibility,
        .subImage = layer->quad.subImage,
        .aspectRatio = width / height
      };
    }

    float minRadius = width / (2.f * (float) M_PI);
    layer->cylinder.radius = MAX(1.f / curve, minRadius);
    layer->cylinder.centralAngle = width / layer->cylinder.radius;
    lovrLayerSetPose(layer, &quadPose.position.x, &quadPose.orientation.x);
  } else if (wasCylinder) {
    layer->quad = (XrCompositionLayerQuad) {
      .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
      .layerFlags = layer->cylinder.layerFlags,
      .eyeVisibility = layer->cylinder.eyeVisibility,
      .subImage = layer->cylinder.subImage,
      .pose = quadPose,
      .size.width = layer->cylinder.radius * layer->cylinder.centralAngle,
      .size.height = layer->cylinder.radius * layer->cylinder.centralAngle / layer->cylinder.aspectRatio
    };
  }

  return true;
}

void lovrLayerGetColor(Layer* layer, float color[4]) {
  color[0] = lovrMathLinearToGamma(layer->color.colorScale.r);
  color[1] = lovrMathLinearToGamma(layer->color.colorScale.g);
  color[2] = lovrMathLinearToGamma(layer->color.colorScale.b);
  color[3] = layer->color.colorScale.a;
}

void lovrLayerSetColor(Layer* layer, float color[4]) {
  layer->color.colorScale.r = lovrMathGammaToLinear(color[0]);
  layer->color.colorScale.g = lovrMathGammaToLinear(color[1]);
  layer->color.colorScale.b = lovrMathGammaToLinear(color[2]);
  layer->color.colorScale.a = color[3];
}

void lovrLayerGetViewport(Layer* layer, int32_t* viewport) {
  viewport[0] = layer->quad.subImage.imageRect.offset.x;
  viewport[1] = layer->quad.subImage.imageRect.offset.y;
  viewport[2] = layer->quad.subImage.imageRect.extent.width;
  viewport[3] = layer->quad.subImage.imageRect.extent.height;
}

void lovrLayerSetViewport(Layer* layer, int32_t* viewport) {
  XrSwapchainSubImage* subimage = layer->curve == 0.f ? &layer->quad.subImage : &layer->cylinder.subImage;
  subimage->imageRect.offset.x = viewport[0];
  subimage->imageRect.offset.y = viewport[1];
  subimage->imageRect.extent.width = viewport[2] ? viewport[2] : layer->info.width - viewport[0];
  subimage->imageRect.extent.height = viewport[3] ? viewport[3] : layer->info.height - viewport[1];
}

Texture* lovrLayerGetTexture(Layer* layer) {
  return lovrSwapchainAcquire(&layer->swapchain);
}

Pass* lovrLayerGetPass(Layer* layer) {
  Texture* texture = lovrLayerGetTexture(layer);
  if (!texture) return NULL;

  if (!layer->pass) {
    if ((layer->pass = lovrPassCreate(NULL)) == NULL) {
      return NULL;
    }

    float background[4][4] = { 0 };
    LoadAction loads[4] = { LOAD_CLEAR };
    lovrPassSetClear(layer->pass, loads, background, LOAD_CLEAR, 0.f);
  }

  Canvas canvas = {
    .color[0].texture = texture,
    .depthFormat = state.depthFormat,
    .samples = state.config.antialias ? 4 : 1
  };

  if (!lovrPassSetCanvas(layer->pass, &canvas)) {
    return NULL;
  }

  float viewMatrix[16] = MAT4_IDENTITY;
  float projection[16];
  mat4_orthographic(projection, 0, layer->info.width, 0, layer->info.height, -1.f, 1.f);

  for (uint32_t i = 0; i < 1u << layer->info.stereo; i++) {
    lovrPassSetViewMatrix(layer->pass, i, viewMatrix);
    lovrPassSetProjection(layer->pass, i, projection);
  }

  return layer->pass;
}

// Private

void lovrHeadsetGetVulkanPhysicalDevice(void* instance, uintptr_t physicalDevice) {
  XrVulkanGraphicsDeviceGetInfoKHR info = {
    .type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
    .systemId = state.system,
    .vulkanInstance = (VkInstance) instance
  };

  XrResult result = xrGetVulkanGraphicsDevice2KHR(state.instance, &info, (VkPhysicalDevice*) physicalDevice);

  if (XR_FAILED(result)) {
    *((VkPhysicalDevice*) physicalDevice) = 0;
  }
}

uint32_t lovrHeadsetCreateVulkanInstance(void* instanceCreateInfo, void* allocator, uintptr_t instance, void* getInstanceProcAddr) {
  XrVulkanInstanceCreateInfoKHR info = {
    .type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
    .systemId = state.system,
    .pfnGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) getInstanceProcAddr,
    .vulkanCreateInfo = instanceCreateInfo,
    .vulkanAllocator = allocator
  };

  VkResult vkResult;
  XrResult result = xrCreateVulkanInstanceKHR(state.instance, &info, (VkInstance*) instance, &vkResult);

  if (XR_FAILED(result)) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  return vkResult;
}

uint32_t lovrHeadsetCreateVulkanDevice(void* instance, void* deviceCreateInfo, void* allocator, uintptr_t device, void* getInstanceProcAddr) {
  XrVulkanDeviceCreateInfoKHR info = {
    .type = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
    .systemId = state.system,
    .pfnGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) getInstanceProcAddr,
    .vulkanPhysicalDevice = (VkPhysicalDevice) gpu_vk_get_physical_device(),
    .vulkanCreateInfo = deviceCreateInfo,
    .vulkanAllocator = allocator
  };

  VkResult result;
  XR(xrCreateVulkanDeviceKHR(state.instance, &info, (VkDevice*) device, &result), "xrCreateVulkanDeviceKHR");
  return result;
}

double lovrHeadsetGetDisplayTime(void) {
  return SESSION_RUNNING(state.sessionState) ? (state.frameState.predictedDisplayTime - state.epoch) / 1e9 : 0.;
}

double lovrHeadsetGetDisplayPeriod(void) {
  return SESSION_RUNNING(state.sessionState) ? state.frameState.predictedDisplayPeriod : 0.;
}

double lovrHeadsetGetDeltaTime(void) {
  return SESSION_RUNNING(state.sessionState) ? (state.frameState.predictedDisplayTime - state.lastDisplayTime) / 1e9 : 0.;
}

// Helpers

static bool lovrSwapchainInit(Swapchain* swapchain, uint32_t width, uint32_t height, uint32_t flags) {
  bool view = flags & VIEW;
  bool stereo = flags & STEREO;
  bool depth = flags & DEPTH;
  bool cube = flags & CUBE;
  bool immutable = flags & STATIC;
  bool foveated = flags & FOVEATED && state.extensions.foveation;

  XrSwapchainCreateInfo info = {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .createFlags = immutable ? XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT : 0,
    .width = width,
    .height = height,
    .sampleCount = 1,
    .faceCount = cube ? 6 : 1,
    .arraySize = view ? state.viewCount : 1 << stereo,
    .mipCount = 1
  };

  XrSwapchainCreateInfoFoveationFB foveation = {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB,
#ifdef LOVR_VK
    .flags = XR_SWAPCHAIN_CREATE_FOVEATION_FRAGMENT_DENSITY_MAP_BIT_FB
#endif
  };

  if (foveated) {
    info.next = &foveation;
  }

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
    info.usageFlags |= XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    info.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    info.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    info.format = VK_FORMAT_R8G8B8A8_SRGB;
  }

  XR(xrCreateSwapchain(state.session, &info, &swapchain->handle), "xrCreateSwapchain");

#ifdef LOVR_VK
  XrSwapchainImageFoveationVulkanFB foveationImages[MAX_IMAGES];
  XrSwapchainImageVulkanKHR images[MAX_IMAGES];
  for (uint32_t i = 0; i < MAX_IMAGES; i++) {
    images[i] = (XrSwapchainImageVulkanKHR) { .type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR };

    if (foveated) {
      foveationImages[i] = (XrSwapchainImageFoveationVulkanFB) { .type = XR_TYPE_SWAPCHAIN_IMAGE_FOVEATION_VULKAN_FB };
      images[i].next = &foveationImages[i];
    }
  }
#else
#error "Unsupported graphics backend"
#endif

  uint32_t textureCount = 0;
  XR(xrEnumerateSwapchainImages(swapchain->handle, MAX_IMAGES, &textureCount, (XrSwapchainImageBaseHeader*) images), "xrEnumerateSwapchainImages");

  for (uint32_t i = 0; i < textureCount; i++, swapchain->textureCount++) {
    swapchain->textures[i] = lovrTextureCreate(&(TextureInfo) {
      .type = cube ? TEXTURE_CUBE : (stereo || view ? TEXTURE_ARRAY : TEXTURE_2D),
      .format = depth ? state.depthFormat : FORMAT_RGBA8,
      .srgb = !depth,
      .width = width,
      .height = height,
      .layers = view ? state.viewCount : ((cube ? 6 : 1) << stereo),
      .usage = TEXTURE_RENDER | TEXTURE_TRANSFER | (depth ? 0 : TEXTURE_SAMPLE),
      .handle = (uintptr_t) images[i].image,
      .label = "OpenXR Swapchain",
      .xr = true
    });

    if (!swapchain->textures[i]) {
      lovrSwapchainDestroy(swapchain);
      return false;
    }

#ifdef LOVR_VK
    if (foveated) {
      swapchain->foveationTextures[i] = lovrTextureCreate(&(TextureInfo) {
        .type = stereo || view ? TEXTURE_ARRAY : TEXTURE_2D,
        .format = FORMAT_RG8,
        .width = foveationImages[i].width,
        .height = foveationImages[i].height,
        .layers = state.viewCount,
        .usage = TEXTURE_FOVEATION,
        .handle = (uintptr_t) foveationImages[i].image,
        .label = "OpenXR Foveation Texture"
      });
    }
#endif
  }

  swapchain->immutable = immutable;

  return true;
}

static void lovrSwapchainDestroy(Swapchain* swapchain) {
  if (!swapchain->handle) return;
  for (uint32_t i = 0; i < swapchain->textureCount; i++) {
    lovrRelease(swapchain->textures[i], lovrTextureDestroy);
  }
  swapchain->textureCount = 0;
  xrDestroySwapchain(swapchain->handle);
  swapchain->handle = XR_NULL_HANDLE;
}

static Texture* lovrSwapchainAcquire(Swapchain* swapchain) {
  if (!swapchain->acquired) {
    if (swapchain->immutable && swapchain->textureIndex == ~0u) {
      lovrSetError("Static Layers can only be updated once");
      return NULL;
    }

    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .timeout = XR_INFINITE_DURATION };
    XR(xrAcquireSwapchainImage(swapchain->handle, NULL, &swapchain->textureIndex), "xrAcquireSwapchainImage");

    for (;;) {
      XrResult result = xrWaitSwapchainImage(swapchain->handle, &waitInfo);
      if (XR_FAILED(result)) {
        lovrLog(LOG_WARN, "XR", "OpenXR failed to wait on swapchain image (%d)", result);
      } else {
        swapchain->acquired = true;
        break;
      }
    }
  }

  return swapchain->textures[swapchain->textureIndex];
}

static bool lovrSwapchainRelease(Swapchain* swapchain) {
  if (swapchain->handle && swapchain->acquired) {
    XR(xrReleaseSwapchainImage(swapchain->handle, NULL), "xrReleaseSwapchainImage");
    swapchain->textureIndex = ~0u; // Mark as released, for immutable swapchains >.>
    swapchain->acquired = false;
  }
  return true;
}

static void disconnect(void) {
  lovrHeadsetStop();
  if (state.actionSet) xrDestroyActionSet(state.actionSet);
  if (state.messenger) xrDestroyDebugUtilsMessengerEXT(state.messenger);
  if (state.instance) xrDestroyInstance(state.instance);
  state.actionSet = XR_NULL_HANDLE;
  state.messenger = XR_NULL_HANDLE;
  state.system = XR_NULL_SYSTEM_ID;
  state.instance = XR_NULL_HANDLE;
}

static void xrthrow(XrResult result, const char* symbol) {
  char name[XR_MAX_RESULT_STRING_SIZE];
  if (state.instance && XR_SUCCEEDED(xrResultToString(state.instance, result, name))) {
    lovrSetError("OpenXR Error: %s returned %s", symbol, name);
  } else {
    lovrSetError("OpenXR Error: %s returned %d", symbol, result);
  }
}

static XrBool32 onMessage(XrDebugUtilsMessageSeverityFlagsEXT severity, XrDebugUtilsMessageTypeFlagsEXT type, const XrDebugUtilsMessengerCallbackDataEXT* data, void* userdata) {
  int level = LOG_DEBUG;
  if (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) level = LOG_DEBUG;
  if (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) level = LOG_INFO;
  if (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) level = LOG_WARN;
  if (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) level = LOG_ERROR;
  if (data->functionName) {
    lovrLog(level, "XR", "%s: %s", data->functionName, data->message);
  } else {
    lovrLog(level, "XR", "%s", data->message);
  }
  return XR_FALSE;
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
  XR(xrConvertWin32PerformanceCounterToTimeKHR(state.instance, &t, &time), "xrConvertWin32PerformanceCounterToTimeKHR");
#else
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  XR(xrConvertTimespecTimeToTimeKHR(state.instance, &t, &time), "xrConvertTimespecTimeToTimeKHR");
#endif
  return time;
}

static bool createReferenceSpace(XrTime time) {
  if (time <= 0) {
    return false;
  }

  XrReferenceSpaceCreateInfo info = {
    .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
    .poseInReferenceSpace = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } }
  };

  // Reference space doesn't need to be recreated for seated experiences (those always use local
  // space), or when local-floor is supported.  Otherwise, vertical offset must be re-measured.
  if (state.referenceSpace && (state.extensions.localFloor || state.config.seated)) {
    return true;
  }

  if (state.config.seated) {
    info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  } else if (state.extensions.localFloor) {
    info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
  } else if (state.spaces[DEVICE_FLOOR]) {
    XrSpace local;
    info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    XR(xrCreateReferenceSpace(state.session, &info, &local), "xrCreateReferenceSpace");

    XrSpaceLocation location = { .type = XR_TYPE_SPACE_LOCATION };
    XR(xrLocateSpace(state.spaces[DEVICE_FLOOR], local, time, &location), "xrLocateSpace");
    xrDestroySpace(local);

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
    xrDestroySpace(state.referenceSpace);
  }

  XR(xrCreateReferenceSpace(state.session, &info, &state.referenceSpace), "xrCreateReferenceSpace");
  return true;
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
    case DEVICE_HAND_LEFT_POINT:
    case DEVICE_HAND_RIGHT_POINT:
      return state.actions[ACTION_POINTER_POSE];
    case DEVICE_HAND_LEFT_PINCH:
    case DEVICE_HAND_RIGHT_PINCH:
      return state.extensions.handInteraction ? state.actions[ACTION_PINCH_POSE] : XR_NULL_HANDLE;
    case DEVICE_HAND_LEFT_POKE:
    case DEVICE_HAND_RIGHT_POKE:
      return state.extensions.handInteraction ? state.actions[ACTION_POKE_POSE] : XR_NULL_HANDLE;
    case DEVICE_HAND_LEFT_PALM:
    case DEVICE_HAND_RIGHT_PALM:
      return state.extensions.palmPose ? state.actions[ACTION_PALM_POSE] : XR_NULL_HANDLE;
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
      return state.extensions.viveTrackers ? state.actions[ACTION_TRACKER_POSE] : XR_NULL_HANDLE;
    case DEVICE_STYLUS:
      return state.extensions.mxInk ? state.actions[ACTION_STYLUS_POSE] : XR_NULL_HANDLE;
    case DEVICE_EYE_GAZE:
      return state.extensions.gaze ? state.actions[ACTION_GAZE_POSE] : XR_NULL_HANDLE;
    default:
      return XR_NULL_HANDLE;
  }
}

// Hand trackers are created lazily because on some implementations xrCreateHandTrackerEXT will
// return XR_ERROR_FEATURE_UNSUPPORTED if called too early.
static XrHandTrackerEXT getHandTracker(Device device) {
  if (!state.session || !state.extensions.handTracking || (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT)) {
    return XR_NULL_HANDLE;
  }

  XrHandTrackerEXT* tracker = &state.handTrackers[device == DEVICE_HAND_RIGHT];

  if (!*tracker) {
    XrHandTrackerCreateInfoEXT info = {
      .type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
      .handJointSet = state.extensions.handTrackingElbow ?
        XR_HAND_JOINT_SET_HAND_WITH_FOREARM_ULTRALEAP :
        XR_HAND_JOINT_SET_DEFAULT_EXT,
      .hand = device == DEVICE_HAND_RIGHT ? XR_HAND_RIGHT_EXT : XR_HAND_LEFT_EXT
    };

    XrHandTrackingDataSourceInfoEXT sourceInfo = {
      .type = XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT,
      .requestedDataSourceCount = state.config.controllerSkeleton == SKELETON_NONE ? 1 : 2,
      .requestedDataSources = (XrHandTrackingDataSourceEXT[2]) {
        XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT,
        XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT
      }
    };

    if (state.extensions.handTrackingDataSource) {
      sourceInfo.next = info.next;
      info.next = &sourceInfo;
    }

    if (XR_FAILED(xrCreateHandTrackerEXT(state.session, &info, tracker))) {
      return XR_NULL_HANDLE;
    }
  }

  return *tracker;
}

static XrBodyTrackerBD getBodyTracker(void) {
  return state.bodyTracker;
}

static bool loadControllerModels(void) {
  uint32_t count = 0;
  XrInteractionRenderModelIdsEnumerateInfoEXT enumerateInfo = { .type = XR_TYPE_INTERACTION_RENDER_MODEL_IDS_ENUMERATE_INFO_EXT };
  XR(xrEnumerateInteractionRenderModelIdsEXT(state.session, &enumerateInfo, 0, &count, NULL), "xrEnumerateInteractionRenderModelIdsEXT");
  XrRenderModelIdEXT* keys = lovrMalloc(count * sizeof(XrRenderModelIdEXT));
  XR(xrEnumerateInteractionRenderModelIdsEXT(state.session, &enumerateInfo, count, &count, keys), "xrEnumerateInteractionRenderModelIdsEXT");

  mtx_lock(&state.modelLock);

  // Destroy models that were removed
  for (uint32_t i = 0; i < state.modelCount; i++) {
    bool destroy = true;

    for (uint32_t j = 0; j < count; j++) {
      if (state.modelKeys[i] == keys[j]) {
        destroy = false;
        break;
      }
    }

    if (destroy) {
      xrDestroyRenderModelEXT(state.models[i].handle);
      xrDestroySpace(state.models[i].space);
      lovrFree(state.models[i].nodeStates);
      lovrFree(state.models[i].nodes);
      state.models[i] = state.models[state.modelCount - 1];
      state.modelKeys[i] = state.modelKeys[state.modelCount - 1];
      state.modelCount--;
      i--;
    }
  }

  if (count == 0) {
    lovrFree(state.models);
    lovrFree(state.modelKeys);
    lovrFree(keys);
    state.models = NULL;
    state.modelKeys = NULL;
    mtx_unlock(&state.modelLock);
    return true;
  }

  state.models = lovrRealloc(state.models, count * sizeof(RenderModel));
  state.modelKeys = lovrRealloc(state.modelKeys, count * sizeof(XrRenderModelIdEXT));

  // Add new models
  for (uint32_t i = 0; i < count; i++) {
    bool found = false;

    for (uint32_t j = 0; j < state.modelCount; j++) {
      if (keys[i] == state.modelKeys[j]) {
        found = true;
        break;
      }
    }

    if (!found) {
      RenderModel* model = &state.models[state.modelCount];
      state.modelKeys[state.modelCount] = keys[i];
      state.modelCount++;

      const char* gltfExtensions[] = {
        "KHR_texture_transform"
      };

      XrRenderModelCreateInfoEXT createInfo = {
        .type = XR_TYPE_RENDER_MODEL_CREATE_INFO_EXT,
        .renderModelId = keys[i],
        .gltfExtensionCount = COUNTOF(gltfExtensions),
        .gltfExtensions = gltfExtensions
      };

      XRG(xrCreateRenderModelEXT(state.session, &createInfo, &model->handle), "xrCreateRenderModelEXT", fail);

      XrRenderModelPropertiesGetInfoEXT info = { .type = XR_TYPE_RENDER_MODEL_PROPERTIES_GET_INFO_EXT };
      XRG(xrGetRenderModelPropertiesEXT(model->handle, &info, &model->properties), "xrGetRenderModelPropertiesEXT", fail);

      model->nodes = lovrMalloc(model->properties.animatableNodeCount * sizeof(uint32_t));
      model->nodeStates = lovrMalloc(model->properties.animatableNodeCount * sizeof(XrRenderModelNodeStateEXT));
      memset(model->nodes, 0xff, model->properties.animatableNodeCount * sizeof(uint32_t));

      XrRenderModelSpaceCreateInfoEXT spaceInfo = {
        .type = XR_TYPE_RENDER_MODEL_SPACE_CREATE_INFO_EXT,
        .renderModel = model->handle
      };

      XRG(xrCreateRenderModelSpaceEXT(state.session, &spaceInfo, &model->space), "xrCreateRenderModelSpaceEXT", fail);
    }
  }

  mtx_unlock(&state.modelLock);
  lovrFree(keys);
  return true;
fail:
  mtx_unlock(&state.modelLock);
  return false;
}

static bool loadVisibilityMask(void) {
  XrViewConfigurationType viewConfig = state.viewConfiguration;
  XrVisibilityMaskTypeKHR type = XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR;
  XrVisibilityMaskKHR info = { .type = XR_TYPE_VISIBILITY_MASK_KHR };

  uint32_t vertexCount = 0;
  uint32_t indexCount = 0;

  for (uint32_t i = 0; i < state.viewCount; i++) {
    XR(xrGetVisibilityMaskKHR(state.session, viewConfig, i, type, &info), "xrGetVisibilityMask");
    lovrCheck(UINT32_MAX - vertexCount >= info.vertexCountOutput, "Too many mask vertices");
    lovrCheck(UINT32_MAX - indexCount >= info.indexCountOutput, "Too many mask indices");
    vertexCount += info.vertexCountOutput;
    indexCount += info.indexCountOutput;
  }

  if (!state.mask || lovrMeshGetVertexFormat(state.mask)->length < vertexCount) {
    state.mask = lovrMeshCreate(&(MeshInfo) {
      .vertexFormat = &(DataField) {
        .name = "VertexPosition",
        .type = TYPE_F32x3,
        .length = vertexCount,
        .stride = 12
      }
    }, NULL);

    if (!state.mask) {
      return false;
    }
  }

  float* vertices = lovrMeshSetVertices(state.mask, 0, vertexCount);
  info.vertices = (void*) vertices;
  info.indices = lovrMeshSetIndices(state.mask, indexCount, TYPE_U32);
  info.vertexCapacityInput = vertexCount;
  info.indexCapacityInput = indexCount;

  if (!info.vertices || !info.indices) {
    lovrRelease(state.mask, lovrMeshDestroy);
    state.mask = NULL;
    return false;
  }

  uint32_t baseVertex = 0;
  for (uint32_t i = 0; i < state.viewCount; i++) {
    if (XR_FAILED(xrGetVisibilityMaskKHR(state.session, viewConfig, i, type, &info))) {
      lovrRelease(state.mask, lovrMeshDestroy);
      state.mask = NULL;
      return false;
    }

    info.vertexCapacityInput -= info.vertexCountOutput;
    info.indexCapacityInput -= info.indexCountOutput;

    // Each vertex from OpenXR is 2 floats, but we store them as vec3 with z == view index
    for (uint32_t j = 0; j < info.vertexCountOutput; j++) {
      uint32_t index = info.vertexCountOutput - j - 1;
      float* dst = vertices + 3 * index;
      float* src = vertices + 2 * index;
      dst[2] = (float) i;
      dst[1] = src[1];
      dst[0] = src[0];
    }

    if (i > 0) {
      for (uint32_t j = 0; j < info.indexCountOutput; j++) {
        info.indices[j] += baseVertex;
      }
    }

    baseVertex += info.vertexCountOutput;
    vertices += 3 * info.vertexCountOutput;
    info.vertices = (void*) vertices;
    info.indices += info.indexCountOutput;
  }

  lovrMeshSetDrawRange(state.mask, 0, indexCount);

  return true;
}

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

EXPORT uintptr_t xr_get_instance(void) {
  return (uintptr_t) state.instance;
}

EXPORT uintptr_t xr_get_system(void) {
  return (uintptr_t) state.system;
}

EXPORT uintptr_t xr_get_session(void) {
  return (uintptr_t) state.session;
}
