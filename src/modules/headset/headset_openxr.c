#include "headset/headset.h"
#include "event/event.h"
#include "filesystem/filesystem.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "graphics/texture.h"
#include "core/ref.h"
#include "core/util.h"
#include <math.h>

#ifdef _WIN32
#define XR_USE_PLATFORM_WIN32
#endif

#define XR_USE_GRAPHICS_API_OPENGL
#define GL_SRGB8_ALPHA8 0x8C43
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

static XrResult handleResult(XrResult result, const char* file, int line) {
  if (XR_FAILED(result)) {
    char message[XR_MAX_RESULT_STRING_SIZE];
    xrResultToString(XR_NULL_HANDLE, result, message);
    lovrThrow("OpenXR Error: %s at %s:%d", message, file, line);
  }

  return result;
}

#define XR(f) handleResult(f, __FILE__, __LINE__)
#define XR_INIT(f) if (XR_FAILED(f)) return openxr_destroy(), false;
#define SESSION_VISIBLE(s) (s == XR_SESSION_STATE_VISIBLE || s == XR_SESSION_STATE_FOCUSED)
#define SESSION_SYNCHRONIZED(s) (s == XR_SESSION_STATE_SYNCHRONIZED || SESSION_VISIBLE(s))
#define MAX_IMAGES 4

enum {
  PROFILE_SIMPLE,
  PROFILE_VIVE,
  PROFILE_TOUCH,
  PROFILE_GO,
  PROFILE_INDEX,
  MAX_PROFILES
} Profile;

const char* profilePaths[MAX_PROFILES] = {
  [PROFILE_SIMPLE] = "/interaction_profiles/khr/simple_controller",
  [PROFILE_VIVE] = "/interaction_profiles/htc/vive_controller",
  [PROFILE_TOUCH] = "/interaction_profiles/oculus/touch_controller",
  [PROFILE_GO] = "/interaction_profiles/oculus/go_controller",
  [PROFILE_INDEX] = "/interaction_profiles/valve/index_controller"
};

typedef enum {
  ACTION_HAND_POSE,
  ACTION_TRIGGER_DOWN,
  ACTION_TRIGGER_TOUCH,
  ACTION_TRIGGER_AXIS,
  ACTION_TRACKPAD_DOWN,
  ACTION_TRACKPAD_TOUCH,
  ACTION_TRACKPAD_AXIS,
  ACTION_THUMBSTICK_DOWN,
  ACTION_THUMBSTICK_TOUCH,
  ACTION_THUMBSTICK_AXIS,
  ACTION_MENU_DOWN,
  ACTION_MENU_TOUCH,
  ACTION_GRIP_DOWN,
  ACTION_GRIP_TOUCH,
  ACTION_GRIP_AXIS,
  ACTION_VIBRATE,
  MAX_ACTIONS
} Action;

