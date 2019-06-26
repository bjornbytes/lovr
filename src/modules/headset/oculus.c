#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "graphics/texture.h"
#include "core/maf.h"
#include "core/ref.h"

#include <stdbool.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>

static struct {
  bool needRefreshTracking;
  bool needRefreshButtons;
  ovrSession session;
  ovrGraphicsLuid luid;
  float clipNear;
  float clipFar;
  float offset;
  ovrSizei size;
  Canvas* canvas;
  ovrTextureSwapChain chain;
  ovrMirrorTexture mirror;
  map_t(Texture*) textureLookup;
} state;

static Texture* lookupTexture(uint32_t handle) {
  char key[4 + 1] = { 0 };
  lovrAssert(handle < 9999, "Texture handle overflow");
  sprintf(key, "%d", handle);
  Texture** texture = map_get(&state.textureLookup, key);
  if (!texture) {
    map_set(&state.textureLookup, key, lovrTextureCreateFromHandle(handle, TEXTURE_2D, 1));
    texture = map_get(&state.textureLookup, key);
  }
  return *texture;
}

static ovrTrackingState *refreshTracking(void) {
  static ovrTrackingState ts;
  if (!state.needRefreshTracking) {
    return &ts;
  }

  ovrSessionStatus status;
  ovr_GetSessionStatus(state.session, &status);

  if (status.ShouldRecenter) {
    ovr_RecenterTrackingOrigin(state.session);
  }

  // get the state head and controllers are predicted to be in at display time,
  // per the manual (frame timing section).
  double predicted = ovr_GetPredictedDisplayTime(state.session, 0);
  ts = ovr_GetTrackingState(state.session, predicted, true);
  state.needRefreshTracking = false;
  return &ts;
}

static ovrInputState *refreshButtons(void) {
  static ovrInputState is;
  if (!state.needRefreshButtons) {
    return &is;
  }

  ovr_GetInputState(state.session, ovrControllerType_Touch, &is);
  state.needRefreshButtons = false;
  return &is;
}

static bool oculus_init(float offset, uint32_t msaa) {
  ovrResult result = ovr_Initialize(NULL);
  if (OVR_FAILURE(result)) {
    return false;
  }

  result = ovr_Create(&state.session, &state.luid);
  if (OVR_FAILURE(result)) {
    ovr_Shutdown();
    return false;
  }

  state.needRefreshTracking = true;
  state.needRefreshButtons = true;
  state.clipNear = 0.1f;
  state.clipFar = 30.f;
  state.offset = offset;

  map_init(&state.textureLookup);

  ovr_SetTrackingOriginType(state.session, ovrTrackingOrigin_FloorLevel);
  return true;
}

static void oculus_destroy(void) {
  const char* key;
  map_iter_t iter = map_iter(&state.textureLookup);
  while ((key = map_next(&state.textureLookup, &iter)) != NULL) {
    Texture* texture = *map_get(&state.textureLookup, key);
    lovrRelease(Texture, texture);
  }
  map_deinit(&state.textureLookup);

  if (state.mirror) {
    ovr_DestroyMirrorTexture(state.session, state.mirror);
    state.mirror = NULL;
  }

  if (state.chain) {
    ovr_DestroyTextureSwapChain(state.session, state.chain);
    state.chain = NULL;
  }

  lovrRelease(Canvas, state.canvas);
  ovr_Destroy(state.session);
  ovr_Shutdown();
  memset(&state, 0, sizeof(state));
}

static bool oculus_getName(char* name, size_t length) {
  ovrHmdDesc desc = ovr_GetHmdDesc(state.session);
  strncpy(name, desc.ProductName, length - 1);
  name[length - 1] = '\0';
  return true;
}

static HeadsetOrigin oculus_getOriginType(void) {
  return ORIGIN_FLOOR;
}

static void oculus_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  ovrHmdDesc desc = ovr_GetHmdDesc(state.session);
  ovrSizei size = ovr_GetFovTextureSize(state.session, ovrEye_Left, desc.DefaultEyeFov[0], 1.0f);

  *width = size.w;
  *height = size.h;
}

static double oculus_getDisplayTime(void) {
  return ovr_GetPredictedDisplayTime(state.session, 0);
}

