#include "api.h"
#include "api/math.h"
#include "headset/headset.h"
#include "data/modelData.h"
#include "graphics/model.h"
#include "graphics/texture.h"
#include "lib/maf.h"

#if defined(EMSCRIPTEN) || defined(LOVR_USE_OCULUS_MOBILE)
#define LOVR_HEADSET_HELPER_USES_REGISTRY
#endif

const char* HeadsetDrivers[] = {
  [DRIVER_DESKTOP] = "desktop",
  [DRIVER_LEAP_MOTION] = "leap",
  [DRIVER_OCULUS] = "oculus",
  [DRIVER_OCULUS_MOBILE] = "oculusmobile",
  [DRIVER_OPENVR] = "openvr",
  [DRIVER_OPENXR] = "openxr",
  [DRIVER_WEBVR] = "webvr",
  NULL
};

const char* HeadsetOrigins[] = {
  [ORIGIN_HEAD] = "head",
  [ORIGIN_FLOOR] = "floor",
  NULL
};

const char* Devices[] = {
  [DEVICE_HEAD] = "head",
  [DEVICE_HAND] = "hand",
  [DEVICE_HAND_LEFT] = "hand/left",
  [DEVICE_HAND_RIGHT] = "hand/right",
  [DEVICE_EYE_LEFT] = "eye/left",
  [DEVICE_EYE_RIGHT] = "eye/left",
  [DEVICE_TRACKER_1] = "tracker/1",
  [DEVICE_TRACKER_2] = "tracker/2",
  [DEVICE_TRACKER_3] = "tracker/3",
  [DEVICE_TRACKER_4] = "tracker/4",
  NULL
};

const char* DeviceButtons[] = {
  [BUTTON_TRIGGER] = "trigger",
  [BUTTON_THUMBSTICK] = "thumbstick",
  [BUTTON_TOUCHPAD] = "touchpad",
  [BUTTON_GRIP] = "grip",
  [BUTTON_MENU] = "menu",
  [BUTTON_A] = "a",
  [BUTTON_B] = "b",
  [BUTTON_X] = "x",
  [BUTTON_Y] = "y",
  [BUTTON_PROXIMITY] = "proximity",
  NULL
};

const char* DeviceAxes[] = {
  [AXIS_TRIGGER] = "trigger",
  [AXIS_THUMBSTICK] = "thumbstick",
  [AXIS_TOUCHPAD] = "touchpad",
  [AXIS_PINCH] = "pinch",
  [AXIS_GRIP] = "grip",
  NULL
};

const char* DeviceBones[] = {
  [BONE_THUMB] = "thumb",
  [BONE_INDEX] = "index",
  [BONE_MIDDLE] = "middle",
  [BONE_RING] = "ring",
  [BONE_PINKY] = "pinky",
  [BONE_THUMB_NULL] = "thumb/null",
  [BONE_THUMB_1] = "thumb/1",
  [BONE_THUMB_2] = "thumb/2",
  [BONE_THUMB_3] = "thumb/3",
  [BONE_INDEX_1] = "index/1",
  [BONE_INDEX_2] = "index/2",
  [BONE_INDEX_3] = "index/3",
  [BONE_INDEX_4] = "index/4",
  [BONE_MIDDLE_1] = "middle/1",
  [BONE_MIDDLE_2] = "middle/2",
  [BONE_MIDDLE_3] = "middle/3",
  [BONE_MIDDLE_4] = "middle/4",
  [BONE_RING_1] = "ring/1",
  [BONE_RING_2] = "ring/2",
  [BONE_RING_3] = "ring/3",
  [BONE_RING_4] = "ring/4",
  [BONE_PINKY_1] = "pinky/1",
  [BONE_PINKY_2] = "pinky/2",
  [BONE_PINKY_3] = "pinky/3",
  [BONE_PINKY_4] = "pinky/4",
  NULL
};

typedef struct {
  lua_State* L;
  int ref;
} HeadsetRenderData;

