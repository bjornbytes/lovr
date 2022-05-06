#include "api.h"
#include "headset/headset.h"
#include "data/modelData.h"
#include "graphics/graphics.h"
#include "graphics/model.h"
#include "graphics/texture.h"
#include "core/maf.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>

StringEntry lovrHeadsetDriver[] = {
  [DRIVER_DESKTOP] = ENTRY("desktop"),
  [DRIVER_OPENXR] = ENTRY("openxr"),
  [DRIVER_WEBXR] = ENTRY("webxr"),
  { 0 }
};

StringEntry lovrHeadsetOrigin[] = {
  [ORIGIN_HEAD] = ENTRY("head"),
  [ORIGIN_FLOOR] = ENTRY("floor"),
  { 0 }
};

StringEntry lovrDevice[] = {
  [DEVICE_HEAD] = ENTRY("head"),
  [DEVICE_HAND_LEFT] = ENTRY("hand/left"),
  [DEVICE_HAND_RIGHT] = ENTRY("hand/right"),
  [DEVICE_HAND_LEFT_POINT] = ENTRY("hand/left/point"),
  [DEVICE_HAND_RIGHT_POINT] = ENTRY("hand/right/point"),
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
  [BUTTON_PROXIMITY] = ENTRY("proximity"),
  { 0 }
};

StringEntry lovrDeviceAxis[] = {
  [AXIS_TRIGGER] = ENTRY("trigger"),
  [AXIS_THUMBSTICK] = ENTRY("thumbstick"),
  [AXIS_TOUCHPAD] = ENTRY("touchpad"),
  [AXIS_GRIP] = ENTRY("grip"),
  { 0 }
};

static void renderHelper(void* userdata) {
  lua_State* L = userdata;
  if (lua_isfunction(L, -1)) {
    lua_call(L, 0, 0);
  }
}

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
  lovrHeadsetInterface->start();
  return 0;
}

