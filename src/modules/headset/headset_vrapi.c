#include "headset/headset.h"
#include "event/event.h"
#include "graphics/canvas.h"
#include "graphics/graphics.h"
#include "core/maf.h"
#include "core/os.h"
#include "core/ref.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>
#include <android_native_app_glue.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc11-extensions"
#pragma clang diagnostic ignored "-Wgnu-empty-initializer"
#pragma clang diagnostic ignored "-Wpedantic"
#include <VrApi.h>
#include <VrApi_Helpers.h>
#include <VrApi_Input.h>
#pragma clang diagnostic pop

#define GL_SRGB8_ALPHA8 0x8C43

// Private platform functions
JNIEnv* lovrPlatformGetJNI(void);
struct ANativeActivity* lovrPlatformGetActivity(void);
int lovrPlatformGetActivityState(void);
ANativeWindow* lovrPlatformGetNativeWindow(void);
EGLDisplay lovrPlatformGetEGLDisplay(void);
EGLContext lovrPlatformGetEGLContext(void);

static struct {
  ovrJava java;
  ovrMobile* session;
  ovrDeviceType deviceType;
  uint64_t frameIndex;
  double displayTime;
  float offset;
  uint32_t msaa;
  ovrVector3f* rawBoundaryPoints;
  float* boundaryPoints;
  uint32_t boundaryPointCount;
  ovrTextureSwapChain* swapchain;
  uint32_t swapchainLength;
  uint32_t swapchainIndex;
  Canvas* canvases[4];
  ovrInputTrackedRemoteCapabilities controllerInfo[2];
  ovrInputStateTrackedRemote input[2];
  ovrInputStateHand handInput[2];
  uint32_t changedButtons[2];
  float hapticStrength[2];
  float hapticDuration[2];
} state;

static bool vrapi_init(float offset, uint32_t msaa) {
  ANativeActivity* activity = lovrPlatformGetActivity();
  state.java.Vm = activity->vm;
  state.java.ActivityObject = activity->clazz;
  state.java.Env = lovrPlatformGetJNI();
  state.offset = offset;
  state.msaa = msaa;
  const ovrInitParms config = vrapi_DefaultInitParms(&state.java);
  if (vrapi_Initialize(&config) != VRAPI_INITIALIZE_SUCCESS) {
    return false;
  }
  state.deviceType = vrapi_GetSystemPropertyInt(&state.java, VRAPI_SYS_PROP_DEVICE_TYPE);
  return true;
}

static void vrapi_destroy() {
  if (state.session) {
    vrapi_LeaveVrMode(state.session);
  }
  vrapi_DestroyTextureSwapChain(state.swapchain);
  vrapi_Shutdown();
  for (uint32_t i = 0; i < 3; i++) {
    lovrRelease(Canvas, state.canvases[i]);
  }
  free(state.rawBoundaryPoints);
  free(state.boundaryPoints);
  memset(&state, 0, sizeof(state));
}

static bool vrapi_getName(char* buffer, size_t length) {
  switch (state.deviceType) {
    case VRAPI_DEVICE_TYPE_OCULUSGO: strncpy(buffer, "Oculus Go", length - 1); break;
    case VRAPI_DEVICE_TYPE_OCULUSQUEST: strncpy(buffer, "Oculus Quest", length - 1); break;
    default: return false;
  }
  buffer[length - 1] = '\0';
  return true;
}

static HeadsetOrigin vrapi_getOriginType(void) {
  return vrapi_GetTrackingSpace(state.session) == VRAPI_TRACKING_SPACE_STAGE ? ORIGIN_FLOOR : ORIGIN_HEAD;
}

static void vrapi_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  *width = (uint32_t) vrapi_GetSystemPropertyInt(&state.java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH);
  *height = (uint32_t) vrapi_GetSystemPropertyInt(&state.java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT);
}

static float vrapi_getDisplayFrequency() {
  return vrapi_GetSystemPropertyFloat(&state.java, VRAPI_SYS_PROP_DISPLAY_REFRESH_RATE);
}

static const float* vrapi_getDisplayMask(uint32_t* count) {
  *count = 0;
  return NULL;
}

static double vrapi_getDisplayTime() {
  return state.displayTime;
}

static uint32_t vrapi_getViewCount() {
  return 2;
}

