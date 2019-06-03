#include "headset/headset.h"
#include "platform.h"
#include "util.h"
#include "core/maf.h"
#include "lib/tinycthread/tinycthread.h"
#include <LeapC.h>
#include <stdlib.h>

static struct {
  LEAP_CONNECTION connection;
  LEAP_CLOCK_REBASER clock;
  LEAP_TRACKING_EVENT* frame;
  LEAP_HAND* leftHand;
  LEAP_HAND* rightHand;
  uint64_t frameSize;
  float headPose[16];
  thrd_t thread;
  bool connected;
} state;

static int loop(void* userdata) {
  LEAP_CONNECTION_MESSAGE message;

  while (1) {
    if (LeapPollConnection(state.connection, 1000, &message) == eLeapRS_Success) {
      switch (message.type) {
        case eLeapEventType_Connection:
          state.connected = true;
          LeapSetPolicyFlags(state.connection, eLeapPolicyFlag_OptimizeHMD, 0);
          break;

        case eLeapEventType_ConnectionLost:
          state.connected = false;
          break;
      }
    }
  }

  return 0;
}

static void leap_destroy(void);
static bool leap_init(float offset, uint32_t msaa) {
  if (LeapCreateConnection(NULL, &state.connection) == eLeapRS_Success) {
    if (LeapOpenConnection(state.connection) == eLeapRS_Success) {
      LeapCreateClockRebaser(&state.clock);
      if (thrd_create(&state.thread, loop, NULL) == thrd_success) {
        return true;
      }
    }
  }

  return leap_destroy(), false;
}

static void leap_destroy(void) {
  free(state.frame);
  thrd_detach(state.thread);
  LeapDestroyClockRebaser(state.clock);
  LeapCloseConnection(state.connection);
  LeapDestroyConnection(state.connection);
  memset(&state, 0, sizeof(state));
}

static void adjustPose(vec3 position, vec3 direction) {
  float temp;

  // Convert units from mm to meters, apply a z offset (leap is in front of HMD), and swap y/z
  vec3_scale(position, -.001f);
  temp = position[1];
  position[1] = position[2];
  position[2] = temp;
  position[2] -= .080f;
  mat4_transform(state.headPose, position);

  // Just swap y/z
  vec3_normalize(direction);
  vec3_scale(direction, -1.f);
  temp = direction[1];
  direction[1] = direction[2];
  direction[2] = temp;
  mat4_transformDirection(state.headPose, direction);
}

static bool leap_getPose(Device device, vec3 position, quat orientation) {
  LEAP_HAND* hand;

  switch (device) {
    case DEVICE_HAND_LEFT: hand = state.leftHand; break;
    case DEVICE_HAND_RIGHT: hand = state.rightHand; break;
    default: return false;
  }

  if (!hand) {
    return false;
  }

  float direction[4];
  vec3_init(position, hand->palm.position.v);
  vec3_init(direction, hand->palm.normal.v);
  adjustPose(position, direction);
  quat_between(orientation, (float[4]) { 0.f, 0.f, -1.f }, direction);
  return true;
}

static bool leap_getBonePose(Device device, DeviceBone bone, vec3 position, quat orientation) {
  LEAP_HAND* hand;

  switch (device) {
    case DEVICE_HAND_LEFT: hand = state.leftHand; break;
    case DEVICE_HAND_RIGHT: hand = state.rightHand; break;
    default: return false;
  }

  if (!hand) {
    return false;
  }

  // Assumes that enum values for non-tip bones are grouped by finger, and start after BONE_PINKY,
  // could be less clever and use a switch if needed
  float direction[4];
  if (bone <= BONE_PINKY) {
    LEAP_DIGIT* finger = &hand->digits[bone];
    vec3 base = finger->distal.prev_joint.v;
    vec3 tip = finger->distal.next_joint.v;
    vec3_sub(vec3_init(direction, tip), base);
    vec3_init(position, tip);
  } else {
    bone -= BONE_PINKY + 1;
    LEAP_DIGIT* finger = &hand->digits[bone / 4];
    LEAP_BONE* leapBone = &finger->bones[bone % 4];
    vec3 base = leapBone->prev_joint.v;
    vec3 tip = leapBone->next_joint.v;
    vec3_sub(vec3_init(direction, tip), base);
    vec3_init(position, base);
  }

  adjustPose(position, direction);
  quat_between(orientation, (float[4]) { 0.f, 0.f, -1.f }, direction);
  return true;
}

