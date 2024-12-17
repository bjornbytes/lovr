#include "api.h"
#include "headset/headset.h"
#include "data/image.h"
#include "data/modelData.h"
#include "graphics/graphics.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>

StringEntry lovrHeadsetDriver[] = {
  [DRIVER_SIMULATOR] = ENTRY("simulator"),
  [DRIVER_OPENXR] = ENTRY("openxr"),
  [DRIVER_WEBXR] = ENTRY("webxr"),
  { 0 }
};

StringEntry lovrControllerSkeletonMode[] = {
  [SKELETON_NONE] = ENTRY("none"),
  [SKELETON_CONTROLLER] = ENTRY("controller"),
  [SKELETON_NATURAL] = ENTRY("natural"),
  { 0 }
};

StringEntry lovrFoveationLevel[] = {
  [FOVEATION_NONE] = ENTRY("none"),
  [FOVEATION_LOW] = ENTRY("low"),
  [FOVEATION_MEDIUM] = ENTRY("medium"),
  [FOVEATION_HIGH] = ENTRY("high"),
  { 0 }
};

StringEntry lovrPassthroughMode[] = {
  [PASSTHROUGH_OPAQUE] = ENTRY("opaque"),
  [PASSTHROUGH_BLEND] = ENTRY("blend"),
  [PASSTHROUGH_ADD] = ENTRY("add"),
  { 0 }
};

StringEntry lovrDevice[] = {
  [DEVICE_HEAD] = ENTRY("head"),
  [DEVICE_FLOOR] = ENTRY("floor"),
  [DEVICE_HAND_LEFT] = ENTRY("hand/left"),
  [DEVICE_HAND_RIGHT] = ENTRY("hand/right"),
  [DEVICE_HAND_LEFT_GRIP] = ENTRY("hand/left/grip"),
  [DEVICE_HAND_RIGHT_GRIP] = ENTRY("hand/right/grip"),
  [DEVICE_HAND_LEFT_POINT] = ENTRY("hand/left/point"),
  [DEVICE_HAND_RIGHT_POINT] = ENTRY("hand/right/point"),
  [DEVICE_HAND_LEFT_PINCH] = ENTRY("hand/left/pinch"),
  [DEVICE_HAND_RIGHT_PINCH] = ENTRY("hand/right/pinch"),
  [DEVICE_HAND_LEFT_POKE] = ENTRY("hand/left/poke"),
  [DEVICE_HAND_RIGHT_POKE] = ENTRY("hand/right/poke"),
  [DEVICE_ELBOW_LEFT] = ENTRY("elbow/left"),
  [DEVICE_ELBOW_RIGHT] = ENTRY("elbow/right"),
  [DEVICE_SHOULDER_LEFT] = ENTRY("shoulder/left"),
  [DEVICE_SHOULDER_RIGHT] = ENTRY("shoulder/right"),
  [DEVICE_CHEST] = ENTRY("chest"),
  [DEVICE_WAIST] = ENTRY("waist"),
  [DEVICE_KNEE_LEFT] = ENTRY("knee/left"),
  [DEVICE_KNEE_RIGHT] = ENTRY("knee/right"),
  [DEVICE_FOOT_LEFT] = ENTRY("foot/left"),
  [DEVICE_FOOT_RIGHT] = ENTRY("foot/right"),
  [DEVICE_CAMERA] = ENTRY("camera"),
  [DEVICE_KEYBOARD] = ENTRY("keyboard"),
  [DEVICE_STYLUS] = ENTRY("stylus"),
  [DEVICE_EYE_LEFT] = ENTRY("eye/left"),
  [DEVICE_EYE_RIGHT] = ENTRY("eye/right"),
  [DEVICE_EYE_GAZE] = ENTRY("eye/gaze"),
  { 0 }
};

StringEntry lovrDeviceButton[] = {
  [BUTTON_TRIGGER] = ENTRY("trigger"),
  [BUTTON_THUMBSTICK] = ENTRY("thumbstick"),
  [BUTTON_THUMBREST] = ENTRY("thumbrest"),
  [BUTTON_TOUCHPAD] = ENTRY("touchpad"),
  [BUTTON_GRIP] = ENTRY("grip"),
  [BUTTON_MENU] = ENTRY("menu"),
  [BUTTON_A] = ENTRY("a"),
  [BUTTON_B] = ENTRY("b"),
  [BUTTON_X] = ENTRY("x"),
  [BUTTON_Y] = ENTRY("y"),
  [BUTTON_NIB] = ENTRY("nib"),
  { 0 }
};

StringEntry lovrDeviceAxis[] = {
  [AXIS_TRIGGER] = ENTRY("trigger"),
  [AXIS_THUMBSTICK] = ENTRY("thumbstick"),
  [AXIS_TOUCHPAD] = ENTRY("touchpad"),
  [AXIS_GRIP] = ENTRY("grip"),
  [AXIS_NIB] = ENTRY("nib"),
  { 0 }
};

static Device luax_optdevice(lua_State* L, int index) {
  const char* str = luaL_optstring(L, 1, "head");
  if (!strcmp(str, "left")) {
    return DEVICE_HAND_LEFT;
  } else if (!strcmp(str, "right")) {
    return DEVICE_HAND_RIGHT;
  }
  return luax_checkenum(L, 1, Device, "head");
}

