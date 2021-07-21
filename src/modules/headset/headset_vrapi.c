#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/maf.h"
#include "core/os.h"
#include "core/util.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <android_native_app_glue.h>
#include <VrApi.h>
#include <VrApi_Helpers.h>
#include <VrApi_Input.h>
#include <VrApi_Vulkan.h>

#define VK_FORMAT_R8G8B8A8_UNORM 37

// Private platform functions
JNIEnv* os_get_jni(void);
struct ANativeActivity* os_get_activity(void);
int os_get_activity_state(void);
ANativeWindow* os_get_native_window(void);
uintptr_t gpu_vk_get_instance(void);
uintptr_t gpu_vk_get_physical_device(void);
uintptr_t gpu_vk_get_device(void);
uintptr_t gpu_vk_get_queue(void);

static struct {
  ovrJava java;
  ovrMobile* session;
  ovrDeviceType deviceType;
  uint64_t frameIndex;
  double displayTime;
  float supersample;
  float offset;
  uint32_t msaa;
  ovrVector3f* rawBoundaryPoints;
  float* boundaryPoints;
  uint32_t boundaryPointCount;
  ovrTextureSwapChain* swapchain;
  uint32_t swapchainLength;
  uint32_t swapchainIndex;
  Canvas* canvas;
  Texture* textures[4];
  ovrTracking tracking[3];
  ovrHandPose handPose[2];
  ovrHandSkeleton skeleton[2];
  ovrInputCapabilityHeader hands[2];
  ovrInputStateTrackedRemote input[2];
  ovrInputStateHand handInput[2];
  uint32_t changedButtons[2];
  float hapticStrength[2];
  float hapticDuration[2];
} state;

static bool vrapi_getVulkanInstanceExtensions(char* buffer, uint32_t size) {
  return vrapi_GetInstanceExtensionsVulkan(buffer, &size) == ovrSuccess;
}

static bool vrapi_getVulkanDeviceExtensions(char* buffer, uint32_t size, uintptr_t physicalDevice) {
  return vrapi_GetDeviceExtensionsVulkan(buffer, &size) == ovrSuccess;
}

static bool vrapi_init(float supersample, float offset, uint32_t msaa, bool overlay) {
  ANativeActivity* activity = os_get_activity();
  JNIEnv* jni = os_get_jni();
  state.java.Vm = activity->vm;
  state.java.ActivityObject = activity->clazz;
  state.java.Env = jni;
  state.supersample = supersample;
  state.offset = offset;
  state.msaa = msaa;
  ovrInitParms config = vrapi_DefaultInitParms(&state.java);
  config.GraphicsAPI = VRAPI_GRAPHICS_API_VULKAN_1;
  if (vrapi_Initialize(&config) != VRAPI_INITIALIZE_SUCCESS) {
    return false;
  }
  state.deviceType = vrapi_GetSystemPropertyInt(&state.java, VRAPI_SYS_PROP_DEVICE_TYPE);
  return true;
}

