#include "headset/headset.h"
#include "resources/actions.json.h"
#include "resources/bindings_vive.json.h"
#include "resources/bindings_knuckles.json.h"
#include "resources/bindings_touch.json.h"
#include "resources/bindings_holographic_controller.json.h"
#include "resources/bindings_vive_tracker_left_elbow.json.h"
#include "resources/bindings_vive_tracker_right_elbow.json.h"
#include "resources/bindings_vive_tracker_left_shoulder.json.h"
#include "resources/bindings_vive_tracker_right_shoulder.json.h"
#include "resources/bindings_vive_tracker_chest.json.h"
#include "resources/bindings_vive_tracker_waist.json.h"
#include "resources/bindings_vive_tracker_left_knee.json.h"
#include "resources/bindings_vive_tracker_right_knee.json.h"
#include "resources/bindings_vive_tracker_left_foot.json.h"
#include "resources/bindings_vive_tracker_right_foot.json.h"
#include "resources/bindings_vive_tracker_camera.json.h"
#include "resources/bindings_vive_tracker_keyboard.json.h"
#include "data/blob.h"
#include "event/event.h"
#include "filesystem/filesystem.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "graphics/model.h"
#include "graphics/texture.h"
#include "core/maf.h"
#include "core/os.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#undef EXTERN_C
#include <openvr_capi.h>

// From openvr_capi.h
extern intptr_t VR_InitInternal(EVRInitError *peError, EVRApplicationType eType);
extern void VR_ShutdownInternal();
extern bool VR_IsHmdPresent();
extern intptr_t VR_GetGenericInterface(const char* pchInterfaceVersion, EVRInitError* peError);
extern bool VR_IsRuntimeInstalled();

#define HEADSET k_unTrackedDeviceIndex_Hmd
#define INVALID_DEVICE k_unTrackedDeviceIndexInvalid

enum {
  eBone_Root = 0,
  eBone_Wrist,
  eBone_Thumb0,
  eBone_Thumb1,
  eBone_Thumb2,
  eBone_Thumb3,
  eBone_IndexFinger0,
  eBone_IndexFinger1,
  eBone_IndexFinger2,
  eBone_IndexFinger3,
  eBone_IndexFinger4,
  eBone_MiddleFinger0,
  eBone_MiddleFinger1,
  eBone_MiddleFinger2,
  eBone_MiddleFinger3,
  eBone_MiddleFinger4,
  eBone_RingFinger0,
  eBone_RingFinger1,
  eBone_RingFinger2,
  eBone_RingFinger3,
  eBone_RingFinger4,
  eBone_PinkyFinger0,
  eBone_PinkyFinger1,
  eBone_PinkyFinger2,
  eBone_PinkyFinger3,
  eBone_PinkyFinger4,
  eBone_Aux_Thumb,
  eBone_Aux_IndexFinger,
  eBone_Aux_MiddleFinger,
  eBone_Aux_RingFinger,
  eBone_Aux_PinkyFinger,
  eBone_Count
};

static struct {
  struct VR_IVRSystem_FnTable* system;
  struct VR_IVRCompositor_FnTable* compositor;
  struct VR_IVRChaperone_FnTable* chaperone;
  struct VR_IVRRenderModels_FnTable* renderModels;
  struct VR_IVRInput_FnTable* input;
  VRActionSetHandle_t actionSet;
  VRActionHandle_t poseActions[MAX_DEVICES];
  VRActionHandle_t buttonActions[2][MAX_BUTTONS];
  VRActionHandle_t touchActions[2][MAX_BUTTONS];
  VRActionHandle_t axisActions[2][MAX_AXES];
  VRActionHandle_t skeletonActions[2];
  VRActionHandle_t hapticActions[2];
  VRInputValueHandle_t inputSources[3];
  TrackedDevicePose_t renderPoses[64];
  Canvas* canvas;
  float* mask;
  float boundsGeometry[16];
  float clipNear;
  float clipFar;
  float supersample;
  float offset;
  int msaa;
} state;

static TrackedDeviceIndex_t getDeviceIndex(Device device) {
  switch (device) {
    case DEVICE_HEAD: return HEADSET;
    case DEVICE_HAND_LEFT: return state.system->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_LeftHand);
    case DEVICE_HAND_RIGHT: return state.system->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_RightHand);
    default: return INVALID_DEVICE;
  }
}