static int l_lovrHeadsetStart(lua_State* L) {
  return luax_pushsuccess(L, lovrHeadsetInterface->start());
}

static int l_lovrHeadsetStop(lua_State* L) {
  lovrHeadsetInterface->stop();
  return 0;
}

static int l_lovrHeadsetGetDriver(lua_State* L) {
  luax_pushenum(L, HeadsetDriver, lovrHeadsetInterface->driverType);
  char name[256];
  if (lovrHeadsetInterface->getDriverName(name, sizeof(name))) {
    lua_pushstring(L, name);
  } else {
    lua_pushnil(L);
  }
  return 2;
}

static int l_lovrHeadsetGetFeatures(lua_State* L) {
  HeadsetFeatures features = { 0 };
  lovrHeadsetInterface->getFeatures(&features);
  lua_newtable(L);
  lua_pushboolean(L, features.overlay), lua_setfield(L, -2, "overlay");
  lua_pushboolean(L, features.proximity), lua_setfield(L, -2, "proximity");
  lua_pushboolean(L, features.passthrough), lua_setfield(L, -2, "passthrough");
  lua_pushboolean(L, features.refreshRate), lua_setfield(L, -2, "refreshRate");
  lua_pushboolean(L, features.depthSubmission), lua_setfield(L, -2, "depthSubmission");
  lua_pushboolean(L, features.eyeTracking), lua_setfield(L, -2, "eyeTracking");
  lua_pushboolean(L, features.handTracking), lua_setfield(L, -2, "handTracking");
  lua_pushboolean(L, features.handTrackingElbow), lua_setfield(L, -2, "handTrackingElbow");
  lua_pushboolean(L, features.keyboardTracking), lua_setfield(L, -2, "keyboardTracking");
  lua_pushboolean(L, features.viveTrackers), lua_setfield(L, -2, "viveTrackers");
  lua_pushboolean(L, features.handModel), lua_setfield(L, -2, "handModel");
  lua_pushboolean(L, features.controllerModel), lua_setfield(L, -2, "controllerModel");
  lua_pushboolean(L, features.controllerSkeleton), lua_setfield(L, -2, "controllerSkeleton");
  lua_pushboolean(L, features.layerCube), lua_setfield(L, -2, "layerCube");
  lua_pushboolean(L, features.layerSphere), lua_setfield(L, -2, "layerSphere");
  lua_pushboolean(L, features.layerCurve), lua_setfield(L, -2, "layerCurve");
  lua_pushboolean(L, features.layerDepthTest), lua_setfield(L, -2, "layerDepthTest");
  lua_pushboolean(L, features.layerFilter), lua_setfield(L, -2, "layerFilter");
  return 1;
}

