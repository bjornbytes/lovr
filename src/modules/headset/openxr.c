#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "graphics/texture.h"
#include "graphics/opengl.h"
#include "util.h"
#include <math.h>

#ifdef _WIN32
#define XR_USE_PLATFORM_WIN32
#define GRAPIHCS_BINDING { XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR, NULL, lovrPlatformGetWindow(), lovrPlatformGetContext() }
#elif __linux__
#else
#define GRAPHICS_BINDING NULL
#endif

#define XR_USE_GRAPHICS_API_OPENGL
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
#define XR_INIT(f) if (XR_FAILED(f)) return destroy(), false;
#define SESSION_VISIBLE(s) (s == XR_SESSION_STATE_VISIBLE || s == XR_SESSION_STATE_FOCUSED)
#define SESSION_RUNNING(s) (s == XR_SESSION_STATE_RUNNING || SESSION_VISIBLE(s))
#define MAX_IMAGES 4

enum {
  PROFILE_SIMPLE,
  PROFILE_VIVE,
  PROFILE_TOUCH,
  PROFILE_GO,
  PROFILE_KNUCKLES,
  MAX_PROFILES
} Profile;

const char* profilePaths[MAX_PROFILES] = {
  [PROFILE_SIMPLE] = "/interaction_profiles/khr/simple_controller",
  [PROFILE_VIVE] = "/interaction_profiles/htc/vive_controller",
  [PROFILE_TOUCH] = "/interaction_profiles/oculus/touch_controller",
  [PROFILE_GO] = "/interaction_profiles/oculus/go_controller",
  [PROFILE_KNUCKLES] = "/interaction_profiles/valve/knuckles_controller"
};

typedef enum {
  ACTION_HAND_POSE,
  ACTION_TRIGGER_DOWN,
  ACTION_TRIGGER_TOUCH,
  ACTION_TRIGGER_AXIS,
  ACTION_TRACKPAD_DOWN,
  ACTION_TRACKPAD_TOUCH,
  ACTION_TRACKPAD_AXIS,
  ACTION_MENU_DOWN,
  ACTION_MENU_TOUCH,
  ACTION_GRIP_DOWN,
  ACTION_GRIP_TOUCH,
  ACTION_GRIP_AXIS,
  ACTION_VIBRATE
} Action;
#define MAX_ACTIONS 14

#define action(id, name, type, subactions) { XR_TYPE_ACTION_CREATE_INFO, NULL, id, type, subactions, NULL, name }
static XrActionCreateInfo defaultActions[MAX_ACTIONS] = {
  [ACTION_HAND_POSE] = action("handPose", "Hand Pose", XR_INPUT_ACTION_TYPE_POSE, 2),
  [ACTION_TRIGGER_DOWN] = action("triggerDown", "Trigger Down", XR_INPUT_ACTION_TYPE_BOOLEAN, 2),
  [ACTION_TRIGGER_TOUCH] = action("triggerTouch", "Trigger Touch", XR_INPUT_ACTION_TYPE_BOOLEAN, 2),
  [ACTION_TRIGGER_AXIS] = action("triggerAxis", "Trigger Axis", XR_INPUT_ACTION_TYPE_VECTOR1F, 2),
  [ACTION_TRACKPAD_AXIS] = action("trackpadAxis", "Trackpad Axis", XR_INPUT_ACTION_TYPE_VECTOR2F, 2),
  [ACTION_MENU_DOWN] = action("menuDown", "Menu Down", XR_INPUT_ACTION_TYPE_BOOLEAN, 2),
  [ACTION_MENU_TOUCH] = action("menuTouch", "Menu Touch", XR_INPUT_ACTION_TYPE_BOOLEAN, 2),
  [ACTION_GRIP_DOWN] = action("gripDown", "Grip Down", XR_INPUT_ACTION_TYPE_BOOLEAN, 2),
  [ACTION_GRIP_TOUCH] = action("gripTouch", "Grip Touch", XR_INPUT_ACTION_TYPE_BOOLEAN, 2),
  [ACTION_GRIP_AXIS] = action("gripAxis", "Grip Axis", XR_INPUT_ACTION_TYPE_VECTOR1F, 2),
  [ACTION_VIBRATE] = action("vibrate", "Vibrate", XR_OUTPUT_ACTION_TYPE_VIBRATION, 2),
};
#undef action