static bool openvr_getName(char* name, size_t length);
static bool openvr_init(float supersample, float offset, uint32_t msaa) {
  if (!VR_IsHmdPresent() || !VR_IsRuntimeInstalled()) {
    return false;
  }

  EVRInitError vrError;
  VR_InitInternal(&vrError, EVRApplicationType_VRApplication_Scene);
  if (vrError != EVRInitError_VRInitError_None) {
    return false;
  }

  char buffer[64];
  sprintf(buffer, "FnTable:%s", IVRSystem_Version), state.system = (struct VR_IVRSystem_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  sprintf(buffer, "FnTable:%s", IVRCompositor_Version), state.compositor = (struct VR_IVRCompositor_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  sprintf(buffer, "FnTable:%s", IVRChaperone_Version), state.chaperone = (struct VR_IVRChaperone_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  sprintf(buffer, "FnTable:%s", IVRRenderModels_Version), state.renderModels = (struct VR_IVRRenderModels_FnTable*) VR_GetGenericInterface(buffer, &vrError);
  sprintf(buffer, "FnTable:%s", IVRInput_Version), state.input = (struct VR_IVRInput_FnTable*) VR_GetGenericInterface(buffer, &vrError);

  if (!state.system || !state.compositor || !state.chaperone || !state.renderModels || !state.input) {
    VR_ShutdownInternal();
    return false;
  }

  // Find the location of the action manifest, create it if it doesn't exist or isn't in the save directory
  const char* actionManifestLocation = lovrFilesystemGetRealDirectory("actions.json");
  if (!actionManifestLocation || strcmp(actionManifestLocation, lovrFilesystemGetSaveDirectory())) {
    if (
      lovrFilesystemWrite("actions.json", (const char*) src_resources_actions_json, src_resources_actions_json_len, false) != src_resources_actions_json_len ||
      lovrFilesystemWrite("bindings_vive.json", (const char*) src_resources_bindings_vive_json, src_resources_bindings_vive_json_len, false) != src_resources_bindings_vive_json_len ||
      lovrFilesystemWrite("bindings_knuckles.json", (const char*) src_resources_bindings_knuckles_json, src_resources_bindings_knuckles_json_len, false) != src_resources_bindings_knuckles_json_len ||
      lovrFilesystemWrite("bindings_touch.json", (const char*) src_resources_bindings_touch_json, src_resources_bindings_touch_json_len, false) != src_resources_bindings_touch_json_len ||
      lovrFilesystemWrite("bindings_holographic_controller.json", (const char*) src_resources_bindings_holographic_controller_json, src_resources_bindings_holographic_controller_json_len, false) != src_resources_bindings_holographic_controller_json_len ||
      lovrFilesystemWrite("bindings_vive_tracker_left_elbow.json", (const char*) src_resources_bindings_vive_tracker_left_elbow_json, src_resources_bindings_vive_tracker_left_elbow_json_len, false) != src_resources_bindings_vive_tracker_left_elbow_json_len ||
      lovrFilesystemWrite("bindings_vive_tracker_right_elbow.json", (const char*) src_resources_bindings_vive_tracker_right_elbow_json, src_resources_bindings_vive_tracker_right_elbow_json_len, false) != src_resources_bindings_vive_tracker_right_elbow_json_len ||
      lovrFilesystemWrite("bindings_vive_tracker_left_shoulder.json", (const char*) src_resources_bindings_vive_tracker_left_shoulder_json, src_resources_bindings_vive_tracker_left_shoulder_json_len, false) != src_resources_bindings_vive_tracker_left_shoulder_json_len ||
      lovrFilesystemWrite("bindings_vive_tracker_right_shoulder.json", (const char*) src_resources_bindings_vive_tracker_right_shoulder_json, src_resources_bindings_vive_tracker_right_shoulder_json_len, false) != src_resources_bindings_vive_tracker_right_shoulder_json_len ||
      lovrFilesystemWrite("bindings_vive_tracker_chest.json", (const char*) src_resources_bindings_vive_tracker_chest_json, src_resources_bindings_vive_tracker_chest_json_len, false) != src_resources_bindings_vive_tracker_chest_json_len ||
      lovrFilesystemWrite("bindings_vive_tracker_waist.json", (const char*) src_resources_bindings_vive_tracker_waist_json, src_resources_bindings_vive_tracker_waist_json_len, false) != src_resources_bindings_vive_tracker_waist_json_len ||
      lovrFilesystemWrite("bindings_vive_tracker_left_knee.json", (const char*) src_resources_bindings_vive_tracker_left_knee_json, src_resources_bindings_vive_tracker_left_knee_json_len, false) != src_resources_bindings_vive_tracker_left_knee_json_len ||
      lovrFilesystemWrite("bindings_vive_tracker_right_knee.json", (const char*) src_resources_bindings_vive_tracker_right_knee_json, src_resources_bindings_vive_tracker_right_knee_json_len, false) != src_resources_bindings_vive_tracker_right_knee_json_len ||
      lovrFilesystemWrite("bindings_vive_tracker_left_foot.json", (const char*) src_resources_bindings_vive_tracker_left_foot_json, src_resources_bindings_vive_tracker_left_foot_json_len, false) != src_resources_bindings_vive_tracker_left_foot_json_len ||
      lovrFilesystemWrite("bindings_vive_tracker_right_foot.json", (const char*) src_resources_bindings_vive_tracker_right_foot_json, src_resources_bindings_vive_tracker_right_foot_json_len, false) != src_resources_bindings_vive_tracker_right_foot_json_len ||
      lovrFilesystemWrite("bindings_vive_tracker_camera.json", (const char*) src_resources_bindings_vive_tracker_camera_json, src_resources_bindings_vive_tracker_camera_json_len, false) != src_resources_bindings_vive_tracker_camera_json_len ||
      lovrFilesystemWrite("bindings_vive_tracker_keyboard.json", (const char*) src_resources_bindings_vive_tracker_keyboard_json, src_resources_bindings_vive_tracker_keyboard_json_len, false) != src_resources_bindings_vive_tracker_keyboard_json_len
    ) {
      VR_ShutdownInternal();
      return false;
    }
  }

  char path[LOVR_PATH_MAX];
  snprintf(path, sizeof(path), "%s%cactions.json", lovrFilesystemGetSaveDirectory(), LOVR_PATH_SEP);
  state.input->SetActionManifestPath(path);
  state.input->GetActionSetHandle("/actions/lovr", &state.actionSet);

  state.input->GetActionHandle("/actions/lovr/in/headPose", &state.poseActions[DEVICE_HEAD]);
  state.input->GetActionHandle("/actions/lovr/in/leftHandPose", &state.poseActions[DEVICE_HAND_LEFT]);
  state.input->GetActionHandle("/actions/lovr/in/rightHandPose", &state.poseActions[DEVICE_HAND_RIGHT]);
  state.input->GetActionHandle("/actions/lovr/in/leftHandPoint", &state.poseActions[DEVICE_HAND_LEFT_POINT]);
  state.input->GetActionHandle("/actions/lovr/in/rightHandPoint", &state.poseActions[DEVICE_HAND_RIGHT_POINT]);
  state.input->GetActionHandle("/actions/lovr/in/leftElbowPose", &state.poseActions[DEVICE_ELBOW_LEFT]);
  state.input->GetActionHandle("/actions/lovr/in/rightElbowPose", &state.poseActions[DEVICE_ELBOW_RIGHT]);
  state.input->GetActionHandle("/actions/lovr/in/leftShoulderPose", &state.poseActions[DEVICE_SHOULDER_LEFT]);
  state.input->GetActionHandle("/actions/lovr/in/rightShoulderPose", &state.poseActions[DEVICE_SHOULDER_RIGHT]);
  state.input->GetActionHandle("/actions/lovr/in/chestPose", &state.poseActions[DEVICE_CHEST]);
  state.input->GetActionHandle("/actions/lovr/in/waistPose", &state.poseActions[DEVICE_WAIST]);
  state.input->GetActionHandle("/actions/lovr/in/leftKneePose", &state.poseActions[DEVICE_KNEE_LEFT]);
  state.input->GetActionHandle("/actions/lovr/in/rightKneePose", &state.poseActions[DEVICE_KNEE_RIGHT]);
  state.input->GetActionHandle("/actions/lovr/in/leftFootPose", &state.poseActions[DEVICE_FOOT_LEFT]);
  state.input->GetActionHandle("/actions/lovr/in/rightFootPose", &state.poseActions[DEVICE_FOOT_RIGHT]);
  state.input->GetActionHandle("/actions/lovr/in/cameraPose", &state.poseActions[DEVICE_CAMERA]);
  state.input->GetActionHandle("/actions/lovr/in/keyboardPose", &state.poseActions[DEVICE_KEYBOARD]);

  state.input->GetActionHandle("/actions/lovr/in/leftTriggerDown", &state.buttonActions[0][BUTTON_TRIGGER]);
  state.input->GetActionHandle("/actions/lovr/in/leftThumbstickDown", &state.buttonActions[0][BUTTON_THUMBSTICK]);
  state.input->GetActionHandle("/actions/lovr/in/leftTouchpadDown", &state.buttonActions[0][BUTTON_TOUCHPAD]);
  state.input->GetActionHandle("/actions/lovr/in/leftGripDown", &state.buttonActions[0][BUTTON_GRIP]);
  state.input->GetActionHandle("/actions/lovr/in/leftMenuDown", &state.buttonActions[0][BUTTON_MENU]);
  state.input->GetActionHandle("/actions/lovr/in/leftADown", &state.buttonActions[0][BUTTON_A]);
  state.input->GetActionHandle("/actions/lovr/in/leftBDown", &state.buttonActions[0][BUTTON_B]);
  state.input->GetActionHandle("/actions/lovr/in/leftXDown", &state.buttonActions[0][BUTTON_X]);
  state.input->GetActionHandle("/actions/lovr/in/leftYDown", &state.buttonActions[0][BUTTON_Y]);

  state.input->GetActionHandle("/actions/lovr/in/rightTriggerDown", &state.buttonActions[1][BUTTON_TRIGGER]);
  state.input->GetActionHandle("/actions/lovr/in/rightThumbstickDown", &state.buttonActions[1][BUTTON_THUMBSTICK]);
  state.input->GetActionHandle("/actions/lovr/in/rightTouchpadDown", &state.buttonActions[1][BUTTON_TOUCHPAD]);
  state.input->GetActionHandle("/actions/lovr/in/rightGripDown", &state.buttonActions[1][BUTTON_GRIP]);
  state.input->GetActionHandle("/actions/lovr/in/rightMenuDown", &state.buttonActions[1][BUTTON_MENU]);
  state.input->GetActionHandle("/actions/lovr/in/rightADown", &state.buttonActions[1][BUTTON_A]);
  state.input->GetActionHandle("/actions/lovr/in/rightBDown", &state.buttonActions[1][BUTTON_B]);
  state.input->GetActionHandle("/actions/lovr/in/rightXDown", &state.buttonActions[1][BUTTON_X]);
  state.input->GetActionHandle("/actions/lovr/in/rightYDown", &state.buttonActions[1][BUTTON_Y]);

  state.input->GetActionHandle("/actions/lovr/in/leftTriggerTouch", &state.touchActions[0][BUTTON_TRIGGER]);
  state.input->GetActionHandle("/actions/lovr/in/leftThumbstickTouch", &state.touchActions[0][BUTTON_THUMBSTICK]);
  state.input->GetActionHandle("/actions/lovr/in/leftTouchpadTouch", &state.touchActions[0][BUTTON_TOUCHPAD]);
  state.input->GetActionHandle("/actions/lovr/in/leftGripTouch", &state.touchActions[0][BUTTON_GRIP]);
  state.input->GetActionHandle("/actions/lovr/in/leftMenuTouch", &state.touchActions[0][BUTTON_MENU]);
  state.input->GetActionHandle("/actions/lovr/in/leftATouch", &state.touchActions[0][BUTTON_A]);
  state.input->GetActionHandle("/actions/lovr/in/leftBTouch", &state.touchActions[0][BUTTON_B]);
  state.input->GetActionHandle("/actions/lovr/in/leftXTouch", &state.touchActions[0][BUTTON_X]);
  state.input->GetActionHandle("/actions/lovr/in/leftYTouch", &state.touchActions[0][BUTTON_Y]);

  state.input->GetActionHandle("/actions/lovr/in/rightTriggerTouch", &state.touchActions[1][BUTTON_TRIGGER]);
  state.input->GetActionHandle("/actions/lovr/in/rightThumbstickTouch", &state.touchActions[1][BUTTON_THUMBSTICK]);
  state.input->GetActionHandle("/actions/lovr/in/rightTouchpadTouch", &state.touchActions[1][BUTTON_TOUCHPAD]);
  state.input->GetActionHandle("/actions/lovr/in/rightGripTouch", &state.touchActions[1][BUTTON_GRIP]);
  state.input->GetActionHandle("/actions/lovr/in/rightMenuTouch", &state.touchActions[1][BUTTON_MENU]);
  state.input->GetActionHandle("/actions/lovr/in/rightATouch", &state.touchActions[1][BUTTON_A]);
  state.input->GetActionHandle("/actions/lovr/in/rightBTouch", &state.touchActions[1][BUTTON_B]);
  state.input->GetActionHandle("/actions/lovr/in/rightXTouch", &state.touchActions[1][BUTTON_X]);
  state.input->GetActionHandle("/actions/lovr/in/rightYTouch", &state.touchActions[1][BUTTON_Y]);

  state.input->GetActionHandle("/actions/lovr/in/leftTriggerAxis", &state.axisActions[0][AXIS_TRIGGER]);
  state.input->GetActionHandle("/actions/lovr/in/leftThumbstickAxis", &state.axisActions[0][AXIS_THUMBSTICK]);
  state.input->GetActionHandle("/actions/lovr/in/leftTouchpadAxis", &state.axisActions[0][AXIS_TOUCHPAD]);
  state.input->GetActionHandle("/actions/lovr/in/leftGripAxis", &state.axisActions[0][AXIS_GRIP]);

  state.input->GetActionHandle("/actions/lovr/in/rightTriggerAxis", &state.axisActions[1][AXIS_TRIGGER]);
  state.input->GetActionHandle("/actions/lovr/in/rightThumbstickAxis", &state.axisActions[1][AXIS_THUMBSTICK]);
  state.input->GetActionHandle("/actions/lovr/in/rightTouchpadAxis", &state.axisActions[1][AXIS_TOUCHPAD]);
  state.input->GetActionHandle("/actions/lovr/in/rightGripAxis", &state.axisActions[1][AXIS_GRIP]);

  state.input->GetActionHandle("/actions/lovr/in/leftHandSkeleton", &state.skeletonActions[0]);
  state.input->GetActionHandle("/actions/lovr/in/rightHandSkeleton", &state.skeletonActions[1]);

  state.input->GetActionHandle("/actions/lovr/out/leftHandBZZ", &state.hapticActions[0]);
  state.input->GetActionHandle("/actions/lovr/out/rightHandBZZ", &state.hapticActions[1]);

  state.input->GetInputSourceHandle("/user/head", &state.inputSources[DEVICE_HEAD]);
  state.input->GetInputSourceHandle("/user/hand/left", &state.inputSources[DEVICE_HAND_LEFT]);
  state.input->GetInputSourceHandle("/user/hand/right", &state.inputSources[DEVICE_HAND_RIGHT]);

  state.clipNear = 0.1f;
  state.clipFar = 100.f;
  state.supersample = supersample;
  state.offset = state.compositor->GetTrackingSpace() == ETrackingUniverseOrigin_TrackingUniverseStanding ? 0. : offset;
  state.msaa = msaa;

  return true;
}

static void openvr_destroy(void) {
  lovrRelease(state.canvas, lovrCanvasDestroy);
  VR_ShutdownInternal();
  free(state.mask);
  memset(&state, 0, sizeof(state));
}

static bool openvr_getName(char* name, size_t length) {
  ETrackedPropertyError error;
  state.system->GetStringTrackedDeviceProperty(HEADSET, ETrackedDeviceProperty_Prop_ManufacturerName_String, name, (uint32_t) length, &error);
  return error == ETrackedPropertyError_TrackedProp_Success;
}

static HeadsetOrigin openvr_getOriginType(void) {
  switch (state.compositor->GetTrackingSpace()) {
    case ETrackingUniverseOrigin_TrackingUniverseSeated: return ORIGIN_HEAD;
    case ETrackingUniverseOrigin_TrackingUniverseStanding: return ORIGIN_FLOOR;
    default: return ORIGIN_HEAD;
  }
}

static void openvr_getDisplayDimensions(uint32_t* width, uint32_t* height) {
  state.system->GetRecommendedRenderTargetSize(width, height);
}

static float openvr_getDisplayFrequency(void) {
  return state.system->GetFloatTrackedDeviceProperty(HEADSET, ETrackedDeviceProperty_Prop_DisplayFrequency_Float, NULL);
}

static const float* openvr_getDisplayMask(uint32_t* count) {
  struct HiddenAreaMesh_t hiddenAreaMesh = state.system->GetHiddenAreaMesh(EVREye_Eye_Left, EHiddenAreaMeshType_k_eHiddenAreaMesh_Standard);

  if (hiddenAreaMesh.unTriangleCount == 0) {
    *count = 0;
    return NULL;
  }

  state.mask = realloc(state.mask, hiddenAreaMesh.unTriangleCount * 3 * 2 * sizeof(float));
  lovrAssert(state.mask, "Out of memory");

  for (uint32_t i = 0; i < 3 * hiddenAreaMesh.unTriangleCount; i++) {
    state.mask[2 * i + 0] = hiddenAreaMesh.pVertexData[i].v[0];
    state.mask[2 * i + 1] = hiddenAreaMesh.pVertexData[i].v[1];
  }

  *count = hiddenAreaMesh.unTriangleCount * 3 * 2;
  return state.mask;
}

static double openvr_getDisplayTime(void) {
  float secondsSinceVsync;
  state.system->GetTimeSinceLastVsync(&secondsSinceVsync, NULL);

  float frequency = openvr_getDisplayFrequency();
  float frameDuration = 1.f / frequency;
  float vsyncToPhotons = state.system->GetFloatTrackedDeviceProperty(HEADSET, ETrackedDeviceProperty_Prop_SecondsFromVsyncToPhotons_Float, NULL);

  return os_get_time() + (double) (frameDuration - secondsSinceVsync + vsyncToPhotons);
}

static uint32_t openvr_getViewCount(void) {
  return 2;
}

static bool openvr_getViewPose(uint32_t view, float* position, float* orientation) {
  EVREye eye = view ? EVREye_Eye_Right : EVREye_Eye_Left;

  float transform[16], offset[16];
  mat4_fromMat34(transform, state.renderPoses[k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m);
  mat4_mul(transform, mat4_fromMat34(offset, state.system->GetEyeToHeadTransform(eye).m));
  mat4_getPosition(transform, position);
  mat4_getOrientation(transform, orientation);
  position[1] += state.offset;

  return view < 2;
}

static bool openvr_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  EVREye eye = view ? EVREye_Eye_Right : EVREye_Eye_Left;
  state.system->GetProjectionRaw(eye, left, right, up, down);
  *left = atanf(*left);
  *right = atanf(*right);
  *up = atanf(*up);
  *down = atanf(*down);
  return view < 2;
}

static void openvr_getClipDistance(float* clipNear, float* clipFar) {
  *clipNear = state.clipNear;
  *clipFar = state.clipFar;
}

static void openvr_setClipDistance(float clipNear, float clipFar) {
  state.clipNear = clipNear;
  state.clipFar = clipFar;
}

static void openvr_getBoundsDimensions(float* width, float* depth) {
  state.chaperone->GetPlayAreaSize(width, depth);
}

static const float* openvr_getBoundsGeometry(uint32_t* count) {
  struct HmdQuad_t quad;
  if (state.chaperone->GetPlayAreaRect(&quad)) {
    for (int i = 0; i < 4; i++) {
      state.boundsGeometry[4 * i + 0] = quad.vCorners[i].v[0];
      state.boundsGeometry[4 * i + 1] = quad.vCorners[i].v[1];
      state.boundsGeometry[4 * i + 2] = quad.vCorners[i].v[2];
    }

    *count = 16;
    return state.boundsGeometry;
  }

  return NULL;
}

static bool openvr_getPose(Device device, vec3 position, quat orientation) {
  float transform[16];

  // Early exit for head pose
  if (device == DEVICE_HEAD) {
    mat4_fromMat34(transform, state.renderPoses[k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m);
    transform[13] += state.offset;
    mat4_getPosition(transform, position);
    mat4_getOrientation(transform, orientation);
    return state.renderPoses[k_unTrackedDeviceIndex_Hmd].bPoseIsValid;
  }

  // Lighthouse devices use the old tracked device index API
  if (device >= DEVICE_BEACON_1 && device <= DEVICE_BEACON_4) {
    TrackedDeviceIndex_t devices[4];
    ETrackedDeviceClass class = ETrackedDeviceClass_TrackedDeviceClass_TrackingReference;
    uint32_t count = state.system->GetSortedTrackedDeviceIndicesOfClass(class, devices, 4, 0);
    uint32_t index = device - DEVICE_BEACON_1;
    if (index >= count) return false;
    TrackedDevicePose_t* pose = &state.renderPoses[devices[index]];
    mat4_fromMat34(transform, pose->mDeviceToAbsoluteTracking.m);
    transform[13] += state.offset;
    mat4_getPosition(transform, position);
    mat4_getOrientation(transform, orientation);
    return pose->bPoseIsValid;
  }

  // Generic device
  if (state.poseActions[device]) {
    InputPoseActionData_t action;
    state.input->GetPoseActionDataForNextFrame(state.poseActions[device], state.compositor->GetTrackingSpace(), &action, sizeof(action), 0);
    mat4_fromMat34(transform, action.pose.mDeviceToAbsoluteTracking.m);
    transform[13] += state.offset;
    mat4_getPosition(transform, position);
    mat4_getOrientation(transform, orientation);
    return action.pose.bPoseIsValid;
  }

  return false;
}

static bool openvr_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  InputPoseActionData_t action;
  TrackedDevicePose_t* pose;

  if (device == DEVICE_HEAD) {
    pose = &state.renderPoses[k_unTrackedDeviceIndex_Hmd];
  } else if (state.poseActions[device]) {
    state.input->GetPoseActionDataForNextFrame(state.poseActions[device], state.compositor->GetTrackingSpace(), &action, sizeof(action), 0);
    pose = &action.pose;
  } else {
    return false;
  }

  vec3_init(velocity, pose->vVelocity.v);
  vec3_init(angularVelocity, pose->vAngularVelocity.v);
  return pose->bPoseIsValid;
}

static bool openvr_isDown(Device device, DeviceButton button, bool* down, bool* changed) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  InputDigitalActionData_t action;
  state.input->GetDigitalActionData(state.buttonActions[device - DEVICE_HAND_LEFT][button], &action, sizeof(action), 0);
  *down = action.bState;
  *changed = action.bChanged;
  return action.bActive;
}

static bool openvr_isTouched(Device device, DeviceButton button, bool* touched) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  InputDigitalActionData_t action;
  state.input->GetDigitalActionData(state.touchActions[device - DEVICE_HAND_LEFT][button], &action, sizeof(action), 0);
  *touched = action.bState;
  return action.bActive;
}

static bool openvr_getAxis(Device device, DeviceAxis axis, vec3 value) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  InputAnalogActionData_t action;
  state.input->GetAnalogActionData(state.axisActions[device - DEVICE_HAND_LEFT][axis], &action, sizeof(action), 0);
  vec3_set(value, action.x, action.y, action.z);
  return action.bActive;
}