static bool vrapi_getViewPose(uint32_t view, float* position, float* orientation) {
  if (view >= 2) return false;
  ovrTracking2 tracking = vrapi_GetPredictedTracking2(state.session, state.displayTime);
  float transform[16];
  mat4_init(transform, (float*) &tracking.Eye[view].ViewMatrix);
  mat4_invert(transform);
  mat4_getPosition(transform, position);
  mat4_getOrientation(transform, orientation);
  uint32_t mask = VRAPI_TRACKING_STATUS_POSITION_VALID | VRAPI_TRACKING_STATUS_ORIENTATION_VALID;
  return (tracking.Status & mask) == mask;
}

static bool vrapi_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  if (view >= 2) return false;
  ovrTracking2 tracking = vrapi_GetPredictedTracking2(state.session, state.displayTime);
  ovrMatrix4f_ExtractFov(&tracking.Eye[view].ProjectionMatrix, left, right, up, down);
  uint32_t mask = VRAPI_TRACKING_STATUS_POSITION_VALID | VRAPI_TRACKING_STATUS_ORIENTATION_VALID;
  return (tracking.Status & mask) == mask;
}

static void vrapi_getClipDistance(float* clipNear, float* clipFar) {
  *clipNear = *clipFar = 0.f; // Unsupported
}

static void vrapi_setClipDistance(float clipNear, float clipFar) {
  // Unsupported
}

static void vrapi_getBoundsDimensions(float* width, float* depth) {
  ovrPosef pose;
  ovrVector3f scale;
  if (vrapi_GetBoundaryOrientedBoundingBox(state.session, &pose, &scale) == ovrSuccess) {
    *width = scale.x * 2.f;
    *depth = scale.z * 2.f;
  } else {
    *width = 0.f;
    *depth = 0.f;
  }
}

static const float* vrapi_getBoundsGeometry(uint32_t* count) {
  if (vrapi_GetBoundaryGeometry(state.session, 0, count, NULL) != ovrSuccess) {
    return NULL;
  }

  if (*count > state.boundaryPointCount) {
    state.boundaryPointCount = *count;
    state.boundaryPoints = realloc(state.boundaryPoints, 4 * *count * sizeof(float));
    state.rawBoundaryPoints = realloc(state.rawBoundaryPoints, *count * sizeof(ovrVector3f));
    lovrAssert(state.boundaryPoints && state.rawBoundaryPoints, "Out of memory");
  }

  if (vrapi_GetBoundaryGeometry(state.session, state.boundaryPointCount, count, state.rawBoundaryPoints) != ovrSuccess) {
    return NULL;
  }

  for (uint32_t i = 0; i < *count; i++) {
    state.boundaryPoints[4 * i + 0] = state.rawBoundaryPoints[i].x;
    state.boundaryPoints[4 * i + 1] = state.rawBoundaryPoints[i].y;
    state.boundaryPoints[4 * i + 2] = state.rawBoundaryPoints[i].z;
  }

  *count *= 4;
  return state.boundaryPoints;
}

static bool getTracking(Device device, ovrTracking* tracking) {
  if (device == DEVICE_HEAD) {
    *tracking = vrapi_GetPredictedTracking(state.session, state.displayTime);
    return true;
  } else if (device == DEVICE_HAND_LEFT || device == DEVICE_HAND_RIGHT) {
    ovrInputCapabilityHeader* header = &state.controllerInfo[device - DEVICE_HAND_LEFT].Header;
    if (header->Type == ovrControllerType_TrackedRemote) {
      return vrapi_GetInputTrackingState(state.session, header->DeviceID, state.displayTime, tracking) == ovrSuccess;
    }
  }

  return false;
}

static bool vrapi_getPose(Device device, float* position, float* orientation) {
  ovrTracking tracking;
  if (!getTracking(device, &tracking)) {
    return false;
  }

  ovrPosef* pose = &tracking.HeadPose.Pose;
  vec3_set(position, pose->Position.x, pose->Position.y + state.offset, pose->Position.z);
  quat_init(orientation, &pose->Orientation.x);
  return tracking.Status & (VRAPI_TRACKING_STATUS_POSITION_VALID | VRAPI_TRACKING_STATUS_ORIENTATION_VALID);
}

static bool vrapi_getVelocity(Device device, float* velocity, float* angularVelocity) {
  ovrTracking tracking;
  if (!getTracking(device, &tracking)) {
    return false;
  }

  ovrVector3f* linear = &tracking.HeadPose.LinearVelocity;
  ovrVector3f* angular = &tracking.HeadPose.AngularVelocity;
  vec3_set(velocity, linear->x, linear->y, linear->z);
  vec3_set(angularVelocity, angular->x, angular->y, angular->z);
  return tracking.Status & (VRAPI_TRACKING_STATUS_POSITION_VALID | VRAPI_TRACKING_STATUS_ORIENTATION_VALID);
}