static const char* defaultBindings[MAX_PROFILES][MAX_ACTIONS][2] = {
  [PROFILE_SIMPLE] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/pointer/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/pointer/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/select/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/select/click",
    [ACTION_MENU_DOWN][0] = "/user/hand/left/input/menu/click",
    [ACTION_MENU_DOWN][1] = "/user/hand/right/input/menu/click",
    [ACTION_VIBRATE][0] = "/user/hand/left/output/vibrate",
    [ACTION_VIBRATE][1] = "/user/hand/right/output/vibrate"
  },
  [PROFILE_VIVE] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/pointer/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/pointer/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/trigger/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/trigger/click",
    [ACTION_TRIGGER_AXIS][0] = "/user/hand/left/input/trigger/value",
    [ACTION_TRIGGER_AXIS][1] = "/user/hand/right/input/trigger/value",
    [ACTION_TRACKPAD_AXIS][0] = "/user/hand/left/input/trackpad",
    [ACTION_TRACKPAD_AXIS][1] = "/user/hand/right/input/trackpad",
    [ACTION_MENU_DOWN][0] = "/user/hand/left/input/menu/click",
    [ACTION_MENU_DOWN][1] = "/user/hand/right/input/menu/click",
    [ACTION_GRIP_DOWN][0] = "/user/hand/left/input/grip/click",
    [ACTION_GRIP_DOWN][1] = "/user/hand/right/input/grip/click",
    [ACTION_VIBRATE][0] = "/user/hand/left/output/vibrate",
    [ACTION_VIBRATE][1] = "/user/hand/right/output/vibrate"
  },
  [PROFILE_TOUCH] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/pointer/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/pointer/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/trigger/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/trigger/click",
    [ACTION_TRIGGER_TOUCH][0] = "/user/hand/left/input/trigger/touch",
    [ACTION_TRIGGER_TOUCH][1] = "/user/hand/right/input/trigger/touch",
    [ACTION_TRIGGER_AXIS][0] = "/user/hand/left/input/trigger/value",
    [ACTION_TRIGGER_AXIS][1] = "/user/hand/right/input/trigger/value",
    [ACTION_TRACKPAD_AXIS][0] = "/user/hand/left/input/trackpad",
    [ACTION_TRACKPAD_AXIS][1] = "/user/hand/right/input/trackpad",
    [ACTION_MENU_DOWN][0] = "/user/hand/left/input/menu/click",
    [ACTION_MENU_DOWN][1] = "/user/hand/right/input/menu/click",
    [ACTION_MENU_TOUCH][0] = "/user/hand/left/input/menu/touch",
    [ACTION_MENU_TOUCH][1] = "/user/hand/right/input/menu/touch",
    [ACTION_GRIP_DOWN][0] = "/user/hand/left/input/grip/click",
    [ACTION_GRIP_DOWN][1] = "/user/hand/right/input/grip/click",
    [ACTION_GRIP_TOUCH][0] = "/user/hand/left/input/grip/touch",
    [ACTION_GRIP_TOUCH][1] = "/user/hand/right/input/grip/touch",
    [ACTION_GRIP_AXIS][0] = "/user/hand/left/input/grip/value",
    [ACTION_GRIP_AXIS][1] = "/user/hand/right/input/grip/value",
    [ACTION_VIBRATE][0] = "/user/hand/left/output/vibrate",
    [ACTION_VIBRATE][1] = "/user/hand/right/output/vibrate"
  },
  [PROFILE_GO] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/pointer/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/pointer/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/trigger/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/trigger/click",
    [ACTION_TRACKPAD_AXIS][0] = "/user/hand/left/input/trackpad",
    [ACTION_TRACKPAD_AXIS][1] = "/user/hand/right/input/trackpad"
  },
  [PROFILE_KNUCKLES] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/pointer/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/pointer/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/trigger/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/trigger/click",
    [ACTION_TRIGGER_TOUCH][0] = "/user/hand/left/input/trigger/touch",
    [ACTION_TRIGGER_TOUCH][1] = "/user/hand/right/input/trigger/touch",
    [ACTION_TRIGGER_AXIS][0] = "/user/hand/left/input/trigger/value",
    [ACTION_TRIGGER_AXIS][1] = "/user/hand/right/input/trigger/value",
    [ACTION_TRACKPAD_AXIS][0] = "/user/hand/left/input/trackpad",
    [ACTION_TRACKPAD_AXIS][1] = "/user/hand/right/input/trackpad",
    [ACTION_GRIP_AXIS][0] = "/user/hand/left/input/grip/value",
    [ACTION_GRIP_AXIS][1] = "/user/hand/right/input/grip/value",
    [ACTION_VIBRATE][0] = "/user/hand/left/output/vibrate",
    [ACTION_VIBRATE][1] = "/user/hand/right/output/vibrate",
  }
};