static bool openvr_getSkeleton(Device device, float* poses) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  // Bone transforms are relative to the hand instead of the origin, so get the hand pose first
  InputPoseActionData_t handPose;
  state.input->GetPoseActionDataForNextFrame(state.poseActions[device], state.compositor->GetTrackingSpace(), &handPose, sizeof(handPose), 0);
  if (!handPose.pose.bPoseIsValid) {
    return false;
  }
  float transform[16], position[4], orientation[4];
  mat4_fromMat34(transform, handPose.pose.mDeviceToAbsoluteTracking.m);
  transform[13] += state.offset;
  mat4_getPosition(transform, position);
  mat4_getOrientation(transform, orientation);

  InputSkeletalActionData_t info;
  VRActionHandle_t action = state.skeletonActions[device - DEVICE_HAND_LEFT];
  EVRInputError error = state.input->GetSkeletalActionData(action, &info, sizeof(info));
  if (error || !info.bActive) {
    return false;
  }

  VRBoneTransform_t bones[32];

  uint32_t boneCount;
  error = state.input->GetBoneCount(action, &boneCount);
  if (error || boneCount > sizeof(bones) / sizeof(bones[0])) {
    return false;
  }

  EVRSkeletalTransformSpace space = EVRSkeletalTransformSpace_VRSkeletalTransformSpace_Model;
  EVRSkeletalMotionRange motionRange = EVRSkeletalMotionRange_VRSkeletalMotionRange_WithController;
  error = state.input->GetSkeletalBoneData(action, space, motionRange, bones, boneCount);
  if (error) {
    printf("No bone data %d\n", error);
    return false;
  }

  // Copy SteamVR bone transform to output (indices match up)
  // Swap x/w (HmdQuaternionf_t has w first)
  // Premultiply by hand pose
  float* pose = poses;

  // SteamVR has a root joint instead of a palm joint, we zero out the root joint so it is the same
  // as the regular hand pose
  memset(&bones[0], 0, sizeof(bones[0]));
  bones[0].orientation.w = 1.f;

  for (uint32_t i = 1; i < HAND_JOINT_COUNT; i++) {
    memcpy(pose, &bones[i].position, 8 * sizeof(float));
    float w = pose[4];
    pose[4] = pose[7];
    pose[7] = w;
    quat_rotate(orientation, pose);
    vec3_add(pose, position);
    quat_mul(pose + 4, orientation, pose + 4);
    pose += 8;
  }

  return true;
}

