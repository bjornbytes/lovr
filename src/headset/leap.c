#include "headset/headset.h"
#include "platform.h"
#include "util.h"
#include "lib/maf.h"
#include "lib/tinycthread/tinycthread.h"
#include <LeapC.h>
#include <stdlib.h>
#include <inttypes.h>

static struct {
  LEAP_CONNECTION connection;
  LEAP_CLOCK_REBASER clock;
  LEAP_TRACKING_EVENT* frame;
  uint64_t frameSize;
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
  if (!state.frame) {
    return false;
  }

  eLeapHandType handType;
  if (!strncmp("hand/left", path, strlen("hand/left"))) {
    handType = eLeapHandType_Left;
    path += strlen("hand/left");
  } else if (!strncmp("hand/right", path, strlen("hand/right"))) {
    handType = eLeapHandType_Right;
    path += strlen("hand/right");
  } else {
    return false;
  }

  for (uint32_t i = 0; i < state.frame->nHands; i++) {
    LEAP_HAND* hand = &state.frame->pHands[i];

    if (hand->type != handType) {
      continue;
    }

    float orientation[4];

    if (*path == '\0') {
      *x = hand->palm.position.x;
      *y = hand->palm.position.y;
      *z = hand->palm.position.z;
      quat_init(orientation, hand->palm.orientation.v);
    } else if (!strcmp(path, "/palm")) {
      *x = hand->palm.position.x;
      *y = hand->palm.position.y;
      *z = hand->palm.position.z;
      quat_between(orientation, (float[3]) { 0.f, 0.f, -1.f }, hand->palm.normal.v);
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
        *y = tip[1];
        *z = tip[2];
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
        *y = base[1];
        *z = base[2];
      } else {
        return false;
      }

      float direction[3];
      vec3_sub(vec3_init(direction, tip), base);
      quat_between(orientation, (float[3]) { 0.f, 0.f, -1.f }, direction);
    }

    float hx, hy, hz, hangle, hax, hay, haz;
    float head[16] = MAT4_IDENTITY;

    // A lot of the matrix stuff here could be cached
    if (lovrHeadsetDriver && lovrHeadsetDriver->getPose("head", &hx, &hy, &hz, &hangle, &hax, &hay, &haz)) {
      mat4_translate(head, hx, hy, hz);
      mat4_rotate(head, hangle, hax, hay, haz);
    }

    float remap[16] = { -1.f, 0.f, 0.f, 0.f, 0.f, 0.f, -1.f, 0.f, 0.f, -1.f, 0.f, 0.f, 0.f, 0.f, -80.f, 1.f };
    mat4_scale(head, .001f, .001f, .001f);
    mat4_multiply(head, remap);

    mat4_transform(head, x, y, z);
    mat4_rotateQuat(head, orientation);

    quat_fromMat4(orientation, head);
    quat_getAngleAxis(orientation, angle, ax, ay, az);
    return true;
  }

  return false;
}

static bool getVelocity(const char* path, float* vx, float* vy, float* vz, float* vax, float* vay, float* vaz) {
  return false;
}

static void update(float dt) {
  if (!state.connected) {
    return;
  }

  double relativePredictedDisplayTime = lovrHeadsetDriver ? lovrHeadsetDriver->getDisplayTime() : 0.;
  int64_t now = (int64_t) ((lovrPlatformGetTime() * (double) 1e6) + .5);
  int64_t predicted = now + (int64_t) ((relativePredictedDisplayTime * (double) 1e6) + .5);
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
  }
}

HeadsetInterface lovrHeadsetLeapMotionDriver = {
  .driverType = DRIVER_LEAP_MOTION,
  .init = init,
  .destroy = destroy,
  .getPose = getPose,
  .getVelocity = getVelocity,
  .update = update
};