static struct {
  XrInstance instance;
  XrSystemId system;
  XrSession session;
  XrSessionState sessionState;
  XrSpace space;
  XrSpace headSpace;
  XrSpace leftHandSpace;
  XrSpace rightHandSpace;
  XrReferenceSpaceType spaceType;
  XrSwapchain swapchain;
  XrCompositionLayerProjection layers[1];
  XrCompositionLayerProjectionView layerViews[2];
  XrTime displayTime;
  Canvas* canvas;
  Texture textures[MAX_IMAGES];
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
      .applicationInfo.engineVersion = LOVR_VERSION_MAJOR * 1000 + LOVR_VERSION_MINOR,
      .enabledExtensionCount = 1,
      .enabledExtensionNames = (const char*[1]) { "XR_KHR_opengl_enable" }
    };

    XR_INIT(xrCreateInstance(&info, &state.instance));
  }

  { // System
    XrSystemGetInfo info = { XR_TYPE_SYSTEM_GET_INFO, NULL, XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY };
    XR_INIT(xrGetSystem(state.instance, &info, &state.system));

    uint32_t viewCount;
    XrViewConfigurationView views[2];
    XR_INIT(xrEnumerateViewConfigurationViews(state.instance, state.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewCount, views));

    if ( // Only 2 views are supported, and since they're rendered together they must be identical
      viewCount != 2 ||
      views[0].recommendedSwapchainSampleCount != views[1].recommendedSwapchainSampleCount ||
      views[0].recommendedImageRectWidth != views[1].recommendedImageRectWidth ||
      views[0].recommendedImageRectHeight != views[1].recommendedImageRectHeight
    ) {
      return destroy(), false;
    }

    state.msaa = views[0].recommendedSwapchainSampleCount;
    state.width = views[0].recommendedImageRectWidth;
    state.height = views[0].recommendedImageRectHeight;
  }

  { // Session
    XrSessionCreateInfo createInfo = { XR_TYPE_SESSION_CREATE_INFO, GRAPHICS_BINDING, 0, state.system };
    XR_INIT(xrCreateSession(state.instance, &createInfo, &state.session));
  }

  { // Spaaaaace
    XrReferenceSpaceCreateInfo info = {
      .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
      .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE
    };

    // First try to create a stage space, then fall back to local space (head-level)
    if (XR_FAILED(xrCreateReferenceSpace(state.session, &info, &state.space))) {
      info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
      info.poseInReferenceSpace.position.y = -offset;
      XR_INIT(xrCreateReferenceSpace(state.session, &info, &state.space));
    }

    // For getOriginType
    state.spaceType = info.referenceSpaceType;
  }

  { // Swapchain
    XrSwapchainCreateInfo info = {
      .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
      .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
      .format = GL_RGBA8,
      .sampleCount = state.msaa,
      .width = state.width * 2,
      .height = state.height,
      .faceCount = 1,
      .arraySize = 1,
      .mipCount = log2(MAX(state.width, state.height) + 1)
    };

    XrSwapchainImageOpenGLKHR images[MAX_IMAGES];
    XR_INIT(xrCreateSwapchain(state.session, &info, &state.swapchain));
    XR_INIT(xrEnumerateSwapchainImages(state.swapchain, MAX_IMAGES, &state.imageCount, (XrSwapchainImageBaseHeader*) images));

    for (uint32_t i = 0; i < state.imageCount; i++) {
      lovrTextureInitFromHandle(&state.textures[i], images[i].image, TEXTURE_2D, 1);
    }

    // Pre-init composition layer
    state.layers[0] = (XrCompositionLayerProjection) {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
      .space = state.space,
      .viewCount = 2,
      .views = state.layerViews
    };

    // Pre-init composition layer views
    state.layerViews[0] = state.layerViews[1] = (XrCompositionLayerProjectionView) {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
      .subImage = { state.swapchain, { { 0, 0 }, { state.width, state.height } }, 0 }
    };

    // Offset the right view for side-by-side submission
    state.layerViews[1].subImage.imageRect.offset.x += state.width;
  }

  { // Actions
    XrActionSetCreateInfo info = {
      .type = XR_TYPE_ACTION_SET_CREATE_INFO,
      .actionSetName = "default",
      .localizedActionSetName = "Default",
      .priority = 0
    };

    XR_INIT(xrCreateActionSet(state.session, &info, &state.actionSet));
    XR_INIT(xrStringToPath(state.instance, "/user/hand/left", &state.actionFilters[0]));
    XR_INIT(xrStringToPath(state.instance, "/user/hand/right", &state.actionFilters[1]));

    for (int action = 0; action < MAX_ACTIONS; action++) {
      defaultActions[action].subactionPaths = defaultActions[action].countSubactionPaths == 2 ? state.actionFilters : NULL;
      XR_INIT(xrCreateAction(state.actionSet, &defaultActions[action], &state.actions[0]));
    }

    XrActionSuggestedBinding bindings[2 * MAX_ACTIONS];
    for (int profile = 0; profile < MAX_PROFILES; profile++) {
      int bindingCount = 0;

      for (int action = 0; action < MAX_ACTIONS; action++) {
        for (int i = 0; i < 2; i++) {
          if (defaultBindings[profile][action][i]) {
            bindings[bindingCount].action = state.actions[action];
            XR_INIT(xrStringToPath(state.instance, defaultBindings[profile][action][i], &bindings[bindingCount].binding));
            bindingCount++;
          }
        }
      }

      XrPath profilePath;
      XR_INIT(xrStringToPath(state.instance, profilePaths[profile], &profilePath));
      XR_INIT(xrSetInteractionProfileSuggestedBindings(state.session, &(XrInteractionProfileSuggestedBinding) {
        .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
        .interactionProfile = profilePath,
        .countSuggestedBindings = bindingCount,
        .suggestedBindings = bindings
      }));
    }

    XrReferenceSpaceCreateInfo headSpaceInfo = {
      .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
      .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW
    };

    XrActionSpaceCreateInfo leftHandSpaceInfo = {
      .type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
      .subactionPath = state.actionFilters[0]
    };

    XrActionSpaceCreateInfo rightHandSpaceInfo = {
      .type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
      .subactionPath = state.actionFilters[1]
    };

    XR_INIT(xrCreateReferenceSpace(state.session, &headSpaceInfo, &state.headSpace));
    XR_INIT(xrCreateActionSpace(state.actions[ACTION_HAND_POSE], &leftHandSpaceInfo, &state.leftHandSpace));
    XR_INIT(xrCreateActionSpace(state.actions[ACTION_HAND_POSE], &rightHandSpaceInfo, &state.rightHandSpace));
  }

  return true;
}

