#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/texture.h"
#include "lib/maf.h"
#include "lib/vec/vec.h"

#include <stdbool.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>

typedef struct {
  bool hmdPresent;
  bool needRefreshTracking;
  bool needRefreshButtons;
  vec_controller_t controllers;
  ovrSession session;
  ovrGraphicsLuid luid;
  float clipNear;
  float clipFar;
  float offset;
  int lastButtonState;
  ovrSizei size;
  Canvas* canvas;
  ovrTextureSwapChain chain;
  ovrMirrorTexture mirror;
  map_void_t textureLookup;
} HeadsetState;

static HeadsetState state;

static Texture* lookupTexture(uint32_t handle) {
  char key[4 + 1] = { 0 };
  lovrAssert(handle < 9999, "Texture handle overflow");
  sprintf(key, "%d", handle);
  Texture** texture = map_get(&state.textureLookup, key);
  if (!texture) {
    map_set(&state.textureLookup, key, lovrTextureCreateFromHandle(handle, TEXTURE_2D));
    texture = map_get(&state.textureLookup, key);
  }
  return *texture;
}

static void checkInput(Controller *controller, int diff, int state, ovrButton button, ControllerButton target) {
  if ((diff & button) > 0) {
    lovrEventPush((Event) {
      .type = (state & button) ? EVENT_CONTROLLER_PRESSED : EVENT_CONTROLLER_RELEASED,
      .data.controller = { controller, target }
    });
  }
}

static ovrTrackingState *refreshTracking() {
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

static ovrInputState *refreshButtons() {
  static ovrInputState is;
  if (!state.needRefreshButtons) {
    return &is;
  }

  ovr_GetInputState(state.session, ovrControllerType_Touch, &is);
  state.needRefreshButtons = false;
  return &is;
}

static bool oculusInit(float offset, int msaa) {
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
  state.lastButtonState = 0;
  state.clipNear = 0.1f;
  state.clipFar = 30.f;
  state.offset = offset;

  vec_init(&state.controllers);

  for (ovrHandType hand = ovrHand_Left; hand < ovrHand_Count; hand++) {
    Controller* controller = lovrAlloc(Controller);
    controller->id = hand;
    vec_push(&state.controllers, controller);
  }

  map_init(&state.textureLookup);

  ovr_SetTrackingOriginType(state.session, ovrTrackingOrigin_FloorLevel);
  return true;
}

static void oculusDestroy() {
  Controller *controller; int i;
  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(Controller, controller);
  }
  vec_deinit(&state.controllers);

  const char* key;
  map_iter_t iter = map_iter(&state.textureLookup);
  while ((key = map_next(&state.textureLookup, &iter)) != NULL) {
    Texture* texture = *(Texture**) map_get(&state.textureLookup, key);
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

static HeadsetType oculusGetType() {
  return HEADSET_RIFT;
}

static HeadsetOrigin oculusGetOriginType() {
  return ORIGIN_FLOOR;
}

static bool oculusIsMounted() {
  return true;
}

static void oculusGetDisplayDimensions(uint32_t* width, uint32_t* height) {
  ovrHmdDesc desc = ovr_GetHmdDesc(state.session);
  ovrSizei size = ovr_GetFovTextureSize(state.session, ovrEye_Left, desc.DefaultEyeFov[0], 1.0f);

  *width = size.w;
  *height = size.h;
}

static void oculusGetClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void oculusSetClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static void oculusGetBoundsDimensions(float* width, float* depth) {
  ovrVector3f dimensions;
  ovr_GetBoundaryDimensions(state.session, ovrBoundary_PlayArea, &dimensions);
  *width = dimensions.x;
  *depth = dimensions.z;
}

static const float* oculusGetBoundsGeometry(int* count) {
  *count = 0;
  return NULL;
}

static bool oculusGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  ovrTrackingState *ts = refreshTracking();
  ovrVector3f pos = ts->HeadPose.ThePose.Position;
  *x = pos.x;
  *y = pos.y + state.offset;
  *z = pos.z;
  ovrQuatf oq = ts->HeadPose.ThePose.Orientation;
  float quat[] = { oq.x, oq.y, oq.z, oq.w };
  quat_getAngleAxis(quat, angle, ax, ay, az);
  return true;
}

static bool oculusGetVelocity(float* vx, float* vy, float* vz) {
  ovrTrackingState *ts = refreshTracking();
  ovrVector3f vel = ts->HeadPose.LinearVelocity;
  *vx = vel.x;
  *vy = vel.y;
  *vz = vel.z;
  return true;
}

static bool oculusGetAngularVelocity(float* vx, float* vy, float* vz) {
  ovrTrackingState *ts = refreshTracking();
  ovrVector3f vel = ts->HeadPose.AngularVelocity;
  *vx = vel.x;
  *vy = vel.y;
  *vz = vel.z;
  return true;
}

static Controller** oculusGetControllers(uint8_t* count) {
  *count = state.controllers.length;
  return state.controllers.data;
}

static bool oculusControllerIsConnected(Controller* controller) {
  ovrInputState *is = refreshButtons();
  switch (controller->id) {
    case ovrHand_Left: return (is->ControllerType & ovrControllerType_LTouch) == ovrControllerType_LTouch;
    case ovrHand_Right: return (is->ControllerType & ovrControllerType_RTouch) == ovrControllerType_RTouch;
    default: return false;
  }
  return false;
}

static ControllerHand oculusControllerGetHand(Controller* controller) {
  switch (controller->id) {
    case ovrHand_Left: return HAND_LEFT;
    case ovrHand_Right: return HAND_RIGHT;
    default: return HAND_UNKNOWN;
  }
}

static void oculusControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  ovrTrackingState *ts = refreshTracking();
  ovrVector3f pos = ts->HandPoses[controller->id].ThePose.Position;
  *x = pos.x;
  *y = pos.y + state.offset;
  *z = pos.z;
  ovrQuatf orient = ts->HandPoses[controller->id].ThePose.Orientation;
  float quat[4] = { orient.x, orient.y, orient.z, orient.w };
  quat_getAngleAxis(quat, angle, ax, ay, az);
}