static HeadsetRenderData headsetRenderData;

static void renderHelper(void* userdata) {
  HeadsetRenderData* renderData = userdata;
  lua_State* L = renderData->L;
#ifdef LOVR_HEADSET_HELPER_USES_REGISTRY
  luax_geterror(L);
  if (lua_isnil(L, -1)) {
    lua_pushcfunction(L, luax_getstack);
    lua_rawgeti(L, LUA_REGISTRYINDEX, renderData->ref);
    if (lua_pcall(L, 0, 0, -2)) {
      luax_seterror(L);
    }
    lua_pop(L, 1); // pop luax_getstack
  }
  lua_pop(L, 1);
#else
  lua_call(L, 0, 0);
#endif
}

static Device luax_optdevice(lua_State* L, int index) {
  const char* str = luaL_optstring(L, 1, "head");
  if (!strcmp(str, "left")) {
    return DEVICE_HAND_LEFT;
  } else if (!strcmp(str, "right")) {
    return DEVICE_HAND_RIGHT;
  }
  return luaL_checkoption(L, 1, "head", Devices);
}

static int l_lovrHeadsetGetDriver(lua_State* L) {
  if (lua_gettop(L) == 0) {
    lua_pushstring(L, HeadsetDrivers[lovrHeadsetDriver->driverType]);
    return 1;
  } else {
    Device device = luax_optdevice(L, 1);
    FOREACH_TRACKING_DRIVER(driver) {
      if (driver->getPose(device, NULL, NULL)) {
        lua_pushstring(L, HeadsetDrivers[driver->driverType]);
        return 1;
      }
    }
  }
  return 0;
}

static int l_lovrHeadsetGetName(lua_State* L) {
  char name[256];
  if (lovrHeadsetDriver->getName(name, sizeof(name))) {
    lua_pushstring(L, name);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrHeadsetGetOriginType(lua_State* L) {
  lua_pushstring(L, HeadsetOrigins[lovrHeadsetDriver->getOriginType()]);
  return 1;
}

static int l_lovrHeadsetGetDisplayWidth(lua_State* L) {
  uint32_t width, height;
  lovrHeadsetDriver->getDisplayDimensions(&width, &height);
  lua_pushinteger(L, width);
  return 1;
}

static int l_lovrHeadsetGetDisplayHeight(lua_State* L) {
  uint32_t width, height;
  lovrHeadsetDriver->getDisplayDimensions(&width, &height);
  lua_pushinteger(L, height);
  return 1;
}

static int l_lovrHeadsetGetDisplayDimensions(lua_State* L) {
  uint32_t width, height;
  lovrHeadsetDriver->getDisplayDimensions(&width, &height);
  lua_pushinteger(L, width);
  lua_pushinteger(L, height);
  return 2;
}

static int l_lovrHeadsetGetClipDistance(lua_State* L) {
  float clipNear, clipFar;
  lovrHeadsetDriver->getClipDistance(&clipNear, &clipFar);
  lua_pushnumber(L, clipNear);
  lua_pushnumber(L, clipFar);
  return 2;
}

static int l_lovrHeadsetSetClipDistance(lua_State* L) {
  float clipNear = luax_checkfloat(L, 1);
  float clipFar = luax_checkfloat(L, 2);
  lovrHeadsetDriver->setClipDistance(clipNear, clipFar);
  return 0;
}

static int l_lovrHeadsetGetBoundsWidth(lua_State* L) {
  float width, depth;
  lovrHeadsetDriver->getBoundsDimensions(&width, &depth);
  lua_pushnumber(L, width);
  return 1;
}

static int l_lovrHeadsetGetBoundsDepth(lua_State* L) {
  float width, depth;
  lovrHeadsetDriver->getBoundsDimensions(&width, &depth);
  lua_pushnumber(L, depth);
  return 1;
}

static int l_lovrHeadsetGetBoundsDimensions(lua_State* L) {
  float width, depth;
  lovrHeadsetDriver->getBoundsDimensions(&width, &depth);
  lua_pushnumber(L, width);
  lua_pushnumber(L, depth);
  return 2;
}

static int l_lovrHeadsetGetBoundsGeometry(lua_State* L) {
  int count;
  const float* points = lovrHeadsetDriver->getBoundsGeometry(&count);

  if (!points) {
    lua_pushnil(L);
    return 1;
  }

  if (lua_type(L, 1) == LUA_TTABLE) {
    lua_settop(L, 1);
  } else {
    lua_settop(L, 0);
    lua_createtable(L, count, 0);
  }

  for (int i = 0; i < count; i++) {
    lua_pushnumber(L, points[i]);
    lua_rawseti(L, 1, i + 1);
  }

  return 1;
}

int l_lovrHeadsetGetPose(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[3], orientation[4];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getPose(device, position, orientation)) {
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
  }
  return 0;
}

int l_lovrHeadsetGetPosition(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[3], orientation[4];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getPose(device, position, orientation)) {
      lua_pushnumber(L, position[0]);
      lua_pushnumber(L, position[1]);
      lua_pushnumber(L, position[2]);
      return 3;
    }
  }
  return 0;
}

