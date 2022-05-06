#include "headset/headset.h"
#include "data/blob.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "graphics/model.h"
#include "graphics/texture.h"
#include "core/os.h"
#include "util.h"
#include <assert.h>
#include <stdlib.h>
#include <math.h>

#if defined(_WIN32)
#define XR_USE_PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <unknwn.h>
#include <windows.h>
#elif defined(__ANDROID__)
#define XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <jni.h>
#elif defined(LOVR_LINUX_X11)
#define XR_USE_PLATFORM_XLIB
typedef unsigned long XID;
typedef struct Display Display;
typedef XID GLXFBConfig;
typedef XID GLXDrawable;
typedef XID GLXContext;
#elif defined(LOVR_LINUX_EGL)
#define XR_USE_PLATFORM_EGL
#define EGL_NO_X11
#include <EGL/egl.h>
#endif

#if defined(LOVR_GL)
#define XR_USE_GRAPHICS_API_OPENGL
#define GRAPHICS_EXTENSION "XR_KHR_opengl_enable"
#elif defined(LOVR_GLES)
#define XR_USE_GRAPHICS_API_OPENGL_ES
#define GRAPHICS_EXTENSION "XR_KHR_opengl_es_enable"
#endif

#define XR_NO_PROTOTYPES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#define XR(f) handleResult(f, __FILE__, __LINE__)
#define XR_INIT(f) if (XR_FAILED(f)) return openxr_destroy(), false;
#define SESSION_ACTIVE(s) (s >= XR_SESSION_STATE_READY && s <= XR_SESSION_STATE_FOCUSED)
#define GL_SRGB8_ALPHA8 0x8C43
#define MAX_IMAGES 4

#if defined(_WIN32)
HANDLE os_get_win32_window(void);
HGLRC os_get_win32_context(void);
#elif defined(__ANDROID__)
struct ANativeActivity* os_get_activity(void);
EGLDisplay os_get_egl_display(void);
EGLContext os_get_egl_context(void);
EGLConfig os_get_egl_config(void);
#elif defined(LOVR_LINUX_X11)
Display* os_get_x11_display(void);
GLXDrawable os_get_glx_drawable(void);
GLXContext os_get_glx_context(void);
#elif defined(LOVR_LINUX_EGL)
PFNEGLGETPROCADDRESSPROC os_get_egl_proc_addr(void);
EGLDisplay os_get_egl_display(void);
EGLContext os_get_egl_context(void);
EGLConfig os_get_egl_config(void);
#endif

#define XR_FOREACH(X)\
  X(xrDestroyInstance)\
  X(xrPollEvent)\
  X(xrResultToString)\
  X(xrGetSystem)\
  X(xrGetSystemProperties)\
  X(xrCreateSession)\
  X(xrDestroySession)\
  X(xrCreateReferenceSpace)\
  X(xrGetReferenceSpaceBoundsRect)\
  X(xrCreateActionSpace)\
  X(xrLocateSpace)\
  X(xrDestroySpace)\
  X(xrEnumerateViewConfigurations)\
  X(xrEnumerateViewConfigurationViews)\
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
  X(xrCreateHandTrackerEXT)\
  X(xrDestroyHandTrackerEXT)\
  X(xrLocateHandJointsEXT)\
  X(xrGetHandMeshFB)\
  X(xrGetDisplayRefreshRateFB) \
  X(xrEnumerateDisplayRefreshRatesFB) \
  X(xrRequestDisplayRefreshRateFB)

#define XR_DECLARE(fn) static PFN_##fn fn;
#define XR_LOAD(fn) xrGetInstanceProcAddr(state.instance, #fn, (PFN_xrVoidFunction*) &fn);
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function);
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char* layerName, uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrExtensionProperties* properties);
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo* createInfo, XrInstance* instance);
XR_FOREACH(XR_DECLARE)

enum {
  ACTION_HAND_POSE,
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

static struct {
  XrInstance instance;
  XrSystemId system;
  XrSession session;
  XrSessionState sessionState;
  XrSpace referenceSpace;
  XrReferenceSpaceType referenceSpaceType;
  XrSpace spaces[MAX_DEVICES];
  XrSwapchain swapchain;
  XrCompositionLayerProjection layers[1];
  XrCompositionLayerProjectionView layerViews[2];
  XrFrameState frameState;
  Canvas* canvases[MAX_IMAGES];
  double lastDisplayTime;
  uint32_t imageIndex;
  uint32_t imageCount;
  uint32_t msaa;
  uint32_t width;
  uint32_t height;
  float clipNear;
  float clipFar;
  float offset;
  bool waited;
  bool hasImage;
  XrActionSet actionSet;
  XrAction actions[MAX_ACTIONS];
  XrPath actionFilters[MAX_DEVICES];
  XrHandTrackerEXT handTrackers[2];
  struct {
    bool gaze;
    bool handTracking;
    bool handTrackingAim;
    bool handTrackingMesh;
    bool overlay;
    bool refreshRate;
    bool viveTrackers;
  } features;
} state;

static XrResult handleResult(XrResult result, const char* file, int line) {
  if (XR_FAILED(result)) {
    char message[XR_MAX_RESULT_STRING_SIZE];
    xrResultToString(state.instance, result, message);
    lovrThrow("OpenXR Error: %s at %s:%d", message, file, line);
  }
  return result;
}

static bool hasExtension(XrExtensionProperties* extensions, uint32_t count, const char* extension) {
  for (uint32_t i = 0; i < count; i++) {
    if (!strcmp(extensions[i].extensionName, extension)) {
      return true;
    }
  }
  return false;
}

static XrAction getPoseActionForDevice(Device device) {
  switch (device) {
    case DEVICE_HEAD:
      return XR_NULL_HANDLE; // Uses reference space
    case DEVICE_HAND_LEFT:
    case DEVICE_HAND_RIGHT:
      return state.actions[ACTION_HAND_POSE];
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
      return state.actions[ACTION_TRACKER_POSE];
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
      .handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT,
      .hand = device == DEVICE_HAND_RIGHT ? XR_HAND_RIGHT_EXT : XR_HAND_LEFT_EXT
    };

    if (XR_FAILED(xrCreateHandTrackerEXT(state.session, &info, tracker))) {
      return XR_NULL_HANDLE;
    }
  }