static void oculus_getClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void oculus_setClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static void oculus_getBoundsDimensions(float* width, float* depth) {
  ovrVector3f dimensions;
  ovr_GetBoundaryDimensions(state.session, ovrBoundary_PlayArea, &dimensions);
  *width = dimensions.x;
  *depth = dimensions.z;
}

static const float* oculus_getBoundsGeometry(uint32_t* count) {
  *count = 0;
  return NULL;
}

ovrPoseStatef* getPose(Device device) {
  ovrTrackingState *ts = refreshTracking();
  switch (device) {
    case DEVICE_HEAD: return &ts->HeadPose;
    case DEVICE_HAND_LEFT: return &ts->HandPoses[ovrHand_Left];
    case DEVICE_HAND_RIGHT: return &ts->HandPoses[ovrHand_Right];
    default: return NULL;
  }
}

static bool oculus_getPose(Device device, vec3 position, quat orientation) {
  ovrPoseStatef* poseState = getPose(device);
  if (!poseState) return false;

  ovrPosef* pose = &poseState->ThePose;
  vec3_set(position, pose->Position.x, pose->Position.y + state.offset, pose->Position.z);
  quat_set(orientation, pose->Orientation.x, pose->Orientation.y, pose->Orientation.z, pose->Orientation.w);
  return true;
}

static bool oculus_getBonePose(Device device, DeviceBone bone, vec3 position, quat orientation) {
  return false;
}

static bool oculus_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  ovrPoseStatef* pose = getPose(device);
  if (!pose) return false;

  vec3_set(velocity, pose->LinearVelocity.x, pose->LinearVelocity.y, pose->LinearVelocity.z);
  vec3_set(angularVelocity, pose->AngularVelocity.x, pose->AngularVelocity.y, pose->AngularVelocity.z);
  return true;
}

static bool oculus_getAcceleration(Device device, vec3 acceleration, vec3 angularAcceleration) {
  ovrPoseStatef* pose = getPose(device);
  if (!pose) return false;

  vec3_set(acceleration, pose->LinearAcceleration.x, pose->LinearAcceleration.y, pose->LinearAcceleration.z);
  vec3_set(angularAcceleration, pose->AngularAcceleration.x, pose->AngularAcceleration.y, pose->AngularAcceleration.z);
  return true;
}

static bool oculus_isDown(Device device, DeviceButton button, bool* down) {
  if (device == DEVICE_HEAD && button == BUTTON_PROXIMITY) {
    ovrSessionStatus status;
    ovr_GetSessionStatus(state.session, &status);
    *down = status.HmdMounted;
    return true;
  } else if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  ovrInputState* is = refreshButtons();
  ovrHandType hand = device == DEVICE_HAND_LEFT ? ovrHand_Left : ovrHand_Right;
  uint32_t buttons = is->Buttons & (device == DEVICE_HAND_LEFT ? ovrButton_LMask : ovrButton_RMask);

  switch (button) {
    case BUTTON_A: return *down = (buttons & ovrButton_A), true;
    case BUTTON_B: return *down = (buttons & ovrButton_B), true;
    case BUTTON_X: return *down = (buttons & ovrButton_X), true;
    case BUTTON_Y: return *down = (buttons & ovrButton_Y), true;
    case BUTTON_MENU: return *down = (buttons & ovrButton_Enter), true;
    case BUTTON_PRIMARY:
    case BUTTON_TRIGGER: return *down = (is->IndexTriggerNoDeadzone[hand] > .5f), true;
    case BUTTON_THUMBSTICK: return *down = (buttons & (ovrButton_LThumb | ovrButton_RThumb)), true;
    case BUTTON_GRIP: return *down = (is->HandTrigger[hand] > .9f), true;
    default: return false;
  }
}