static void openxr_destroy() {
  lovrRelease(Canvas, state.canvas);
  for (uint32_t i = 0; i < state.imageCount; i++) {
    lovrRelease(Texture, &state.textures[i]);
  }

  for (size_t i = 0; i < MAX_ACTIONS; i++) {
    xrDestroyAction(state.actions[i]);
  }

  xrDestroyActionSet(state.actionSet);
  xrDestroySwapchain(state.swapchain);
  xrDestroySpace(state.rightHandSpace);
  xrDestroySpace(state.leftHandSpace);
  xrDestroySpace(state.headSpace);
  xrDestroySpace(state.space);
  xrEndSession(state.session);
  xrDestroyInstance(state.instance);
}

static bool openxr_getName(char* name, size_t length) {
  XrSystemProperties properties;
  XR(xrGetSystemProperties(state.instance, state.system, &properties));
  strncpy(name, properties.systemName, length - 1);
  name[length - 1] = '\0';
  return true;
}

static HeadsetOrigin openxr_getOriginType() {
  return state.spaceType == XR_REFERENCE_SPACE_TYPE_STAGE ? ORIGIN_FLOOR : ORIGIN_HEAD;
}

static void openxr_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  *width = state.width;
  *height = state.height;
}

static const float* openxr_getDisplayMask(uint32_t* count) {
  return *count = 0, NULL;
}

static double openxr_getDisplayTime(void) {
  return state.displayTime / 1e9;
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
  XR(xrGetReferenceSpaceBoundsRect(state.session, state.spaceType, &bounds));
  *width = bounds.width;
  *depth = bounds.height;
}

static const float* openxr_getBoundsGeometry(uint32_t* count) {
  *count = 0;
  return NULL;
}

