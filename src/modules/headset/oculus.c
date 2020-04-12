#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "graphics/texture.h"
#include "core/arr.h"
#include "core/hash.h"
#include "core/maf.h"
#include "core/map.h"
#include "core/ref.h"
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

static struct {
  bool needRefreshTracking;
  bool needRefreshButtons;
  ovrHmdDesc desc;
  ovrSession session;
  long long frameIndex;
  ovrGraphicsLuid luid;
  float clipNear;
  float clipFar;
  ovrSizei size;
  Canvas* canvas;
  ovrTextureSwapChain chain;
  ovrMirrorTexture mirror;
  float hapticFrequency[2];
  float hapticStrength[2];
  float hapticDuration[2];
  double hapticLastTime;
  arr_t(Texture*) textures;
  map_t textureLookup;
} state;

static Texture* lookupTexture(uint32_t handle) {
  uint64_t hash = hash64(&handle, sizeof(handle));
  uint64_t index = map_get(&state.textureLookup, hash);

  if (index == MAP_NIL) {
    index = state.textures.length;
    map_set(&state.textureLookup, hash, index);
    arr_push(&state.textures, lovrTextureCreateFromHandle(handle, TEXTURE_2D, 1));
  }

  return state.textures.data[index];
}

static double oculus_getDisplayTime(void) {
  return ovr_GetPredictedDisplayTime(state.session, state.frameIndex);
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
  double predicted = oculus_getDisplayTime();
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

  state.desc = ovr_GetHmdDesc(state.session);

  state.needRefreshTracking = true;
  state.needRefreshButtons = true;
  state.clipNear = .1f;
  state.clipFar = 100.f;

  map_init(&state.textureLookup, 4);

  ovr_SetTrackingOriginType(state.session, ovrTrackingOrigin_FloorLevel);
  return true;
}

static void oculus_destroy(void) {
  for (size_t i = 0; i < state.textures.length; i++) {
    lovrRelease(Texture, state.textures.data[i]);
  }
  map_free(&state.textureLookup);

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
  strncpy(name, state.desc.ProductName, length - 1);
  name[length - 1] = '\0';
  return true;
}

static HeadsetOrigin oculus_getOriginType(void) {
  return ORIGIN_FLOOR;
}

static void oculus_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  ovrSizei size = ovr_GetFovTextureSize(state.session, ovrEye_Left, state.desc.DefaultEyeFov[0], 1.0f);
  *width = size.w;
  *height = size.h;
}

static const float* oculus_getDisplayMask(uint32_t* count) {
  *count = 0;
  return NULL;
}

static void getEyePoses(ovrPosef poses[2], double* sensorSampleTime) {
  ovrEyeRenderDesc eyeRenderDesc[2] = {
    ovr_GetRenderDesc(state.session, ovrEye_Left, state.desc.DefaultEyeFov[0]),
    ovr_GetRenderDesc(state.session, ovrEye_Right, state.desc.DefaultEyeFov[1])
  };

  ovrPosef offsets[2] = {
    eyeRenderDesc[0].HmdToEyePose,
    eyeRenderDesc[1].HmdToEyePose
  };

  ovr_GetEyePoses(state.session, 0, ovrFalse, offsets, poses, sensorSampleTime);
}

static uint32_t oculus_getViewCount(void) {
  return 2;
}

static bool oculus_getViewPose(uint32_t view, float* position, float* orientation) {
  if (view > 1) return false;
  ovrPosef poses[2];
  getEyePoses(poses, NULL);
  ovrPosef* pose = &poses[view];
  vec3_set(position, pose->Position.x, pose->Position.y, pose->Position.z);
  quat_set(orientation, pose->Orientation.x, pose->Orientation.y, pose->Orientation.z, pose->Orientation.w);
  return true;
}

