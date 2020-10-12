#include "headset/headset.h"
#include "event/event.h"
#include "filesystem/filesystem.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "graphics/texture.h"
#include "core/ref.h"
#include "core/util.h"
#include <stdlib.h>
#include <math.h>
#if defined(_WIN32)
  #define XR_USE_PLATFORM_WIN32
  #include <windows.h>
#elif defined(__ANDROID__)
  #define XR_USE_PLATFORM_ANDROID
  #include <android_native_app_glue.h>
  #include <EGL/egl.h>
  #include <jni.h>
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
#ifdef __ANDROID__
#include <openxr/openxr_oculus.h>
#endif
#include "resources/openxr_actions.h"

#define XR(f) handleResult(f, __FILE__, __LINE__)
#define XR_INIT(f) if (XR_FAILED(f)) return openxr_destroy(), false;
#define SESSION_ACTIVE(s) (s >= XR_SESSION_STATE_READY && s <= XR_SESSION_STATE_FOCUSED)
#define GL_SRGB8_ALPHA8 0x8C43
#define MAX_IMAGES 4

#if defined(_WIN32)
HANDLE lovrPlatformGetWindow(void);
HGLRC lovrPlatformGetContext(void);
#elif defined(__ANDROID__)
struct ANativeActivity* lovrPlatformGetActivity(void);
EGLDisplay lovrPlatformGetEGLDisplay(void);
EGLContext lovrPlatformGetEGLContext(void);
EGLConfig lovrPlatformGetEGLConfig(void);
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
  X(xrSyncActions)\
  X(xrApplyHapticFeedback)\
  X(xrCreateHandTrackerEXT)\
  X(xrDestroyHandTrackerEXT)\
  X(xrLocateHandJointsEXT)