int l_lovrHeadsetGetOrientation(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[3], orientation[4];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getPose(device, position, orientation)) {
      float angle, ax, ay, az;
      quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
      lua_pushnumber(L, angle);
      lua_pushnumber(L, ax);
      lua_pushnumber(L, ay);
      lua_pushnumber(L, az);
      return 4;
    }
  }
  return 0;
}

int l_lovrHeadsetGetDirection(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[3], orientation[4];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getPose(device, position, orientation)) {
      float v[3] = { 0.f, 0.f, -1.f };
      quat_rotate(orientation, v);
      lua_pushnumber(L, v[0]);
      lua_pushnumber(L, v[1]);
      lua_pushnumber(L, v[2]);
      return 3;
    }
  }
  return 0;
}

int l_lovrHeadsetGetBonePose(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceBone bone = luaL_checkoption(L, 2, NULL, DeviceBones);
  float position[3], orientation[4];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getBonePose(device, bone, position, orientation)) {
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
  }
  return 0;
}

int l_lovrHeadsetGetVelocity(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float velocity[3], angularVelocity[3];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getVelocity(device, velocity, angularVelocity)) {
      lua_pushnumber(L, velocity[0]);
      lua_pushnumber(L, velocity[1]);
      lua_pushnumber(L, velocity[2]);
      return 3;
    }
  }
  return 0;
}

int l_lovrHeadsetGetAngularVelocity(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float velocity[3], angularVelocity[3];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getVelocity(device, velocity, angularVelocity)) {
      lua_pushnumber(L, angularVelocity[0]);
      lua_pushnumber(L, angularVelocity[1]);
      lua_pushnumber(L, angularVelocity[2]);
      return 3;
    }
  }
  return 0;
}

int l_lovrHeadsetGetAcceleration(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float acceleration[3], angularAcceleration[3];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getAcceleration(device, acceleration, angularAcceleration)) {
      lua_pushnumber(L, acceleration[0]);
      lua_pushnumber(L, acceleration[1]);
      lua_pushnumber(L, acceleration[2]);
      return 3;
    }
  }
  return 0;
}

int l_lovrHeadsetGetAngularAcceleration(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float acceleration[3], angularAcceleration[3];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getAcceleration(device, acceleration, angularAcceleration)) {
      lua_pushnumber(L, angularAcceleration[0]);
      lua_pushnumber(L, angularAcceleration[1]);
      lua_pushnumber(L, angularAcceleration[2]);
      return 3;
    }
  }
  return 0;
}

int l_lovrHeadsetIsDown(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceButton button = luaL_checkoption(L, 2, NULL, DeviceButtons);
  bool down;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->isDown(device, button, &down)) {
      lua_pushboolean(L, down);
      return 1;
    }
  }
  return 0;
}

