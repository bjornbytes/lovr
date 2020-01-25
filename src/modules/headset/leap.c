#include "headset/headset.h"
#include "core/maf.h"
#include "core/os.h"
#include "core/util.h"
#include "lib/tinycthread/tinycthread.h"
#include <LeapC.h>
#include <stdlib.h>

static struct {
  LEAP_CONNECTION connection;
  LEAP_CLOCK_REBASER clock;
  LEAP_TRACKING_EVENT* frame;
  LEAP_HAND* hands[2];
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

  leap_destroy();
  return false;
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
  if (device == DEVICE_HAND_LEFT || device == DEVICE_HAND_RIGHT) {
    LEAP_HAND* hand = state.hands[device - DEVICE_HAND_LEFT];

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

  LEAP_BONE* distal;
  if (state.hands[0] && device >= DEVICE_HAND_LEFT_FINGER_THUMB && device <= DEVICE_HAND_LEFT_FINGER_PINKY) {
    distal = &state.hands[0]->digits[device - DEVICE_HAND_LEFT_FINGER_THUMB].distal;
  } else if (state.hands[1] && device >= DEVICE_HAND_RIGHT_FINGER_THUMB && device <= DEVICE_HAND_RIGHT_FINGER_PINKY) {
    distal = &state.hands[1]->digits[device - DEVICE_HAND_RIGHT_FINGER_THUMB].distal;
  } else {
    return false;
  }

  float direction[4];
  vec3_init(position, distal->next_joint.v);
  vec3_init(direction, distal->next_joint.v);
  vec3_sub(direction, distal->prev_joint.v);
  adjustPose(position, direction);
  quat_between(orientation, (float[4]) { 0.f, 0.f, -1.f }, direction);
  return true;
}

static bool leap_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  if ((device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) || !state.hands[device - DEVICE_HAND_LEFT]) {
    return false;
  }

  LEAP_HAND* hand = state.hands[device - DEVICE_HAND_LEFT];
  vec3_set(velocity, hand->palm.velocity.x, hand->palm.velocity.z, hand->palm.velocity.y); // Swap z and y
  vec3_scale(velocity, -.001f);
  mat4_transformDirection(state.headPose, velocity);
  vec3_set(angularVelocity, 0.f, 0.f, 0.f);
  return true;
}

static bool leap_isDown(Device device, DeviceButton button, bool* down, bool* changed) {
  if ((device != DEVICE_HAND_LEFT && device != DEVICE_HAND_RIGHT) || !state.hands[device - DEVICE_HAND_LEFT]) {
    return false;
  }

  LEAP_HAND* hand = state.hands[device - DEVICE_HAND_LEFT];

  *changed = false; // TODO

  switch (button) {
    case BUTTON_TRIGGER: *down = hand->pinch_strength > .5f; return true;
    case BUTTON_GRIP: *down = hand->grab_strength > .5f; return true;
    default: return false;
  }
}

static bool leap_isTouched(Device device, DeviceButton button, bool* touched) {
  return false;
}

static bool leap_getAxis(Device device, DeviceAxis axis, float* value) {
  if (device == DEVICE_HAND_LEFT || device == DEVICE_HAND_RIGHT) {
    LEAP_HAND* hand = state.hands[device - DEVICE_HAND_LEFT];

    if (!hand) {
      return false;
    }

    switch (axis) {
      case AXIS_TRIGGER: value[0] = hand->pinch_strength; return true;
      case AXIS_GRIP: value[0] = hand->grab_strength; return true;
      default: return false;
    }
  }

  uint32_t finger;
  LEAP_HAND* hand;
  if (state.hands[0] && device >= DEVICE_HAND_LEFT_FINGER_THUMB && device <= DEVICE_HAND_LEFT_FINGER_PINKY) {
    hand = state.hands[0];
    finger = device - DEVICE_HAND_LEFT_FINGER_THUMB;
  } else if (state.hands[1] && device >= DEVICE_HAND_RIGHT_FINGER_THUMB && device <= DEVICE_HAND_RIGHT_FINGER_PINKY) {
    hand = state.hands[1];
    finger = device - DEVICE_HAND_RIGHT_FINGER_THUMB;
  } else {
    return false;
  }

  if (axis == AXIS_CURL) {
    float curl = 1.f;
    float direction[4];
    float lastDirection[4];

    bool thumb = (finger == 0);
    LEAP_DIGIT* digit = &hand->digits[finger];
    vec3_init(lastDirection, digit->bones[0 + thumb].next_joint.v);
    vec3_sub(lastDirection, digit->bones[0 + thumb].prev_joint.v);
    vec3_normalize(lastDirection);

    // Multiply the dot products of all successive finger bone directions
    for (uint32_t i = 1 + thumb; i < 4; i++) {
      vec3_init(direction, digit->bones[i].next_joint.v);
      vec3_sub(direction, digit->bones[i].prev_joint.v);
      vec3_normalize(direction);
      curl *= vec3_dot(direction, lastDirection);
      vec3_init(lastDirection, direction);
    }

    // Exaggerate thumb curliness, it has fewer bones
    if (thumb) {
      curl = curl * curl * curl;
    }

    value[0] = 1.f - curl;
    return true;
  } else if (axis == AXIS_SPLAY) {
    float direction[4];
    float otherDirection[4];

    // Get the direction of the first knuckle, comparing it to the knuckles of any adjacent fingers
    vec3_init(direction, hand->digits[finger].bones[1 + (finger == 0)].next_joint.v);
    vec3_sub(direction, hand->digits[finger].bones[1 + (finger == 0)].prev_joint.v);
    vec3_normalize(direction);

    if (finger > 0) {
      LEAP_BONE* proximal = &hand->digits[finger - 1].bones[1 + ((finger - 1) == 0)];
      vec3_init(otherDirection, proximal->next_joint.v);
      vec3_sub(otherDirection, proximal->prev_joint.v);
      vec3_normalize(otherDirection);
      float divisor = ((finger - 1) == 0) ? .9f : .12f;
      value[0] = MIN((1.f - vec3_dot(direction, otherDirection)) / divisor, 1.f);
    } else {
      value[0] = 0.f;
    }

    if (finger < 4) {
      LEAP_BONE* proximal = &hand->digits[finger + 1].bones[1];
      vec3_init(otherDirection, proximal->next_joint.v);
      vec3_sub(otherDirection, proximal->prev_joint.v);
      vec3_normalize(otherDirection);
      float divisor = (finger == 0) ? .9f : .12f;
      value[1] = MIN((1.f - vec3_dot(direction, otherDirection)) / divisor, 1.f);
    } else {
      value[1] = 0.f;
    }

    return true;
  }

  return false;
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

    state.hands[0] = state.hands[1] = NULL;
    for (uint32_t i = 0; i < state.frame->nHands; i++) {
      LEAP_HAND* hand = &state.frame->pHands[i];
      state.hands[hand->type == eLeapHandType_Right] = hand;
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
  .getVelocity = leap_getVelocity,
  .isDown = leap_isDown,
  .isTouched = leap_isTouched,
  .getAxis = leap_getAxis,
  .vibrate = leap_vibrate,
  .newModelData = leap_newModelData,
  .update = leap_update
};