#define XR_DECLARE(fn) static PFN_##fn fn;
#define XR_LOAD(fn) xrGetInstanceProcAddr(state.instance, #fn, (PFN_xrVoidFunction*) &fn);
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function);
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char* layerName, uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrExtensionProperties* properties);
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo* createInfo, XrInstance* instance);
XR_FOREACH(XR_DECLARE)

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
  uint32_t imageIndex;
  uint32_t imageCount;
  uint32_t msaa;
  uint32_t width;
  uint32_t height;
  float clipNear;
  float clipFar;
  XrActionSet actionSet;
  XrAction actions[MAX_ACTIONS];
  XrPath actionFilters[2];
  XrHandTrackerEXT handTrackers[2];
  struct {
    bool handTracking;
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

static void openxr_destroy();

static bool openxr_init(float supersample, float offset, uint32_t msaa) {
  state.msaa = msaa;

#ifdef __ANDROID__
  static PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
  XR_LOAD(xrInitializeLoaderKHR);
  if (!xrInitializeLoaderKHR) {
    return false;
  }

  ANativeActivity* activity = lovrPlatformGetActivity();
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
    xrEnumerateInstanceExtensionProperties(NULL, 0, &extensionCount, NULL);
    XrExtensionProperties* extensions = calloc(extensionCount, sizeof(*extensions));
    lovrAssert(extensions, "Out of memory");
    for (uint32_t i = 0; i < extensionCount; i++) extensions[i].type = XR_TYPE_EXTENSION_PROPERTIES;
    xrEnumerateInstanceExtensionProperties(NULL, 32, &extensionCount, extensions);

    const char* enabledExtensionNames[4];
    uint32_t enabledExtensionCount = 0;

#ifdef __ANDROID__
    enabledExtensionNames[enabledExtensionCount++] = XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME;
#endif

    enabledExtensionNames[enabledExtensionCount++] = GRAPHICS_EXTENSION;

    if (hasExtension(extensions, extensionCount, XR_EXT_HAND_TRACKING_EXTENSION_NAME)) {
      enabledExtensionNames[enabledExtensionCount++] = XR_EXT_HAND_TRACKING_EXTENSION_NAME;
      state.features.handTracking = true;
    }

    free(extensions);

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

    XrSystemHandTrackingPropertiesEXT handTrackingProperties = {
      .type = XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT,
      .supportsHandTracking = false
    };

    XrSystemProperties properties = {
      .type = XR_TYPE_SYSTEM_PROPERTIES,
      .next = state.features.handTracking ? &handTrackingProperties : NULL
    };

    XR_INIT(xrGetSystemProperties(state.instance, state.system, &properties));
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
      .actionSetName = "default",
      .localizedActionSetName = "Default",
      .priority = 0
    };

    XR_INIT(xrCreateActionSet(state.instance, &info, &state.actionSet));
    XR_INIT(xrStringToPath(state.instance, "/user/hand/left", &state.actionFilters[0]));
    XR_INIT(xrStringToPath(state.instance, "/user/hand/right", &state.actionFilters[1]));

    for (uint32_t i = 0; i < MAX_ACTIONS; i++) {
      actionCreateInfo[i].subactionPaths = state.actionFilters;
      XR_INIT(xrCreateAction(state.actionSet, &actionCreateInfo[i], &state.actions[i]));
    }

    XrActionSuggestedBinding suggestedBindings[2 * MAX_ACTIONS];
    for (uint32_t profile = 0, count = 0; profile < MAX_PROFILES; profile++, count = 0) {
      for (uint32_t action = 0; action < MAX_ACTIONS; action++) {
        for (uint32_t hand = 0; hand < 2; hand++) {
          if (bindings[profile][action][hand]) {
            suggestedBindings[count].action = state.actions[action];
            XR_INIT(xrStringToPath(state.instance, bindings[profile][action][hand], &suggestedBindings[count].binding));
            count++;
          }
        }
      }

      XrPath profilePath;
      XR_INIT(xrStringToPath(state.instance, interactionProfiles[profile], &profilePath));
      XR_INIT(xrSuggestInteractionProfileBindings(state.instance, &(XrInteractionProfileSuggestedBinding) {
        .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
        .interactionProfile = profilePath,
        .countSuggestedBindings = count,
        .suggestedBindings = suggestedBindings
      }));
    }
  }

  { // Session
#if defined(LOVR_GL)
    XrGraphicsRequirementsOpenGLKHR requirements = { .type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR, NULL };
    PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR;
    XR_LOAD(xrGetOpenGLGraphicsRequirementsKHR);
    XR_INIT(xrGetOpenGLGraphicsRequirementsKHR(state.instance, state.system, &requirements));
    // TODO validate OpenGL versions
#elif defined(LOVR_GLES)
    XrGraphicsRequirementsOpenGLESKHR requirements = { .type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR, NULL };
    PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR;
    XR_LOAD(xrGetOpenGLESGraphicsRequirementsKHR);
    XR_INIT(xrGetOpenGLESGraphicsRequirementsKHR(state.instance, state.system, &requirements));
    // TODO validate OpenGLES versions
#endif

#if defined(_WIN32) && defined(LOVR_GL)
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding = {
      .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR,
      .hDC = lovrPlatformGetWindow(),
      .hGLRC = lovrPlatformGetContext()
    };
#elif defined(__ANDROID__) && defined(LOVR_GLES)
    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding = {
      .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR,
      .display = lovrPlatformGetEGLDisplay(),
      .config = lovrPlatformGetEGLConfig(),
      .context = lovrPlatformGetEGLContext()
    };
#else
#error "Unsupported OpenXR platform/graphics combination"
#endif

    XrSessionCreateInfo info = {
      .type = XR_TYPE_SESSION_CREATE_INFO,
      .next = &graphicsBinding,
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
      .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE,
      .poseInReferenceSpace = { .orientation = { 0.f, 0.f, 0.f, 1.f }, .position = { 0.f, 0.f, 0.f } }
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
      .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW,
      .poseInReferenceSpace = { .orientation = { 0.f, 0.f, 0.f, 1.f }, .position = { 0.f, 0.f, 0.f } }
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

    XR_INIT(xrCreateSwapchain(state.session, &info, &state.swapchain));
    XR_INIT(xrEnumerateSwapchainImages(state.swapchain, MAX_IMAGES, &state.imageCount, (XrSwapchainImageBaseHeader*) images));

    CanvasFlags flags = { .depth = { true, false, FORMAT_D24S8 }, .stereo = true, .mipmaps = false, .msaa = state.msaa };
    for (uint32_t i = 0; i < state.imageCount; i++) {
      Texture* texture = lovrTextureCreateFromHandle(images[i].image, textureType, arraySize, state.msaa);
      state.canvases[i] = lovrCanvasCreate(state.width, state.height, flags);
      lovrCanvasSetAttachments(state.canvases[i], &(Attachment) { texture, 0, 0 }, 1);
      lovrRelease(Texture, texture);
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

    // Copy the left view to the right view and offset either the viewport or array index
    state.layerViews[1] = state.layerViews[0];
#if defined(XR_USE_GRAPHICS_API_OPENGL)
    state.layerViews[1].subImage.imageRect.offset.x += state.width;
#elif defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    state.layerViews[1].subImage.imageArrayIndex = 1;
#endif
  }

  state.clipNear = .1f;
  state.clipFar = 100.f;

  state.frameState.type = XR_TYPE_FRAME_STATE;
  lovrPlatformSetSwapInterval(0);

  return true;
}

static void openxr_destroy(void) {
  for (uint32_t i = 0; i < state.imageCount; i++) {
    lovrRelease(Canvas, state.canvases[i]);
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
  XrSystemProperties properties;
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

  XrViewState viewState = { .type = XR_TYPE_VIEW_STATE };
  XR(xrLocateViews(state.session, &viewLocateInfo, &viewState, 2 * sizeof(XrView), count, views));
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

  XrSpaceLocation location = { .next = NULL };
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
  XrSpaceLocation location = { .next = &velocity };
  xrLocateSpace(state.spaces[device], state.referenceSpace, state.frameState.predictedDisplayTime, &location);
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

static bool getButtonState(Device device, DeviceButton button, bool* value, bool* changed, bool touch) {
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

static bool getFloatAction(uint32_t action, XrPath filter, float* value) {
  XrActionStateGetInfo info = {
    .type = XR_TYPE_ACTION_STATE_GET_INFO,
    .action = state.actions[action],
    .subactionPath = filter
  };

  XrActionStateFloat actionState;
  XR(xrGetActionStateFloat(state.session, &info, &actionState));
  *value = actionState.currentState;
  return actionState.isActive;
}

static bool openxr_getAxis(Device device, DeviceAxis axis, float* value) {
  XrPath filter = getActionFilter(device);

  if (filter == XR_NULL_PATH) {
    return false;
  }

  switch (axis) {
    case AXIS_TRIGGER: return getFloatAction(ACTION_TRIGGER_AXIS, filter, &value[0]);
    case AXIS_THUMBSTICK: return getFloatAction(ACTION_THUMBSTICK_X, filter, &value[0]) && getFloatAction(ACTION_THUMBSTICK_Y, filter, &value[1]);
    case AXIS_TOUCHPAD: return getFloatAction(ACTION_TRACKPAD_X, filter, &value[0]) && getFloatAction(ACTION_TRACKPAD_Y, filter, &value[1]);
    case AXIS_GRIP: return getFloatAction(ACTION_GRIP_AXIS, filter, &value[0]);
    default: return false;
  }
}

static bool openxr_getSkeleton(Device device, float* poses) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  XrHandTrackerEXT* handTracker = &state.handTrackers[device - DEVICE_HAND_LEFT];

  // Hand trackers are created lazily because on some implementations xrCreateHandTrackerEXT will
  // return XR_ERROR_FEATURE_UNSUPPORTED if called too early.
  if (!*handTracker) {
    XrHandTrackerCreateInfoEXT info = {
      .type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
      .handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT,
      .hand = device == DEVICE_HAND_LEFT ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT
    };

    if (XR_FAILED(xrCreateHandTrackerEXT(state.session, &info, handTracker))) {
      return false;
    }
  }

  XrHandJointsLocateInfoEXT info = {
    .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
    .baseSpace = state.referenceSpace,
    .time = state.frameState.predictedDisplayTime
  };

  XrHandJointLocationEXT joints[26];
  XrHandJointLocationsEXT hand = {
    .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
    .jointCount = sizeof(joints) / sizeof(joints[0]),
    .jointLocations = joints
  };

  if (XR_FAILED(xrLocateHandJointsEXT(*handTracker, &info, &hand)) || !hand.isActive) {
    return false;
  }

  float* pose = poses;
  for (uint32_t i = 0; i < sizeof(joints) / sizeof(joints[0]); i++) {
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

static struct ModelData* openxr_newModelData(Device device, bool animated) {
  return NULL;
}

static bool openxr_animate(Device device, struct Model* model) {
  return false;
}

static void openxr_renderTo(void (*callback)(void*), void* userdata) {
  if (!SESSION_ACTIVE(state.sessionState)) { return; }

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
    XR(xrAcquireSwapchainImage(state.swapchain, NULL, &state.imageIndex));
    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .timeout = 1e9 };

    if (XR(xrWaitSwapchainImage(state.swapchain, &waitInfo)) != XR_TIMEOUT_EXPIRED) {
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
    }

    XR(xrReleaseSwapchainImage(state.swapchain, NULL));
  }

  XR(xrEndFrame(state.session, &endInfo));
  lovrGpuDirtyTexture();
}

static Texture* openxr_getMirrorTexture(void) {
  Canvas* canvas = state.canvases[state.imageIndex];
  return canvas ? lovrCanvasGetAttachments(canvas, NULL)[0].texture : NULL;
}

static void openxr_update(float dt) {
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
  }

  if (SESSION_ACTIVE(state.sessionState)) {
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
  .getSkeleton = openxr_getSkeleton,
  .vibrate = openxr_vibrate,
  .newModelData = openxr_newModelData,
  .animate = openxr_animate,
  .renderTo = openxr_renderTo,
  .getMirrorTexture = openxr_getMirrorTexture,
  .update = openxr_update
};