  return *tracker;
}

static void openxr_destroy();

static bool openxr_init(float supersample, float offset, uint32_t msaa, bool overlay) {
  state.msaa = msaa;
  state.offset = offset;

#ifdef __ANDROID__
  static PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
  XR_LOAD(xrInitializeLoaderKHR);
  if (!xrInitializeLoaderKHR) {
    return false;
  }

  ANativeActivity* activity = os_get_activity();
  XrLoaderInitInfoAndroidKHR loaderInfo = {
    .type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,
    .applicationVM = activity->vm,
    .applicationContext = activity->clazz
  };

  if (XR_FAILED(xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*) &loaderInfo))) {
    return false;
  }
#endif

  { // Instance
    uint32_t extensionCount;
    XR_INIT(xrEnumerateInstanceExtensionProperties(NULL, 0, &extensionCount, NULL));
    XrExtensionProperties* extensionProperties = calloc(extensionCount, sizeof(*extensionProperties));
    lovrAssert(extensionProperties, "Out of memory");
    for (uint32_t i = 0; i < extensionCount; i++) extensionProperties[i].type = XR_TYPE_EXTENSION_PROPERTIES;
    xrEnumerateInstanceExtensionProperties(NULL, extensionCount, &extensionCount, extensionProperties);

    // Extensions without a feature are required
    struct { const char* name; bool* feature; bool disable; } extensions[] = {
#ifdef __ANDROID__
      { "XR_KHR_android_create_instance", NULL, false },
#endif
#ifdef LOVR_LINUX_EGL
      { "XR_MNDX_egl_enable", NULL, false },
#endif
      { GRAPHICS_EXTENSION, NULL, false },
      { "XR_EXT_eye_gaze_interaction", &state.features.gaze, false },
      { "XR_EXT_hand_tracking", &state.features.handTracking, false },
      { "XR_FB_display_refresh_rate", &state.features.refreshRate, false },
      { "XR_FB_hand_tracking_aim", &state.features.handTrackingAim, false },
      { "XR_FB_hand_tracking_mesh", &state.features.handTrackingMesh, false },
      { "XR_EXTX_overlay", &state.features.overlay, !overlay },
      { "XR_HTCX_vive_tracker_interaction", &state.features.viveTrackers, false },
    };

    uint32_t enabledExtensionCount = 0;
    const char* enabledExtensionNames[COUNTOF(extensions)];
    for (uint32_t i = 0; i < COUNTOF(extensions); i++) {
      if (extensions[i].disable) continue;
      if (!extensions[i].feature || hasExtension(extensionProperties, extensionCount, extensions[i].name)) {
        enabledExtensionNames[enabledExtensionCount++] = extensions[i].name;
        if (extensions[i].feature) *extensions[i].feature = true;
      }
    }

    free(extensionProperties);

    XrInstanceCreateInfo info = {
      .type = XR_TYPE_INSTANCE_CREATE_INFO,
#ifdef __ANDROID__
      .next = &(XrInstanceCreateInfoAndroidKHR) {
        .type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR,
        .applicationVM = activity->vm,
        .applicationActivity = activity->clazz
      },
#endif
      .applicationInfo.engineName = "LÖVR",
      .applicationInfo.engineVersion = (LOVR_VERSION_MAJOR << 24) + (LOVR_VERSION_MINOR << 16) + LOVR_VERSION_PATCH,
      .applicationInfo.applicationName = "LÖVR",
      .applicationInfo.applicationVersion = 0,
      .applicationInfo.apiVersion = XR_CURRENT_API_VERSION,
      .enabledExtensionCount = enabledExtensionCount,
      .enabledExtensionNames = enabledExtensionNames
    };

    XR_INIT(xrCreateInstance(&info, &state.instance));
    XR_FOREACH(XR_LOAD)
  }

  { // System
    XrSystemGetInfo info = {
      .type = XR_TYPE_SYSTEM_GET_INFO,
      .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY
    };

    XR_INIT(xrGetSystem(state.instance, &info, &state.system));

    XrSystemEyeGazeInteractionPropertiesEXT eyeGazeProperties = {
      .type = XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT,
      .supportsEyeGazeInteraction = false
    };

    XrSystemHandTrackingPropertiesEXT handTrackingProperties = {
      .type = XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT,
      .supportsHandTracking = false
    };

    XrSystemProperties properties = {
      .type = XR_TYPE_SYSTEM_PROPERTIES
    };

    if (state.features.gaze) {
      eyeGazeProperties.next = properties.next;
      properties.next = &eyeGazeProperties;
    }

    if (state.features.handTracking) {
      handTrackingProperties.next = properties.next;
      properties.next = &handTrackingProperties;
    }

    XR_INIT(xrGetSystemProperties(state.instance, state.system, &properties));
    state.features.gaze = eyeGazeProperties.supportsEyeGazeInteraction;
    state.features.handTracking = handTrackingProperties.supportsHandTracking;

    uint32_t viewConfigurationCount;
    XrViewConfigurationType viewConfigurations[2];
    XR_INIT(xrEnumerateViewConfigurations(state.instance, state.system, 2, &viewConfigurationCount, viewConfigurations));

    uint32_t viewCount;
    XrViewConfigurationView views[2] = { [0].type = XR_TYPE_VIEW_CONFIGURATION_VIEW, [1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW };
    XR_INIT(xrEnumerateViewConfigurationViews(state.instance, state.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, NULL));
    XR_INIT(xrEnumerateViewConfigurationViews(state.instance, state.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewCount, views));

    if ( // Only 2 views are supported, and since they're rendered together they must be identical
      viewCount != 2 ||
      views[0].recommendedSwapchainSampleCount != views[1].recommendedSwapchainSampleCount ||
      views[0].recommendedImageRectWidth != views[1].recommendedImageRectWidth ||
      views[0].recommendedImageRectHeight != views[1].recommendedImageRectHeight
    ) {
      openxr_destroy();
      return false;
    }

    state.width = MIN(views[0].recommendedImageRectWidth * supersample, views[0].maxImageRectWidth);
    state.height = MIN(views[0].recommendedImageRectHeight * supersample, views[0].maxImageRectHeight);
  }

  { // Actions
    XrActionSetCreateInfo info = {
      .type = XR_TYPE_ACTION_SET_CREATE_INFO,
      .localizedActionSetName = "Default",
      .actionSetName = "default"
    };

    XR_INIT(xrCreateActionSet(state.instance, &info, &state.actionSet));

    // Subaction paths, for filtering actions by device
    XR_INIT(xrStringToPath(state.instance, "/user/hand/left", &state.actionFilters[DEVICE_HAND_LEFT]));
    XR_INIT(xrStringToPath(state.instance, "/user/hand/right", &state.actionFilters[DEVICE_HAND_RIGHT]));

    state.actionFilters[DEVICE_HAND_LEFT_POINT] = state.actionFilters[DEVICE_HAND_LEFT];
    state.actionFilters[DEVICE_HAND_RIGHT_POINT] = state.actionFilters[DEVICE_HAND_RIGHT];

    if (state.features.viveTrackers) {
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/left_elbow", &state.actionFilters[DEVICE_ELBOW_LEFT]));
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/right_elbow", &state.actionFilters[DEVICE_ELBOW_RIGHT]));
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/left_shoulder", &state.actionFilters[DEVICE_SHOULDER_LEFT]));
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/right_shoulder", &state.actionFilters[DEVICE_SHOULDER_RIGHT]));
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/chest", &state.actionFilters[DEVICE_CHEST]));
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/waist", &state.actionFilters[DEVICE_WAIST]));
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/left_knee", &state.actionFilters[DEVICE_KNEE_LEFT]));
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/right_knee", &state.actionFilters[DEVICE_KNEE_RIGHT]));
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/left_foot", &state.actionFilters[DEVICE_FOOT_LEFT]));
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/right_foot", &state.actionFilters[DEVICE_FOOT_RIGHT]));
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/camera", &state.actionFilters[DEVICE_CAMERA]));
      XR_INIT(xrStringToPath(state.instance, "/user/vive_tracker_htcx/role/keyboard", &state.actionFilters[DEVICE_KEYBOARD]));
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
      { 0, NULL, "hand_pose",        XR_ACTION_TYPE_POSE_INPUT,       2, hands, "Hand Pose" },
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
      XR_INIT(xrCreateAction(state.actionSet, &actionInfo[i], &state.actions[i]));
    }

    enum {
      PROFILE_SIMPLE,
      PROFILE_VIVE,
      PROFILE_TOUCH,
      PROFILE_GO,
      PROFILE_INDEX,
      PROFILE_WMR,
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
      [PROFILE_TRACKER] = "/interaction_profiles/htc/vive_tracker_htcx",
      [PROFILE_GAZE] = "/interaction_profiles/ext/eye_gaze_interaction"
    };

    typedef struct {
      int action;
      const char* path;
    } Binding;

    Binding* bindings[] = {
      [PROFILE_SIMPLE] = (Binding[]) {
        { ACTION_HAND_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_HAND_POSE, "/user/hand/right/input/grip/pose" },
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
        { ACTION_HAND_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_HAND_POSE, "/user/hand/right/input/grip/pose" },
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
        { ACTION_HAND_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_HAND_POSE, "/user/hand/right/input/grip/pose" },
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
        { ACTION_HAND_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_HAND_POSE, "/user/hand/right/input/grip/pose" },
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
        { ACTION_HAND_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_HAND_POSE, "/user/hand/right/input/grip/pose" },
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
        { ACTION_HAND_POSE, "/user/hand/left/input/grip/pose" },
        { ACTION_HAND_POSE, "/user/hand/right/input/grip/pose" },
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
    if (!state.features.viveTrackers) {
      bindings[PROFILE_TRACKER][0].path = NULL;
    }

    if (!state.features.gaze) {
      bindings[PROFILE_GAZE][0].path = NULL;
    }

    XrPath path;
    XrActionSuggestedBinding suggestedBindings[64];
    for (uint32_t i = 0, count = 0; i < MAX_PROFILES; i++, count = 0) {
      for (uint32_t j = 0; bindings[i][j].path; j++, count++) {
        XR_INIT(xrStringToPath(state.instance, bindings[i][j].path, &path));
        suggestedBindings[j].action = state.actions[bindings[i][j].action];
        suggestedBindings[j].binding = path;
      }

      if (count > 0) {
        XR_INIT(xrStringToPath(state.instance, interactionProfilePaths[i], &path));
        XR_INIT(xrSuggestInteractionProfileBindings(state.instance, &(XrInteractionProfileSuggestedBinding) {
          .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
          .interactionProfile = path,
          .countSuggestedBindings = count,
          .suggestedBindings = suggestedBindings
        }));
      }
    }
  }

  state.clipNear = .1f;
  state.clipFar = 100.f;
  state.frameState.type = XR_TYPE_FRAME_STATE;
  return true;
}

static void openxr_start(void) {
  { // Session
#if defined(LOVR_GL)
    XrGraphicsRequirementsOpenGLKHR requirements = { .type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR, NULL };
    PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR;
    XR_LOAD(xrGetOpenGLGraphicsRequirementsKHR);
    xrGetOpenGLGraphicsRequirementsKHR(state.instance, state.system, &requirements);
    // TODO validate OpenGL versions, potentially in init
#elif defined(LOVR_GLES)
    XrGraphicsRequirementsOpenGLESKHR requirements = { .type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR, NULL };
    PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR;
    XR_LOAD(xrGetOpenGLESGraphicsRequirementsKHR);
    xrGetOpenGLESGraphicsRequirementsKHR(state.instance, state.system, &requirements);
    // TODO validate OpenGLES versions, potentially in init
#endif

#if defined(_WIN32) && defined(LOVR_GL)
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding = {
      .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR,
      .hDC = GetDC(os_get_win32_window()),
      .hGLRC = os_get_win32_context()
    };
#elif defined(__ANDROID__) && defined(LOVR_GLES)
    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding = {
      .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR,
      .display = os_get_egl_display(),
      .config = os_get_egl_config(),
      .context = os_get_egl_context()
    };
#elif defined(LOVR_LINUX_X11)
    XrGraphicsBindingOpenGLXlibKHR graphicsBinding = {
      .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR,
      .next = NULL,
      .xDisplay = os_get_x11_display(),
      .visualid = 0,
      .glxFBConfig = 0,
      .glxDrawable = os_get_glx_drawable(),
      .glxContext = os_get_glx_context(),
    };
#elif defined(LOVR_LINUX_EGL)
    XrGraphicsBindingEGLMNDX graphicsBinding = {
      .type = XR_TYPE_GRAPHICS_BINDING_EGL_MNDX,
      .next = NULL,
      .getProcAddress = os_get_egl_proc_addr(),
      .display = os_get_egl_display(),
      .config = os_get_egl_config(),
      .context = os_get_egl_context(),
    };
#else
#error "Unsupported OpenXR platform/graphics combination"
#endif

    XrSessionCreateInfo info = {
      .type = XR_TYPE_SESSION_CREATE_INFO,
      .next = &graphicsBinding,
      .systemId = state.system
    };

#ifdef XR_EXTX_overlay
    XrSessionCreateInfoOverlayEXTX overlayInfo = {
      .type = XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX,
      .next = info.next
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

    XR(xrCreateSession(state.instance, &info, &state.session));
    XR(xrAttachSessionActionSets(state.session, &attachInfo));
  }

  { // Spaaace

    // Main reference space (can be stage or local)
    XrReferenceSpaceCreateInfo info = {
      .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
      .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE,
      .poseInReferenceSpace = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } }
    };

    if (XR_FAILED(xrCreateReferenceSpace(state.session, &info, &state.referenceSpace))) {
      info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
      info.poseInReferenceSpace.position.y = -state.offset;
      XR(xrCreateReferenceSpace(state.session, &info, &state.referenceSpace));
    }

    state.referenceSpaceType = info.referenceSpaceType;

    // Head space (for head pose)
    XrReferenceSpaceCreateInfo headSpaceInfo = {
      .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
      .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW,
      .poseInReferenceSpace = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } }
    };

    XR(xrCreateReferenceSpace(state.session, &headSpaceInfo, &state.spaces[DEVICE_HEAD]));

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

      XR(xrCreateActionSpace(state.session, &actionSpaceInfo, &state.spaces[i]));
    }
  }

  { // Swapchain
#if defined(XR_USE_GRAPHICS_API_OPENGL)
    TextureType textureType = TEXTURE_2D;
    uint32_t width = state.width * 2;
    uint32_t arraySize = 1;
    XrSwapchainImageOpenGLKHR images[MAX_IMAGES];
    for (uint32_t i = 0; i < MAX_IMAGES; i++) {
      images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
      images[i].next = NULL;
    }
#elif defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    TextureType textureType = TEXTURE_ARRAY;
    uint32_t width = state.width;
    uint32_t arraySize = 2;
    XrSwapchainImageOpenGLESKHR images[MAX_IMAGES];
    for (uint32_t i = 0; i < MAX_IMAGES; i++) {
      images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
      images[i].next = NULL;
    }
#endif

    XrSwapchainCreateInfo info = {
      .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
      .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
      .format = GL_SRGB8_ALPHA8,
      .width = width,
      .height = state.height,
      .sampleCount = 1,
      .faceCount = 1,
      .arraySize = arraySize,
      .mipCount = 1
    };

    XR(xrCreateSwapchain(state.session, &info, &state.swapchain));
    XR(xrEnumerateSwapchainImages(state.swapchain, MAX_IMAGES, &state.imageCount, (XrSwapchainImageBaseHeader*) images));

    CanvasFlags flags = { .depth = { true, false, FORMAT_D24S8 }, .stereo = true, .mipmaps = false, .msaa = state.msaa };
    for (uint32_t i = 0; i < state.imageCount; i++) {
      Texture* texture = lovrTextureCreateFromHandle(images[i].image, textureType, arraySize, state.msaa);
      state.canvases[i] = lovrCanvasCreate(state.width, state.height, flags);
      lovrCanvasSetAttachments(state.canvases[i], &(Attachment) { texture, 0, 0 }, 1);
      lovrRelease(texture, lovrTextureDestroy);
    }

    XrCompositionLayerFlags layerFlags = 0;

    if (state.features.overlay) {
      layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    }

    // Pre-init composition layer
    state.layers[0] = (XrCompositionLayerProjection) {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
      .space = state.referenceSpace,
      .layerFlags = layerFlags,
      .viewCount = 2,
      .views = state.layerViews
    };

    // Pre-init composition layer views
    state.layerViews[0] = (XrCompositionLayerProjectionView) {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
      .subImage = { state.swapchain, { { 0, 0 }, { state.width, state.height } }, 0 }
    };

    // Copy the left view to the right view and offset either the viewport or array index
    state.layerViews[1] = state.layerViews[0];
#if defined(XR_USE_GRAPHICS_API_OPENGL)
    state.layerViews[1].subImage.imageRect.offset.x += state.width;
#elif defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    state.layerViews[1].subImage.imageArrayIndex = 1;
#endif
  }

  os_window_set_vsync(0);
}