static bool vrapi_isDown(Device device, DeviceButton button, bool* down, bool* changed) {
  if (device == DEVICE_HEAD && button == BUTTON_PROXIMITY) {
    *down = vrapi_GetSystemStatusInt(&state.java, VRAPI_SYS_STATUS_MOUNTED);
    return true;
  }

  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  if (state.controllerInfo[device - DEVICE_HAND_LEFT].Header.Type != ovrControllerType_TrackedRemote) {
    return false;
  }

  uint32_t mask;
  if (state.deviceType == VRAPI_DEVICE_TYPE_OCULUSGO) {
    switch (button) {
      case BUTTON_TRIGGER: mask = ovrButton_Trigger; break;
      case BUTTON_TOUCHPAD: mask = ovrButton_Enter; break;
      case BUTTON_MENU: mask = ovrButton_Back; break;
      default: return false;
    }
  } else if (state.deviceType == VRAPI_DEVICE_TYPE_OCULUSQUEST) {
    switch (button) {
      case BUTTON_TRIGGER: mask = ovrButton_Trigger; break;
      case BUTTON_THUMBSTICK: mask = ovrButton_Joystick; break;
      case BUTTON_GRIP: mask = ovrButton_GripTrigger; break;
      case BUTTON_MENU: mask = ovrButton_Enter; break;
      case BUTTON_A: mask = ovrButton_A; break;
      case BUTTON_B: mask = ovrButton_B; break;
      case BUTTON_X: mask = ovrButton_X; break;
      case BUTTON_Y: mask = ovrButton_Y; break;
      default: return false;
    }
  } else {
    return false;
  }

  uint32_t index = device - DEVICE_HAND_LEFT;
  *down = state.input[index].Buttons & mask;
  *changed = state.changedButtons[index] & mask;
  return true;
}

static bool vrapi_isTouched(Device device, DeviceButton button, bool* touched) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  if (state.controllerInfo[device - DEVICE_HAND_LEFT].Header.Type != ovrControllerType_TrackedRemote) {
    return false;
  }

  ovrInputStateTrackedRemote* input = &state.input[device - DEVICE_HAND_LEFT];

  if (state.deviceType == VRAPI_DEVICE_TYPE_OCULUSGO) {
    switch (button) {
      case BUTTON_TOUCHPAD: *touched = input->Touches & ovrTouch_TrackPad; return true;
      default: return false;
    }
  } else if (state.deviceType == VRAPI_DEVICE_TYPE_OCULUSQUEST) {
    switch (button) {
      case BUTTON_TRIGGER: *touched = input->Touches & ovrTouch_IndexTrigger; return true;
      case BUTTON_THUMBSTICK: *touched = input->Touches & ovrTouch_Joystick; return true;
      case BUTTON_A: *touched = input->Touches & ovrTouch_A; return true;
      case BUTTON_B: *touched = input->Touches & ovrTouch_B; return true;
      case BUTTON_X: *touched = input->Touches & ovrTouch_X; return true;
      case BUTTON_Y: *touched = input->Touches & ovrTouch_Y; return true;
      default: return false;
    }
  }

  return false;
}

static bool vrapi_getAxis(Device device, DeviceAxis axis, float* value) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  ovrInputStateTrackedRemote* input = &state.input[device - DEVICE_HAND_LEFT];

  if (state.deviceType == VRAPI_DEVICE_TYPE_OCULUSGO) {
    switch (axis) {
      case AXIS_TOUCHPAD:
        value[0] = (input->TrackpadPosition.x - 160.f) / 160.f;
        value[1] = (input->TrackpadPosition.y - 160.f) / 160.f;
        return true;
      case AXIS_TRIGGER: value[0] = (input->Buttons & ovrButton_Trigger) ? 1.f : 0.f; return true;
      default: return false;
    }
  } else if (state.deviceType == VRAPI_DEVICE_TYPE_OCULUSQUEST) {
    switch (axis) {
      case AXIS_THUMBSTICK:
        value[0] = input->Joystick.x;
        value[1] = input->Joystick.y;
        return true;
      case AXIS_TRIGGER: value[0] = input->IndexTrigger; return true;
      case AXIS_GRIP: value[0] = input->GripTrigger; return true;
      default: return false;
    }
  }

  return false;
}

