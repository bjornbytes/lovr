#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/texture.h"
#include "lib/vec/vec.h"
#include "math/mat4.h"
#include "math/quat.h"

#include <stdbool.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>

typedef struct {
  bool isInitialized;
  bool isRendering;
  bool isMirrored;
  bool hmdPresent;
  headsetRenderCallback renderCallback;
  vec_controller_t controllers;
  ovrSession session;
  ovrGraphicsLuid luid;
  ovrTrackingState ts;
  ovrInputState is;
  int frameIndex;
  float clipNear;
  float clipFar;
  int lastButtonState;
  struct {
    ovrSizei size;
    GLuint fboId;
    GLuint depthId;
    ovrTextureSwapChain chain;
  } eyeTextures[2];
  ovrMirrorTexture mirrorTexture;
  GLuint mirrorFBO;
} HeadsetState;

static HeadsetState state;

void lovrHeadsetInit() {
  ovrResult result = ovr_Initialize(NULL);
  if (OVR_FAILURE(result))
    return;

  result = ovr_Create(&state.session, &state.luid);
  if (OVR_FAILURE(result)) {
    ovr_Shutdown();
    return;
  }

  state.lastButtonState = 0;
  state.isInitialized = true;
  state.isMirrored = true;
  state.mirrorTexture = NULL;
  int i;
  for (i = 0; i < 2; i++) {
    state.eyeTextures[i].size.w = 0;
    state.eyeTextures[i].size.h = 0;
    state.eyeTextures[i].fboId = 0;
    state.eyeTextures[i].depthId = 0;
    state.eyeTextures[i].chain = NULL;
  }
  state.clipNear = 0.1f;
  state.clipFar = 30.f;

  vec_init(&state.controllers);

  // per the docs, ovrHand* is intended as array indices - so we use it directly.
  for (i = ovrHand_Left; i < ovrHand_Count; i++) {
    Controller *controller = lovrAlloc(sizeof(Controller), lovrControllerDestroy);
    controller->id = ovrHand_Left+i;
    vec_push(&state.controllers, controller);
  }

  ovr_SetTrackingOriginType(state.session, ovrTrackingOrigin_FloorLevel);
  lovrEventAddPump(lovrHeadsetPoll);
  atexit(lovrHeadsetDestroy);
}

void lovrHeadsetDestroy() {
  int i;
  Controller *controller;
  vec_foreach(&state.controllers, controller, i) {
    lovrRelease(&controller->ref);
  }

  vec_deinit(&state.controllers);

  if (state.mirrorTexture) {
    ovr_DestroyMirrorTexture(state.session, state.mirrorTexture);
    state.mirrorTexture = NULL;
  }

  for (i = 0; i < 2; i++) {
    if (state.eyeTextures[i].chain) {
      ovr_DestroyTextureSwapChain(state.session, state.eyeTextures[i].chain);
      state.eyeTextures[i].chain = NULL;
    }
  }

  ovr_Destroy(state.session);
  ovr_Shutdown();

  state.isInitialized = false;
  state.frameIndex = 0;
}

static void checkInput(Controller *controller, int diff, int state, ovrButton button, ControllerButton target) {
  if ((diff & button) > 0) {
    Event e;
    if ((state & button) > 0) {
      e.type = EVENT_CONTROLLER_PRESSED;
      e.data.controllerpressed.controller = controller;
      e.data.controllerpressed.button = target;
    }
    else {
      e.type = EVENT_CONTROLLER_RELEASED;
      e.data.controllerreleased.controller = controller;
      e.data.controllerreleased.button = target;
    }
    lovrEventPush(e);
  }
}