static bool openvr_vibrate(Device device, float strength, float duration, float frequency) {
  if (duration <= 0.f || (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT)) return false;

  if (frequency <= 0.f) {
    frequency = 1.f;
  }

  state.input->TriggerHapticVibrationAction(state.hapticActions[device - DEVICE_HAND_LEFT], 0.f, duration, frequency, strength, 0);
  return true;
}

static bool loadRenderModel(char* name, RenderModel_t** model, RenderModel_TextureMap_t** texture) {
  loadModel:
  switch (state.renderModels->LoadRenderModel_Async(name, model)) {
    case EVRRenderModelError_VRRenderModelError_Loading: os_sleep(.001); goto loadModel;
    case EVRRenderModelError_VRRenderModelError_None: break;
    default: return false;
  }

  loadTexture:
  switch (state.renderModels->LoadTexture_Async((*model)->diffuseTextureId, texture)) {
    case EVRRenderModelError_VRRenderModelError_Loading: os_sleep(.001); goto loadTexture;
    case EVRRenderModelError_VRRenderModelError_None: break;
    default: state.renderModels->FreeRenderModel(*model); return false;
  }

  return true;
}

static ModelData* openvr_newModelData(Device device, bool animated) {
  TrackedDeviceIndex_t index = getDeviceIndex(device);
  if (index == INVALID_DEVICE) return NULL;

  RenderModel_t* renderModel = NULL;
  RenderModel_TextureMap_t* renderModelTexture = NULL;
  RenderModel_t** renderModels = NULL;
  RenderModel_TextureMap_t** renderModelTextures = NULL;
  uint32_t modelCount = 0;

  char renderModelName[1024];
  ETrackedDeviceProperty renderModelNameProperty = ETrackedDeviceProperty_Prop_RenderModelName_String;
  state.system->GetStringTrackedDeviceProperty(index, renderModelNameProperty, renderModelName, 1024, NULL);

  char* names = NULL;
  size_t namesSize = 0;
  uint32_t charCount = 0;

  if (!animated) {
    modelCount = 1;
    renderModels = &renderModel;
    renderModelTextures = &renderModelTexture;
    if (!loadRenderModel(renderModelName, &renderModel, &renderModelTexture)) {
      return NULL;
    }
  } else {
    uint32_t componentCount = state.renderModels->GetComponentCount(renderModelName);
    renderModels = malloc(componentCount * sizeof(*renderModels));
    renderModelTextures = malloc(componentCount * sizeof(*renderModelTextures));
    lovrAssert(renderModels && renderModelTextures, "Out of memory");
    for (uint32_t i = 0; i < componentCount; i++) {
      if (namesSize < charCount + 256) {
        namesSize += 256;
        names = realloc(names, namesSize);
        lovrAssert(names, "Out of memory");
      }

      char componentModel[1024];
      uint32_t size = state.renderModels->GetComponentName(renderModelName, i, names + charCount, 256);
      if (!state.renderModels->GetComponentRenderModelName(renderModelName, names + charCount, componentModel, sizeof(componentModel))) {
        continue;
      }

      // Sadly this loads the components serially...
      if (!loadRenderModel(componentModel, &renderModels[modelCount], &renderModelTextures[modelCount])) {
        for (uint32_t j = 0; modelCount > 0 && j < modelCount; j++) {
          state.renderModels->FreeRenderModel(renderModels[j]);
          state.renderModels->FreeTexture(renderModelTextures[j]);
        }
        free(renderModels);
        free(renderModelTextures);
        return NULL;
      }

      charCount += size;
      modelCount++;
    }
  }

  ModelData* model = calloc(1, sizeof(ModelData));
  lovrAssert(model, "Out of memory");
  model->ref = 1;
  model->blobCount = 2;
  model->nodeCount = animated ? (1 + modelCount) : 1;
  model->bufferCount = 2 * modelCount;
  model->attributeCount = 4 * modelCount;
  model->imageCount = modelCount;
  model->materialCount = modelCount;
  model->primitiveCount = modelCount;
  model->childCount = animated ? modelCount : 0;
  model->charCount = charCount;

  lovrModelDataAllocate(model);

  memcpy(model->chars, names, charCount);
  free(names);

  uint32_t totalVertexCount = 0;
  uint32_t totalIndexCount = 0;

  for (uint32_t i = 0; i < modelCount; i++) {
    lovrAssert(renderModels[i]->unTriangleCount > 0, "Unsupported OpenVR model: no index buffer (please report this)");
    totalVertexCount += renderModels[i]->unVertexCount;
    totalIndexCount += renderModels[i]->unTriangleCount * 3;
  }

  size_t vertexSize = sizeof(RenderModel_Vertex_t);
  float* vertices = malloc(totalVertexCount * vertexSize);
  uint16_t* indices = malloc(totalIndexCount * sizeof(uint16_t));
  lovrAssert(vertices && indices, "Out of memory");

  model->blobs[0] = lovrBlobCreate(vertices, totalVertexCount * vertexSize, "OpenVR Model Vertices");
  model->blobs[1] = lovrBlobCreate(indices, totalIndexCount * sizeof(uint16_t), "OpenVR Model Indices");

  char* name = model->chars;

  for (uint32_t i = 0; i < modelCount; i++) {
    uint32_t vertexCount = renderModels[i]->unVertexCount;
    uint32_t indexCount = renderModels[i]->unTriangleCount * 3;

    memcpy(vertices, renderModels[i]->rVertexData, vertexCount * vertexSize);
    memcpy(indices, renderModels[i]->rIndexData, indexCount * sizeof(uint16_t));

    model->buffers[2 * i + 0] = (ModelBuffer) { .data = (char*) vertices, .size = vertexCount * vertexSize, .stride = vertexSize };
    model->buffers[2 * i + 1] = (ModelBuffer) { .data = (char*) indices, .size = indexCount * sizeof(uint16_t), .stride = sizeof(uint16_t) };

    vertices += vertexCount * vertexSize / sizeof(float);
    indices += indexCount;

    model->attributes[4 * i + 0] = (ModelAttribute) {
      .buffer = 2 * i,
      .offset = offsetof(RenderModel_Vertex_t, vPosition),
      .count = vertexCount,
      .type = F32,
      .components = 3
    };

    model->attributes[4 * i + 1] = (ModelAttribute) {
      .buffer = 2 * i,
      .offset = offsetof(RenderModel_Vertex_t, vNormal),
      .count = vertexCount,
      .type = F32,
      .components = 3
    };

    model->attributes[4 * i + 2] = (ModelAttribute) {
      .buffer = 2 * i,
      .offset = offsetof(RenderModel_Vertex_t, rfTextureCoord),
      .count = vertexCount,
      .type = F32,
      .components = 2
    };

    model->attributes[4 * i + 3] = (ModelAttribute) {
      .buffer = 2 * i + 1,
      .offset = 0,
      .count = indexCount,
      .type = U16,
      .components = 1
    };

    RenderModel_TextureMap_t* texture = renderModelTextures[i];
    model->images[i] = lovrImageCreate(texture->unWidth, texture->unHeight, NULL, 0, FORMAT_RGBA);
    memcpy(model->images[i]->blob->data, texture->rubTextureMapData, texture->unWidth * texture->unHeight * 4);

    model->materials[i] = (ModelMaterial) {
      .colors[COLOR_DIFFUSE] = { 1.f, 1.f, 1.f, 1.f },
      .images[TEXTURE_DIFFUSE] = i,
      .filters[TEXTURE_DIFFUSE] = lovrGraphicsGetDefaultFilter()
    };

    model->primitives[i] = (ModelPrimitive) {
      .mode = DRAW_TRIANGLES,
      .attributes = {
        [ATTR_POSITION] = &model->attributes[4 * i + 0],
        [ATTR_NORMAL] = &model->attributes[4 * i + 1],
        [ATTR_TEXCOORD] = &model->attributes[4 * i + 2]
      },
      .indices = &model->attributes[4 * i + 3],
      .material = i
    };

    model->nodes[i] = (ModelNode) {
      .name = name,
      .transform.matrix = MAT4_IDENTITY,
      .primitiveIndex = i,
      .primitiveCount = 1,
      .skin = ~0u,
      .matrix = true
    };

    name = strchr(name, '\0') + 1;
  }

  for (uint32_t i = 0; i < modelCount; i++) {
    state.renderModels->FreeRenderModel(renderModels[i]);
    state.renderModels->FreeTexture(renderModelTextures[i]);
  }

  // Root node
  if (animated) {
    for (uint32_t i = 0; i < modelCount; i++) {
      model->children[i] = i;
    }

    model->rootNode = modelCount;

    model->nodes[model->rootNode] = (ModelNode) {
      .transform.matrix = MAT4_IDENTITY,
      .matrix = true,
      .childCount = modelCount,
      .children = model->children,
      .skin = ~0u
    };

    free(renderModels);
    free(renderModelTextures);
  }

  return model;
}