static void openxr_destroy(void) {
  for (uint32_t i = 0; i < state.imageCount; i++) {
    lovrRelease(state.canvases[i], lovrCanvasDestroy);
  }

  for (size_t i = 0; i < MAX_ACTIONS; i++) {
    if (state.actions[i]) {
      xrDestroyAction(state.actions[i]);
    }
  }

  for (size_t i = 0; i < MAX_DEVICES; i++) {
    if (state.spaces[i]) {
      xrDestroySpace(state.spaces[i]);
    }
  }

  if (state.handTrackers[0]) xrDestroyHandTrackerEXT(state.handTrackers[0]);
  if (state.handTrackers[1]) xrDestroyHandTrackerEXT(state.handTrackers[1]);
  if (state.actionSet) xrDestroyActionSet(state.actionSet);
  if (state.swapchain) xrDestroySwapchain(state.swapchain);
  if (state.referenceSpace) xrDestroySpace(state.referenceSpace);
  if (state.session) xrEndSession(state.session);
  if (state.instance) xrDestroyInstance(state.instance);
  memset(&state, 0, sizeof(state));
}

static bool openxr_getName(char* name, size_t length) {
  XrSystemProperties properties = { .type = XR_TYPE_SYSTEM_PROPERTIES };
  XR(xrGetSystemProperties(state.instance, state.system, &properties));
  strncpy(name, properties.systemName, length - 1);
  name[length - 1] = '\0';
  return true;
}