void lovrHeadsetPoll() {
  state.ts = ovr_GetTrackingState(state.session, 0, true);
  ovr_GetInputState(state.session, ovrControllerType_Touch, &state.is);

  ovrSessionStatus status;
  ovr_GetSessionStatus(state.session, &status);

  if (status.ShouldRecenter) {
    ovr_RecenterTrackingOrigin(state.session);
    ovr_ClearShouldRecenterFlag(state.session);
  }

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
  int diff = state.is.Buttons ^ state.lastButtonState;
  int istate = state.is.Buttons;
  checkInput(right, diff, istate, ovrButton_A, CONTROLLER_BUTTON_A);
  checkInput(right, diff, istate, ovrButton_B, CONTROLLER_BUTTON_B);
  checkInput(right, diff, istate, ovrButton_RShoulder, CONTROLLER_BUTTON_TRIGGER);
  checkInput(right, diff, istate, ovrButton_RThumb, CONTROLLER_BUTTON_JOYSTICK);

  checkInput(left, diff, istate, ovrButton_X, CONTROLLER_BUTTON_X);
  checkInput(left, diff, istate, ovrButton_Y, CONTROLLER_BUTTON_Y);
  checkInput(left, diff, istate, ovrButton_LShoulder, CONTROLLER_BUTTON_TRIGGER);
  checkInput(left, diff, istate, ovrButton_LThumb, CONTROLLER_BUTTON_JOYSTICK);
  checkInput(left, diff, istate, ovrButton_Enter, CONTROLLER_BUTTON_MENU);

  state.lastButtonState = state.is.Buttons;
}

int lovrHeadsetIsPresent() {
  return state.hmdPresent? 1 : 0;
}

HeadsetType lovrHeadsetGetType() {
  return HEADSET_RIFT;
}

HeadsetOrigin lovrHeadsetGetOriginType() {
  return ORIGIN_FLOOR;
}

int lovrHeadsetIsMirrored() {
  return (int)state.isMirrored;
}

void lovrHeadsetSetMirrored(int mirror) {
  state.isMirrored = mirror? true : false;
}

void lovrHeadsetGetDisplayDimensions(int* width, int* height) {
  ovrHmdDesc desc = ovr_GetHmdDesc(state.session);
  ovrSizei size = ovr_GetFovTextureSize(state.session, ovrEye_Left, desc.DefaultEyeFov[0], 1.0f);

  *width = size.w;
  *height = size.h;
}

void lovrHeadsetGetClipDistance(float* near, float* far) {
  if (!state.isInitialized) {
    *near = *far = 0.f;
  } else {
    *near = state.clipNear;
    *far = state.clipFar;
  }
}

void lovrHeadsetSetClipDistance(float near, float far) {
  if (!state.isInitialized) return;
  state.clipNear = near;
  state.clipFar = far;
}

float lovrHeadsetGetBoundsWidth() {
  ovrVector3f dimensions;
  ovr_GetBoundaryDimensions(state.session, ovrBoundary_PlayArea, &dimensions);
  return dimensions.x;
}

float lovrHeadsetGetBoundsDepth() {
  ovrVector3f dimensions;
  ovr_GetBoundaryDimensions(state.session, ovrBoundary_PlayArea, &dimensions);
  return dimensions.z;
}

void lovrHeadsetGetBoundsGeometry(float* geometry) {
  // TODO
  memset(geometry, 0, 12 * sizeof(float));
}

void lovrHeadsetGetPosition(float* x, float* y, float* z) {
  ovrVector3f pos = state.ts.HeadPose.ThePose.Position;
  *x = pos.x;
  *y = pos.y;
  *z = pos.z;
}

void lovrHeadsetGetEyePosition(HeadsetEye eye, float* x, float* y, float* z) {
  // we don't actually know these until render time, does this even need to be exposed?
  *x = 0.0f;
  *y = 0.0f;
  *z = 0.0f;
}

void lovrHeadsetGetOrientation(float* angle, float* x, float* y, float* z) {
  ovrQuatf oq = state.ts.HeadPose.ThePose.Orientation;
  float quat[] = { oq.x, oq.y, oq.z, oq.w };
  quat_getAngleAxis(quat, angle, x, y, z);
}

void lovrHeadsetGetVelocity(float* x, float* y, float* z) {
  ovrVector3f vel = state.ts.HeadPose.LinearVelocity;
  *x = vel.x;
  *y = vel.y;
  *z = vel.z;
}