static void vrapi_start(void) {
  ovrSystemCreateInfoVulkan vulkanInfo = {
    .Instance = (VkInstance) gpu_vk_get_instance(),
    .PhysicalDevice = (VkPhysicalDevice) gpu_vk_get_physical_device(),
    .Device = (VkDevice) gpu_vk_get_device()
  };
  lovrAssert(vrapi_CreateSystemVulkan(&vulkanInfo) == ovrSuccess, "Headset failed to initialize vulkan");

  uint32_t width = (uint32_t) vrapi_GetSystemPropertyInt(&state.java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH) * state.supersample;
  uint32_t height = (uint32_t) vrapi_GetSystemPropertyInt(&state.java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT) * state.supersample;

  state.swapchain = vrapi_CreateTextureSwapChain4(&(ovrSwapChainCreateInfo) {
    .Format = VK_FORMAT_R8G8B8A8_UNORM,
    .Width = width,
    .Height = height,
    .Levels = 1,
    .FaceCount = 1,
    .ArraySize = 2,
    .BufferCount = 3,
    .UsageFlags = VRAPI_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
  });
  lovrAssert(state.swapchain, "Failed to create VrApi swapchain");

  state.swapchainLength = vrapi_GetTextureSwapChainLength(state.swapchain);
  lovrAssert(state.swapchainLength <= COUNTOF(state.textures), "Wow, the VrApi swapchain is too long");

  for (uint32_t i = 0; i < state.swapchainLength; i++) {
    state.textures[i] = lovrTextureCreate(&(TextureInfo) {
      .type = TEXTURE_ARRAY,
      .format = FORMAT_RGBA8,
      .width = width,
      .height = height,
      .depth = 2,
      .mipmaps = 1,
      .samples = 1,
      .flags = TEXTURE_RENDER,
      .handle = (uintptr_t) vrapi_GetTextureSwapChainBufferVulkan(state.swapchain, i)
    });
  }

  state.canvas = lovrCanvasCreate(&(CanvasInfo) {
    .color[0].texture = state.textures[0],
    .color[0].load = LOAD_CLEAR,
    .color[0].save = SAVE_KEEP,
    .depth.enabled = true,
    .depth.format = FORMAT_D24S8,
    .depth.load = LOAD_CLEAR,
    .depth.stencilLoad = LOAD_CLEAR,
    .depth.save = SAVE_DISCARD,
    .depth.stencilSave = SAVE_DISCARD,
    .samples = 1,
    .count = 1
  });

  lovrCanvasSetClear(state.canvas, &(float[4]) { 0.f, 0.f, 0.f, 1.f }, 1.f, 0);
}

static void vrapi_destroy() {
  if (state.session) {
    vrapi_LeaveVrMode(state.session);
  }
  vrapi_DestroyTextureSwapChain(state.swapchain);
  vrapi_DestroySystemVulkan();
  vrapi_Shutdown();
  for (size_t i = 0; i < COUNTOF(state.textures); i++) {
    lovrRelease(state.textures[i], lovrTextureDestroy);
  }
  lovrRelease(state.canvas, lovrCanvasDestroy);
  free(state.rawBoundaryPoints);
  free(state.boundaryPoints);
  memset(&state, 0, sizeof(state));
}

static bool vrapi_getName(char* buffer, size_t length) {
  switch ((int) state.deviceType) {
    case VRAPI_DEVICE_TYPE_OCULUSQUEST: strncpy(buffer, "Oculus Quest", length - 1); break;
    case VRAPI_DEVICE_TYPE_OCULUSQUEST2: strncpy(buffer, "Oculus Quest 2", length - 1); break;
    default: return false;
  }
  buffer[length - 1] = '\0';
  return true;
}

static HeadsetOrigin vrapi_getOriginType(void) {
  return vrapi_GetTrackingSpace(state.session) == VRAPI_TRACKING_SPACE_LOCAL_FLOOR ? ORIGIN_FLOOR : ORIGIN_HEAD;
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
  mat4_transpose(transform);
  mat4_invert(transform);
  mat4_getPosition(transform, position);
  mat4_getOrientation(transform, orientation);
  return tracking.Status & VRAPI_TRACKING_STATUS_ORIENTATION_VALID;
}