static HeadsetOrigin openxr_getOriginType(void) {
  return state.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE ? ORIGIN_FLOOR : ORIGIN_HEAD;
}

static void openxr_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  *width = state.width;
  *height = state.height;
}

static float openxr_getDisplayFrequency(void) {
  if (!state.features.refreshRate) return 0.f;
  float frequency;
  XR(xrGetDisplayRefreshRateFB(state.session, &frequency));
  return frequency;
}

static float* openxr_getDisplayFrequencies(uint32_t* count) {
  if (!state.features.refreshRate || !count) return NULL;
  XR(xrEnumerateDisplayRefreshRatesFB(state.session, 0, count, NULL));
  float *frequencies = malloc(*count * sizeof(float));
  lovrAssert(frequencies, "Out of Memory");
  XR(xrEnumerateDisplayRefreshRatesFB(state.session, *count, count, frequencies));
  return frequencies;
}

static bool openxr_setDisplayFrequency(float frequency) {
  if (!state.features.refreshRate) return false;
  XR(xrRequestDisplayRefreshRateFB(state.session, frequency));
  return true;
}
static double openxr_getDisplayTime(void) {
  return state.frameState.predictedDisplayTime / 1e9;
}

static double openxr_getDeltaTime(void) {
  return (state.frameState.predictedDisplayTime - state.lastDisplayTime) / 1e9;
}