static int l_lovrHeadsetGetDriver(lua_State* L) {
  luax_pushenum(L, HeadsetDriver, lovrHeadsetInterface->driverType);
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

static int l_lovrHeadsetGetOriginType(lua_State* L) {
  luax_pushenum(L, HeadsetOrigin, lovrHeadsetInterface->getOriginType());
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

static int l_lovrHeadsetGetDisplayFrequency(lua_State* L) {
  float frequency = lovrHeadsetInterface->getDisplayFrequency ? lovrHeadsetInterface->getDisplayFrequency() : 0.f;
  if (frequency == 0.f) {
    lua_pushnil(L);
  } else {
    lua_pushnumber(L, frequency);
  }
  return 1;
}

static int l_lovrHeadsetGetDisplayFrequencies(lua_State* L) {
  if (!lovrHeadsetInterface->getDisplayFrequencies) {
    lua_pushnil(L);
    return 1;
  }

  uint32_t count = 0;
  float* frequencies = lovrHeadsetInterface->getDisplayFrequencies(&count);
  if (!frequencies) {
    lua_pushnil(L);
    return 1;
  }

  lua_settop(L, 0);
  lua_createtable(L, count, 0);
  for (uint32_t i = 0; i < count; ++i) {
    lua_pushnumber(L, frequencies[i]);
    lua_rawseti(L, 1, i + 1);
  }

  free(frequencies);

  return 1;
}

static int l_lovrHeadsetSetDisplayFrequency(lua_State* L) {
  float frequency = luax_checkfloat(L, 1);
  bool res = false;
  if (lovrHeadsetInterface->setDisplayFrequency) {
    res = lovrHeadsetInterface->setDisplayFrequency(frequency);
  }
  lua_pushboolean(L, res);
  return 1;
}

static int l_lovrHeadsetGetViewCount(lua_State* L) {
  lua_pushinteger(L, lovrHeadsetInterface->getViewCount());
  return 1;
}

static int l_lovrHeadsetGetViewPose(lua_State* L) {
  float position[4], orientation[4];
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
  float position[4], orientation[4];
  lua_pushboolean(L, lovrHeadsetInterface->getPose(device, position, orientation));
  return 1;
}

static int l_lovrHeadsetGetPose(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[4], orientation[4];
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
  float position[4], orientation[4];
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
  float position[4], orientation[4];
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

static int l_lovrHeadsetGetVelocity(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float velocity[4], angularVelocity[4];
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
  float velocity[4], angularVelocity[4];
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
  return 0;
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
  return 0;
}

static const int axisCounts[MAX_AXES] = {
  [AXIS_TRIGGER] = 1,
  [AXIS_THUMBSTICK] = 2,
  [AXIS_TOUCHPAD] = 2,
  [AXIS_GRIP] = 1
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
  if (lovrHeadsetInterface->getSkeleton(device, poses)) {
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
      lua_pushnumber(L, angle);
      lua_pushnumber(L, ax);
      lua_pushnumber(L, ay);
      lua_pushnumber(L, az);
      lua_rawseti(L, -8, 7);
      lua_rawseti(L, -7, 6);
      lua_rawseti(L, -6, 5);
      lua_rawseti(L, -5, 4);
      lua_rawseti(L, -4, 3);
      lua_rawseti(L, -3, 2);
      lua_rawseti(L, -2, 1);

      lua_rawseti(L, -2, i + 1);
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
  if (lovrHeadsetInterface->vibrate(device, strength, duration, frequency)) {
    lua_pushboolean(L, true);
    return 1;
  }
  lua_pushboolean(L, false);
  return 1;
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
    Model* model = lovrModelCreate(modelData);
    luax_pushtype(L, Model, model);
    lovrRelease(modelData, lovrModelDataDestroy);
    lovrRelease(model, lovrModelDestroy);
    return 1;
  }

  return 0;
}

static int l_lovrHeadsetAnimate(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  Model* model = luax_checktype(L, 2, Model);
  if (lovrHeadsetInterface->animate(device, model)) {
    lua_pushboolean(L, true);
    return 1;
  }
  lua_pushboolean(L, false);
  return 1;
}

static int l_lovrHeadsetRenderTo(lua_State* L) {
  lua_settop(L, 1);
  lovrHeadsetInterface->renderTo(renderHelper, L);
  lovrGraphicsSetViewMatrix(0, NULL);
  lovrGraphicsSetViewMatrix(1, NULL);
  lovrGraphicsSetProjection(0, NULL);
  lovrGraphicsSetProjection(1, NULL);
  return 0;
}

static int l_lovrHeadsetIsFocused(lua_State* L) {
  lua_pushboolean(L, lovrHeadsetInterface->isFocused());
  return 1;
}

static int l_lovrHeadsetUpdate(lua_State* L) {
  double dt = 0.;

  if (lovrHeadsetInterface->update) {
    dt = lovrHeadsetInterface->update();
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

static int l_lovrHeadsetGetMirrorTexture(lua_State* L) {
  if (lovrHeadsetInterface->getMirrorTexture) {
    Texture* texture = lovrHeadsetInterface->getMirrorTexture();
    luax_pushtype(L, Texture, texture);
    return 1;
  }
  return 0;
}

static int l_lovrHeadsetGetHands(lua_State* L) {
  if (lua_istable(L, 1)) {
    lua_settop(L, 1);
  } else {
    lua_newtable(L);
  }

  int count = 0;
  float position[4], orientation[4];
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

static const luaL_Reg lovrHeadset[] = {
  { "start", l_lovrHeadsetStart },
  { "getDriver", l_lovrHeadsetGetDriver },
  { "getName", l_lovrHeadsetGetName },
  { "getOriginType", l_lovrHeadsetGetOriginType },
  { "getDisplayWidth", l_lovrHeadsetGetDisplayWidth },
  { "getDisplayHeight", l_lovrHeadsetGetDisplayHeight },
  { "getDisplayDimensions", l_lovrHeadsetGetDisplayDimensions },
  { "getDisplayFrequency", l_lovrHeadsetGetDisplayFrequency },
  { "getDisplayFrequencies", l_lovrHeadsetGetDisplayFrequencies },
  { "setDisplayFrequency", l_lovrHeadsetSetDisplayFrequency },
  { "getViewCount", l_lovrHeadsetGetViewCount },
  { "getViewPose", l_lovrHeadsetGetViewPose },
  { "getViewAngles", l_lovrHeadsetGetViewAngles },
  { "getClipDistance", l_lovrHeadsetGetClipDistance },
  { "setClipDistance", l_lovrHeadsetSetClipDistance },
  { "getBoundsWidth", l_lovrHeadsetGetBoundsWidth },
  { "getBoundsDepth", l_lovrHeadsetGetBoundsDepth },
  { "getBoundsDimensions", l_lovrHeadsetGetBoundsDimensions },
  { "getBoundsGeometry", l_lovrHeadsetGetBoundsGeometry },
  { "isTracked", l_lovrHeadsetIsTracked },
  { "getPose", l_lovrHeadsetGetPose },
  { "getPosition", l_lovrHeadsetGetPosition },
  { "getOrientation", l_lovrHeadsetGetOrientation },
  { "getVelocity", l_lovrHeadsetGetVelocity },
  { "getAngularVelocity", l_lovrHeadsetGetAngularVelocity },
  { "isDown", l_lovrHeadsetIsDown },
  { "wasPressed", l_lovrHeadsetWasPressed },
  { "wasReleased", l_lovrHeadsetWasReleased },
  { "isTouched", l_lovrHeadsetIsTouched },
  { "getAxis", l_lovrHeadsetGetAxis },
  { "vibrate", l_lovrHeadsetVibrate },
  { "newModel", l_lovrHeadsetNewModel },
  { "animate", l_lovrHeadsetAnimate },
  { "getSkeleton", l_lovrHeadsetGetSkeleton },
  { "renderTo", l_lovrHeadsetRenderTo },
  { "isFocused", l_lovrHeadsetIsFocused },
  { "update", l_lovrHeadsetUpdate },
  { "getTime", l_lovrHeadsetGetTime },
  { "getDeltaTime", l_lovrHeadsetGetDeltaTime },
  { "getMirrorTexture", l_lovrHeadsetGetMirrorTexture },
  { "getHands", l_lovrHeadsetGetHands },
  { NULL, NULL }
};

int luaopen_lovr_headset(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrHeadset);

  size_t driverCount = 0;
  HeadsetDriver drivers[8];
  float supersample = 1.f;
  float offset = 1.7f;
  int msaa = 4;
  bool overlay = false;

  luax_pushconf(L);
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "headset");
    if (lua_istable(L, -1)) {

      // Drivers
      lua_getfield(L, -1, "drivers");
      int n = luax_len(L, -1);
      for (int i = 0; i < n; i++) {
        lua_rawgeti(L, -1, i + 1);
        drivers[driverCount++] = luax_checkenum(L, -1, HeadsetDriver, NULL);
        lovrAssert(driverCount < COUNTOF(drivers), "Too many headset drivers specified in conf.lua");
        lua_pop(L, 1);
      }
      lua_pop(L, 1);

      // Supersample
      lua_getfield(L, -1, "supersample");
      if (lua_type(L, -1) == LUA_TBOOLEAN) {
        supersample = lua_toboolean(L, -1) ? 2.f : 1.f;
      } else {
        supersample = luax_optfloat(L, -1, 1.f);
      }
      lua_pop(L, 1);

      // Offset
      lua_getfield(L, -1, "offset");
      offset = luax_optfloat(L, -1, 1.7f);
      lua_pop(L, 1);

      // MSAA
      lua_getfield(L, -1, "msaa");
      msaa = luaL_optinteger(L, -1, 4);
      lua_pop(L, 1);

      // Overlay
      lua_getfield(L, -1, "overlay");
      overlay = lua_toboolean(L, -1);
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  luax_atexit(L, lovrHeadsetDestroy);
  lovrHeadsetInit(drivers, driverCount, supersample, offset, msaa, overlay);
  return 1;
}