static bool getRelation(Device device, XrSpaceRelation* relation) {
  XrSpace space;

  switch (device) {
    case DEVICE_HEAD: space = state.headSpace; break;
    case DEVICE_HAND_LEFT: space = state.leftHandSpace; break;
    case DEVICE_HAND_RIGHT: space = state.rightHandSpace; break;
    default: return false;
  }

  XR(xrLocateSpace(space, state.space, state.displayTime, relation));
  return true;
}

static bool openxr_getPose(Device device, vec3 position, quat orientation) {
  XrSpaceRelation relation;

  if (getRelation(device, &relation) && (relation.relationFlags & (XR_SPACE_RELATION_POSITION_VALID_BIT | XR_SPACE_RELATION_ORIENTATION_VALID_BIT))) {
    XrPosef* pose = &relation.pose;
    vec3_set(position, pose->position.x, pose->position.y, pose->position.z);
    quat_set(orientation, pose->orientation.x, pose->orientation.y, pose->orientation.z, pose->orientation.w);
    return true;
  }

  return false;
}

static bool openxr_getBonePose(Device device, DeviceBone bone, vec3 position, quat orientation) {
  return false;
}

static bool openxr_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  XrSpaceRelation relation;

  if (getRelation(device, &relation) && (relation.relationFlags & (XR_SPACE_RELATION_LINEAR_VELOCITY_VALID_BIT | XR_SPACE_RELATION_ANGULAR_VELOCITY_VALID_BIT))) {
    vec3_set(velocity, relation.linearVelocity.x, relation.linearVelocity.y, relation.linearVelocity.z);
    vec3_set(angularVelocity, relation.angularVelocity.x, relation.angularVelocity.y, relation.angularVelocity.z);
    return true;
  }

  return false;
}

static bool openxr_getAcceleration(Device device, vec3 acceleration, vec3 angularAcceleration) {
  XrSpaceRelation relation;

  if (getRelation(device, &relation) && (relation.relationFlags & (XR_SPACE_RELATION_LINEAR_ACCELERATION_VALID_BIT | XR_SPACE_RELATION_ANGULAR_ACCELERATION_VALID_BIT))) {
    vec3_set(acceleration, relation.linearAcceleration.x, relation.linearAcceleration.y, relation.linearAcceleration.z);
    vec3_set(angularAcceleration, relation.angularAcceleration.x, relation.angularAcceleration.y, relation.angularAcceleration.z);
    return true;
  }

  return false;
}

static XrPath getActionFilter(Device device) {
  switch (device) {
    case DEVICE_HAND_LEFT: return state.actionFilters[0];
    case DEVICE_HAND_RIGHT: return state.actionFilters[1];
    default: return XR_NULL_PATH;
  }
}

static bool getButtonState(Device device, DeviceButton button, bool* value, bool touch) {
  XrPath filter = getActionFilter(device);

  if (filter == XR_NULL_PATH) {
    return false;
  }

  Action type;
  switch (button) {
    case BUTTON_TRIGGER: type = ACTION_TRIGGER_DOWN + touch; break;
    case BUTTON_TOUCHPAD: type = ACTION_TRACKPAD_DOWN + touch; break;
    case BUTTON_MENU: type = ACTION_MENU_DOWN + touch; break;
    case BUTTON_GRIP: type = ACTION_GRIP_DOWN + touch; break;
    default: return false;
  }

  XrActionStateBoolean actionState;
  XR(xrGetActionStateBoolean(state.actions[type], 1, &filter, &actionState));
  *value = actionState.currentState;
  return actionState.isActive;
}

static bool openxr_isDown(Device device, DeviceButton button, bool* down) {
  return getButtonState(device, button, down, false);
}

static bool openxr_isTouched(Device device, DeviceButton button, bool* touched) {
  return getButtonState(device, button, touched, true);
}

static bool openxr_getAxis(Device device, DeviceAxis axis, float* value) {
  XrPath filter = getActionFilter(device);

  if (filter == XR_NULL_PATH) {
    return false;
  }

  Action type;
  switch (axis) {
    case AXIS_TRIGGER: type = ACTION_TRIGGER_AXIS; break;
    case AXIS_TOUCHPAD: type = ACTION_TRACKPAD_AXIS; break;
    case AXIS_GRIP: type = ACTION_GRIP_AXIS; break;
    default: return false;
  }

  XrActionStateVector1f actionState;
  XR(xrGetActionStateVector1f(state.actions[type], 1, &filter, &actionState));
  *value = actionState.currentState;
  return true;
}