#define action(id, name, type, subactions) { XR_TYPE_ACTION_CREATE_INFO, NULL, id, type, subactions, NULL, name }
static XrActionCreateInfo defaultActions[MAX_ACTIONS] = {
  [ACTION_HAND_POSE] = action("handPose", "Hand Pose", XR_ACTION_TYPE_POSE_INPUT, 2),
  [ACTION_TRIGGER_DOWN] = action("triggerDown", "Trigger Down", XR_ACTION_TYPE_BOOLEAN_INPUT, 2),
  [ACTION_TRIGGER_TOUCH] = action("triggerTouch", "Trigger Touch", XR_ACTION_TYPE_BOOLEAN_INPUT, 2),
  [ACTION_TRIGGER_AXIS] = action("triggerAxis", "Trigger Axis", XR_ACTION_TYPE_FLOAT_INPUT, 2),
  [ACTION_TRACKPAD_DOWN] = action("trackpadDown", "Trackpad Down", XR_ACTION_TYPE_BOOLEAN_INPUT, 2),
  [ACTION_TRACKPAD_TOUCH] = action("trackpadTouch", "Trackpad Touch", XR_ACTION_TYPE_BOOLEAN_INPUT, 2),
  [ACTION_TRACKPAD_AXIS] = action("trackpadAxis", "Trackpad Axis", XR_ACTION_TYPE_VECTOR2F_INPUT, 2),
  [ACTION_THUMBSTICK_DOWN] = action("thumbstickDown", "Thumbstick Down", XR_ACTION_TYPE_BOOLEAN_INPUT, 2),
  [ACTION_THUMBSTICK_TOUCH] = action("thumbstickTouch", "Thumbstick Touch", XR_ACTION_TYPE_BOOLEAN_INPUT, 2),
  [ACTION_THUMBSTICK_AXIS] = action("thumbstickAxis", "Thumbstick Axis", XR_ACTION_TYPE_VECTOR2F_INPUT, 2),
  [ACTION_MENU_DOWN] = action("menuDown", "Menu Down", XR_ACTION_TYPE_BOOLEAN_INPUT, 2),
  [ACTION_MENU_TOUCH] = action("menuTouch", "Menu Touch", XR_ACTION_TYPE_BOOLEAN_INPUT, 2),
  [ACTION_GRIP_DOWN] = action("gripDown", "Grip Down", XR_ACTION_TYPE_BOOLEAN_INPUT, 2),
  [ACTION_GRIP_TOUCH] = action("gripTouch", "Grip Touch", XR_ACTION_TYPE_BOOLEAN_INPUT, 2),
  [ACTION_GRIP_AXIS] = action("gripAxis", "Grip Axis", XR_ACTION_TYPE_FLOAT_INPUT, 2),
  [ACTION_VIBRATE] = action("vibrate", "Vibrate", XR_ACTION_TYPE_VIBRATION_OUTPUT, 2),
};
#undef action