static bool oculus_isTouched(Device device, DeviceButton button, bool* touched) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  ovrInputState* is = refreshButtons();
  ovrHandType hand = device == DEVICE_HAND_LEFT ? ovrHand_Left : ovrHand_Right;
  uint32_t touches = is->Touches & (device == DEVICE_HAND_LEFT ? ovrTouch_LButtonMask : ovrTouch_RButtonMask);

  switch (button) {
    case BUTTON_A: return *touched = (touches & ovrTouch_A), true;
    case BUTTON_B: return *touched = (touches & ovrTouch_B), true;
    case BUTTON_X: return *touched = (touches & ovrTouch_X), true;
    case BUTTON_Y: return *touched = (touches & ovrTouch_Y), true;
    case BUTTON_PRIMARY:
    case BUTTON_TRIGGER: return *touched = (touches & (ovrTouch_LIndexTrigger | ovrTouch_RIndexTrigger)), true;
    case BUTTON_THUMBSTICK: return *touched = (touches & (ovrTouch_LThumb | ovrTouch_RThumb)), true;
    default: return false;
  }
}

static bool oculus_getAxis(Device device, DeviceAxis axis, vec3 value) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  ovrInputState* is = refreshButtons();
  ovrHandType hand = device == DEVICE_HAND_LEFT ? ovrHand_Left : ovrHand_Right;

  switch (axis) {
    case AXIS_GRIP: return *value = is->HandTriggerNoDeadzone[hand], true;
    case AXIS_TRIGGER: return *value = is->IndexTriggerNoDeadzone[hand], true;
    case AXIS_PRIMARY:
    case AXIS_THUMBSTICK:
      value[0] = is->ThumbstickNoDeadzone[hand].x;
      value[1] = is->ThumbstickNoDeadzone[hand].y;
      return true;
    default: return false;
  }
}

static bool oculus_vibrate(Device device, float strength, float duration, float frequency) {
  return false; // TODO
}

static ModelData* oculus_newModelData(Device device) {
  return NULL; // TODO
}

static void oculus_renderTo(void (*callback)(void*), void* userdata) {
  ovrHmdDesc desc = ovr_GetHmdDesc(state.session);
  if (!state.canvas) {
    state.size = ovr_GetFovTextureSize(state.session, ovrEye_Left, desc.DefaultEyeFov[ovrEye_Left], 1.0f);

    ovrTextureSwapChainDesc swdesc = {
      .Type = ovrTexture_2D,
      .ArraySize = 1,
      .Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB,
      .Width = 2 * state.size.w,
      .Height = state.size.h,
      .MipLevels = 1,
      .SampleCount = 1,
      .StaticImage = ovrFalse
    };
    lovrAssert(OVR_SUCCESS(ovr_CreateTextureSwapChainGL(state.session, &swdesc, &state.chain)), "Unable to create swapchain");

    ovrMirrorTextureDesc mdesc = {
      .Width = lovrGraphicsGetWidth(),
      .Height = lovrGraphicsGetHeight(),
      .Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB,
      .MirrorOptions = ovrMirrorOption_PostDistortion
    };
    lovrAssert(OVR_SUCCESS(ovr_CreateMirrorTextureWithOptionsGL(state.session, &mdesc, &state.mirror)), "Unable to create mirror texture");

    CanvasFlags flags = { .depth = { .enabled = true, .format = FORMAT_D24S8 }, .stereo = true };
    state.canvas = lovrCanvasCreate(2 * state.size.w, state.size.h, flags);

    lovrPlatformSetSwapInterval(0);
  }

  ovrEyeRenderDesc eyeRenderDesc[2];
  eyeRenderDesc[0] = ovr_GetRenderDesc(state.session, ovrEye_Left, desc.DefaultEyeFov[0]);
  eyeRenderDesc[1] = ovr_GetRenderDesc(state.session, ovrEye_Right, desc.DefaultEyeFov[1]);
  ovrPosef HmdToEyeOffset[2] = {
    eyeRenderDesc[0].HmdToEyePose,
    eyeRenderDesc[1].HmdToEyePose
  };
  ovrPosef EyeRenderPose[2];
  double sensorSampleTime;
  ovr_GetEyePoses(state.session, 0, ovrTrue, HmdToEyeOffset, EyeRenderPose, &sensorSampleTime);

  Camera camera = { .canvas = state.canvas };

  for (int eye = 0; eye < 2; eye++) {
    float orient[] = {
      EyeRenderPose[eye].Orientation.x,
      EyeRenderPose[eye].Orientation.y,
      EyeRenderPose[eye].Orientation.z,
      -EyeRenderPose[eye].Orientation.w
    };
    float pos[] = {
      EyeRenderPose[eye].Position.x,
      EyeRenderPose[eye].Position.y,
      EyeRenderPose[eye].Position.z,
    };
    mat4 transform = camera.viewMatrix[eye];
    mat4_identity(transform);
    mat4_rotateQuat(transform, orient);
    transform[12] = -(transform[0] * pos[0] + transform[4] * pos[1] + transform[8] * pos[2]);
    transform[13] = -(transform[1] * pos[0] + transform[5] * pos[1] + transform[9] * pos[2]);
    transform[14] = -(transform[2] * pos[0] + transform[6] * pos[1] + transform[10] * pos[2]);

    ovrMatrix4f projection = ovrMatrix4f_Projection(desc.DefaultEyeFov[eye], state.clipNear, state.clipFar, ovrProjection_ClipRangeOpenGL);
    mat4_fromMat44(camera.projection[eye], projection.M);
  }

  int curIndex;
  uint32_t curTexId;
  ovr_GetTextureSwapChainCurrentIndex(state.session, state.chain, &curIndex);
  ovr_GetTextureSwapChainBufferGL(state.session, state.chain, curIndex, &curTexId);
  Texture* texture = lookupTexture(curTexId);
  lovrCanvasSetAttachments(state.canvas, &(Attachment) { texture, 0, 0 }, 1);

  lovrGraphicsSetCamera(&camera, true);
  callback(userdata);
  lovrGraphicsSetCamera(NULL, false);

  ovr_CommitTextureSwapChain(state.session, state.chain);

  ovrLayerEyeFov ld;
  ld.Header.Type = ovrLayerType_EyeFov;
  ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
  for (int eye = 0; eye < 2; eye++) {
    ld.ColorTexture[eye] = state.chain;
    ovrRecti vp;
    vp.Pos.x = state.size.w * eye;
    vp.Pos.y = 0;
    vp.Size.w = state.size.w;
    vp.Size.h = state.size.h;
    ld.Viewport[eye] = vp;
    ld.Fov[eye] = desc.DefaultEyeFov[eye];
    ld.RenderPose[eye] = EyeRenderPose[eye];
    ld.SensorSampleTime = sensorSampleTime;
  }

  const ovrLayerHeader* layers = &ld.Header;
  ovr_SubmitFrame(state.session, 0, NULL, &layers, 1);

  state.needRefreshTracking = true;
  state.needRefreshButtons = true;
}