static int l_lovrHeadsetGetName(lua_State* L) {
  char name[256];
  if (lovrHeadsetInterface->getName(name, sizeof(name))) {
    lua_pushstring(L, name);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrHeadsetIsSeated(lua_State* L) {
  lua_pushboolean(L, lovrHeadsetInterface->isSeated());
  return 1;
}

static int l_lovrHeadsetGetDisplayWidth(lua_State* L) {
  uint32_t width, height;
  lovrHeadsetInterface->getDisplayDimensions(&width, &height);
  lua_pushinteger(L, width);
  return 1;
}

static int l_lovrHeadsetGetDisplayHeight(lua_State* L) {
  uint32_t width, height;
  lovrHeadsetInterface->getDisplayDimensions(&width, &height);
  lua_pushinteger(L, height);
  return 1;
}

static int l_lovrHeadsetGetDisplayDimensions(lua_State* L) {
  uint32_t width, height;
  lovrHeadsetInterface->getDisplayDimensions(&width, &height);
  lua_pushinteger(L, width);
  lua_pushinteger(L, height);
  return 2;
}

static int l_lovrHeadsetGetRefreshRate(lua_State* L) {
  float refreshRate = lovrHeadsetInterface->getRefreshRate ? lovrHeadsetInterface->getRefreshRate() : 0.f;
  if (refreshRate == 0.f) {
    lua_pushnil(L);
  } else {
    lua_pushnumber(L, refreshRate);
  }
  return 1;
}

static int l_lovrHeadsetSetRefreshRate(lua_State* L) {
  float refreshRate = luax_checkfloat(L, 1);
  bool success = lovrHeadsetInterface->setRefreshRate(refreshRate);
  lua_pushboolean(L, success);
  return 1;
}

static int l_lovrHeadsetGetRefreshRates(lua_State* L) {
  uint32_t count;
  const float* refreshRates = lovrHeadsetInterface->getRefreshRates(&count);

  if (!refreshRates) {
    lua_pushnil(L);
  } else {
    lua_settop(L, 0);
    lua_createtable(L, count, 0);
    for (uint32_t i = 0; i < count; ++i) {
      lua_pushnumber(L, refreshRates[i]);
      lua_rawseti(L, 1, i + 1);
    }
  }

  return 1;
}

static int l_lovrHeadsetGetFoveation(lua_State* L) {
  FoveationLevel level;
  bool dynamic;
  lovrHeadsetInterface->getFoveation(&level, &dynamic);
  luax_pushenum(L, FoveationLevel, level);
  lua_pushboolean(L, dynamic);
  return 2;
}

static int l_lovrHeadsetSetFoveation(lua_State* L) {
  if (lua_isnoneornil(L, 1)) {
    bool success = lovrHeadsetInterface->setFoveation(FOVEATION_NONE, false);
    lua_pushboolean(L, success);
    return 1;
  } else {
    FoveationLevel level = luax_checkenum(L, 1, FoveationLevel, NULL);
    bool dynamic = lua_isnoneornil(L, -1) ? true : lua_toboolean(L, 2);
    bool success = lovrHeadsetInterface->setFoveation(level, dynamic);
    lua_pushboolean(L, success);
    return 1;
  }
}

static int l_lovrHeadsetGetPassthrough(lua_State* L) {
  PassthroughMode mode = lovrHeadsetInterface->getPassthrough();
  luax_pushenum(L, PassthroughMode, mode);
  return 1;
}

static int l_lovrHeadsetSetPassthrough(lua_State* L) {
  PassthroughMode mode;

  if (lua_isnoneornil(L, 1)) {
    mode = PASSTHROUGH_DEFAULT;
  } else if (lua_isboolean(L, 1)) {
    mode = lua_toboolean(L, 1) ? PASSTHROUGH_TRANSPARENT : PASSTHROUGH_OPAQUE;
  } else {
    mode = luax_checkenum(L, 1, PassthroughMode, NULL);
  }

  bool success = lovrHeadsetInterface->setPassthrough(mode);
  lua_pushboolean(L, success);
  return 1;
}

static int l_lovrHeadsetGetPassthroughModes(lua_State* L) {
  lua_createtable(L, 0, 3);
  for (int i = 0; lovrPassthroughMode[i].length > 0; i++) {
    lua_pushlstring(L, lovrPassthroughMode[i].string, lovrPassthroughMode[i].length);
    lua_pushboolean(L, lovrHeadsetInterface->isPassthroughSupported(i));
    lua_settable(L, -3);
  }
  return 1;
}

static int l_lovrHeadsetGetViewCount(lua_State* L) {
  lua_pushinteger(L, lovrHeadsetInterface->getViewCount());
  return 1;
}

static int l_lovrHeadsetGetViewPose(lua_State* L) {
  float position[3], orientation[4];
  uint32_t view = luax_checku32(L, 1) - 1;
  if (!lovrHeadsetInterface->getViewPose(view, position, orientation)) {
    lua_pushnil(L);
    return 1;
  }
  float angle, ax, ay, az;
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrHeadsetGetViewAngles(lua_State* L) {
  float left, right, up, down;
  uint32_t view = luax_checku32(L, 1) - 1;
  if (!lovrHeadsetInterface->getViewAngles(view, &left, &right, &up, &down)) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushnumber(L, left);
  lua_pushnumber(L, right);
  lua_pushnumber(L, up);
  lua_pushnumber(L, down);
  return 4;
}

static int l_lovrHeadsetGetClipDistance(lua_State* L) {
  float clipNear, clipFar;
  lovrHeadsetInterface->getClipDistance(&clipNear, &clipFar);
  lua_pushnumber(L, clipNear);
  lua_pushnumber(L, clipFar);
  return 2;
}

static int l_lovrHeadsetSetClipDistance(lua_State* L) {
  float clipNear = luax_checkfloat(L, 1);
  float clipFar = luax_checkfloat(L, 2);
  lovrHeadsetInterface->setClipDistance(clipNear, clipFar);
  return 0;
}

static int l_lovrHeadsetGetBoundsWidth(lua_State* L) {
  float width, depth;
  lovrHeadsetInterface->getBoundsDimensions(&width, &depth);
  lua_pushnumber(L, width);
  return 1;
}

static int l_lovrHeadsetGetBoundsDepth(lua_State* L) {
  float width, depth;
  lovrHeadsetInterface->getBoundsDimensions(&width, &depth);
  lua_pushnumber(L, depth);
  return 1;
}

static int l_lovrHeadsetGetBoundsDimensions(lua_State* L) {
  float width, depth;
  lovrHeadsetInterface->getBoundsDimensions(&width, &depth);
  lua_pushnumber(L, width);
  lua_pushnumber(L, depth);
  return 2;
}

static int l_lovrHeadsetGetBoundsGeometry(lua_State* L) {
  uint32_t count;
  const float* points = lovrHeadsetInterface->getBoundsGeometry(&count);

  if (!points) {
    lua_pushnil(L);
    return 1;
  }

  if (lua_type(L, 1) == LUA_TTABLE) {
    lua_settop(L, 1);
  } else {
    lua_settop(L, 0);
    lua_createtable(L, count / 4, 0);
  }

  int j = 1;
  for (uint32_t i = 0; i < count; i += 4) {
    lua_pushnumber(L, points[i + 0]);
    lua_rawseti(L, 1, j++);
    lua_pushnumber(L, points[i + 1]);
    lua_rawseti(L, 1, j++);
    lua_pushnumber(L, points[i + 2]);
    lua_rawseti(L, 1, j++);
  }

  return 1;
}

static int l_lovrHeadsetIsTracked(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[3], orientation[4];
  lua_pushboolean(L, lovrHeadsetInterface->getPose(device, position, orientation));
  return 1;
}

static int l_lovrHeadsetGetPose(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[3], orientation[4];
  if (lovrHeadsetInterface->getPose(device, position, orientation)) {
    float angle, ax, ay, az;
    quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
    lua_pushnumber(L, position[0]);
    lua_pushnumber(L, position[1]);
    lua_pushnumber(L, position[2]);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
    return 7;
  }
  for (int i = 0; i < 7; i++) {
    lua_pushnumber(L, 0.);
  }
  return 7;
}

static int l_lovrHeadsetGetPosition(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[3], orientation[4];
  if (lovrHeadsetInterface->getPose(device, position, orientation)) {
    lua_pushnumber(L, position[0]);
    lua_pushnumber(L, position[1]);
    lua_pushnumber(L, position[2]);
    return 3;
  }
  for (int i = 0; i < 3; i++) {
    lua_pushnumber(L, 0.);
  }
  return 3;
}

static int l_lovrHeadsetGetOrientation(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[3], orientation[4];
  if (lovrHeadsetInterface->getPose(device, position, orientation)) {
    float angle, ax, ay, az;
    quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
    return 4;
  }
  for (int i = 0; i < 4; i++) {
    lua_pushnumber(L, 0.);
  }
  return 4;
}

static int l_lovrHeadsetGetDirection(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[3], orientation[4];
  if (lovrHeadsetInterface->getPose(device, position, orientation)) {
    float direction[3];
    quat_getDirection(orientation, direction);
    lua_pushnumber(L, direction[0]);
    lua_pushnumber(L, direction[1]);
    lua_pushnumber(L, direction[2]);
    return 3;
  }
  for (int i = 0; i < 3; i++) {
    lua_pushnumber(L, 0.);
  }
  return 3;
}

static int l_lovrHeadsetGetVelocity(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float velocity[3], angularVelocity[3];
  if (lovrHeadsetInterface->getVelocity(device, velocity, angularVelocity)) {
    lua_pushnumber(L, velocity[0]);
    lua_pushnumber(L, velocity[1]);
    lua_pushnumber(L, velocity[2]);
    return 3;
  }
  for (int i = 0; i < 3; i++) {
    lua_pushnumber(L, 0.);
  }
  return 3;
}

static int l_lovrHeadsetGetAngularVelocity(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float velocity[3], angularVelocity[3];
  if (lovrHeadsetInterface->getVelocity(device, velocity, angularVelocity)) {
    lua_pushnumber(L, angularVelocity[0]);
    lua_pushnumber(L, angularVelocity[1]);
    lua_pushnumber(L, angularVelocity[2]);
    return 3;
  }
  for (int i = 0; i < 3; i++) {
    lua_pushnumber(L, 0.);
  }
  return 3;
}

static int l_lovrHeadsetIsDown(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceButton button = luax_checkenum(L, 2, DeviceButton, NULL);
  bool down, changed;
  if (lovrHeadsetInterface->isDown(device, button, &down, &changed)) {
    lua_pushboolean(L, down);
    return 1;
  }
  lua_pushnil(L);
  return 1;
}

static int l_lovrHeadsetWasPressed(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceButton button = luax_checkenum(L, 2, DeviceButton, NULL);
  bool down, changed;
  if (lovrHeadsetInterface->isDown(device, button, &down, &changed)) {
    lua_pushboolean(L, down && changed);
    return 1;
  }
  lua_pushboolean(L, false);
  return 1;
}

static int l_lovrHeadsetWasReleased(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceButton button = luax_checkenum(L, 2, DeviceButton, NULL);
  bool down, changed;
  if (lovrHeadsetInterface->isDown(device, button, &down, &changed)) {
    lua_pushboolean(L, !down && changed);
    return 1;
  }
  lua_pushboolean(L, false);
  return 1;
}

static int l_lovrHeadsetIsTouched(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceButton button = luax_checkenum(L, 2, DeviceButton, NULL);
  bool touched;
  if (lovrHeadsetInterface->isTouched(device, button, &touched)) {
    lua_pushboolean(L, touched);
    return 1;
  }
  lua_pushnil(L);
  return 1;
}

static const int axisCounts[MAX_AXES] = {
  [AXIS_TRIGGER] = 1,
  [AXIS_THUMBSTICK] = 2,
  [AXIS_TOUCHPAD] = 2,
  [AXIS_GRIP] = 1,
  [AXIS_NIB] = 1
};

static int l_lovrHeadsetGetAxis(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceAxis axis = luax_checkenum(L, 2, DeviceAxis, NULL);
  int count = axisCounts[axis];
  float value[4];
  if (lovrHeadsetInterface->getAxis(device, axis, value)) {
    for (int i = 0; i < count; i++) {
      lua_pushnumber(L, value[i]);
    }
    return count;
  }
  for (int i = 0; i < count; i++) {
    lua_pushnumber(L, 0.);
  }
  return count;
}

static int l_lovrHeadsetGetSkeleton(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float poses[HAND_JOINT_COUNT * 8];
  SkeletonSource source = SOURCE_UNKNOWN;
  if (lovrHeadsetInterface->getSkeleton(device, poses, &source)) {
    if (!lua_istable(L, 2)) {
      lua_createtable(L, HAND_JOINT_COUNT, 0);
    } else {
      lua_settop(L, 2);
    }

    for (uint32_t i = 0; i < HAND_JOINT_COUNT; i++) {
      lua_createtable(L, 8, 0);

      float angle, ax, ay, az;
      float* pose = poses + i * 8;
      quat_getAngleAxis(pose + 4, &angle, &ax, &ay, &az);
      lua_pushnumber(L, pose[0]);
      lua_pushnumber(L, pose[1]);
      lua_pushnumber(L, pose[2]);
      lua_pushnumber(L, pose[3]);
      lua_pushnumber(L, angle);
      lua_pushnumber(L, ax);
      lua_pushnumber(L, ay);
      lua_pushnumber(L, az);
      lua_rawseti(L, -9, 8);
      lua_rawseti(L, -8, 7);
      lua_rawseti(L, -7, 6);
      lua_rawseti(L, -6, 5);
      lua_rawseti(L, -5, 4);
      lua_rawseti(L, -4, 3);
      lua_rawseti(L, -3, 2);
      lua_rawseti(L, -2, 1);

      lua_pushnumber(L, pose[3]);
      lua_setfield(L, -2, "radius");

      lua_rawseti(L, -2, i + 1);
    }

    if (source != SOURCE_UNKNOWN) {
      lua_pushboolean(L, source == SOURCE_CONTROLLER);
      lua_setfield(L, -2, "controller");
    }

    return 1;
  }
  lua_pushnil(L);
  return 1;
}

static int l_lovrHeadsetVibrate(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float strength = luax_optfloat(L, 2, 1.f);
  float duration = luax_optfloat(L, 3, .5f);
  float frequency = luax_optfloat(L, 4, 0.f);
  bool success = lovrHeadsetInterface->vibrate(device, strength, duration, frequency);
  lua_pushboolean(L, success);
  return 1;
}

static int l_lovrHeadsetStopVibration(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  lovrHeadsetInterface->stopVibration(device);
  return 0;
}

static int l_lovrHeadsetNewModel(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  bool animated = false;

  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "animated");
    animated = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }

  ModelData* modelData = lovrHeadsetInterface->newModelData(device, animated);

  if (modelData) {
    ModelInfo info = { .data = modelData, .mipmaps = true };
    Model* model = lovrModelCreate(&info);
    lovrRelease(modelData, lovrModelDataDestroy);
    luax_assert(L, model);
    luax_pushtype(L, Model, model);
    lovrRelease(model, lovrModelDestroy);
    return 1;
  }

  return 0;
}

static int l_lovrHeadsetAnimate(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  lua_pushboolean(L, lovrHeadsetInterface->animate(model));
  return 1;
}

static void luax_checkimages(lua_State* L, int index, Image** images, uint32_t capacity, uint32_t* count, uint32_t* layers) {
  if (lua_istable(L, index)) {
    uint32_t length = luax_len(L, 1);
    luax_check(L, length <= capacity, "Too many images!");
    for (uint32_t i = 0; i < length; i++) {
      lua_rawgeti(L, 1, i + 1);
      Image* image = luax_checkimage(L, -1);
      luax_check(L, image, "Expected a table of Images");
      luax_check(L, i == 0 || lovrImageGetWidth(image, 0) == lovrImageGetWidth(images[0], 0), "Layer image sizes must match");
      luax_check(L, i == 0 || lovrImageGetHeight(image, 0) == lovrImageGetHeight(images[0], 0), "Layer image sizes must match");
      luax_check(L, lovrImageGetLayerCount(image) == 1, "When providing a table of Images, they can only have a single array layer");
      luax_check(L, lovrImageGetFormat(image) == FORMAT_RGBA8, "Currently, Layer images must be rgba8");
      images[i] = image;
      lua_pop(L, 1);
    }
    *layers = length;
    *count = length;
  } else {
    images[0] = luax_checkimage(L, 1);
    luax_check(L, lovrImageGetFormat(images[0]) == FORMAT_RGBA8, "Currently, Layer images must be rgba8");
    *layers = lovrImageGetLayerCount(images[0]);
    *count = 1;
  }
}

static int l_lovrHeadsetSetBackground(lua_State* L) {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t layers = 0;
  Image* images[6];
  uint32_t imageCount = 0;
  Texture* texture = NULL;

  if ((texture = luax_totype(L, 1, Texture)) != NULL) {
    const TextureInfo* info = lovrTextureGetInfo(texture);
    width = info->width;
    height = info->height;
    layers = info->layers;
  } else {
    luax_checkimages(L, 1, images, COUNTOF(images), &imageCount, &layers);
    luax_check(L, imageCount > 1, "Must have at least 1 image");
    width = lovrImageGetWidth(images[0], 0);
    height = lovrImageGetHeight(images[0], 0);
  }

  luax_check(L, layers == 1 || layers == 6, "Currently, background must have 1 or 6 layers");

  Texture* background = lovrHeadsetInterface->setBackground(width, height, layers);

  if (!background) {
    for (uint32_t i = 0; i < imageCount; i++) {
      lovrRelease(images[i], lovrImageDestroy);
    }
    luax_assert(L, false);
  }

  if (texture) {
    uint32_t srcOffset[4] = { 0 };
    uint32_t dstOffset[4] = { 0 };
    uint32_t extent[3] = { width, height, layers };
    luax_assert(L, lovrTextureCopy(texture, background, srcOffset, dstOffset, extent));
  } else if (imageCount > 0) {
    for (uint32_t i = 0; i < imageCount; i++) {
      uint32_t texOffset[4] = { 0, 0, i, 0 };
      uint32_t imgOffset[4] = { 0, 0, 0, 0 };
      uint32_t extent[3] = { width, height, lovrImageGetLayerCount(images[i]) };
      luax_assert(L, lovrTextureSetPixels(background, images[i], texOffset, imgOffset, extent));
      lovrRelease(images[i], lovrImageDestroy);
    }
  }

  return 0;
}

static int l_lovrHeadsetNewLayer(lua_State* L) {
  LayerInfo info = { .filter = true };

  int index;
  Image* images[2];
  uint32_t imageCount = 0;
  Texture* texture = NULL;
  uint32_t arraySize = 0;

  if (lua_type(L, 1) == LUA_TNUMBER) {
    info.width = luax_checku32(L, 1);
    info.height = luax_checku32(L, 2);
    arraySize = 1;
    index = 3;
  } else if ((texture = luax_totype(L, 1, Texture)) != NULL) {
    const TextureInfo* textureInfo = lovrTextureGetInfo(texture);
    luax_check(L, textureInfo->format == FORMAT_RGBA8, "Currently, Layer images must be rgba8");
    info.width = textureInfo->width;
    info.height = textureInfo->height;
    arraySize = textureInfo->layers;
    index = 2;
  } else {
    luax_checkimages(L, 2, images, COUNTOF(images), &imageCount, &arraySize);
    luax_check(L, imageCount > 1, "Must have at least 1 image");
    info.width = lovrImageGetWidth(images[0], 0);
    info.height = lovrImageGetHeight(images[0], 0);
    index = 2;
  }

  info.stereo = arraySize == 2;
  info.immutable = texture || imageCount > 0;

  if (lua_istable(L, index)) {
    lua_getfield(L, index, "stereo");
    if (!lua_isnil(L, -1)) info.stereo = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "static");
    if (!lua_isnil(L, -1)) info.immutable = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "transparent");
    if (!lua_isnil(L, -1)) info.transparent = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "filter");
    if (!lua_isnil(L, -1)) info.filter = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }

  if (texture || imageCount > 0) {
    uint32_t expected = 1 << info.stereo;
    luax_check(L, arraySize == expected, "Expected %d images for %s layer", expected, info.stereo ? "stereo" : "mono");
  }

  Layer* layer = lovrHeadsetInterface->newLayer(&info);
  luax_assert(L, layer);

  if (texture || imageCount > 0) {
    Texture* layerTexture = lovrHeadsetInterface->getLayerTexture(layer);

    if (!layerTexture && imageCount > 0) {
      for (uint32_t i = 0; i < imageCount; i++) {
        lovrRelease(images[i], lovrImageDestroy);
      }
    }

    luax_assert(L, layerTexture);

    if (texture) {
      uint32_t srcOffset[4] = { 0 };
      uint32_t dstOffset[4] = { 0 };
      uint32_t extent[3] = { info.width, info.height, arraySize };
      luax_assert(L, lovrTextureCopy(texture, layerTexture, srcOffset, dstOffset, extent));
    } else if (imageCount > 0) {
      for (uint32_t i = 0; i < imageCount; i++) {
        uint32_t texOffset[4] = { 0, 0, i, 0 };
        uint32_t imgOffset[4] = { 0, 0, 0, 0 };
        uint32_t extent[3] = { info.width, info.height, lovrImageGetLayerCount(images[i]) };
        luax_assert(L, lovrTextureSetPixels(layerTexture, images[i], texOffset, imgOffset, extent));
        lovrRelease(images[i], lovrImageDestroy);
      }
    }
  }

  luax_pushtype(L, Layer, layer);
  lovrRelease(layer, lovrLayerDestroy);
  return 1;
}

