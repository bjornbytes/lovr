#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "graphics/texture.h"
#include "lib/maf.h"
#include "lib/vec/vec.h"

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
    map_set(&state.textureLookup, key, lovrTextureCreateFromHandle(handle, TEXTURE_2D));
    texture = map_get(&state.textureLookup, key);
  }
  return *texture;
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

static bool init(float offset, int msaa) {
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

static void destroy() {
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

static const char* getName() {
  // return ovr_GetHmdDesc(state.session).ProductName; // MEMORY
  return NULL;
}

static HeadsetOrigin getOriginType() {
  return ORIGIN_FLOOR;
}

static void getDisplayDimensions(uint32_t* width, uint32_t* height) {
  ovrHmdDesc desc = ovr_GetHmdDesc(state.session);
  ovrSizei size = ovr_GetFovTextureSize(state.session, ovrEye_Left, desc.DefaultEyeFov[0], 1.0f);

  *width = size.w;
  *height = size.h;
}

static void getClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void setClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static void getBoundsDimensions(float* width, float* depth) {
  ovrVector3f dimensions;
  ovr_GetBoundaryDimensions(state.session, ovrBoundary_PlayArea, &dimensions);
  *width = dimensions.x;
  *depth = dimensions.z;
}

static const float* getBoundsGeometry(int* count) {
  *count = 0;
  return NULL;
}

static bool getPose(Path path, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  ovrTrackingState *ts = refreshTracking();
  ovrPosef* pose;

  if (PATH_EQ(path, P_HEAD)) {
    pose = &ts->HeadPose.ThePose;
  } else if (PATH_STARTS_WITH(path, P_HAND) && (path.p[1] == P_LEFT || path.p[1] == P_RIGHT)) {
    pose = &ts->HandPoses[path.p[1] - P_LEFT].ThePose;
  } else {
    return false;
  }

  if (x) {
    *x = pose->Position.x;;
    *y = pose->Position.y + state.offset;
    *z = pose->Position.z;
  }

  if (angle) {
    float quat[4] = { pose->Orientation.x, pose->Orientation.y, pose->Orientation.z, pose->Orientation.w };
    quat_getAngleAxis(quat, angle, ax, ay, az);
  }

  return true;
}

static bool getVelocity(Path path, float* vx, float* vy, float* vz, float* vax, float* vay, float* vaz) {
  ovrTrackingState *ts = refreshTracking();
  ovrPosef* pose;

  if (PATH_EQ(path, P_HEAD)) {
    velocity = &ts->HeadPose;
  } else if (PATH_STARTS_WITH(path, P_HAND) && (path.p[1] == P_LEFT || path.p[1] == P_RIGHT)) {
    velocity = &ts->HandPoses[path.p[1] - P_LEFT];
  } else {
    return false;
  }

  if (vx) {
    *vx = pose->LinearVelocity.x;
    *vy = pose->LinearVelocity.y;
    *vz = pose->LinearVelocity.z;
  }

  if (vax) {
    *vax = pose->AngularVelocity.x;
    *vay = pose->AngularVelocity.y;
    *vaz = pose->AngularVelocity.z;
  }

  return true;
}

static bool isDown(Path path, bool* down) {
  if (PATH_EQ(path, P_HEAD, P_PROXIMITY)) {
    ovrSessionStatus status;
    ovr_GetSessionStatus(state.session, &status);
    *down = status.HmdMounted;
    return true;
  }

  // Make sure the path starts with /hand/left or /hand/right and only has 3 pieces
  if (path.p[0] != P_HAND || (path.p[1] != P_LEFT && path.p[1] != P_RIGHT) || path.p[3] != P_NONE) {
    return false;
  }

  ovrHandType hand = path.p[1] - P_LEFT;
  ovrInputState* is = refreshButtons();
  uint32_t mask = is->Buttons & (hand == ovrHand_Left ? ovrButton_LMask : ovrButton_RMask);

  switch (path.p[2]) {
    case P_A: *down = (mask & ovrButton_A); return true;
    case P_B: *down = (mask & ovrButton_B); return true;
    case P_X: *down = (mask & ovrButton_X); return true;
    case P_Y: *down = (mask & ovrButton_Y); return true;
    case P_MENU: *down = (mask & ovrButton_Enter); return true;
    case P_TRIGGER: *down = (is->IndexTriggerNoDeadzone[hand] > 0.5f); return true;
    case P_JOYSTICK: *down = (mask & (ovrButton_LThumb | ovrButton_RThumb)); return true;
    case P_GRIP: *down = (is->HandTrigger[hand] > 0.9f); return true;
  }

  return false;
}

static bool isTouched(Path path, bool* touched) {

  // Make sure the path starts with /hand/left or /hand/right and only has 3 pieces
  if (path.p[0] != P_HAND || (path.p[1] != P_LEFT && path.p[1] != P_RIGHT) || path.p[3] != P_NONE) {
    return false;
  }

  ovrHandType hand = path.p[1] - P_LEFT;
  ovrInputState* is = refreshButtons();
  uint32_t mask = is->Buttons & (hand == ovrHand_Left ? ovrButton_LMask : ovrButton_RMask);

  switch (path.p[2]) {
    case P_A: *touched = (mask & ovrTouch_A); return true;
    case P_B: *touched = (mask & ovrTouch_B); return true;
    case P_X: *touched = (mask & ovrTouch_X); return true;
    case P_Y: *touched = (mask & ovrTouch_Y); return true;
    case P_TRIGGER: *touched = (mask & (ovrTouch_LIndexTrigger | ovrTouch_RIndexTrigger)); return true;
    case P_JOYSTICK: *touched = (mask & (ovrTouch_LThumb | ovrTouch_RThumb)); return true;
  }

  return false;
}

static int getAxis(Path path, float* x, float* y, float* z) {

  // Make sure the path starts with /hand/left or /hand/right and only has 3 pieces
  if (path.p[0] != P_HAND || (path.p[1] != P_LEFT && path.p[1] != P_RIGHT) || path.p[3] != P_NONE) {
    return false;
  }

  ovrInputState *is = refreshButtons();
  ovrHandType hand = path.p[1] - P_LEFT;

  switch (path.p[2]) {
    case P_GRIP: *x = is->HandTriggerNoDeadzone[hand]; return 1;
    case P_TRIGGER: *x = is->IndexTriggerNoDeadzone[hand]; return 1;
    case P_JOYSTICK:
      *x = is->ThumbstickNoDeadzone[hand].x;
      *y = is->ThumbstickNoDeadzone[hand].y;
      return 2;
  }

  return 0;
}

static bool vibrate(Path path, float strength, float duration, float frequency) {
  return false; // TODO
}

static ModelData* newModelData(Path path) {
  return NULL; // TODO
}

static void renderTo(void (*callback)(void*), void* userdata) {
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

static Texture* getMirrorTexture() {
  uint32_t handle;
  ovr_GetMirrorTextureBufferGL(state.session, state.mirror, &handle);
  return lookupTexture(handle);
}

static void update(float dt) {
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
  .init = init,
  .destroy = destroy,
  .getName = getName,
  .getOriginType = getOriginType,
  .getDisplayDimensions = getDisplayDimensions,
  .getClipDistance = getClipDistance,
  .setClipDistance = setClipDistance,
  .getBoundsDimensions = getBoundsDimensions,
  .getBoundsGeometry = getBoundsGeometry,
  .getPose = getPose,
  .getVelocity = getVelocity,
  .isDown = isDown,
  .isTouched = isTouched,
  .getAxis = getAxis,
  .vibrate = vibrate,
  .newModelData = newModelData,
  .renderTo = renderTo,
  .getMirrorTexture = getMirrorTexture,
  .update = update
};