static void oculusControllerGetVelocity(Controller* controller, float* vx, float* vy, float* vz) {
  ovrTrackingState *ts = refreshTracking();
  ovrVector3f vel = ts->HandPoses[controller->id].LinearVelocity;
  *vx = vel.x;
  *vy = vel.y;
  *vz = vel.z;
}

static void oculusControllerGetAngularVelocity(Controller* controller, float* vx, float* vy, float* vz) {
  ovrTrackingState *ts = refreshTracking();
  ovrVector3f vel = ts->HandPoses[controller->id].AngularVelocity;
  *vx = vel.x;
  *vy = vel.y;
  *vz = vel.z;
}

static float oculusControllerGetAxis(Controller* controller, ControllerAxis axis) {
  ovrInputState *is = refreshButtons();
  switch (axis) {
    case CONTROLLER_AXIS_GRIP: return is->HandTriggerNoDeadzone[controller->id];
    case CONTROLLER_AXIS_TRIGGER: return is->IndexTriggerNoDeadzone[controller->id];
    case CONTROLLER_AXIS_TOUCHPAD_X: return is->ThumbstickNoDeadzone[controller->id].x;
    case CONTROLLER_AXIS_TOUCHPAD_Y: return is->ThumbstickNoDeadzone[controller->id].y;
    default: return 0.0f;
  }
  return 0.0f;
}

static int oculusControllerIsDown(Controller* controller, ControllerButton button) {
  ovrInputState *is = refreshButtons();
  int relevant = is->Buttons & ((controller->id == ovrHand_Left) ? ovrButton_LMask : ovrButton_RMask);
  switch (button) {
    case CONTROLLER_BUTTON_A: return (relevant & ovrButton_A) > 0;
    case CONTROLLER_BUTTON_B: return (relevant & ovrButton_B) > 0;
    case CONTROLLER_BUTTON_X: return (relevant & ovrButton_X) > 0;
    case CONTROLLER_BUTTON_Y: return (relevant & ovrButton_Y) > 0;
    case CONTROLLER_BUTTON_MENU: return (relevant & ovrButton_Enter) > 0;
    case CONTROLLER_BUTTON_TRIGGER: return is->IndexTriggerNoDeadzone[controller->id] > 0.5f;
    case CONTROLLER_BUTTON_TOUCHPAD: return (relevant & (ovrButton_LThumb | ovrButton_RThumb)) > 0;
    case CONTROLLER_BUTTON_GRIP: return is->HandTrigger[controller->id] > 0.9f;
    default: return 0;
  }
}

static bool oculusControllerIsTouched(Controller* controller, ControllerButton button) {
  ovrInputState *is = refreshButtons();
  int relevant = is->Touches & ((controller->id == ovrHand_Left) ? ovrTouch_LButtonMask : ovrTouch_RButtonMask);
  switch (button) {
    case CONTROLLER_BUTTON_A: return (relevant & ovrTouch_A) > 0;
    case CONTROLLER_BUTTON_B: return (relevant & ovrTouch_B) > 0;
    case CONTROLLER_BUTTON_X: return (relevant & ovrTouch_X) > 0;
    case CONTROLLER_BUTTON_Y: return (relevant & ovrTouch_Y) > 0;
    case CONTROLLER_BUTTON_TRIGGER: return (relevant & (ovrTouch_LIndexTrigger | ovrTouch_RIndexTrigger)) > 0;
    case CONTROLLER_BUTTON_TOUCHPAD:return (relevant & (ovrTouch_LThumb | ovrTouch_RThumb)) > 0;
    default: return false;
  }
}