static bool vrapi_getViewAngles(uint32_t view, float* left, float* right, float* up, float* down) {
  if (view >= 2) return false;
  float projection[16];
  ovrTracking2 tracking = vrapi_GetPredictedTracking2(state.session, state.displayTime);
  mat4_init(projection, (float*) &tracking.Eye[view].ProjectionMatrix);
  mat4_transpose(projection);
  mat4_getFov(projection, left, right, up, down);
  return tracking.Status & VRAPI_TRACKING_STATUS_ORIENTATION_VALID;
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

static bool vrapi_getPose(Device device, float* position, float* orientation) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT && device != DEVICE_HEAD) {
    return false;
  }

  ovrPosef* pose;
  bool valid;

  uint32_t index = device - DEVICE_HAND_LEFT;
  if (index < 2 && state.hands[index].Type == ovrControllerType_Hand) {
    pose = &state.handPose[index].RootPose;
    valid = state.handPose[index].HandConfidence == ovrConfidence_HIGH;
  } else {
    ovrTracking* tracking = &state.tracking[device];
    pose = &tracking->HeadPose.Pose;
    valid = tracking->Status & (VRAPI_TRACKING_STATUS_POSITION_VALID | VRAPI_TRACKING_STATUS_ORIENTATION_VALID);
  }

  vec3_set(position, pose->Position.x, pose->Position.y + state.offset, pose->Position.z);
  quat_init(orientation, &pose->Orientation.x);

  // Make tracked hands face -Z
  if (index < 2 && state.hands[index].Type == ovrControllerType_Hand) {
    float rotation[4];
    if (device == DEVICE_HAND_LEFT) {
      float q[4];
      quat_fromAngleAxis(rotation, (float) M_PI, 0.f, 0.f, 1.f);
      quat_mul(rotation, rotation, quat_fromAngleAxis(q, (float) M_PI / 2.f, 0.f, 1.f, 0.f));
    } else if (device == DEVICE_HAND_RIGHT) {
      quat_fromAngleAxis(rotation, (float) M_PI / 2.f, 0.f, 1.f, 0.f);
    }
    quat_mul(orientation, orientation, rotation);
  }

  return valid;
}