static bool openvr_animate(Device device, Model* model) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  TrackedDeviceIndex_t index = getDeviceIndex(device);
  if (index == INVALID_DEVICE) {
    return false;
  }

  char renderModelName[1024];
  ETrackedDeviceProperty renderModelNameProperty = ETrackedDeviceProperty_Prop_RenderModelName_String;
  state.system->GetStringTrackedDeviceProperty(index, renderModelNameProperty, renderModelName, 1024, NULL);

  bool success = false;
  ModelData* modelData = lovrModelGetModelData(model);
  VRInputValueHandle_t inputSource = state.inputSources[device];
  for (uint32_t i = 0; i < modelData->nodeCount; i++) {
    ModelNode* node = &modelData->nodes[i];

    if (!node->name || !state.renderModels->RenderModelHasComponent(renderModelName, (char*) node->name)) {
      continue;
    }

    RenderModel_ComponentState_t componentState;
    if (!state.renderModels->GetComponentStateForDevicePath(renderModelName, (char*) node->name, inputSource, NULL, &componentState)) {
      continue;
    }

    float transform[16], position[4], orientation[4];
    mat4_fromMat34(transform, componentState.mTrackingToComponentRenderModel.m);
    mat4_getPosition(transform, position);
    mat4_getOrientation(transform, orientation);
    if (!(componentState.uProperties & EVRComponentProperty_VRComponentProperty_IsVisible)) {
      vec3_set(position, 1e10, 1e10, 1e10); // Hide the node somehow (FIXME)
    }

    lovrModelPose(model, i, position, orientation, 1.f);
    success = true;
  }

  return success;
}