static bool leap_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  LEAP_HAND* hand;

  switch (device) {
    case DEVICE_HAND_LEFT: hand = state.leftHand; break;
    case DEVICE_HAND_RIGHT: hand = state.rightHand; break;
    default: return false;
  }

  if (!hand) {
    return false;
  }

  vec3_set(velocity, hand->palm.velocity.x, hand->palm.velocity.z, hand->palm.velocity.y);
  vec3_scale(velocity, -.001f);
  mat4_transformDirection(state.headPose, velocity);
  vec3_set(angularVelocity, 0.f, 0.f, 0.f);
  return true;
}

static bool leap_getAcceleration(Device device, vec3 acceleration, vec3 angularAcceleration) {
  return false;
}

static bool leap_isDown(Device device, DeviceButton button, bool* down) {
  return false;
}

static bool leap_isTouched(Device device, DeviceButton button, bool* touched) {
  return false;
}

static bool leap_getAxis(Device device, DeviceAxis axis, float* value) {
  LEAP_HAND* hand;

  switch (device) {
    case DEVICE_HAND_LEFT: hand = state.leftHand; break;
    case DEVICE_HAND_RIGHT: hand = state.rightHand; break;
    default: return false;
  }

  if (!hand) {
    return false;
  }

  switch (axis) {
    case AXIS_PINCH: return *value = hand->pinch_strength, true;
    case AXIS_GRIP: return *value = hand->grab_strength, true;
    default: return false;
  }
}

static bool leap_vibrate(Device device, float strength, float duration, float frequency) {
  return false;
}

static struct ModelData* leap_newModelData(Device device) {
  return NULL;
}

static void leap_update(float dt) {
  if (!state.connected) {
    return;
  }

  double displayTime = lovrHeadsetDriver ? lovrHeadsetDriver->getDisplayTime() : 0.;
  int64_t now = (int64_t) ((lovrPlatformGetTime() * (double) 1e6) + .5);
  int64_t predicted = (int64_t) ((displayTime * (double) 1e6) + .5);
  LeapUpdateRebase(state.clock, now, LeapGetNow());

  int64_t targetTime;
  LeapRebaseClock(state.clock, predicted, &targetTime);

  uint64_t size;
  if (LeapGetFrameSize(state.connection, targetTime, &size) == eLeapRS_Success) {
    if (state.frameSize < size) {
      lovrAssert(size <= SIZE_MAX, "Out of memory");
      state.frameSize = size;
      state.frame = realloc(state.frame, (size_t) state.frameSize);
      lovrAssert(state.frame, "Out of memory");
    }

    LeapInterpolateFrame(state.connection, targetTime, state.frame, size);

    state.leftHand = state.rightHand = NULL;
    for (uint32_t i = 0; i < state.frame->nHands; i++) {
      if (!state.leftHand && state.frame->pHands[i].type == eLeapHandType_Left) {
        state.leftHand = &state.frame->pHands[i];
      } else if (!state.rightHand && state.frame->pHands[i].type == eLeapHandType_Right) {
        state.rightHand = &state.frame->pHands[i];
      }
    }

    float position[4], orientation[4];
    if (lovrHeadsetDriver && lovrHeadsetDriver->getPose(DEVICE_HEAD, position, orientation)) {
      mat4 m = state.headPose;
      mat4_identity(m);
      mat4_translate(m, position[0], position[1], position[2]);
      mat4_rotateQuat(m, orientation);
    }
  }
}

HeadsetInterface lovrHeadsetLeapMotionDriver = {
  .driverType = DRIVER_LEAP_MOTION,
  .init = leap_init,
  .destroy = leap_destroy,
  .getPose = leap_getPose,
  .getBonePose = leap_getBonePose,
  .getVelocity = leap_getVelocity,
  .getAcceleration = leap_getAcceleration,
  .isDown = leap_isDown,
  .isTouched = leap_isTouched,
  .getAxis = leap_getAxis,
  .vibrate = leap_vibrate,
  .newModelData = leap_newModelData,
  .update = leap_update
};