static bool vrapi_getVelocity(Device device, float* velocity, float* angularVelocity) {
  if (device != DEVICE_HEAD && device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  ovrTracking* tracking = &state.tracking[device];
  ovrVector3f* linear = &tracking->HeadPose.LinearVelocity;
  ovrVector3f* angular = &tracking->HeadPose.AngularVelocity;
  vec3_set(velocity, linear->x, linear->y, linear->z);
  vec3_set(angularVelocity, angular->x, angular->y, angular->z);
  return tracking->Status & (VRAPI_TRACKING_STATUS_POSITION_VALID | VRAPI_TRACKING_STATUS_ORIENTATION_VALID);
}

static bool vrapi_isDown(Device device, DeviceButton button, bool* down, bool* changed) {
  if (device == DEVICE_HEAD && button == BUTTON_PROXIMITY) {
    *down = vrapi_GetSystemStatusInt(&state.java, VRAPI_SYS_STATUS_MOUNTED);
    return true;
  }

  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  if (state.hands[device - DEVICE_HAND_LEFT].Type != ovrControllerType_TrackedRemote) {
    return false;
  }

  uint32_t mask;
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

  uint32_t index = device - DEVICE_HAND_LEFT;
  *down = state.input[index].Buttons & mask;
  *changed = state.changedButtons[index] & mask;
  return true;
}

static bool vrapi_isTouched(Device device, DeviceButton button, bool* touched) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  if (state.hands[device - DEVICE_HAND_LEFT].Type != ovrControllerType_TrackedRemote) {
    return false;
  }

  ovrInputStateTrackedRemote* input = &state.input[device - DEVICE_HAND_LEFT];

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

static bool vrapi_getAxis(Device device, DeviceAxis axis, float* value) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  uint32_t index = device - DEVICE_HAND_LEFT;

  if (state.hands[index].Type == ovrControllerType_Hand && axis == AXIS_TRIGGER) {
    value[0] = state.handInput[index].PinchStrength[ovrHandPinchStrength_Index];
    return state.handPose[index].HandConfidence == ovrConfidence_HIGH;
  }

  ovrInputStateTrackedRemote* input = &state.input[index];

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

static bool vrapi_getSkeleton(Device device, float* poses) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  uint32_t index = device - DEVICE_HAND_LEFT;
  ovrHandPose* handPose = &state.handPose[index];
  ovrHandSkeleton* skeleton = &state.skeleton[index];
  if (state.hands[index].Type != ovrControllerType_Hand || skeleton->Header.Version == 0 || handPose->HandConfidence != ovrConfidence_HIGH) {
    return false;
  }

  float globalPoses[ovrHandBone_Max * 8];
  for (uint32_t i = 0; i < ovrHandBone_Max; i++) {
    float* pose = &globalPoses[i * 8];

    if (skeleton->BoneParentIndices[i] >= 0) {
      memcpy(pose, &globalPoses[skeleton->BoneParentIndices[i] * 8], 8 * sizeof(float));
    } else {
      memcpy(pose + 0, &handPose->RootPose.Position.x, 3 * sizeof(float));
      memcpy(pose + 4, &handPose->RootPose.Orientation.x, 4 * sizeof(float));
    }

    float translation[4];
    memcpy(translation, &skeleton->BonePoses[i].Position.x, 3 * sizeof(float));
    quat_rotate(pose + 4, translation);
    vec3_add(pose + 0, translation);
    quat_mul(pose + 4, pose + 4, &handPose->BoneRotations[i].x);
  }

  // We try our best, okay?
  static const uint32_t boneMap[HAND_JOINT_COUNT] = {
    [JOINT_WRIST] = ovrHandBone_WristRoot,
    [JOINT_THUMB_METACARPAL] = ovrHandBone_Thumb0,
    [JOINT_THUMB_PROXIMAL] = ovrHandBone_Thumb2,
    [JOINT_THUMB_DISTAL] = ovrHandBone_Thumb3,
    [JOINT_THUMB_TIP] = ovrHandBone_ThumbTip,
    [JOINT_INDEX_METACARPAL] = ovrHandBone_WristRoot,
    [JOINT_INDEX_PROXIMAL] = ovrHandBone_Index1,
    [JOINT_INDEX_INTERMEDIATE] = ovrHandBone_Index2,
    [JOINT_INDEX_DISTAL] = ovrHandBone_Index3,
    [JOINT_INDEX_TIP] = ovrHandBone_IndexTip,
    [JOINT_MIDDLE_METACARPAL] = ovrHandBone_WristRoot,
    [JOINT_MIDDLE_PROXIMAL] = ovrHandBone_Middle1,
    [JOINT_MIDDLE_INTERMEDIATE] = ovrHandBone_Middle2,
    [JOINT_MIDDLE_DISTAL] = ovrHandBone_Middle3,
    [JOINT_MIDDLE_TIP] = ovrHandBone_MiddleTip,
    [JOINT_RING_METACARPAL] = ovrHandBone_WristRoot,
    [JOINT_RING_PROXIMAL] = ovrHandBone_Ring1,
    [JOINT_RING_INTERMEDIATE] = ovrHandBone_Ring2,
    [JOINT_RING_DISTAL] = ovrHandBone_Ring3,
    [JOINT_RING_TIP] = ovrHandBone_RingTip,
    [JOINT_PINKY_METACARPAL] = ovrHandBone_Pinky0,
    [JOINT_PINKY_PROXIMAL] = ovrHandBone_Pinky1,
    [JOINT_PINKY_INTERMEDIATE] = ovrHandBone_Pinky2,
    [JOINT_PINKY_DISTAL] = ovrHandBone_Pinky3,
    [JOINT_PINKY_TIP] = ovrHandBone_PinkyTip
  };

  for (uint32_t i = 1; i < HAND_JOINT_COUNT; i++) {
    memcpy(&poses[i * 8], &globalPoses[boneMap[i] * 8], 8 * sizeof(float));
  }

  memcpy(poses + 0, &handPose->RootPose.Position.x, 3 * sizeof(float));
  memcpy(poses + 4, &handPose->RootPose.Orientation.x, 4 * sizeof(float));

  float rotation[4];
  if (index == 0) {
    float q[4];
    quat_fromAngleAxis(rotation, (float) M_PI, 0.f, 0.f, 1.f);
    quat_mul(rotation, rotation, quat_fromAngleAxis(q, (float) M_PI / 2.f, 0.f, 1.f, 0.f));
  } else {
    quat_fromAngleAxis(rotation, (float) M_PI / 2.f, 0.f, 1.f, 0.f);
  }

  for (uint32_t i = 0; i < HAND_JOINT_COUNT; i++) {
    float* pose = &poses[i * 8];
    quat_mul(pose + 4, pose + 4, rotation);
  }

  return true;
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

static struct ModelData* vrapi_newModelData(Device device, bool animated) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return NULL;
  }

  if (state.hands[device - DEVICE_HAND_LEFT].Type != ovrControllerType_Hand) {
    return NULL;
  }

  if (state.skeleton[device - DEVICE_HAND_LEFT].Header.Version == 0) {
    return NULL;
  }

  return NULL;

  /*
  ovrHandSkeleton* skeleton = &state.skeleton[device - DEVICE_HAND_LEFT];

  ovrHandMesh* mesh = malloc(sizeof(ovrHandMesh));
  float* inverseBindMatrices = malloc(ovrHandBone_MaxSkinnable * 16 * sizeof(float));
  lovrAssert(mesh && inverseBindMatrices, "Out of memory");

  mesh->Header.Version = ovrHandVersion_1;
  ovrHandedness hand = device == DEVICE_HAND_LEFT ? VRAPI_HAND_LEFT : VRAPI_HAND_RIGHT;
  if (vrapi_GetHandMesh(state.session, hand, &mesh->Header) != ovrSuccess) {
    free(mesh);
    return NULL;
  }

  ModelData* model = calloc(1, sizeof(ModelData));
  lovrAssert(model, "Out of memory");
  model->ref = 1;
  model->blobCount = 2;
  model->bufferCount = 6;
  model->attributeCount = 6;
  model->primitiveCount = 1;
  model->skinCount = 1;
  model->jointCount = ovrHandBone_MaxSkinnable;
  model->childCount = ovrHandBone_MaxSkinnable + 1;
  model->nodeCount = 2 + model->jointCount;
  lovrModelDataAllocate(model);

  model->blobs[0] = lovrBlobCreate(mesh, sizeof(ovrHandMesh), "Oculus Hand Mesh");
  model->blobs[1] = lovrBlobCreate(inverseBindMatrices, model->jointCount * 16 * sizeof(float), "Hand Mesh Inverse Bind Matrices");

  model->buffers[0] = (ModelBuffer) {
    .blob = 0,
    .offset = (char*) mesh->VertexPositions - (char*) mesh,
    .data = (char*) mesh->VertexPositions,
    .size = sizeof(mesh->VertexPositions),
    .stride = sizeof(mesh->VertexPositions[0])
  };

  model->buffers[1] = (ModelBuffer) {
    .blob = 0,
    .offset = (char*) mesh->VertexNormals - (char*) mesh,
    .data = (char*) mesh->VertexNormals,
    .size = sizeof(mesh->VertexNormals),
    .stride = sizeof(mesh->VertexNormals[0]),
  };

  model->buffers[2] = (ModelBuffer) {
    .blob = 0,
    .offset = (char*) mesh->VertexUV0 - (char*) mesh,
    .data = (char*) mesh->VertexUV0,
    .size = sizeof(mesh->VertexUV0),
    .stride = sizeof(mesh->VertexUV0[0]),
  };

  model->buffers[3] = (ModelBuffer) {
    .blob = 0,
    .offset = (char*) mesh->BlendIndices - (char*) mesh,
    .data = (char*) mesh->BlendIndices,
    .size = sizeof(mesh->BlendIndices),
    .stride = sizeof(mesh->BlendIndices[0]),
  };

  model->buffers[4] = (ModelBuffer) {
    .blob = 0,
    .offset = (char*) mesh->BlendWeights - (char*) mesh,
    .data = (char*) mesh->BlendWeights,
    .size = sizeof(mesh->BlendWeights),
    .stride = sizeof(mesh->BlendWeights[0]),
  };

  model->buffers[5] = (ModelBuffer) {
    .blob = 0,
    .offset = (char*) mesh->Indices - (char*) mesh,
    .data = (char*) mesh->Indices,
    .size = sizeof(mesh->Indices),
    .stride = sizeof(mesh->Indices[0])
  };

  model->attributes[0] = (ModelAttribute) { .buffer = 0, .type = F32, .components = 3 };
  model->attributes[1] = (ModelAttribute) { .buffer = 1, .type = F32, .components = 3 };
  model->attributes[2] = (ModelAttribute) { .buffer = 2, .type = F32, .components = 2 };
  model->attributes[3] = (ModelAttribute) { .buffer = 3, .type = I16, .components = 4 };
  model->attributes[4] = (ModelAttribute) { .buffer = 4, .type = F32, .components = 4 };
  model->attributes[5] = (ModelAttribute) { .buffer = 5, .type = U16, .count = mesh->NumIndices };

  model->primitives[0] = (ModelPrimitive) {
    .mode = DRAW_TRIANGLES,
    .attributes = {
      [ATTR_POSITION] = &model->attributes[0],
      [ATTR_NORMAL] = &model->attributes[1],
      [ATTR_TEXCOORD] = &model->attributes[2],
      [ATTR_BONES] = &model->attributes[3],
      [ATTR_WEIGHTS] = &model->attributes[4]
    },
    .indices = &model->attributes[5],
    .material = ~0u
  };

  // The nodes in the Model correspond directly to the joints in the skin, for convenience
  ovrMatrix4f globalTransforms[ovrHandBone_MaxSkinnable];
  uint32_t* children = model->children;
  model->skins[0].joints = model->joints;
  model->skins[0].jointCount = model->jointCount;
  model->skins[0].inverseBindMatrices = inverseBindMatrices;
  for (uint32_t i = 0; i < model->jointCount; i++) {
    ovrVector3f* position = &skeleton->BonePoses[i].Position;
    ovrQuatf* orientation = &skeleton->BonePoses[i].Orientation;

    model->nodes[i] = (ModelNode) {
      .transform.properties.translation = { position->x, position->y, position->z },
      .transform.properties.rotation = { orientation->x, orientation->y, orientation->z, orientation->w },
      .transform.properties.scale = { 1.f, 1.f, 1.f },
      .skin = ~0u
    };

    model->joints[i] = i;

    // Turn the bone's pose into a matrix
    ovrMatrix4f translation = ovrMatrix4f_CreateTranslation(position->x, position->y, position->z);
    ovrMatrix4f rotation = ovrMatrix4f_CreateFromQuaternion(orientation);
    ovrMatrix4f localPose = ovrMatrix4f_Multiply(&translation, &rotation);

    // Get the global transform of the bone by multiplying by the parent's global transform
    // This relies on the bones being ordered in hierarchical order
    ovrMatrix4f parentTransform = ovrMatrix4f_CreateIdentity();
    if (skeleton->BoneParentIndices[i] >= 0) {
      parentTransform = globalTransforms[skeleton->BoneParentIndices[i]];
    }
    globalTransforms[i] = ovrMatrix4f_Multiply(&parentTransform, &localPose);

    // The inverse of the global transform is the bone's inverse bind pose
    // It needs to be transposed because Oculus matrices are row-major
    ovrMatrix4f inverseBindPose = ovrMatrix4f_Inverse(&globalTransforms[i]);
    ovrMatrix4f inverseBindPoseTransposed = ovrMatrix4f_Transpose(&inverseBindPose);
    memcpy(model->skins[0].inverseBindMatrices + 16 * i, &inverseBindPoseTransposed.M[0][0], 16 * sizeof(float));

    // Add child bones by looking for any bones that have a parent of the current bone.
    // This is somewhat slow, we use the fact that bones are sorted to reduce the work a bit.
    model->nodes[i].childCount = 0;
    model->nodes[i].children = children;
    for (uint32_t j = i + 1; j < model->jointCount && j < skeleton->NumBones; j++) {
      if ((uint32_t) skeleton->BoneParentIndices[j] == i) {
        model->nodes[i].children[model->nodes[i].childCount++] = j;
        children++;
      }
    }
  }

  // Add a node that holds the skinned mesh
  model->nodes[model->jointCount] = (ModelNode) {
    .transform.properties.scale = { 1.f, 1.f, 1.f },
    .primitiveIndex = 0,
    .primitiveCount = 1,
    .skin = 0
  };

  // The root node has the mesh node and root joint as children
  model->rootNode = model->jointCount + 1;
  model->nodes[model->rootNode] = (ModelNode) {
    .matrix = true,
    .transform = { MAT4_IDENTITY },
    .childCount = 2,
    .children = children,
    .skin = ~0u
  };

  // Root node transform has a rotation compensation for different coordinate spaces
  if (device == DEVICE_HAND_LEFT) {
    mat4_rotate(model->nodes[model->rootNode].transform.matrix, M_PI, 0.f, 0.f, 1.f);
    mat4_rotate(model->nodes[model->rootNode].transform.matrix, M_PI / 2.f, 0.f, 1.f, 0.f);
  } else {
    mat4_rotate(model->nodes[model->rootNode].transform.matrix, -M_PI / 2.f, 0.f, 1.f, 0.f);
  }

  // Add the children to the root node
  *children++ = 0;
  *children++ = model->jointCount;

  return model;
  */
}