static const char* defaultBindings[MAX_PROFILES][MAX_ACTIONS][2] = {
  [PROFILE_SIMPLE] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/grip/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/grip/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/select/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/select/click",
    [ACTION_MENU_DOWN][0] = "/user/hand/left/input/menu/click",
    [ACTION_MENU_DOWN][1] = "/user/hand/right/input/menu/click",
    [ACTION_VIBRATE][0] = "/user/hand/left/output/haptic",
    [ACTION_VIBRATE][1] = "/user/hand/right/output/haptic"
  },
  [PROFILE_VIVE] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/grip/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/grip/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/trigger/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/trigger/click",
    [ACTION_TRIGGER_AXIS][0] = "/user/hand/left/input/trigger/value",
    [ACTION_TRIGGER_AXIS][1] = "/user/hand/right/input/trigger/value",
    [ACTION_TRACKPAD_DOWN][0] = "/user/hand/left/input/trackpad/click",
    [ACTION_TRACKPAD_DOWN][1] = "/user/hand/right/input/trackpad/click",
    [ACTION_TRACKPAD_TOUCH][0] = "/user/hand/left/input/trackpad/touch",
    [ACTION_TRACKPAD_TOUCH][1] = "/user/hand/right/input/trackpad/touch",
    [ACTION_TRACKPAD_AXIS][0] = "/user/hand/left/input/trackpad",
    [ACTION_TRACKPAD_AXIS][1] = "/user/hand/right/input/trackpad",
    [ACTION_MENU_DOWN][0] = "/user/hand/left/input/menu/click",
    [ACTION_MENU_DOWN][1] = "/user/hand/right/input/menu/click",
    [ACTION_GRIP_DOWN][0] = "/user/hand/left/input/squeeze/click",
    [ACTION_GRIP_DOWN][1] = "/user/hand/right/input/squeeze/click",
    [ACTION_VIBRATE][0] = "/user/hand/left/output/haptic",
    [ACTION_VIBRATE][1] = "/user/hand/right/output/haptic"
  },
  [PROFILE_TOUCH] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/grip/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/grip/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/trigger/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/trigger/click",
    [ACTION_TRIGGER_TOUCH][0] = "/user/hand/left/input/trigger/touch",
    [ACTION_TRIGGER_TOUCH][1] = "/user/hand/right/input/trigger/touch",
    [ACTION_TRIGGER_AXIS][0] = "/user/hand/left/input/trigger/value",
    [ACTION_TRIGGER_AXIS][1] = "/user/hand/right/input/trigger/value",
    [ACTION_THUMBSTICK_DOWN][0] = "/user/hand/left/input/thumbstick/click",
    [ACTION_THUMBSTICK_DOWN][1] = "/user/hand/right/input/thumbstick/click",
    [ACTION_THUMBSTICK_TOUCH][0] = "/user/hand/left/input/thumbstick/touch",
    [ACTION_THUMBSTICK_TOUCH][1] = "/user/hand/right/input/thumbstick/touch",
    [ACTION_THUMBSTICK_AXIS][0] = "/user/hand/left/input/thumbstick",
    [ACTION_THUMBSTICK_AXIS][1] = "/user/hand/right/input/thumbstick",
    [ACTION_MENU_DOWN][0] = "/user/hand/left/input/menu/click",
    [ACTION_MENU_DOWN][1] = "/user/hand/right/input/menu/click",
    [ACTION_MENU_TOUCH][0] = "/user/hand/left/input/menu/touch",
    [ACTION_MENU_TOUCH][1] = "/user/hand/right/input/menu/touch",
    [ACTION_GRIP_DOWN][0] = "/user/hand/left/input/squeeze/click",
    [ACTION_GRIP_DOWN][1] = "/user/hand/right/input/squeeze/click",
    [ACTION_GRIP_TOUCH][0] = "/user/hand/left/input/squeeze/touch",
    [ACTION_GRIP_TOUCH][1] = "/user/hand/right/input/squeeze/touch",
    [ACTION_GRIP_AXIS][0] = "/user/hand/left/input/squeeze/value",
    [ACTION_GRIP_AXIS][1] = "/user/hand/right/input/squeeze/value",
    [ACTION_VIBRATE][0] = "/user/hand/left/output/haptic",
    [ACTION_VIBRATE][1] = "/user/hand/right/output/haptic"
  },
  [PROFILE_GO] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/grip/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/grip/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/trigger/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/trigger/click",
    [ACTION_TRACKPAD_DOWN][0] = "/user/hand/left/input/trackpad/click",
    [ACTION_TRACKPAD_DOWN][1] = "/user/hand/right/input/trackpad/click",
    [ACTION_TRACKPAD_TOUCH][0] = "/user/hand/left/input/trackpad/touch",
    [ACTION_TRACKPAD_TOUCH][1] = "/user/hand/right/input/trackpad/touch",
    [ACTION_TRACKPAD_AXIS][0] = "/user/hand/left/input/trackpad",
    [ACTION_TRACKPAD_AXIS][1] = "/user/hand/right/input/trackpad"
  },
  [PROFILE_INDEX] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/grip/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/grip/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/trigger/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/trigger/click",
    [ACTION_TRIGGER_TOUCH][0] = "/user/hand/left/input/trigger/touch",
    [ACTION_TRIGGER_TOUCH][1] = "/user/hand/right/input/trigger/touch",
    [ACTION_TRIGGER_AXIS][0] = "/user/hand/left/input/trigger/value",
    [ACTION_TRIGGER_AXIS][1] = "/user/hand/right/input/trigger/value",
    [ACTION_TRACKPAD_DOWN][0] = "/user/hand/left/input/trackpad/click",
    [ACTION_TRACKPAD_DOWN][1] = "/user/hand/right/input/trackpad/click",
    [ACTION_TRACKPAD_TOUCH][0] = "/user/hand/left/input/trackpad/touch",
    [ACTION_TRACKPAD_TOUCH][1] = "/user/hand/right/input/trackpad/touch",
    [ACTION_TRACKPAD_AXIS][0] = "/user/hand/left/input/trackpad",
    [ACTION_TRACKPAD_AXIS][1] = "/user/hand/right/input/trackpad",
    [ACTION_THUMBSTICK_DOWN][0] = "/user/hand/left/input/thumbstick/click",
    [ACTION_THUMBSTICK_DOWN][1] = "/user/hand/right/input/thumbstick/click",
    [ACTION_THUMBSTICK_TOUCH][0] = "/user/hand/left/input/thumbstick/touch",
    [ACTION_THUMBSTICK_TOUCH][1] = "/user/hand/right/input/thumbstick/touch",
    [ACTION_THUMBSTICK_AXIS][0] = "/user/hand/left/input/thumbstick",
    [ACTION_THUMBSTICK_AXIS][1] = "/user/hand/right/input/thumbstick",
    [ACTION_GRIP_AXIS][0] = "/user/hand/left/input/squeeze/value",
    [ACTION_GRIP_AXIS][1] = "/user/hand/right/input/squeeze/value",
    [ACTION_VIBRATE][0] = "/user/hand/left/output/haptic",
    [ACTION_VIBRATE][1] = "/user/hand/right/output/haptic"
  }
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
  Canvas* canvas;
  Texture* textures[MAX_IMAGES];
  uint32_t imageCount;
  uint32_t msaa;
  uint32_t width;
  uint32_t height;
  float clipNear;
  float clipFar;
  XrActionSet actionSet;
  XrAction actions[MAX_ACTIONS];
  XrPath actionFilters[2];
} state;