void lovrHeadsetGetAngularVelocity(float* x, float* y, float* z) {
  ovrVector3f vel = state.ts.HeadPose.AngularVelocity;
  *x = vel.x;
  *y = vel.y;
  *z = vel.z;
}

vec_controller_t* lovrHeadsetGetControllers() {
  return &state.controllers;
}

int lovrHeadsetControllerIsPresent(Controller* controller) {
  switch (controller->id) {
    case ovrHand_Left: return (state.is.ControllerType & ovrControllerType_LTouch) == ovrControllerType_LTouch;
    case ovrHand_Right: return (state.is.ControllerType & ovrControllerType_RTouch) == ovrControllerType_RTouch;
    default: return 0;
  }
  return 0;
}

ControllerHand lovrHeadsetControllerGetHand(Controller* controller) {
  switch (controller->id) {
    case ovrHand_Left: return HAND_LEFT;
    case ovrHand_Right: return HAND_RIGHT;
    default: return HAND_UNKNOWN;
  }
  return HAND_UNKNOWN;
}

void lovrHeadsetControllerGetPosition(Controller* controller, float* x, float* y, float* z) {
  ovrVector3f pos = state.ts.HandPoses[controller->id].ThePose.Position;
  *x = pos.x;
  *y = pos.y;
  *z = pos.z;
}

void lovrHeadsetControllerGetOrientation(Controller* controller, float* angle, float* x, float* y, float* z) {
  ovrQuatf orient = state.ts.HandPoses[controller->id].ThePose.Orientation;
  float quat[4] = { orient.x, orient.y, orient.z, orient.w };
  quat_getAngleAxis(quat, angle, x, y, z);
}

float lovrHeadsetControllerGetAxis(Controller* controller, ControllerAxis axis) {
  switch (axis) {
    case CONTROLLER_AXIS_GRIP: return state.is.HandTriggerNoDeadzone[controller->id];
    case CONTROLLER_AXIS_TRIGGER: return state.is.IndexTriggerNoDeadzone[controller->id];
    case CONTROLLER_AXIS_TOUCHPAD_X: return state.is.ThumbstickNoDeadzone[controller->id].x;
    case CONTROLLER_AXIS_TOUCHPAD_Y: return state.is.ThumbstickNoDeadzone[controller->id].y;
    default: return 0.0f;
  }
  return 0.0f;
}

int lovrHeadsetControllerIsDown(Controller* controller, ControllerButton button) {
  int relevant = state.is.Buttons & ((controller->id == ovrHand_Left) ? ovrButton_LMask : ovrButton_RMask);
  switch (button) {
    case CONTROLLER_BUTTON_A: return (relevant & ovrButton_A) > 0;
    case CONTROLLER_BUTTON_B: return (relevant & ovrButton_B) > 0;
    case CONTROLLER_BUTTON_X: return (relevant & ovrButton_X) > 0;
    case CONTROLLER_BUTTON_Y: return (relevant & ovrButton_Y) > 0;
    case CONTROLLER_BUTTON_MENU: return (relevant & ovrButton_Enter) > 0;
    case CONTROLLER_BUTTON_TRIGGER: return (relevant & (ovrButton_LShoulder | ovrButton_RShoulder)) > 0;
    case CONTROLLER_BUTTON_TOUCHPAD:
    case CONTROLLER_BUTTON_JOYSTICK: return (relevant & (ovrButton_LThumb | ovrButton_RThumb)) > 0;
    case CONTROLLER_BUTTON_GRIP: return state.is.HandTrigger[controller->id] > 0.0f;
    default: return 0;
  }
  return 0;
}