static bool vrapi_vibrate(Device device, float strength, float duration, float frequency) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  uint32_t index = device - DEVICE_HAND_LEFT;
  state.hapticStrength[index] = CLAMP(strength, 0.0f, 1.0f);
  state.hapticDuration[index] = MAX(duration, 0.0f);
  return true;
}

static struct ModelData* vrapi_newModelData(Device device) {
  return NULL;
}

static void vrapi_renderTo(void (*callback)(void*), void* userdata) {
  if (!state.session) return;

  // Lazily create swapchain and canvases
  if (!state.swapchain) {
    CanvasFlags flags = {
      .depth.enabled = true,
      .depth.readable = false,
      .depth.format = FORMAT_D24S8,
      .msaa = state.msaa,
      .stereo = true,
      .mipmaps = false
    };

    uint32_t width, height;
    vrapi_getDisplayDimensions(&width, &height);
    state.swapchain = vrapi_CreateTextureSwapChain3(VRAPI_TEXTURE_TYPE_2D_ARRAY, GL_SRGB8_ALPHA8, width, height, 1, 3);
    state.swapchainLength = vrapi_GetTextureSwapChainLength(state.swapchain);
    lovrAssert(state.swapchainLength <= sizeof(state.canvases) / sizeof(state.canvases[0]), "VrApi: The swapchain is too long");

    for (uint32_t i = 0; i < state.swapchainLength; i++) {
      state.canvases[i] = lovrCanvasCreate(width, height, flags);
      uint32_t handle = vrapi_GetTextureSwapChainHandle(state.swapchain, i);
      Texture* texture = lovrTextureCreateFromHandle(handle, TEXTURE_ARRAY, 2);
      lovrCanvasSetAttachments(state.canvases[i], &(Attachment) { .texture = texture }, 1);
      lovrRelease(Texture, texture);
    }
  }

  ovrTracking2 tracking = vrapi_GetPredictedTracking2(state.session, state.displayTime);

  // Set up camera
  Camera camera;
  camera.canvas = state.canvases[state.swapchainIndex];
  for (uint32_t i = 0; i < 2; i++) {
    mat4_init(camera.viewMatrix[i], &tracking.Eye[i].ViewMatrix.M[0][0]);
    mat4_init(camera.projection[i], &tracking.Eye[i].ProjectionMatrix.M[0][0]);
    mat4_transpose(camera.projection[i]);
    mat4_transpose(camera.viewMatrix[i]);
    mat4_translate(camera.viewMatrix[i], 0.f, -state.offset, 0.f);
  }

  // Render
  lovrGraphicsSetCamera(&camera, true);
  callback(userdata);
  lovrGraphicsDiscard(false, true, true);
  lovrGraphicsSetCamera(NULL, false);

  // Submit a layer to VrApi
  ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
  layer.HeadPose = tracking.HeadPose;
  layer.Textures[0].ColorSwapChain = state.swapchain;
  layer.Textures[1].ColorSwapChain = state.swapchain;
  layer.Textures[0].SwapChainIndex = state.swapchainIndex;
  layer.Textures[1].SwapChainIndex = state.swapchainIndex;
  layer.Textures[0].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&tracking.Eye[0].ProjectionMatrix);
  layer.Textures[1].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&tracking.Eye[1].ProjectionMatrix);

  ovrSubmitFrameDescription2 frame = {
    .SwapInterval = 1,
    .FrameIndex = state.frameIndex,
    .DisplayTime = state.displayTime,
    .LayerCount = 1,
    .Layers = (const ovrLayerHeader2*[]) { &layer.Header }
  };

  vrapi_SubmitFrame2(state.session, &frame);
  state.swapchainIndex = (state.swapchainIndex + 1) % state.swapchainLength;
}