static void openxr_destroy();

static bool openxr_init(float offset, uint32_t msaa) {

  { // Instance
    XrInstanceCreateInfo info = {
      .type = XR_TYPE_INSTANCE_CREATE_INFO,
      .applicationInfo.engineName = "LÖVR",
      .applicationInfo.engineVersion = ((LOVR_VERSION_MAJOR & 0xff) << 24) + ((LOVR_VERSION_MINOR & 0xff) << 16) + (LOVR_VERSION_PATCH & 0xffff),
      .applicationInfo.applicationName = "LÖVR",
      .applicationInfo.applicationVersion = 0,
      .applicationInfo.apiVersion = XR_CURRENT_API_VERSION,
      .enabledExtensionCount = 1,
      .enabledExtensionNames = (const char*[1]) { "XR_KHR_opengl_enable" }
    };

    XR_INIT(xrCreateInstance(&info, &state.instance));
  }

  { // System
    XrSystemGetInfo info = {
      .type = XR_TYPE_SYSTEM_GET_INFO,
      .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY
    };

    XR_INIT(xrGetSystem(state.instance, &info, &state.system));

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

    state.msaa = views[0].recommendedSwapchainSampleCount;
    state.width = views[0].recommendedImageRectWidth;
    state.height = views[0].recommendedImageRectHeight;
  }

  { // Actions...
    XrActionSetCreateInfo info = {
      .type = XR_TYPE_ACTION_SET_CREATE_INFO,
      .actionSetName = "default",
      .localizedActionSetName = "Default",
      .priority = 0
    };

    XR_INIT(xrCreateActionSet(state.instance, &info, &state.actionSet));
    XR_INIT(xrStringToPath(state.instance, "/user/hand/left", &state.actionFilters[0]));
    XR_INIT(xrStringToPath(state.instance, "/user/hand/right", &state.actionFilters[1]));

    for (size_t action = 0; action < MAX_ACTIONS; action++) {
      defaultActions[action].subactionPaths = defaultActions[action].countSubactionPaths == 2 ? state.actionFilters : NULL;
      XR_INIT(xrCreateAction(state.actionSet, &defaultActions[action], &state.actions[action]));
    }

    XrActionSuggestedBinding bindings[2 * MAX_ACTIONS];
    for (size_t profile = 0; profile < MAX_PROFILES; profile++) {
      int bindingCount = 0;

      for (size_t action = 0; action < MAX_ACTIONS; action++) {
        for (size_t i = 0; i < 2; i++) {
          if (defaultBindings[profile][action][i]) {
            bindings[bindingCount].action = state.actions[action];
            XR_INIT(xrStringToPath(state.instance, defaultBindings[profile][action][i], &bindings[bindingCount].binding));
            bindingCount++;
          }
        }
      }

      XrPath profilePath;
      XR_INIT(xrStringToPath(state.instance, profilePaths[profile], &profilePath));
      XR_INIT(xrSuggestInteractionProfileBindings(state.instance, &(XrInteractionProfileSuggestedBinding) {
        .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
        .interactionProfile = profilePath,
        .countSuggestedBindings = bindingCount,
        .suggestedBindings = bindings
      }));
    }
  }

  { // Session
    XrSessionCreateInfo info = {
      .type = XR_TYPE_SESSION_CREATE_INFO,
#ifdef _WIN32
      .next = &(XrGraphicsBindingOpenGLWin32KHR) {
        .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR,
        .hDC = lovrPlatformGetWindow(),
        .hGLRC = lovrPlatformGetContext()
      },
#else
#error "OpenXR is not supported on this platform!"
#endif
      .systemId = state.system
    };

    XrSessionActionSetsAttachInfo attachInfo = {
      .type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
      .countActionSets = 1,
      .actionSets = &state.actionSet
    };

    XR_INIT(xrCreateSession(state.instance, &info, &state.session));
    XR_INIT(xrAttachSessionActionSets(state.session, &attachInfo));
  }

  { // Spaaaaace

    // Main reference space (can be stage or local)
    XrReferenceSpaceCreateInfo info = {
      .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
      .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE
    };

    if (XR_FAILED(xrCreateReferenceSpace(state.session, &info, &state.referenceSpace))) {
      info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
      info.poseInReferenceSpace.position.y = -offset;
      XR_INIT(xrCreateReferenceSpace(state.session, &info, &state.referenceSpace));
    }

    state.referenceSpaceType = info.referenceSpaceType;

    // Head space (for head pose)
    XrReferenceSpaceCreateInfo headSpaceInfo = {
      .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
      .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW
    };

    XR_INIT(xrCreateReferenceSpace(state.session, &headSpaceInfo, &state.spaces[DEVICE_HEAD]));

    // Left hand space
    XrActionSpaceCreateInfo leftHandSpaceInfo = {
      .type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
      .action = state.actions[ACTION_HAND_POSE],
      .subactionPath = state.actionFilters[0]
    };

    XR_INIT(xrCreateActionSpace(state.session, &leftHandSpaceInfo, &state.spaces[DEVICE_HAND_LEFT]));

    // Right hand space
    XrActionSpaceCreateInfo rightHandSpaceInfo = {
      .type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
      .action = state.actions[ACTION_HAND_POSE],
      .subactionPath = state.actionFilters[1]
    };

    XR_INIT(xrCreateActionSpace(state.session, &rightHandSpaceInfo, &state.spaces[DEVICE_HAND_RIGHT]));
  }

  { // Swapchain
    XrSwapchainCreateInfo info = {
      .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
      .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
      .format = GL_SRGB8_ALPHA8,
      .sampleCount = state.msaa,
      .width = state.width * 2,
      .height = state.height,
      .faceCount = 1,
      .arraySize = 1,
      .mipCount = 1
    };

    XrSwapchainImageOpenGLKHR images[MAX_IMAGES];
    XR_INIT(xrCreateSwapchain(state.session, &info, &state.swapchain));
    XR_INIT(xrEnumerateSwapchainImages(state.swapchain, MAX_IMAGES, &state.imageCount, (XrSwapchainImageBaseHeader*) images));

    for (uint32_t i = 0; i < state.imageCount; i++) {
      state.textures[i] = lovrTextureCreateFromHandle(images[i].image, TEXTURE_2D, 1);
    }

    // Pre-init composition layer
    state.layers[0] = (XrCompositionLayerProjection) {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
      .space = state.referenceSpace,
      .viewCount = 2,
      .views = state.layerViews
    };

    // Pre-init composition layer views
    state.layerViews[0] = (XrCompositionLayerProjectionView) {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
      .subImage = { state.swapchain, { { 0, 0 }, { state.width, state.height } }, 0 }
    };

    // Copy the left view to the right view and offset for side-by-side submission
    state.layerViews[1] = state.layerViews[0];
    state.layerViews[1].subImage.imageRect.offset.x += state.width;
  }

  state.clipNear = .1f;
  state.clipNear = 100.f;

  return true;
}