int l_lovrHeadsetIsTouched(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceButton button = luaL_checkoption(L, 2, NULL, DeviceButtons);
  bool touched;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->isDown(device, button, &touched)) {
      lua_pushboolean(L, touched);
      return 1;
    }
  }
  return 0;
}

int l_lovrHeadsetGetAxis(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceAxis axis = luaL_checkoption(L, 2, NULL, DeviceAxes);
  float value[3];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getAxis(device, axis, value)) {
      switch (axis) {
        case MAX_AXES:
        case AXIS_TRIGGER:
        case AXIS_PINCH:
        case AXIS_GRIP:
          lua_pushnumber(L, value[0]);
          return 1;
        case AXIS_THUMBSTICK:
        case AXIS_TOUCHPAD:
          lua_pushnumber(L, value[0]);
          lua_pushnumber(L, value[1]);
          return 2;
      }
    }
  }
  return 0;
}

int l_lovrHeadsetVibrate(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float strength = luax_optfloat(L, 2, 1.f);
  float duration = luax_optfloat(L, 3, .5f);
  float frequency = luax_optfloat(L, 4, 0.f);
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->vibrate(device, strength, duration, frequency)) {
      lua_pushboolean(L, true);
      return 1;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

int l_lovrHeadsetNewModel(lua_State* L) {
  Device device = luax_optdevice(L, 1);

  ModelData* modelData = NULL;
  FOREACH_TRACKING_DRIVER(driver) {
    if ((modelData = driver->newModelData(device)) != NULL) {
      break;
    }
  }

  if (modelData) {
    Model* model = lovrModelCreate(modelData);
    luax_pushobject(L, model);
    lovrRelease(ModelData, modelData);
    lovrRelease(Model, model);
    return 1;
  }

  return 0;
}

static int l_lovrHeadsetRenderTo(lua_State* L) {
  lua_settop(L, 1);
  luaL_checktype(L, 1, LUA_TFUNCTION);

#ifdef LOVR_HEADSET_HELPER_USES_REGISTRY
  if (headsetRenderData.ref != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, headsetRenderData.ref);
  }

  headsetRenderData.ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  headsetRenderData.L = lua_tothread(L, -1);
  lua_pop(L, 1);
#else
  headsetRenderData.L = L;
#endif
  lovrHeadsetDriver->renderTo(renderHelper, &headsetRenderData);
  return 0;
}

static int l_lovrHeadsetUpdate(lua_State* L) {
  float dt = luax_checkfloat(L, 1);

  if (lovrHeadsetDriver->update) {
    lovrHeadsetDriver->update(dt);
  }

  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->update && driver != lovrHeadsetDriver) {
      driver->update(dt);
    }
  }

  return 0;
}

static int l_lovrHeadsetGetMirrorTexture(lua_State* L) {
  Texture *texture = NULL;
  if (lovrHeadsetDriver->getMirrorTexture)
    texture = lovrHeadsetDriver->getMirrorTexture();
  luax_pushobject(L, texture);

  return 1;
}

static int deviceIterator(lua_State* L) {
  size_t index = lua_tointeger(L, lua_upvalueindex(1));
  Device* devices = (Device*) lua_touserdata(L, lua_upvalueindex(2));
  size_t count = lua_tointeger(L, lua_upvalueindex(3));
  float position[3], orientation[4];

  while (index < count) {
    FOREACH_TRACKING_DRIVER(driver) {
      if (driver->getPose(devices[index], position, orientation)) {
        lua_pushstring(L, Devices[devices[index]]);
        lua_pushinteger(L, ++index);
        lua_replace(L, lua_upvalueindex(1));
        return 1;
      }
    }
    index++;
  }

  return 0;
}

static Device hands[] = {
  DEVICE_HAND,
  DEVICE_HAND_LEFT,
  DEVICE_HAND_RIGHT
};