static int l_lovrHeadsetGetLayers(lua_State* L) {
  bool main;
  uint32_t count;
  Layer** layers = lovrHeadsetInterface->getLayers(&count, &main);
  lua_createtable(L, (int) count, 0);
  for (uint32_t i = 0; i < count; i++) {
    luax_pushtype(L, Layer, layers[i]);
    lua_rawseti(L, -2, (int) i + 1);
  }
  lua_pushboolean(L, main);
  lua_setfield(L, -2, "main");
  return 1;
}

static int l_lovrHeadsetSetLayers(lua_State* L) {
  Layer* layers[MAX_LAYERS];
  uint32_t count = 0;
  bool main = true;
  if (lua_type(L, 1) == LUA_TTABLE) {
    count = luax_len(L, 1);
    luax_check(L, count <= MAX_LAYERS, "Too many layers (max is %d)", MAX_LAYERS);
    for (uint32_t i = 0; i < count; i++) {
      lua_rawgeti(L, 1, (int) i + 1);
      layers[i] = luax_checktype(L, -1, Layer);
      lua_pop(L, 1);
    }
    lua_getfield(L, 1, "main");
    if (!lua_isnil(L, -1)) main = lua_toboolean(L, -1);
    lua_pop(L, 1);
  } else {
    count = lua_gettop(L);
    luax_check(L, count <= MAX_LAYERS, "Too many layers (max is %d)", MAX_LAYERS);
    for (uint32_t i = 0; i < count; i++) {
      layers[i] = luax_checktype(L, (int) i + 1, Layer);
    }
  }
  bool success = lovrHeadsetInterface->setLayers(layers, count, main);
  luax_assert(L, success);
  return 0;
}

