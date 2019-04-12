#include "headset/headset.h"
#include "graphics/graphics.h"
#include "lib/maf.h"
#include "platform.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

static struct {
  float offset;

  float clipNear;
  float clipFar;

  float position[3];
  float velocity[3];
  float localVelocity[3];
  float angularVelocity[3];

  float yaw;
  float pitch;
  float transform[16];

  double prevCursorX;
  double prevCursorY;
} state;

static bool init(float offset, int msaa) {
  state.offset = offset;
  state.clipNear = 0.1f;
  state.clipFar = 100.f;
  mat4_identity(state.transform);
  return true;
}

static void destroy(void) {
  memset(&state, 0, sizeof(state));
}

static const char* getName(void) {
  return "VR Simulator";
}

static HeadsetOrigin getOriginType(void) {
  return ORIGIN_HEAD;
}

static void getDisplayDimensions(uint32_t* width, uint32_t* height) {
  int w, h;
  lovrPlatformGetFramebufferSize(&w, &h);
  *width = (uint32_t) w;
  *height = (uint32_t) h;
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
  *width = *depth = 0.f;
}

static const float* getBoundsGeometry(int* count) {
  *count = 0;
  return NULL;
}

static bool getPose(Path path, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az) {
  bool head = PATH_EQ(path, P_HEAD);
  bool hand = PATH_EQ(path, P_HAND, P_LEFT) || PATH_EQ(path, P_HAND, P_RIGHT);

  if (!head && !hand) {
    return false;
  }

  if (x) {
    *x = *y = 0.f;
    *z = hand ? -.75f : 0.f;
    mat4_transform(state.transform, x, y, z);
  }

  if (angle) {
    float q[4];
    quat_fromMat4(q, state.transform);
    quat_getAngleAxis(q, angle, ax, ay, az);
  }

  return true;
}

static bool getVelocity(Path path, float* vx, float* vy, float* vz) {
  if (PATH_EQ(path, P_HEAD)) {
    *vx = state.velocity[0];
    *vy = state.velocity[1];
    *vz = state.velocity[2];
    return true;
  } else if (PATH_EQ(path, P_HAND, P_LEFT) || PATH_EQ(path, P_HAND, P_RIGHT)) {
    *vx = *vy = *vz = 0.f;
    return true;
  }

  return false;
}

static bool getAngularVelocity(Path path, float* vx, float* vy, float* vz) {
  if (PATH_EQ(path, P_HEAD)) {
    *vx = state.angularVelocity[0];
    *vy = state.angularVelocity[1];
    *vz = state.angularVelocity[2];
    return true;
  } else if (PATH_EQ(path, P_HAND, P_LEFT) || PATH_EQ(path, P_HAND, P_RIGHT)) {
    *vx = *vy = *vz = 0.f;
    return true;
  }

  return false;
}

static bool isDown(Path path, bool* down) {
  if (PATH_EQ(path, P_HAND, P_LEFT) || PATH_EQ(path, P_HAND, P_RIGHT)) {
    *down = lovrPlatformIsMouseDown(MOUSE_RIGHT);
    return true;
  }

  return false;
}

static bool isTouched(Path path, bool* touched) {
  return false;
}

static int getAxis(Path path, float* x, float* y, float* z) {
  return 0;
}

static bool vibrate(Path path, float strength, float duration, float frequency) {
  return false;
}

static ModelData* newModelData(Path path) {
  return NULL;
}

static void renderTo(void (*callback)(void*), void* userdata) {
  uint32_t width, height;
  getDisplayDimensions(&width, &height);
  Camera camera = { .canvas = NULL, .viewMatrix = { MAT4_IDENTITY }, .stereo = true };
  mat4_perspective(camera.projection[0], state.clipNear, state.clipFar, 67.f * M_PI / 180.f, (float) width / 2.f / height);
  mat4_multiply(camera.viewMatrix[0], state.transform);
  mat4_invertPose(camera.viewMatrix[0]);
  mat4_set(camera.projection[1], camera.projection[0]);
  mat4_set(camera.viewMatrix[1], camera.viewMatrix[0]);
  lovrGraphicsSetCamera(&camera, true);
  callback(userdata);
  lovrGraphicsSetCamera(NULL, false);
}