static void openvr_renderTo(void (*callback)(void*), void* userdata) {
  if (!state.canvas) {
    uint32_t width, height;
    openvr_getDisplayDimensions(&width, &height);
    width *= state.supersample;
    height *= state.supersample;
    CanvasFlags flags = { .depth = { true, false, FORMAT_D24S8 }, .stereo = true, .mipmaps = true, .msaa = state.msaa };
    state.canvas = lovrCanvasCreate(width, height, flags);
    Texture* texture = lovrTextureCreate(TEXTURE_2D, NULL, 0, true, true, state.msaa);
    lovrTextureAllocate(texture, width * 2, height, 1, FORMAT_RGBA);
    lovrTextureSetFilter(texture, lovrGraphicsGetDefaultFilter());
    lovrCanvasSetAttachments(state.canvas, &(Attachment) { texture, 0, 0 }, 1);
    lovrRelease(texture, lovrTextureDestroy);
    os_window_set_vsync(0);
  }

  float head[16];
  mat4_fromMat34(head, state.renderPoses[k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m);

  for (int i = 0; i < 2; i++) {
    float matrix[16], eye[16];
    EVREye vrEye = (i == 0) ? EVREye_Eye_Left : EVREye_Eye_Right;
    mat4_init(matrix, head);
    mat4_mul(matrix, mat4_fromMat34(eye, state.system->GetEyeToHeadTransform(vrEye).m));
    mat4_invert(matrix);
    lovrGraphicsSetViewMatrix(i, matrix);
    mat4_fromMat44(matrix, state.system->GetProjectionMatrix(vrEye, state.clipNear, state.clipFar).m);
    lovrGraphicsSetProjection(i, matrix);
  }

  lovrGraphicsSetBackbuffer(state.canvas, true, true);
  callback(userdata);
  lovrGraphicsSetBackbuffer(NULL, false, false);

  // Submit
  const Attachment* attachments = lovrCanvasGetAttachments(state.canvas, NULL);
  ptrdiff_t id = lovrTextureGetId(attachments[0].texture);
  Texture_t eyeTexture = { (void*) id, ETextureType_TextureType_OpenGL, EColorSpace_ColorSpace_Linear };
  VRTextureBounds_t left = { 0.f, 0.f, .5f, 1.f };
  VRTextureBounds_t right = { .5f, 0.f, 1.f, 1.f };
  state.compositor->Submit(EVREye_Eye_Left, &eyeTexture, &left, EVRSubmitFlags_Submit_Default);
  state.compositor->Submit(EVREye_Eye_Right, &eyeTexture, &right, EVRSubmitFlags_Submit_Default);
  lovrGpuDirtyTexture();
}

static Texture* openvr_getMirrorTexture(void) {
  return lovrCanvasGetAttachments(state.canvas, NULL)[0].texture;
}

static void openvr_update(float dt) {
  state.compositor->WaitGetPoses(state.renderPoses, sizeof(state.renderPoses) / sizeof(state.renderPoses[0]), NULL, 0);
  VRActiveActionSet_t activeActionSet = { .ulActionSet = state.actionSet };
  state.input->UpdateActionState(&activeActionSet, sizeof(activeActionSet), 1);

  struct VREvent_t vrEvent;
  while (state.system->PollNextEvent(&vrEvent, sizeof(vrEvent))) {
    switch (vrEvent.eventType) {
      case EVREventType_VREvent_InputFocusCaptured:
      case EVREventType_VREvent_InputFocusReleased: {
        bool isFocused = vrEvent.eventType == EVREventType_VREvent_InputFocusReleased;
        lovrEventPush((Event) { .type = EVENT_FOCUS, .data.boolean = { isFocused } });
        break;
      }

      case EVREventType_VREvent_Quit:
        lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit = { .exitCode = 0 } });
        break;

      default: break;
    }
  }
}