static void oculusControllerVibrate(Controller* controller, float duration, float power) {
  //ovr_SetControllerVibration(state.session, ovrControllerType_LTouch, freq, power);
  // TODO
}

static ModelData* oculusControllerNewModelData(Controller* controller) {
  // TODO
  return NULL;
}

static void oculusRenderTo(void (*callback)(void*), void* userdata) {
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

  for (HeadsetEye eye = EYE_LEFT; eye <= EYE_RIGHT; eye++) {
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

  ovrLayerHeader* layers = &ld.Header;
  ovr_SubmitFrame(state.session, 0, NULL, &layers, 1);

  state.needRefreshTracking = true;
  state.needRefreshButtons = true;
}

static Texture* oculusGetMirrorTexture() {
  uint32_t handle;
  ovr_GetMirrorTextureBufferGL(state.session, state.mirror, &handle);
  return lookupTexture(handle);
}

static void oculusUpdate(float dt) {
  ovrInputState *is = refreshButtons();

  ovrSessionStatus status;
  ovr_GetSessionStatus(state.session, &status);

  if (status.ShouldQuit) {
    Event e;
    e.type = EVENT_QUIT;
    e.data.quit.exitCode = 0;
    lovrEventPush(e);
  }

  //if (!status.HmdMounted) // TODO: update mirror only?

  state.hmdPresent = (status.HmdPresent == ovrTrue)? true : false;

  Controller* left = state.controllers.data[ovrHand_Left];
  Controller* right = state.controllers.data[ovrHand_Right];
  int diff = is->Buttons ^ state.lastButtonState;
  int istate = is->Buttons;
  checkInput(right, diff, istate, ovrButton_A, CONTROLLER_BUTTON_A);
  checkInput(right, diff, istate, ovrButton_B, CONTROLLER_BUTTON_B);
  checkInput(right, diff, istate, ovrButton_RShoulder, CONTROLLER_BUTTON_TRIGGER);
  checkInput(right, diff, istate, ovrButton_RThumb, CONTROLLER_BUTTON_TOUCHPAD);

  checkInput(left, diff, istate, ovrButton_X, CONTROLLER_BUTTON_X);
  checkInput(left, diff, istate, ovrButton_Y, CONTROLLER_BUTTON_Y);
  checkInput(left, diff, istate, ovrButton_LShoulder, CONTROLLER_BUTTON_TRIGGER);
  checkInput(left, diff, istate, ovrButton_LThumb, CONTROLLER_BUTTON_TOUCHPAD);
  checkInput(left, diff, istate, ovrButton_Enter, CONTROLLER_BUTTON_MENU);

  state.lastButtonState = is->Buttons;
}

HeadsetInterface lovrHeadsetOculusDriver = {
  .driverType = DRIVER_OCULUS,
  .init = oculusInit,
  .destroy = oculusDestroy,
  .getType = oculusGetType,
  .getOriginType = oculusGetOriginType,
  .isMounted = oculusIsMounted,
  .getDisplayDimensions = oculusGetDisplayDimensions,
  .getClipDistance = oculusGetClipDistance,
  .setClipDistance = oculusSetClipDistance,
  .getBoundsDimensions = oculusGetBoundsDimensions,
  .getBoundsGeometry = oculusGetBoundsGeometry,
  .getPose = oculusGetPose,
  .getVelocity = oculusGetVelocity,
  .getAngularVelocity = oculusGetAngularVelocity,
  .getControllers = oculusGetControllers,
  .controllerIsConnected = oculusControllerIsConnected,
  .controllerGetHand = oculusControllerGetHand,
  .controllerGetPose = oculusControllerGetPose,
  .controllerGetVelocity = oculusControllerGetVelocity,
  .controllerGetAngularVelocity = oculusControllerGetAngularVelocity,
  .controllerGetAxis = oculusControllerGetAxis,
  .controllerIsDown = oculusControllerIsDown,
  .controllerIsTouched = oculusControllerIsTouched,
  .controllerVibrate = oculusControllerVibrate,
  .controllerNewModelData = oculusControllerNewModelData,
  .renderTo = oculusRenderTo,
  .getMirrorTexture = oculusGetMirrorTexture,
  .update = oculusUpdate
};