int lovrHeadsetControllerIsTouched(Controller* controller, ControllerButton button) {
  int relevant = state.is.Touches & ((controller->id == ovrHand_Left) ? ovrTouch_LButtonMask : ovrTouch_RButtonMask);
  switch (button) {
    case CONTROLLER_BUTTON_A: return (relevant & ovrTouch_A) > 0;
    case CONTROLLER_BUTTON_B: return (relevant & ovrTouch_B) > 0;
    case CONTROLLER_BUTTON_X: return (relevant & ovrTouch_X) > 0;
    case CONTROLLER_BUTTON_Y: return (relevant & ovrTouch_Y) > 0;
    case CONTROLLER_BUTTON_TRIGGER: return (relevant & (ovrTouch_LIndexTrigger | ovrTouch_RIndexTrigger)) > 0;
    case CONTROLLER_BUTTON_TOUCHPAD:
    case CONTROLLER_BUTTON_JOYSTICK: return (relevant & (ovrTouch_LThumb | ovrTouch_RThumb)) > 0;
    default: return 0;
  }
  return 0;
}

void lovrHeadsetControllerVibrate(Controller* controller, float duration, float power) {
  //ovr_SetControllerVibration(state.session, ovrControllerType_LTouch, freq, power);
  // TODO
}

ModelData* lovrHeadsetControllerNewModelData(Controller* controller) {
  // TODO
  return NULL;
}

TextureData* lovrHeadsetControllerNewTextureData(Controller* controller) {
  // TODO
  return NULL;
}