static Device trackers[] = {
  DEVICE_TRACKER_1,
  DEVICE_TRACKER_2,
  DEVICE_TRACKER_3,
  DEVICE_TRACKER_4
};

static int l_lovrHeadsetHands(lua_State* L) {
  lua_pushinteger(L, 0);
  lua_pushlightuserdata(L, hands);
  lua_pushinteger(L, sizeof(hands) / sizeof(hands[0]));
  lua_pushcclosure(L, deviceIterator, 3);
  return 1;
}

static int l_lovrHeadsetTrackers(lua_State* L) {
  lua_pushinteger(L, 0);
  lua_pushlightuserdata(L, trackers);
  lua_pushinteger(L, sizeof(trackers) / sizeof(trackers[0]));
  lua_pushcclosure(L, deviceIterator, 3);
  return 1;
}

static const luaL_Reg lovrHeadset[] = {
  { "getDriver", l_lovrHeadsetGetDriver },
  { "getName", l_lovrHeadsetGetName },
  { "getOriginType", l_lovrHeadsetGetOriginType },
  { "getDisplayWidth", l_lovrHeadsetGetDisplayWidth },
  { "getDisplayHeight", l_lovrHeadsetGetDisplayHeight },
  { "getDisplayDimensions", l_lovrHeadsetGetDisplayDimensions },
  { "getClipDistance", l_lovrHeadsetGetClipDistance },
  { "setClipDistance", l_lovrHeadsetSetClipDistance },
  { "getBoundsWidth", l_lovrHeadsetGetBoundsWidth },
  { "getBoundsDepth", l_lovrHeadsetGetBoundsDepth },
  { "getBoundsDimensions", l_lovrHeadsetGetBoundsDimensions },
  { "getBoundsGeometry", l_lovrHeadsetGetBoundsGeometry },
  { "getPose", l_lovrHeadsetGetPose },
  { "getPosition", l_lovrHeadsetGetPosition },
  { "getOrientation", l_lovrHeadsetGetOrientation },
  { "getDirection", l_lovrHeadsetGetDirection },
  { "getBonePose", l_lovrHeadsetGetBonePose },
  { "getVelocity", l_lovrHeadsetGetVelocity },
  { "getAngularVelocity", l_lovrHeadsetGetAngularVelocity },
  { "getAcceleration", l_lovrHeadsetGetAcceleration },
  { "getAngularAcceleration", l_lovrHeadsetGetAngularAcceleration },
  { "isDown", l_lovrHeadsetIsDown },
  { "isTouched", l_lovrHeadsetIsTouched },
  { "getAxis", l_lovrHeadsetGetAxis },
  { "vibrate", l_lovrHeadsetVibrate },
  { "newModel", l_lovrHeadsetNewModel },
  { "renderTo", l_lovrHeadsetRenderTo },
  { "update", l_lovrHeadsetUpdate },
  { "getMirrorTexture", l_lovrHeadsetGetMirrorTexture },
  { "hands", l_lovrHeadsetHands },
  { "trackers", l_lovrHeadsetTrackers },
  { NULL, NULL }
};

int luaopen_lovr_headset(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrHeadset);

  luax_pushconf(L);
  lua_getfield(L, -1, "headset");

  vec_t(HeadsetDriver) drivers;
  vec_init(&drivers);
  float offset = 1.7f;
  int msaa = 4;

  if (lua_istable(L, -1)) {

    // Drivers
    lua_getfield(L, -1, "drivers");
    int n = luax_len(L, -1);
    for (int i = 0; i < n; i++) {
      lua_rawgeti(L, -1, i + 1);
      vec_push(&drivers, luaL_checkoption(L, -1, NULL, HeadsetDrivers));
      lua_pop(L, 1);
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
  }

  if (lovrHeadsetInit(drivers.data, drivers.length, offset, msaa)) {
    luax_atexit(L, lovrHeadsetDestroy);
  }

  vec_deinit(&drivers);
  lua_pop(L, 2);

  headsetRenderData.ref = LUA_NOREF;

  return 1;
}
