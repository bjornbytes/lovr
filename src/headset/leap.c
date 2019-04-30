#include "headset/headset.h"
#include "platform.h"
#include "util.h"
#include "lib/maf.h"
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

static void destroy(void);
static bool init(float offset, int msaa) {
  if (LeapCreateConnection(NULL, &state.connection) == eLeapRS_Success) {
    if (LeapOpenConnection(state.connection) == eLeapRS_Success) {
      LeapCreateClockRebaser(&state.clock);
      if (thrd_create(&state.thread, loop, NULL) == thrd_success) {
        return true;
      }
    }
  }

  return destroy(), false;
}

static void destroy(void) {
  free(state.frame);
  thrd_detach(state.thread);
  LeapDestroyClockRebaser(state.clock);
  LeapCloseConnection(state.connection);
  LeapDestroyConnection(state.connection);
  memset(&state, 0, sizeof(state));
}

static bool getPose(const char* path, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  LEAP_HAND* hand;
  if (state.leftHand && !strncmp("hand/left", path, strlen("hand/left"))) {
    hand = state.leftHand;
    path += strlen("hand/left");
  } else if (state.rightHand && !strncmp("hand/right", path, strlen("hand/right"))) {
    hand = state.rightHand;
    path += strlen("hand/right");
  } else {
    return false;
  }

  float direction[3];

  if (*path == '\0') {
    *x = hand->palm.position.x;
    *y = hand->palm.position.z;
    *z = hand->palm.position.y;
    vec3_init(direction, hand->palm.normal.v);
  } else if (!strncmp("/finger/", path, strlen("/finger/"))) {
    path += strlen("/finger/");

    int fingerIndex;
    if (!strncmp("thumb", path, strlen("thumb"))) { fingerIndex = 0; path += strlen("thumb"); }
    else if (!strncmp("index", path, strlen("index"))) { fingerIndex = 1; path += strlen("index"); }
    else if (!strncmp("middle", path, strlen("middle"))) { fingerIndex = 2; path += strlen("middle"); }
    else if (!strncmp("ring", path, strlen("ring"))) { fingerIndex = 3; path += strlen("ring"); }
    else if (!strncmp("pinky", path, strlen("pinky"))) { fingerIndex = 4; path += strlen("pinky"); }
    else { return false; }

    LEAP_DIGIT* finger = &hand->digits[fingerIndex];
    vec3 base;
    vec3 tip;

    if (*path == '\0') {
      tip = finger->distal.next_joint.v;
      base = finger->distal.prev_joint.v;
      *x = tip[0];
      *y = tip[2];
      *z = tip[1];
    } else if (!strncmp("/bone/", path, strlen("/bone/"))) {
      path += strlen("/bone/");

      int boneIndex;
      if (!strcmp(path, "metacarpal")) { boneIndex = 0; }
      else if (!strcmp(path, "proximal")) { boneIndex = 1; }
      else if (!strcmp(path, "intermediate")) { boneIndex = 2; }
      else if (!strcmp(path, "distal")) { boneIndex = 3; }
      else { return false; }

      LEAP_BONE* bone = &finger->bones[boneIndex];
      tip = bone->next_joint.v;
      base = bone->prev_joint.v;
      *x = base[0];
      *y = base[2];
      *z = base[1];
    } else {
      return false;
    }

    vec3_sub(vec3_init(direction, tip), base);
  }

  // Convert hand positions to meters, and push them back a bit to account for the discrepancy
  // between the leap motion device and the HMD
  *x *= -.001f;
  *y *= -.001f;
  *z *= -.001f;
  *z += -.080f;
  mat4_transform(state.headPose, x, y, z);

  vec3_normalize(direction);
  vec3_scale(direction, -1.f);
  float temp = direction[1];
  direction[1] = direction[2];
  direction[2] = temp;
  mat4_transformDirection(state.headPose, &direction[0], &direction[1], &direction[2]);

  float orientation[4];
  quat_between(orientation, (float[3]) { 0.f, 0.f, -1.f }, direction);
  quat_getAngleAxis(orientation, angle, ax, ay, az);
  return true;
}

static bool getVelocity(const char* path, float* vx, float* vy, float* vz, float* vax, float* vay, float* vaz) {
  LEAP_HAND* hand;
  if (state.leftHand && !strcmp(path, "hand/left")) {
    hand = state.leftHand;
  } else if (state.rightHand && !strcmp(path, "hand/right")) {
    hand = state.rightHand;
  } else {
    return false;
  }

  *vx = -hand->palm.velocity.x * .001f;
  *vy = -hand->palm.velocity.z * .001f;
  *vz = -hand->palm.velocity.y * .001f;
  mat4_transformDirection(state.headPose, vx, vy, vz);
  return true;
}

static bool isDown(const char* path, bool* down) {
  return false;
}

static int getAxis(const char* path, float* x, float* y, float* z) {
  LEAP_HAND* hand;
  if (state.leftHand && !strncmp("hand/left", path, strlen("hand/left"))) {
    hand = state.leftHand;
    path += strlen("hand/left");
  } else if (state.rightHand && !strncmp("hand/right", path, strlen("hand/right"))) {
    hand = state.rightHand;
    path += strlen("hand/right");
  } else {
    return 0;
  }

  if (!strcmp(path, "/pinch")) {
    *x = hand->pinch_strength;
    return 1;
  } else if (!strcmp(path, "/grip")) {
    *x = hand->grab_strength;
    return 1;
  }

  return 0;
}

static bool vibrate(const char* path, float strength, float duration, float frequency) {
  return false;
}

static struct ModelData* newModelData(const char* path) {
  return NULL;
}

static void update(float dt) {
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

    float x, y, z, angle, ax, ay, az;
    if (lovrHeadsetDriver && lovrHeadsetDriver->getPose("head", &x, &y, &z, &angle, &ax, &ay, &az)) {
      mat4 m = state.headPose;
      mat4_identity(m);
      mat4_translate(m, x, y, z);
      mat4_rotate(m, angle, ax, ay, az);
    }
  }
}

HeadsetInterface lovrHeadsetLeapMotionDriver = {
  .driverType = DRIVER_LEAP_MOTION,
  .init = init,
  .destroy = destroy,
  .getPose = getPose,
  .getVelocity = getVelocity,
  .isDown = isDown,
  .getAxis = getAxis,
  .vibrate = vibrate,
  .newModelData = newModelData,
  .update = update
};