// TODO: need to set up swap chain textures for the eyes and finish view transforms
void lovrHeadsetRenderTo(headsetRenderCallback callback, void* userdata) {
  if (!state.isInitialized) return;

  state.renderCallback = callback;

  ovrHmdDesc desc = ovr_GetHmdDesc(state.session);
  if (!state.mirrorTexture) {
    int i;
    for (i = 0; i < 2; i++) {
      state.eyeTextures[i].size = ovr_GetFovTextureSize(state.session, ovrEye_Left+i, desc.DefaultEyeFov[ovrEye_Left+i], 1.0f);

      ovrTextureSwapChainDesc swdesc = {0};
      swdesc.Type = ovrTexture_2D;
      swdesc.ArraySize = 1;
      swdesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
      swdesc.Width = state.eyeTextures[i].size.w;
      swdesc.Height = state.eyeTextures[i].size.h;
      swdesc.MipLevels = 1;
      swdesc.SampleCount = 1;
      swdesc.StaticImage = ovrFalse;

      ovrTextureSwapChain chain;
      if (OVR_SUCCESS(ovr_CreateTextureSwapChainGL(state.session, &swdesc, &chain))) {
        state.eyeTextures[i].chain = chain;

        int len;
        ovr_GetTextureSwapChainLength(state.session, state.eyeTextures[i].chain, &len);
        int j;
        for (j = 0; j < len; j++) {
          GLuint texId;
          ovr_GetTextureSwapChainBufferGL(state.session, state.eyeTextures[i].chain, j, &texId);
          glBindTexture(GL_TEXTURE_2D, texId);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
          glBindTexture(GL_TEXTURE_2D, 0);
        }
      }
      else {
        lovrAssert(chain != NULL, "Unable to create swap chain.");
        break;
      }
      glGenFramebuffers(1, &state.eyeTextures[i].fboId);

      glGenTextures(1, &state.eyeTextures[i].depthId);
      glBindTexture(GL_TEXTURE_2D, state.eyeTextures[i].depthId);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, state.eyeTextures[i].size.w, state.eyeTextures[i].size.h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    }

    ovrMirrorTextureDesc mdesc;
    memset(&mdesc, 0, sizeof(mdesc));
    mdesc.Width = lovrGraphicsGetWidth();
    mdesc.Height = lovrGraphicsGetHeight();
    mdesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;

    // Create mirror texture and an FBO used to copy mirror texture to back buffer
    ovr_CreateMirrorTextureGL(state.session, &mdesc, &state.mirrorTexture);

    GLuint texId;
    ovr_GetMirrorTextureBufferGL(state.session, state.mirrorTexture, &texId);

    glGenFramebuffers(1, &state.mirrorFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, state.mirrorFBO);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  }

  lovrGraphicsPushCanvas();
  state.isRendering = true;

  ovrEyeRenderDesc eyeRenderDesc[2];
  eyeRenderDesc[0] = ovr_GetRenderDesc(state.session, ovrEye_Left, desc.DefaultEyeFov[0]);
  eyeRenderDesc[1] = ovr_GetRenderDesc(state.session, ovrEye_Right, desc.DefaultEyeFov[1]);
  ovrVector3f HmdToEyeOffset[2] = {
    eyeRenderDesc[0].HmdToEyeOffset,
    eyeRenderDesc[1].HmdToEyeOffset
  };
  ovrPosef EyeRenderPose[2];
  double sensorSampleTime;
  ovr_GetEyePoses(state.session, state.frameIndex, ovrTrue, HmdToEyeOffset, EyeRenderPose, &sensorSampleTime);

  float transform[16];
  float projection[16];
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
    mat4_identity(transform);
    mat4_rotateQuat(transform, orient);
    transform[12] = -(transform[0] * pos[0] + transform[4] * pos[1] + transform[8] * pos[2]);
    transform[13] = -(transform[1] * pos[0] + transform[5] * pos[1] + transform[9] * pos[2]);
    transform[14] = -(transform[2] * pos[0] + transform[6] * pos[1] + transform[10] * pos[2]);

    ovrTextureSwapChain chain = state.eyeTextures[eye].chain;
    if (chain == NULL) {
      lovrAssert(chain != NULL, "Swap chain is broken. This is a bug.");
      continue;
    }

    int curIndex;
    GLuint curTexId;
    ovr_GetTextureSwapChainCurrentIndex(state.session, chain, &curIndex);
    ovr_GetTextureSwapChainBufferGL(state.session, chain, curIndex, &curTexId);
    glBindFramebuffer(GL_FRAMEBUFFER, state.eyeTextures[eye].fboId);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curTexId, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, state.eyeTextures[eye].depthId, 0);
    glViewport(0, 0, state.eyeTextures[eye].size.w, state.eyeTextures[eye].size.h);

    lovrGraphicsPush();
    lovrGraphicsMatrixTransform(MATRIX_VIEW, transform);
    ovrMatrix4f proj = ovrMatrix4f_Projection(desc.DefaultEyeFov[eye], state.clipNear, state.clipFar, ovrProjection_ClipRangeOpenGL);
    mat4_fromMat44(projection, proj.M);
    lovrGraphicsSetProjection(projection);
    lovrGraphicsClear(1, 1);
    callback(eye, userdata);
    lovrGraphicsPop();

    glBindFramebuffer(GL_FRAMEBUFFER, state.eyeTextures[eye].fboId);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

    ovr_CommitTextureSwapChain(state.session, chain);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  ovrLayerEyeFov ld;
  ld.Header.Type = ovrLayerType_EyeFov;
  ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
  for (int eye = 0; eye < 2; eye++) {
    ld.ColorTexture[eye] = state.eyeTextures[eye].chain;
    ovrRecti vp;
    vp.Pos.x = 0;
    vp.Pos.y = 0;
    vp.Size.w = state.eyeTextures[eye].size.w;
    vp.Size.h = state.eyeTextures[eye].size.h;
    ld.Viewport[eye] = vp;
    ld.Fov[eye] = desc.DefaultEyeFov[eye];
    ld.RenderPose[eye] = EyeRenderPose[eye];
    ld.SensorSampleTime = sensorSampleTime;
  }

  ovrLayerHeader* layers = &ld.Header;
  ovr_SubmitFrame(state.session, state.frameIndex, NULL, &layers, 1);
  // apparently if this happens we should kill the session and reinit as long as we're getting ovrError_DisplayLost,
  // lest oculus get upset should you try to get anything on the store.
  // if (!OVR_SUCCESS(result))
  //   goto Done;

  state.frameIndex++;
  state.isRendering = false;
  lovrGraphicsPopCanvas();

  if (state.isMirrored || 1) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, state.mirrorFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    int w = lovrGraphicsGetWidth();
    int h = lovrGraphicsGetHeight();
    glBlitFramebuffer(0, h, w, 0, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  }
}