static bool vrapi_animate(Device device, struct Model* model) {
  if (device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) {
    return false;
  }

  /*
  ovrInputCapabilityHeader* header = &state.hands[device - DEVICE_HAND_LEFT];
  ovrHandPose* handPose = &state.handPose[device - DEVICE_HAND_LEFT];
  if (header->Type != ovrControllerType_Hand || handPose->HandConfidence != ovrConfidence_HIGH) {
    return false;
  }

  ModelData* modelData = lovrModelGetModelData(model);
  if (modelData->nodeCount >= modelData->jointCount) {
    float scale = handPose->HandScale;
    vec3_set(modelData->nodes[modelData->jointCount].transform.properties.scale, scale, scale, scale);
  }

  lovrModelResetPose(model);

  // Replace node rotations with the rotations in the hand pose, keeping the position the same
  for (uint32_t i = 0; i < ovrHandBone_MaxSkinnable && i < modelData->nodeCount; i++) {
    float position[4], orientation[4];
    vec3_init(position, modelData->nodes[i].transform.properties.translation);
    quat_init(orientation, &handPose->BoneRotations[i].x);
    lovrModelPose(model, i, position, orientation, 1.f);
  }
  */

  return true;
}

static void vrapi_renderTo(Batch* batch) {
  if (!state.session) return;

  ovrTracking2 tracking = vrapi_GetPredictedTracking2(state.session, state.displayTime);

  // Camera
  for (uint32_t i = 0; i < 2; i++) {
    float view[16];
    mat4_init(view, &tracking.Eye[i].ViewMatrix.M[0][0]);
    mat4_transpose(view);
    view[13] -= state.offset;
    //lovrCanvasSetViewMatrix(state.canvas, i, view);

    float projection[16];
    mat4_init(projection, &tracking.Eye[i].ProjectionMatrix.M[0][0]);
    mat4_transpose(projection);
    //lovrCanvasSetProjection(state.canvas, i, projection);
  }

  // Render
  lovrCanvasSetTextures(state.canvas, (Texture*[1]) { state.textures[state.swapchainIndex] }, NULL);
  lovrGraphicsBegin();
  lovrGraphicsRender(state.canvas, &batch, 1);
  lovrGraphicsSubmit();

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
  int appState = os_get_activity_state();
  ANativeWindow* window = os_get_native_window();

  // Session
  if (!state.session && appState == APP_CMD_RESUME && window) {
    ovrModeParmsVulkan config = vrapi_DefaultModeParmsVulkan(&state.java, (unsigned long long) gpu_vk_get_queue());
    config.ModeParms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;
    config.ModeParms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
    config.ModeParms.Flags |= VRAPI_MODE_FLAG_FRONT_BUFFER_SRGB;
    config.ModeParms.WindowSurface = (size_t) window;
    state.session = vrapi_EnterVrMode(&config.ModeParms);
    state.frameIndex = 0;
    vrapi_SetTrackingSpace(state.session, VRAPI_TRACKING_SPACE_LOCAL_FLOOR);
    state.offset = 0.f;
  } else if (state.session && (appState != APP_CMD_RESUME || !window)) {
    vrapi_LeaveVrMode(state.session);
    state.session = NULL;
  }

  if (!state.session) return;

  // Events
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

  // Tracking
  state.frameIndex++;
  state.displayTime = vrapi_GetPredictedDisplayTime(state.session, state.frameIndex);
  state.tracking[DEVICE_HEAD] = vrapi_GetPredictedTracking(state.session, state.displayTime);

  // Sort out the controller devices
  ovrInputCapabilityHeader header;
  state.hands[0].Type = ovrControllerType_None;
  state.hands[1].Type = ovrControllerType_None;
  for (uint32_t i = 0; vrapi_EnumerateInputDevices(state.session, i, &header) == ovrSuccess; i++) {
    if (header.Type == ovrControllerType_TrackedRemote) {
      ovrInputTrackedRemoteCapabilities info = { .Header = header };
      vrapi_GetInputDeviceCapabilities(state.session, &info.Header);
      state.hands[(info.ControllerCapabilities & ovrControllerCaps_LeftHand) ? 0 : 1] = header;
    } else if (header.Type == ovrControllerType_Hand) {
      ovrInputHandCapabilities info = { .Header = header };
      vrapi_GetInputDeviceCapabilities(state.session, &info.Header);
      state.hands[(info.HandCapabilities & ovrHandCaps_LeftHand) ? 0 : 1] = header;
    }
  }

  // Update controllers
  for (uint32_t i = 0; i < 2; i++) {
    Device device = DEVICE_HAND_LEFT + i;
    ovrInputCapabilityHeader* header = &state.hands[i];
    vrapi_GetInputTrackingState(state.session, header->DeviceID, state.displayTime, &state.tracking[device]);

    switch (state.hands[i].Type) {
      case ovrControllerType_TrackedRemote: {
        uint32_t lastButtons = state.input[i].Buttons;
        state.input[i].Header.ControllerType = header->Type;
        vrapi_GetCurrentInputState(state.session, header->DeviceID, &state.input[i].Header);
        state.changedButtons[i] = state.input[i].Buttons ^ lastButtons;

        // Haptics
        state.hapticDuration[i] -= dt;
        float strength = state.hapticDuration[i] > 0.f ? state.hapticStrength[i] : 0.f;
        vrapi_SetHapticVibrationSimple(state.session, header->DeviceID, strength);
        break;
      }

      case ovrControllerType_Hand:
        // Cache hand skeleton
        if (state.skeleton[i].Header.Version == 0) {
          state.skeleton[i].Header.Version = ovrHandVersion_1;
          ovrHandedness hand = i == 0 ? VRAPI_HAND_LEFT : VRAPI_HAND_RIGHT;
          if (vrapi_GetHandSkeleton(state.session, hand, &state.skeleton[i].Header) != ovrSuccess) {
            state.skeleton[i].Header.Version = 0;
          }
        }

        state.handPose[i].Header.Version = ovrHandVersion_1;
        vrapi_GetHandPose(state.session, header->DeviceID, state.displayTime, &state.handPose[i].Header);

        state.handInput[i].Header.ControllerType = header->Type;
        vrapi_GetCurrentInputState(state.session, header->DeviceID, &state.handInput[i].Header);
        break;

      default: break;
    }
  }
}

HeadsetInterface lovrHeadsetVrApiDriver = {
  .driverType = DRIVER_VRAPI,
  .getVulkanInstanceExtensions = vrapi_getVulkanInstanceExtensions,
  .getVulkanDeviceExtensions = vrapi_getVulkanDeviceExtensions,
  .init = vrapi_init,
  .start = vrapi_start,
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
  .getSkeleton = vrapi_getSkeleton,
  .vibrate = vrapi_vibrate,
  .newModelData = vrapi_newModelData,
  .animate = vrapi_animate,
  .renderTo = vrapi_renderTo,
  .update = vrapi_update
};