static bool oculus_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  if (view > 1) return false;
  ovrFovPort* fov = &state.desc.DefaultEyeFov[view];
  *left = atanf(fov->LeftTan);
  *right = atanf(fov->RightTan);
  *up = atanf(fov->UpTan);
  *down = atanf(fov->DownTan);
  return true;
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
  vec3_set(position, pose->Position.x, pose->Position.y, pose->Position.z);
  quat_set(orientation, pose->Orientation.x, pose->Orientation.y, pose->Orientation.z, pose->Orientation.w);
  return true;
}

static bool oculus_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  ovrPoseStatef* pose = getPose(device);
  if (!pose) return false;

  vec3_set(velocity, pose->LinearVelocity.x, pose->LinearVelocity.y, pose->LinearVelocity.z);
  vec3_set(angularVelocity, pose->AngularVelocity.x, pose->AngularVelocity.y, pose->AngularVelocity.z);
  return true;
}

// FIXME: Write "changed"
static bool oculus_isDown(Device device, DeviceButton button, bool* down, bool *changed) {
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
    case BUTTON_A: *down = (buttons & ovrButton_A); return true;
    case BUTTON_B: *down = (buttons & ovrButton_B); return true;
    case BUTTON_X: *down = (buttons & ovrButton_X); return true;
    case BUTTON_Y: *down = (buttons & ovrButton_Y); return true;
    case BUTTON_MENU: *down = (buttons & ovrButton_Enter); return true;
    case BUTTON_TRIGGER: *down = (is->IndexTriggerNoDeadzone[hand] > .5f); return true;
    case BUTTON_THUMBSTICK: *down = (buttons & (ovrButton_LThumb | ovrButton_RThumb)); return true;
    case BUTTON_GRIP: *down = (is->HandTrigger[hand] > .9f); return true;
    default: return false;
  }
}

static bool oculus_isTouched(Device device, DeviceButton button, bool* touched) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  ovrInputState* is = refreshButtons();
  uint32_t touches = is->Touches & (device == DEVICE_HAND_LEFT ? ovrTouch_LButtonMask : ovrTouch_RButtonMask);

  switch (button) {
    case BUTTON_A: *touched = (touches & ovrTouch_A); return true;
    case BUTTON_B: *touched = (touches & ovrTouch_B); return true;
    case BUTTON_X: *touched = (touches & ovrTouch_X); return true;
    case BUTTON_Y: *touched = (touches & ovrTouch_Y); return true;
    case BUTTON_TRIGGER: *touched = (touches & (ovrTouch_LIndexTrigger | ovrTouch_RIndexTrigger)); return true;
    case BUTTON_THUMBSTICK: *touched = (touches & (ovrTouch_LThumb | ovrTouch_RThumb)); return true;
    default: return false;
  }
}

static bool oculus_didChange(Device device, DeviceButton button, bool* changed) {
  return false; // TODO
}

static bool oculus_getAxis(Device device, DeviceAxis axis, vec3 value) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  ovrInputState* is = refreshButtons();
  ovrHandType hand = device == DEVICE_HAND_LEFT ? ovrHand_Left : ovrHand_Right;

  switch (axis) {
    case AXIS_GRIP: *value = is->HandTriggerNoDeadzone[hand]; return true;
    case AXIS_TRIGGER: *value = is->IndexTriggerNoDeadzone[hand]; return true;
    case AXIS_THUMBSTICK:
      value[0] = is->ThumbstickNoDeadzone[hand].x;
      value[1] = is->ThumbstickNoDeadzone[hand].y;
      return true;
    default: return false;
  }
}

static bool oculus_vibrate(Device device, float strength, float duration, float frequency) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }
  int idx = device == DEVICE_HAND_LEFT ? 0 : 1;
  state.hapticStrength[idx] = CLAMP(strength, 0.0f, 1.0f);
  state.hapticDuration[idx] = MAX(duration, 0.0f);
  float freq = CLAMP(frequency / 320.0f, 0.0f, 1.0f); // 1.0 = 320hz, limit on Rift CV1 touch controllers.
  state.hapticFrequency[idx] = freq;
  return true;
}