static int l_lovrHeadsetGetTexture(lua_State* L) {
  Texture* texture = NULL;
  bool success = lovrHeadsetInterface->getTexture(&texture);
  luax_assert(L, success);
  luax_pushtype(L, Texture, texture);
  return 1;
}

static int l_lovrHeadsetGetPass(lua_State* L) {
  Pass* pass = NULL;
  bool success = lovrHeadsetInterface->getPass(&pass);
  luax_assert(L, success);
  luax_pushtype(L, Pass, pass);
  return 1;
}

static int l_lovrHeadsetSubmit(lua_State* L) {
  luax_assert(L, lovrHeadsetInterface->submit());
  return 0;
}

static int l_lovrHeadsetIsActive(lua_State* L) {
  lua_pushboolean(L, lovrHeadsetInterface->isActive());
  return 1;
}

static int l_lovrHeadsetIsVisible(lua_State* L) {
  lua_pushboolean(L, lovrHeadsetInterface->isVisible());
  return 1;
}

static int l_lovrHeadsetIsFocused(lua_State* L) {
  lua_pushboolean(L, lovrHeadsetInterface->isFocused());
  return 1;
}

static int l_lovrHeadsetIsMounted(lua_State* L) {
  lua_pushboolean(L, lovrHeadsetInterface->isMounted());
  return 1;
}