static void update(float dt) {
  bool front = lovrPlatformIsKeyDown(KEY_W) || lovrPlatformIsKeyDown(KEY_UP);
  bool back = lovrPlatformIsKeyDown(KEY_S) || lovrPlatformIsKeyDown(KEY_DOWN);
  bool left = lovrPlatformIsKeyDown(KEY_A) || lovrPlatformIsKeyDown(KEY_LEFT);
  bool right = lovrPlatformIsKeyDown(KEY_D) || lovrPlatformIsKeyDown(KEY_RIGHT);
  bool up = lovrPlatformIsKeyDown(KEY_Q);
  bool down = lovrPlatformIsKeyDown(KEY_E);

  float movespeed = 3.f * dt;
  float turnspeed = 3.f * dt;
  float damping = MAX(1.f - 20.f * dt, 0);

  if (lovrPlatformIsMouseDown(MOUSE_LEFT)) {
    lovrPlatformSetMouseMode(MOUSE_MODE_GRABBED);

    int width, height;
    lovrPlatformGetWindowSize(&width, &height);

    double mx, my;
    lovrPlatformGetMousePosition(&mx, &my);

    if (state.prevCursorX == -1 && state.prevCursorY == -1) {
      state.prevCursorX = mx;
      state.prevCursorY = my;
    }

    float aspect = (float) width / height;
    float dx = (float) (mx - state.prevCursorX) / ((float) width);
    float dy = (float) (my - state.prevCursorY) / ((float) height * aspect);
    state.angularVelocity[0] = dy / dt;
    state.angularVelocity[1] = dx / dt;
    state.prevCursorX = mx;
    state.prevCursorY = my;
  } else {
    lovrPlatformSetMouseMode(MOUSE_MODE_NORMAL);
    vec3_scale(state.angularVelocity, damping);
    state.prevCursorX = state.prevCursorY = -1;
  }

  // Update velocity
  state.localVelocity[0] = left ? -movespeed : (right ? movespeed : state.localVelocity[0]);
  state.localVelocity[1] = up ? movespeed : (down ? -movespeed : state.localVelocity[1]);
  state.localVelocity[2] = front ? -movespeed : (back ? movespeed : state.localVelocity[2]);
  vec3_init(state.velocity, state.localVelocity);
  mat4_transformDirection(state.transform, &state.velocity[0], &state.velocity[1], &state.velocity[2]);
  vec3_scale(state.localVelocity, damping);

  // Update position
  vec3_add(state.position, state.velocity);

  // Update orientation
  state.pitch = CLAMP(state.pitch - state.angularVelocity[0] * turnspeed, -(float) M_PI / 2.f, (float) M_PI / 2.f);
  state.yaw -= state.angularVelocity[1] * turnspeed;

  // Update transform
  mat4_identity(state.transform);
  mat4_translate(state.transform, 0.f, state.offset, 0.f);
  mat4_translate(state.transform, state.position[0], state.position[1], state.position[2]);
  mat4_rotate(state.transform, state.yaw, 0.f, 1.f, 0.f);
  mat4_rotate(state.transform, state.pitch, 1.f, 0.f, 0.f);
}

HeadsetInterface lovrHeadsetDesktopDriver = {
  .driverType = DRIVER_DESKTOP,
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
  .getAngularVelocity = getAngularVelocity,
  .isDown = isDown,
  .isTouched = isTouched,
  .getAxis = getAxis,
  .vibrate = vibrate,
  .newModelData = newModelData,
  .renderTo = renderTo,
  .update = update
};