static void vrapi_update(float dt) {
  int appState = lovrPlatformGetActivityState();
  ANativeWindow* window = lovrPlatformGetNativeWindow();

  if (!state.session && appState == APP_CMD_RESUME && window) {
    ovrModeParms config = vrapi_DefaultModeParms(&state.java);
    config.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;
    config.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
    config.Flags |= VRAPI_MODE_FLAG_FRONT_BUFFER_SRGB;
    config.Display = (size_t) lovrPlatformGetEGLDisplay();
    config.ShareContext = (size_t) lovrPlatformGetEGLContext();
    config.WindowSurface = (size_t) window;
    state.session = vrapi_EnterVrMode(&config);
    state.frameIndex = 0;
    if (state.deviceType == VRAPI_DEVICE_TYPE_OCULUSQUEST) {
      vrapi_SetTrackingSpace(state.session, VRAPI_TRACKING_SPACE_STAGE);
      state.offset = 0.f;
    }
  } else if (state.session && (appState != APP_CMD_RESUME || !window)) {
    vrapi_LeaveVrMode(state.session);
    state.session = NULL;
  }

  if (!state.session) return;

  VRAPI_LARGEST_EVENT_TYPE event;
  while (vrapi_PollEvent(&event.EventHeader) == ovrSuccess) {
    switch (event.EventHeader.EventType) {
      case VRAPI_EVENT_FOCUS_GAINED:
        lovrEventPush((Event) { .type = EVENT_FOCUS, .data.boolean = { true } });
        break;

      case VRAPI_EVENT_FOCUS_LOST:
        lovrEventPush((Event) { .type = EVENT_FOCUS, .data.boolean = { false } });
        break;

      default: break;
    }
  }

  state.frameIndex++;
  state.displayTime = vrapi_GetPredictedDisplayTime(state.session, state.frameIndex);

  state.controllerInfo[0].Header.Type = ovrControllerType_None;
  state.controllerInfo[1].Header.Type = ovrControllerType_None;

  ovrInputCapabilityHeader header;
  for (uint32_t i = 0; vrapi_EnumerateInputDevices(state.session, i, &header) == ovrSuccess; i++) {
    if (header.Type == ovrControllerType_TrackedRemote) {
      ovrInputTrackedRemoteCapabilities info;
      info.Header = header;
      vrapi_GetInputDeviceCapabilities(state.session, &info.Header);
      uint32_t index = (info.ControllerCapabilities & ovrControllerCaps_LeftHand) ? 0 : 1;
      state.controllerInfo[index] = info;
      state.input[index].Header.ControllerType = header.Type;
      uint32_t lastButtons = state.input[index].Buttons;
      vrapi_GetCurrentInputState(state.session, header.DeviceID, &state.input[index].Header);
      state.changedButtons[index] = state.input[index].Buttons ^ lastButtons;
    } else if (header.Type == ovrControllerType_Hand) {
      ovrInputHandCapabilities info;
      info.Header = header;
      vrapi_GetInputDeviceCapabilities(state.session, &info.Header);
      uint32_t index = (info.HandCapabilities & ovrHandCaps_LeftHand) ? 0 : 1;
      state.controllerInfo[index].Header.Type = header.Type;
      state.handInput[index].Header.ControllerType = header.Type;
      vrapi_GetCurrentInputState(state.session, header.DeviceID, &state.handInput[index].Header);
    }
  }

  for (uint32_t i = 0; i < 2; i++) {
    ovrInputCapabilityHeader* header = &state.controllerInfo[i].Header;
    if (header->Type == ovrControllerType_TrackedRemote) {
      state.hapticDuration[i] -= dt;
      float strength = state.hapticDuration[i] > 0.f ? state.hapticStrength[i] : 0.f;
      vrapi_SetHapticVibrationSimple(state.session, header->DeviceID, strength);
    }
  }
}

HeadsetInterface lovrHeadsetVrApiDriver = {
  .driverType = DRIVER_VRAPI,
  .init = vrapi_init,
  .destroy = vrapi_destroy,
  .getName = vrapi_getName,
  .getOriginType = vrapi_getOriginType,
  .getDisplayDimensions = vrapi_getDisplayDimensions,
  .getDisplayFrequency = vrapi_getDisplayFrequency,
  .getDisplayMask = vrapi_getDisplayMask,
  .getDisplayTime = vrapi_getDisplayTime,
  .getViewCount = vrapi_getViewCount,
  .getViewPose = vrapi_getViewPose,
  .getViewAngles = vrapi_getViewAngles,
  .getClipDistance = vrapi_getClipDistance,
  .setClipDistance = vrapi_setClipDistance,
  .getBoundsDimensions = vrapi_getBoundsDimensions,
  .getBoundsGeometry = vrapi_getBoundsGeometry,
  .getPose = vrapi_getPose,
  .getVelocity = vrapi_getVelocity,
  .isDown = vrapi_isDown,
  .isTouched = vrapi_isTouched,
  .getAxis = vrapi_getAxis,
  .vibrate = vrapi_vibrate,
  .newModelData = vrapi_newModelData,
  .renderTo = vrapi_renderTo,
  .update = vrapi_update
};