static bool openxr_vibrate(Device device, float power, float duration, float frequency) {
  XrPath filter = getActionFilter(device);

  if (filter == XR_NULL_PATH) {
    return false;
  }

  XrHapticVibration vibration = {
    .type = XR_TYPE_HAPTIC_VIBRATION,
    .duration = (int64_t) (duration * 1e9f + .5f),
    .frequency = frequency,
    .amplitude = power
  };

  XR(xrApplyHapticFeedback(state.actions[ACTION_VIBRATE], 1, &filter, (XrHapticBaseHeader*) &vibration));
  return true;
}

static struct ModelData* openxr_newModelData(Device device) {
  return NULL;
}

static void openxr_renderTo(void (*callback)(void*), void* userdata) {
  if (!SESSION_RUNNING(state.sessionState)) { return; }

  XrFrameBeginInfo beginInfo = { XR_TYPE_FRAME_BEGIN_INFO, NULL };
  XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO, NULL, state.displayTime, XR_ENVIRONMENT_BLEND_MODE_OPAQUE, 0, NULL };

  XR(xrBeginFrame(state.session, &beginInfo));

  if (SESSION_VISIBLE(state.sessionState)) {
    uint32_t imageIndex;
    XR(xrAcquireSwapchainImage(state.swapchain, NULL, &imageIndex));
    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .timeout = 1e9 };

    if (XR(xrWaitSwapchainImage(state.swapchain, &waitInfo)) != XR_TIMEOUT_EXPIRED) {
      XrView views[2];
      XrViewState viewState;
      XrViewLocateInfo viewLocateInfo = { XR_TYPE_VIEW_LOCATE_INFO, NULL, state.displayTime, state.space };
      XR(xrLocateViews(state.session, &viewLocateInfo, &viewState, 2, NULL, views));

      if (!state.canvas) {
        CanvasFlags flags = { .depth = { true, false, FORMAT_D24S8 }, .stereo = true, .mipmaps = true, .msaa = state.msaa };
        state.canvas = lovrCanvasCreate(state.width, state.height, flags);
        lovrPlatformSetSwapInterval(0);
      }

      Camera camera = { .canvas = state.canvas, .stereo = true };

      for (int eye = 0; eye < 2; eye++) {
        XrView* view = &views[eye];
        XrVector3f* v = &view->pose.position;
        XrQuaternionf* q = &view->pose.orientation;
        XrFovf* fov = &view->fov;
        mat4_fov(camera.projection[eye], fov->angleLeft, fov->angleRight, fov->angleUp, fov->angleDown, state.clipNear, state.clipFar);
        mat4_identity(camera.viewMatrix[eye]);
        mat4_translate(camera.viewMatrix[eye], v->x, v->y, v->z);
        mat4_rotateQuat(camera.viewMatrix[eye], (float[4]) { q->x, q->y, q->z, q->w });
        mat4_invert(camera.viewMatrix[eye]);
      }

      lovrCanvasSetAttachments(state.canvas, &(Attachment) { &state.textures[imageIndex], 0, 0 }, 1);
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
  if (SESSION_RUNNING(state.sessionState)) {
    XrFrameState frameState;
    XR(xrWaitFrame(state.session, NULL, &frameState));
    state.displayTime = frameState.predictedDisplayTime;

    XrActiveActionSet set = { XR_TYPE_ACTIVE_ACTION_SET, NULL, state.actionSet, XR_NULL_PATH };
    XR(xrSyncActionData(state.session, 1, &set));
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
  .getClipDistance = openxr_getClipDistance,
  .setClipDistance = openxr_setClipDistance,
  .getBoundsDimensions = openxr_getBoundsDimensions,
  .getBoundsGeometry = openxr_getBoundsGeometry,
  .getPose = openxr_getPose,
  .getBonePose = openxr_getBonePose,
  .getVelocity = openxr_getVelocity,
  .getAcceleration = openxr_getAcceleration,
  .isDown = openxr_isDown,
  .isTouched = openxr_isTouched,
  .getAxis = openxr_getAxis,
  .vibrate = openxr_vibrate,
  .newModelData = openxr_newModelData,
  .renderTo = openxr_renderTo,
  .update = openxr_update
};