static int l_lovrHeadsetUpdate(lua_State* L) {
  double dt = 0.;

  if (lovrHeadsetInterface->update) {
    luax_assert(L, lovrHeadsetInterface->update(&dt));
  }

  lua_pushnumber(L, dt);
  return 1;
}

static int l_lovrHeadsetGetTime(lua_State* L) {
  lua_pushnumber(L, lovrHeadsetInterface->getDisplayTime());
  return 1;
}

static int l_lovrHeadsetGetDeltaTime(lua_State* L) {
  lua_pushnumber(L, lovrHeadsetInterface->getDeltaTime());
  return 1;
}

static int l_lovrHeadsetGetHands(lua_State* L) {
  if (lua_istable(L, 1)) {
    lua_settop(L, 1);
  } else {
    lua_newtable(L);
  }

  int count = 0;
  float position[3], orientation[4];
  Device hands[] = { DEVICE_HAND_LEFT, DEVICE_HAND_RIGHT };
  for (size_t i = 0; i < COUNTOF(hands); i++) {
    if (lovrHeadsetInterface->getPose(hands[i], position, orientation)) {
      luax_pushenum(L, Device, hands[i]);
      lua_rawseti(L, -2, ++count);
    }
  }
  lua_pushnil(L);
  lua_rawseti(L, -2, ++count);
  return 1;
}