static void openxr_destroy() {
  lovrRelease(Canvas, state.canvas);
  for (uint32_t i = 0; i < state.imageCount; i++) {
    lovrRelease(Texture, state.textures[i]);
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

  if (state.actionSet) xrDestroyActionSet(state.actionSet);
  if (state.swapchain) xrDestroySwapchain(state.swapchain);
  if (state.referenceSpace) xrDestroySpace(state.referenceSpace);
  if (state.session) xrEndSession(state.session);
  if (state.instance) xrDestroyInstance(state.instance);
  memset(&state, 0, sizeof(state));
}

static bool openxr_getName(char* name, size_t length) {
  XrSystemProperties properties;
  XR(xrGetSystemProperties(state.instance, state.system, &properties));
  strncpy(name, properties.systemName, length - 1);
  name[length - 1] = '\0';
  return true;
}

static HeadsetOrigin openxr_getOriginType() {
  return state.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE ? ORIGIN_FLOOR : ORIGIN_HEAD;
}

static void openxr_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  *width = state.width;
  *height = state.height;
}

static const float* openxr_getDisplayMask(uint32_t* count) {
  *count = 0;
  return NULL;
}

static double openxr_getDisplayTime(void) {
  return state.frameState.predictedDisplayTime / 1e9;
}

static void getViews(XrView views[2], uint32_t* count) {
  XrViewLocateInfo viewLocateInfo = {
    .type = XR_TYPE_VIEW_LOCATE_INFO,
    .viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
    .displayTime = state.frameState.predictedDisplayTime,
    .space = state.referenceSpace
  };

  XrViewState viewState;
  XR(xrLocateViews(state.session, &viewLocateInfo, &viewState, sizeof(views) / sizeof(views[0]), count, views));
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
    memcpy(position, views[view].pose.position, 3 * sizeof(float));
    memcpy(orientation, views[view].pose.orientation, 4 * sizeof(float));
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

  XrSpaceLocation location;
  XR(xrLocateSpace(state.spaces[device], state.referenceSpace, state.frameState.predictedDisplayTime, &location));
  memcpy(orientation, &location.pose.orientation, 4 * sizeof(float));
  memcpy(position, &location.pose.position, 3 * sizeof(float));
  return location.locationFlags & (XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
}

static bool openxr_getVelocity(Device device, vec3 linearVelocity, vec3 angularVelocity) {
  if (!state.spaces[device]) {
    return false;
  }

  XrSpaceVelocity velocity = { .type = XR_TYPE_SPACE_VELOCITY };
  XrSpaceLocation location = { .next = &velocity };
  XR(xrLocateSpace(state.spaces[device], state.referenceSpace, state.frameState.predictedDisplayTime, &location));
  memcpy(linearVelocity, &velocity.linearVelocity, 3 * sizeof(float));
  memcpy(angularVelocity, &velocity.angularVelocity, 3 * sizeof(float));
  return velocity.velocityFlags & (XR_SPACE_VELOCITY_LINEAR_VALID_BIT | XR_SPACE_VELOCITY_ANGULAR_VALID_BIT);
}

static XrPath getActionFilter(Device device) {
  switch (device) {
    case DEVICE_HAND_LEFT: return state.actionFilters[0];
    case DEVICE_HAND_RIGHT: return state.actionFilters[1];
    default: return XR_NULL_PATH;
  }
}

static bool getButtonState(Device device, DeviceButton button, bool* value, bool touch) {
  XrActionStateGetInfo info = {
    .type = XR_TYPE_ACTION_STATE_GET_INFO,
    .subactionPath = getActionFilter(device)
  };

  if (info.subactionPath == XR_NULL_PATH) {
    return false;
  }

  switch (button) {
    case BUTTON_TRIGGER: info.action = state.actions[ACTION_TRIGGER_DOWN + touch]; break;
    case BUTTON_TOUCHPAD: info.action = state.actions[ACTION_TRACKPAD_DOWN + touch]; break;
    case BUTTON_MENU: info.action = state.actions[ACTION_MENU_DOWN + touch]; break;
    case BUTTON_GRIP: info.action = state.actions[ACTION_GRIP_DOWN + touch]; break;
    default: return false;
  }

  XrActionStateBoolean actionState;
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

static bool openxr_getAxis(Device device, DeviceAxis axis, float* value) {
  XrActionStateGetInfo info = {
    .type = XR_TYPE_ACTION_STATE_GET_INFO,
    .subactionPath = getActionFilter(device)
  };

  if (info.subactionPath == XR_NULL_PATH) {
    return false;
  }

  switch (axis) {
    case AXIS_TRIGGER: info.action = state.actions[ACTION_TRIGGER_AXIS]; break;
    case AXIS_TOUCHPAD: info.action = state.actions[ACTION_TRACKPAD_AXIS]; break;
    case AXIS_GRIP: info.action = state.actions[ACTION_GRIP_AXIS]; break;
    default: return false;
  }

  switch (axis) {
    case AXIS_TRIGGER:
    case AXIS_GRIP: {
      XrActionStateFloat actionState;
      XR(xrGetActionStateFloat(state.session, &info, &actionState));
      *value = actionState.currentState;
      return actionState.isActive;
    }

    case AXIS_THUMBSTICK:
    case AXIS_TOUCHPAD: {
      XrActionStateVector2f actionState;
      XR(xrGetActionStateVector2f(state.session, &info, &actionState));
      value[0] = actionState.currentState.x;
      value[1] = actionState.currentState.y;
      return actionState.isActive;
    }

    default: return false;
  }
}

static bool openxr_vibrate(Device device, float power, float duration, float frequency) {
  XrHapticActionInfo info = {
    .type = XR_TYPE_HAPTIC_ACTION_INFO,
    .action = state.actions[ACTION_VIBRATE],
    .subactionPath = getActionFilter(device)
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

static struct ModelData* openxr_newModelData(Device device) {
  return NULL;
}

static void openxr_renderTo(void (*callback)(void*), void* userdata) {
  if (!SESSION_SYNCHRONIZED(state.sessionState)) { return; }

  XrFrameBeginInfo beginInfo = { XR_TYPE_FRAME_BEGIN_INFO, NULL };
  XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO, NULL, state.frameState.predictedDisplayTime, XR_ENVIRONMENT_BLEND_MODE_OPAQUE, 0, NULL };

  XR(xrBeginFrame(state.session, &beginInfo));

  if (state.frameState.shouldRender) {
    uint32_t imageIndex;
    XR(xrAcquireSwapchainImage(state.swapchain, NULL, &imageIndex));
    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .timeout = 1e9 };

    if (XR(xrWaitSwapchainImage(state.swapchain, &waitInfo)) != XR_TIMEOUT_EXPIRED) {
      if (!state.canvas) {
        CanvasFlags flags = { .depth = { true, false, FORMAT_D24S8 }, .stereo = true, .mipmaps = true, .msaa = state.msaa };
        state.canvas = lovrCanvasCreate(state.width, state.height, flags);
        lovrPlatformSetSwapInterval(0);
      }

      Camera camera = { .canvas = state.canvas, .stereo = true };

      uint32_t count;
      XrView views[2];
      getViews(views, &count);

      for (int eye = 0; eye < 2; eye++) {
        XrView* view = &views[eye];
        XrVector3f* v = &view->pose.position;
        XrQuaternionf* q = &view->pose.orientation;
        XrFovf* fov = &view->fov;
        float left = tanf(fov->angleLeft);
        float right = tanf(fov->angleRight);
        float up = tanf(fov->angleUp);
        float down = tanf(fov->angleDown);
        mat4_fov(camera.projection[eye], left, right, up, down, state.clipNear, state.clipFar);
        mat4_identity(camera.viewMatrix[eye]);
        mat4_translate(camera.viewMatrix[eye], v->x, v->y, v->z);
        mat4_rotateQuat(camera.viewMatrix[eye], (float[4]) { q->x, q->y, q->z, q->w });
        mat4_invert(camera.viewMatrix[eye]);
      }

      lovrCanvasSetAttachments(state.canvas, &(Attachment) { state.textures[imageIndex], 0, 0 }, 1);
      lovrGraphicsSetCamera(&camera, true);
      callback(userdata);
      lovrGraphicsSetCamera(NULL, false);

      state.layerViews[0].pose = views[0].pose;
      state.layerViews[0].fov = views[0].fov;
      state.layerViews[1].pose = views[1].pose;
      state.layerViews[1].fov = views[1].fov;
      endInfo.layerCount = 1;
      endInfo.layers = (const XrCompositionLayerBaseHeader**) &state.layers;
    }

    XR(xrReleaseSwapchainImage(state.swapchain, NULL));
  }

  XR(xrEndFrame(state.session, &endInfo));
}

static void openxr_update(float dt) {
  if (SESSION_SYNCHRONIZED(state.sessionState)) {
    XR(xrWaitFrame(state.session, NULL, &state.frameState));

    XrActionsSyncInfo syncInfo = {
      .type = XR_TYPE_ACTIONS_SYNC_INFO,
      .countActiveActionSets = 1,
      .activeActionSets = (XrActiveActionSet[]) {
        { state.actionSet, state.actionFilters[0] },
        { state.actionSet, state.actionFilters[1] }
      }
    };

    XR(xrSyncActions(state.session, &syncInfo));
  }

  // Not using designated initializers here to avoid an implicit 4k zero
  XrEventDataBuffer e;
  e.type = XR_TYPE_EVENT_DATA_BUFFER;
  e.next = NULL;

  while (XR_SUCCEEDED(xrPollEvent(state.instance, &e))) {
    switch (e.type) {
      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
        XrEventDataSessionStateChanged* event = (XrEventDataSessionStateChanged*) &e;
        state.sessionState = event->state;

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
            lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit = { false, 0 } });
            break;

          default: break;
        }
        break;
      }

      default: break;
    }
  }
}

HeadsetInterface lovrHeadsetOpenXRDriver = {
  .driverType = DRIVER_OPENXR,
  .init = openxr_init,
  .destroy = openxr_destroy,
  .getName = openxr_getName,
  .getOriginType = openxr_getOriginType,
  .getDisplayDimensions = openxr_getDisplayDimensions,
  .getDisplayMask = openxr_getDisplayMask,
  .getDisplayTime = openxr_getDisplayTime,
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
  .vibrate = openxr_vibrate,
  .newModelData = openxr_newModelData,
  .renderTo = openxr_renderTo,
  .update = openxr_update
};