static ModelData* oculus_newModelData(Device device) {
  return NULL; // TODO
}

static void oculus_renderTo(void (*callback)(void*), void* userdata) {
  if (!state.canvas) {
    state.size = ovr_GetFovTextureSize(state.session, ovrEye_Left, state.desc.DefaultEyeFov[ovrEye_Left], 1.0f);

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
      .MirrorOptions = ovrMirrorOption_LeftEyeOnly
    };
    lovrAssert(OVR_SUCCESS(ovr_CreateMirrorTextureWithOptionsGL(state.session, &mdesc, &state.mirror)), "Unable to create mirror texture");

    CanvasFlags flags = { .depth = { .enabled = true, .format = FORMAT_D24S8 }, .stereo = true };
    state.canvas = lovrCanvasCreate(state.size.w, state.size.h, flags);

    lovrPlatformSetSwapInterval(0);
  }

  ovrPosef EyeRenderPose[2];
  double sensorSampleTime;
  getEyePoses(EyeRenderPose, &sensorSampleTime);

  float delta = (float)(state.hapticLastTime - sensorSampleTime);
  state.hapticLastTime = sensorSampleTime;
  for (int i = 0; i < 2; ++i) {
    ovr_SetControllerVibration(state.session, ovrControllerType_LTouch + i, state.hapticFrequency[i], state.hapticStrength[i]);
    state.hapticDuration[i] -= delta;
    if (state.hapticDuration[i] <= 0.0f) {
      state.hapticStrength[i] = 0.0f;
    }
  }

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
      EyeRenderPose[eye].Position.z
    };
    mat4 transform = camera.viewMatrix[eye];
    mat4_identity(transform);
    mat4_rotateQuat(transform, orient);
    transform[12] = -(transform[0] * pos[0] + transform[4] * pos[1] + transform[8] * pos[2]);
    transform[13] = -(transform[1] * pos[0] + transform[5] * pos[1] + transform[9] * pos[2]);
    transform[14] = -(transform[2] * pos[0] + transform[6] * pos[1] + transform[10] * pos[2]);

    ovrMatrix4f projection = ovrMatrix4f_Projection(state.desc.DefaultEyeFov[eye], state.clipNear, state.clipFar, ovrProjection_ClipRangeOpenGL);
    mat4_fromMat44(camera.projection[eye], projection.M);
  }

  ovr_WaitToBeginFrame(state.session, state.frameIndex);
  ovr_BeginFrame(state.session, state.frameIndex);

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
    ld.Fov[eye] = state.desc.DefaultEyeFov[eye];
    ld.RenderPose[eye] = EyeRenderPose[eye];
    ld.SensorSampleTime = sensorSampleTime;
  }

  const ovrLayerHeader* layers = &ld.Header;
  ovr_EndFrame(state.session, state.frameIndex, NULL, &layers, 1);
  ++state.frameIndex;

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
  .getDisplayMask = oculus_getDisplayMask,
  .getDisplayTime = oculus_getDisplayTime,
  .getViewCount = oculus_getViewCount,
  .getViewPose = oculus_getViewPose,
  .getViewAngles = oculus_getViewAngles,
  .getClipDistance = oculus_getClipDistance,
  .setClipDistance = oculus_setClipDistance,
  .getBoundsDimensions = oculus_getBoundsDimensions,
  .getBoundsGeometry = oculus_getBoundsGeometry,
  .getPose = oculus_getPose,
  .getVelocity = oculus_getVelocity,
  .isDown = oculus_isDown,
  .isTouched = oculus_isTouched,
  .getAxis = oculus_getAxis,
  .vibrate = oculus_vibrate,
  .newModelData = oculus_newModelData,
  .renderTo = oculus_renderTo,
  .getMirrorTexture = oculus_getMirrorTexture,
  .update = oculus_update
};