static Texture* oculus_getMirrorTexture(void) {
  uint32_t handle;
  ovr_GetMirrorTextureBufferGL(state.session, state.mirror, &handle);
  return lookupTexture(handle);
}

static void oculus_update(float dt) {
  ovrSessionStatus status;
  ovr_GetSessionStatus(state.session, &status);

  if (status.ShouldQuit) {
    Event e;
    e.type = EVENT_QUIT;
    e.data.quit.exitCode = 0;
    lovrEventPush(e);
  }
}

HeadsetInterface lovrHeadsetOculusDriver = {
  .driverType = DRIVER_OCULUS,
  .init = oculus_init,
  .destroy = oculus_destroy,
  .getName = oculus_getName,
  .getOriginType = oculus_getOriginType,
  .getDisplayDimensions = oculus_getDisplayDimensions,
  .getDisplayTime = oculus_getDisplayTime,
  .getClipDistance = oculus_getClipDistance,
  .setClipDistance = oculus_setClipDistance,
  .getBoundsDimensions = oculus_getBoundsDimensions,
  .getBoundsGeometry = oculus_getBoundsGeometry,
  .getPose = oculus_getPose,
  .getBonePose = oculus_getBonePose,
  .getVelocity = oculus_getVelocity,
  .getAcceleration = oculus_getAcceleration,
  .isDown = oculus_isDown,
  .isTouched = oculus_isTouched,
  .getAxis = oculus_getAxis,
  .vibrate = oculus_vibrate,
  .newModelData = oculus_newModelData,
  .renderTo = oculus_renderTo,
  .getMirrorTexture = oculus_getMirrorTexture,
  .update = oculus_update
};