static void getViews(XrView views[2], uint32_t* count) {
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
  XR(xrLocateViews(state.session, &viewLocateInfo, &viewState, 2, count, views));
}

static uint32_t openxr_getViewCount(void) {
  uint32_t count;
  XrView views[2];
  getViews(views, &count);
  return count;
}

static bool openxr_getViewPose(uint32_t view, float* position, float* orientation) {
  uint32_t count;
  XrView views[2];
  getViews(views, &count);
  if (view < count) {
    memcpy(position, &views[view].pose.position.x, 3 * sizeof(float));
    memcpy(orientation, &views[view].pose.orientation.x, 4 * sizeof(float));
    return true;
  } else {
    return false;
  }
}

static bool openxr_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  uint32_t count;
  XrView views[2];
  getViews(views, &count);
  if (view < count) {
    *left = views[view].fov.angleLeft;
    *right = views[view].fov.angleRight;
    *up = views[view].fov.angleUp;
    *down = views[view].fov.angleDown;
    return true;
  } else {
    return false;
  }
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
  if (XR_SUCCEEDED(xrGetReferenceSpaceBoundsRect(state.session, state.referenceSpaceType, &bounds))) {
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

static bool openxr_getPose(Device device, vec3 position, quat orientation) {
  if (!state.spaces[device]) {
    return false;
  }

  // If there's a pose action for this device, see if the action is active before locating its space
  XrAction action = getPoseActionForDevice(device);

  if (action) {
    XrActionStateGetInfo info = {
      .type = XR_TYPE_ACTION_STATE_GET_INFO,
      .action = action,
      .subactionPath = state.actionFilters[device]
    };

    XrActionStatePose poseState = {
      .type = XR_TYPE_ACTION_STATE_POSE
    };

    XR(xrGetActionStatePose(state.session, &info, &poseState));

    // If the action isn't active, try fallbacks for some devices (hand tracking), pardon the mess
    if (!poseState.isActive) {
      bool point = false;

      if (state.features.handTrackingAim && (device == DEVICE_HAND_LEFT_POINT || device == DEVICE_HAND_RIGHT_POINT)) {
        device = DEVICE_HAND_LEFT + (device == DEVICE_HAND_RIGHT_POINT);
        point = true;
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

      XrHandJointLocationEXT joints[26];
      XrHandJointLocationsEXT hand = {
        .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
        .jointCount = COUNTOF(joints),
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

      XrPosef* pose = point ? &aimState.aimPose : &joints[XR_HAND_JOINT_WRIST_EXT].pose;
      memcpy(orientation, &pose->orientation, 4 * sizeof(float));
      memcpy(position, &pose->position, 3 * sizeof(float));
      return true;
    }
  }

  XrSpaceLocation location = { .type = XR_TYPE_SPACE_LOCATION };
  xrLocateSpace(state.spaces[device], state.referenceSpace, state.frameState.predictedDisplayTime, &location);
  memcpy(orientation, &location.pose.orientation, 4 * sizeof(float));
  memcpy(position, &location.pose.position, 3 * sizeof(float));
  return location.locationFlags & (XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
}

static bool openxr_getVelocity(Device device, vec3 linearVelocity, vec3 angularVelocity) {
  if (!state.spaces[device]) {
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
  XR(xrGetActionStateBoolean(state.session, &info, &actionState));
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
  XR(xrGetActionStateFloat(state.session, &info, &actionState));
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

      XrHandJointLocationEXT joints[26];
      XrHandJointLocationsEXT hand = {
        .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
        .next = &aimState,
        .jointCount = COUNTOF(joints),
        .jointLocations = joints
      };

      if (XR_FAILED(xrLocateHandJointsEXT(tracker, &info, &hand))) {
        return false;
      }

      *value = aimState.pinchStrengthIndex;
      return aimState.status & XR_HAND_TRACKING_AIM_INDEX_PINCHING_BIT_FB;
    case AXIS_THUMBSTICK: return getFloatAction(ACTION_THUMBSTICK_X, filter, &value[0]) && getFloatAction(ACTION_THUMBSTICK_Y, filter, &value[1]);
    case AXIS_TOUCHPAD: return getFloatAction(ACTION_TRACKPAD_X, filter, &value[0]) && getFloatAction(ACTION_TRACKPAD_Y, filter, &value[1]);
    case AXIS_GRIP: return getFloatAction(ACTION_GRIP_AXIS, filter, &value[0]);
    default: return false;
  }
}

static bool openxr_getSkeleton(Device device, float* poses) {
  XrHandTrackerEXT tracker = getHandTracker(device);

  if (!tracker) {
    return false;
  }

  XrHandJointsLocateInfoEXT info = {
    .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
    .baseSpace = state.referenceSpace,
    .time = state.frameState.predictedDisplayTime
  };

  XrHandJointLocationEXT joints[26];
  XrHandJointLocationsEXT hand = {
    .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
    .jointCount = COUNTOF(joints),
    .jointLocations = joints
  };

  if (XR_FAILED(xrLocateHandJointsEXT(tracker, &info, &hand)) || !hand.isActive) {
    return false;
  }

  float* pose = poses;
  for (uint32_t i = 0; i < COUNTOF(joints); i++) {
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

  XR(xrApplyHapticFeedback(state.session, &info, (XrHapticBaseHeader*) &vibration));
  return true;
}

static struct ModelData* openxr_newModelData(Device device, bool animated) {
  if (!state.features.handTrackingMesh) {
    return NULL;
  }

  XrHandTrackerEXT tracker = getHandTracker(device);

  if (!tracker) {
    return NULL;
  }

  XrHandTrackingMeshFB mesh = { .type = XR_TYPE_HAND_TRACKING_MESH_FB };
  XrResult result = xrGetHandMeshFB(tracker, &mesh);

  if (XR_FAILED(result)) {
    return NULL;
  }

  uint32_t jointCount = mesh.jointCapacityInput = mesh.jointCountOutput;
  uint32_t vertexCount = mesh.vertexCapacityInput = mesh.vertexCountOutput;
  uint32_t indexCount = mesh.indexCapacityInput = mesh.indexCountOutput;

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

  char* meshData = malloc(totalSize);
  if (!meshData) return NULL;

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
  lovrAssert(offset == totalSize, "Unreachable");

  result = xrGetHandMeshFB(tracker, &mesh);
  if (XR_FAILED(result)) {
    free(meshData);
    return NULL;
  }

  ModelData* model = calloc(1, sizeof(ModelData));
  lovrAssert(model, "Out of memory");
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

  model->attributes[0] = (ModelAttribute) { .buffer = 0, .type = F32, .components = 3 };
  model->attributes[1] = (ModelAttribute) { .buffer = 1, .type = F32, .components = 3 };
  model->attributes[2] = (ModelAttribute) { .buffer = 2, .type = F32, .components = 2 };
  model->attributes[3] = (ModelAttribute) { .buffer = 3, .type = I16, .components = 4 };
  model->attributes[4] = (ModelAttribute) { .buffer = 4, .type = F32, .components = 4 };
  model->attributes[5] = (ModelAttribute) { .buffer = 5, .type = U16, .count = indexCount };

  model->primitives[0] = (ModelPrimitive) {
    .mode = DRAW_TRIANGLES,
    .attributes = {
      [ATTR_POSITION] = &model->attributes[0],
      [ATTR_NORMAL] = &model->attributes[1],
      [ATTR_TEXCOORD] = &model->attributes[2],
      [ATTR_BONES] = &model->attributes[3],
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
      .transform.properties.translation = { 0.f, 0.f, 0.f },
      .transform.properties.rotation = { 0.f, 0.f, 0.f, 1.f },
      .transform.properties.scale = { 1.f, 1.f, 1.f },
      .skin = ~0u
    };

    // Inverse bind matrix
    XrPosef* pose = &mesh.jointBindPoses[i];
    float* inverseBindMatrix = inverseBindMatrices + 16 * i;
    mat4_fromQuat(inverseBindMatrix, &pose->orientation.x);
    memcpy(inverseBindMatrix + 12, &pose->position.x, 3 * sizeof(float));
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
    .transform.properties.translation = { 0.f, 0.f, 0.f },
    .transform.properties.rotation = { 0.f, 0.f, 0.f, 1.f },
    .transform.properties.scale = { 1.f, 1.f, 1.f },
    .primitiveIndex = 0,
    .primitiveCount = 1,
    .skin = 0
  };

  // The root node has the mesh node and root joint as children
  model->rootNode = model->jointCount + 1;
  model->nodes[model->rootNode] = (ModelNode) {
    .matrix = true,
    .transform = { MAT4_IDENTITY },
    .childCount = 2,
    .children = children,
    .skin = ~0u
  };

  // Add the children to the root node
  *children++ = XR_HAND_JOINT_WRIST_EXT;
  *children++ = model->jointCount;

  return model;
}

static bool openxr_animate(Device device, struct Model* model) {
  XrHandTrackerEXT tracker = getHandTracker(device);

  if (!tracker) {
    return false;
  }

  // TODO might be nice to cache joints so getSkeleton/animate only locate joints once (profile)
  XrHandJointsLocateInfoEXT info = {
    .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
    .baseSpace = state.referenceSpace,
    .time = state.frameState.predictedDisplayTime
  };

  XrHandJointLocationEXT joints[26];
  XrHandJointLocationsEXT hand = {
    .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
    .jointCount = COUNTOF(joints),
    .jointLocations = joints
  };

  if (XR_FAILED(xrLocateHandJointsEXT(tracker, &info, &hand)) || !hand.isActive) {
    return false;
  }

  lovrModelResetPose(model);

  // This is kinda brittle, ideally we would use the jointParents from the actual mesh object
  uint32_t jointParents[26] = {
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

  // The following can be optimized a lot (ideally we would set the global transform for the nodes)
  for (uint32_t i = 0; i < COUNTOF(joints); i++) {
    if (jointParents[i] == ~0u) {
      float position[4] = { 0.f, 0.f, 0.f };
      float orientation[4] = { 0.f, 0.f, 0.f, 1.f };
      lovrModelPose(model, i, position, orientation, 1.f);
    } else {
      XrPosef* parent = &joints[jointParents[i]].pose;
      XrPosef* pose = &joints[i].pose;

      // Convert global pose to parent-local pose (premultiply with inverse of parent pose)
      // TODO there should be maf for this
      float position[4], orientation[4];
      vec3_init(position, &pose->position.x);
      vec3_sub(position, &parent->position.x);

      quat_init(orientation, &parent->orientation.x);
      quat_conjugate(orientation);

      quat_rotate(orientation, position);
      quat_mul(orientation, orientation, &pose->orientation.x);

      lovrModelPose(model, i, position, orientation, 1.f);
    }
  }

  return true;
}

static void openxr_renderTo(void (*callback)(void*), void* userdata) {
  if (!SESSION_ACTIVE(state.sessionState)) {
    state.waited = false;
    return;
  }

  XrFrameBeginInfo beginInfo = {
    .type = XR_TYPE_FRAME_BEGIN_INFO
  };

  XrFrameEndInfo endInfo = {
    .type = XR_TYPE_FRAME_END_INFO,
    .displayTime = state.frameState.predictedDisplayTime,
    .environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
    .layers = (const XrCompositionLayerBaseHeader*[1]) { (XrCompositionLayerBaseHeader*) &state.layers[0] }
  };

  XR(xrBeginFrame(state.session, &beginInfo));

  if (state.frameState.shouldRender) {
    if (state.hasImage) {
      XR(xrReleaseSwapchainImage(state.swapchain, NULL));
    }

    XrSwapchainImageWaitInfo waitInfo = {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
      .timeout = XR_INFINITE_DURATION
    };

    XR(xrAcquireSwapchainImage(state.swapchain, NULL, &state.imageIndex));
    XR(xrWaitSwapchainImage(state.swapchain, &waitInfo));
    state.hasImage = true;

    uint32_t count;
    XrView views[2];
    getViews(views, &count);

    for (int eye = 0; eye < 2; eye++) {
      float viewMatrix[16];
      XrView* view = &views[eye];
      mat4_fromQuat(viewMatrix, &view->pose.orientation.x);
      memcpy(viewMatrix + 12, &view->pose.position.x, 3 * sizeof(float));
      mat4_invert(viewMatrix);
      lovrGraphicsSetViewMatrix(eye, viewMatrix);

      float projection[16];
      XrFovf* fov = &view->fov;
      mat4_fov(projection, -fov->angleLeft, fov->angleRight, fov->angleUp, -fov->angleDown, state.clipNear, state.clipFar);
      lovrGraphicsSetProjection(eye, projection);
    }

    lovrGraphicsSetBackbuffer(state.canvases[state.imageIndex], true, true);
    callback(userdata);
    lovrGraphicsSetBackbuffer(NULL, false, false);

    endInfo.layerCount = 1;
    state.layerViews[0].pose = views[0].pose;
    state.layerViews[0].fov = views[0].fov;
    state.layerViews[1].pose = views[1].pose;
    state.layerViews[1].fov = views[1].fov;

    XR(xrReleaseSwapchainImage(state.swapchain, NULL));
    state.hasImage = false;
  }

  XR(xrEndFrame(state.session, &endInfo));
  lovrGpuDirtyTexture();
  state.waited = false;
}

static Texture* openxr_getMirrorTexture(void) {
  Canvas* canvas = state.canvases[state.imageIndex];
  return canvas ? lovrCanvasGetAttachments(canvas, NULL)[0].texture : NULL;
}

static bool openxr_isFocused(void) {
  return state.sessionState == XR_SESSION_STATE_FOCUSED;
}

static double openxr_update(void) {
  if (state.waited && !state.hasImage) return openxr_getDeltaTime();

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
            }));
            break;

          case XR_SESSION_STATE_STOPPING:
            XR(xrEndSession(state.session));
            break;

          case XR_SESSION_STATE_EXITING:
          case XR_SESSION_STATE_LOSS_PENDING:
            lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit.exitCode = 0 });
            break;

          default: break;
        }

        bool wasFocused = state.sessionState == XR_SESSION_STATE_FOCUSED;
        bool isFocused = event->state == XR_SESSION_STATE_FOCUSED;
        if (wasFocused != isFocused) {
          lovrEventPush((Event) { .type = EVENT_FOCUS, .data.boolean.value = isFocused });
        }

        state.sessionState = event->state;
        break;
      }

      default: break;
    }
    e.type = XR_TYPE_EVENT_DATA_BUFFER;
  }

  if (SESSION_ACTIVE(state.sessionState)) {
    state.lastDisplayTime = state.frameState.predictedDisplayTime;
    XR(xrWaitFrame(state.session, NULL, &state.frameState));
    state.waited = true;

    if (state.lastDisplayTime == 0.) {
      state.lastDisplayTime = state.frameState.predictedDisplayTime - state.frameState.predictedDisplayPeriod;
    }

    XrActiveActionSet activeSets[] = {
      { state.actionSet, XR_NULL_PATH }
    };

    XrActionsSyncInfo syncInfo = {
      .type = XR_TYPE_ACTIONS_SYNC_INFO,
      .countActiveActionSets = COUNTOF(activeSets),
      .activeActionSets = activeSets
    };

    XR(xrSyncActions(state.session, &syncInfo));
  }

  return openxr_getDeltaTime();
}

HeadsetInterface lovrHeadsetOpenXRDriver = {
  .driverType = DRIVER_OPENXR,
  .init = openxr_init,
  .start = openxr_start,
  .destroy = openxr_destroy,
  .getName = openxr_getName,
  .getOriginType = openxr_getOriginType,
  .getDisplayDimensions = openxr_getDisplayDimensions,
  .getDisplayFrequency = openxr_getDisplayFrequency,
  .getDisplayFrequencies = openxr_getDisplayFrequencies,
  .setDisplayFrequency = openxr_setDisplayFrequency,
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
  .newModelData = openxr_newModelData,
  .animate = openxr_animate,
  .renderTo = openxr_renderTo,
  .getMirrorTexture = openxr_getMirrorTexture,
  .isFocused = openxr_isFocused,
  .update = openxr_update
};