static int l_lovrHeadsetGetHandles(lua_State* L) {
  if (lovrHeadsetInterface->driverType == DRIVER_OPENXR) {
    lua_pushlightuserdata(L, (void*) lovrHeadsetInterface->getOpenXRInstanceHandle());
    lua_pushlightuserdata(L, (void*) lovrHeadsetInterface->getOpenXRSessionHandle());
    return 2;
  } else {
    return 0;
  }
}

static const luaL_Reg lovrHeadset[] = {
  { "start", l_lovrHeadsetStart },
  { "stop", l_lovrHeadsetStop },
  { "getDriver", l_lovrHeadsetGetDriver },
  { "getFeatures", l_lovrHeadsetGetFeatures },
  { "getName", l_lovrHeadsetGetName },
  { "getDisplayWidth", l_lovrHeadsetGetDisplayWidth },
  { "getDisplayHeight", l_lovrHeadsetGetDisplayHeight },
  { "getDisplayDimensions", l_lovrHeadsetGetDisplayDimensions },
  { "getRefreshRate", l_lovrHeadsetGetRefreshRate },
  { "setRefreshRate", l_lovrHeadsetSetRefreshRate },
  { "getRefreshRates", l_lovrHeadsetGetRefreshRates },
  { "getFoveation", l_lovrHeadsetGetFoveation },
  { "setFoveation", l_lovrHeadsetSetFoveation },
  { "getPassthrough", l_lovrHeadsetGetPassthrough },
  { "setPassthrough", l_lovrHeadsetSetPassthrough },
  { "getPassthroughModes", l_lovrHeadsetGetPassthroughModes },
  { "getViewCount", l_lovrHeadsetGetViewCount },
  { "getViewPose", l_lovrHeadsetGetViewPose },
  { "getViewAngles", l_lovrHeadsetGetViewAngles },
  { "getClipDistance", l_lovrHeadsetGetClipDistance },
  { "setClipDistance", l_lovrHeadsetSetClipDistance },
  { "isSeated", l_lovrHeadsetIsSeated },
  { "getBoundsWidth", l_lovrHeadsetGetBoundsWidth },
  { "getBoundsDepth", l_lovrHeadsetGetBoundsDepth },
  { "getBoundsDimensions", l_lovrHeadsetGetBoundsDimensions },
  { "getBoundsGeometry", l_lovrHeadsetGetBoundsGeometry },
  { "isTracked", l_lovrHeadsetIsTracked },
  { "getPose", l_lovrHeadsetGetPose },
  { "getPosition", l_lovrHeadsetGetPosition },
  { "getOrientation", l_lovrHeadsetGetOrientation },
  { "getDirection", l_lovrHeadsetGetDirection },
  { "getVelocity", l_lovrHeadsetGetVelocity },
  { "getAngularVelocity", l_lovrHeadsetGetAngularVelocity },
  { "isDown", l_lovrHeadsetIsDown },
  { "wasPressed", l_lovrHeadsetWasPressed },
  { "wasReleased", l_lovrHeadsetWasReleased },
  { "isTouched", l_lovrHeadsetIsTouched },
  { "getAxis", l_lovrHeadsetGetAxis },
  { "getSkeleton", l_lovrHeadsetGetSkeleton },
  { "vibrate", l_lovrHeadsetVibrate },
  { "stopVibration", l_lovrHeadsetStopVibration },
  { "newModel", l_lovrHeadsetNewModel },
  { "animate", l_lovrHeadsetAnimate },
  { "setBackground", l_lovrHeadsetSetBackground },
  { "newLayer", l_lovrHeadsetNewLayer },
  { "getLayers", l_lovrHeadsetGetLayers },
  { "setLayers", l_lovrHeadsetSetLayers },
  { "getTexture", l_lovrHeadsetGetTexture },
  { "getPass", l_lovrHeadsetGetPass },
  { "submit", l_lovrHeadsetSubmit },
  { "isActive", l_lovrHeadsetIsActive },
  { "isVisible", l_lovrHeadsetIsVisible },
  { "isFocused", l_lovrHeadsetIsFocused },
  { "isMounted", l_lovrHeadsetIsMounted },
  { "update", l_lovrHeadsetUpdate },
  { "getTime", l_lovrHeadsetGetTime },
  { "getDeltaTime", l_lovrHeadsetGetDeltaTime },
  { "getHands", l_lovrHeadsetGetHands },
  { "getHandles", l_lovrHeadsetGetHandles },
  { NULL, NULL }
};