HeadsetInterface lovrHeadsetOpenVRDriver = {
  .driverType = DRIVER_OPENVR,
  .init = openvr_init,
  .destroy = openvr_destroy,
  .getName = openvr_getName,
  .getOriginType = openvr_getOriginType,
  .getDisplayDimensions = openvr_getDisplayDimensions,
  .getDisplayFrequency = openvr_getDisplayFrequency,
  .getDisplayMask = openvr_getDisplayMask,
  .getDisplayTime = openvr_getDisplayTime,
  .getViewCount = openvr_getViewCount,
  .getViewPose = openvr_getViewPose,
  .getViewAngles = openvr_getViewAngles,
  .getClipDistance = openvr_getClipDistance,
  .setClipDistance = openvr_setClipDistance,
  .getBoundsDimensions = openvr_getBoundsDimensions,
  .getBoundsGeometry = openvr_getBoundsGeometry,
  .getPose = openvr_getPose,
  .getVelocity = openvr_getVelocity,
  .isDown = openvr_isDown,
  .isTouched = openvr_isTouched,
  .getAxis = openvr_getAxis,
  .getSkeleton = openvr_getSkeleton,
  .vibrate = openvr_vibrate,
  .newModelData = openvr_newModelData,
  .animate = openvr_animate,
  .renderTo = openvr_renderTo,
  .getMirrorTexture = openvr_getMirrorTexture,
  .update = openvr_update
};