extern const luaL_Reg lovrLayer[];

int luaopen_lovr_headset(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrHeadset);
  luax_registertype(L, Layer);

  HeadsetDriver drivers[8];

  HeadsetConfig config = {
    .drivers = drivers,
    .driverCount = 0,
    .supersample = 1.f,
    .seated = false,
    .mask = true,
    .stencil = false,
    .antialias = true,
    .submitDepth = true,
    .overlay = false,
    .overlayOrder = 0,
    .controllerSkeleton = SKELETON_CONTROLLER
  };

  luax_pushconf(L);
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "headset");
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "drivers");
      int n = luax_len(L, -1);
      for (int i = 0; i < n; i++) {
        lua_rawgeti(L, -1, i + 1);

        // Deprecated
        if (lua_type(L, -1) == LUA_TSTRING && !strcmp(lua_tostring(L, -1), "desktop")) {
          lua_pushliteral(L, "simulator");
          lua_replace(L, -2);
        }

        config.drivers[config.driverCount++] = luax_checkenum(L, -1, HeadsetDriver, NULL);
        luax_check(L, config.driverCount < COUNTOF(drivers), "Too many headset drivers specified in conf.lua");
        lua_pop(L, 1);
      }
      lua_pop(L, 1);

      lua_getfield(L, -1, "supersample");
      if (lua_type(L, -1) == LUA_TBOOLEAN) {
        config.supersample = lua_toboolean(L, -1) ? 2.f : 1.f;
      } else {
        config.supersample = luax_optfloat(L, -1, 1.f);
      }
      lua_pop(L, 1);

      lua_getfield(L, -1, "debug");
      config.debug = lua_toboolean(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "seated");
      config.seated = lua_toboolean(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "mask");
      config.mask = lua_isnil(L, -1) ? true : lua_toboolean(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "stencil");
      config.stencil = lua_toboolean(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "antialias");
      config.antialias = lua_isnil(L, -1) ? true : lua_toboolean(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "submitdepth");
      config.submitDepth = lua_toboolean(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "overlay");
      config.overlay = lua_toboolean(L, -1);
      config.overlayOrder = lua_type(L, -1) == LUA_TNUMBER ? luax_optu32(L, -1, 0) : 0;
      lua_pop(L, 1);

      lua_getfield(L, -1, "controllerskeleton");
      if (!lua_isnil(L, -1)) config.controllerSkeleton = luax_checkenum(L, -1, ControllerSkeletonMode, NULL);
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  luax_atexit(L, lovrHeadsetDestroy);
  luax_assert(L, lovrHeadsetInit(&config));
  return 1;
}
