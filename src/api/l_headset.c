#include "api.h"
#include "headset/headset.h"
#include "data/modelData.h"
#include "graphics/model.h"
#include "graphics/texture.h"
#include "core/arr.h"
#include "core/maf.h"
#include "core/ref.h"
#include <stdlib.h>

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
  [DEVICE_HAND_LEFT] = "hand/left",
  [DEVICE_HAND_RIGHT] = "hand/right",
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
  [AXIS_GRIP] = "grip",
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

static int l_lovrHeadsetGetDisplayFrequency(lua_State* L) {
  float frequency = lovrHeadsetDriver->getDisplayFrequency ? lovrHeadsetDriver->getDisplayFrequency() : 0.f;
  if (frequency == 0.f) {
    lua_pushnil(L);
  } else {
    lua_pushnumber(L, frequency);
  }
  return 1;
}

static int l_lovrHeadsetGetDisplayMask(lua_State* L) {
  uint32_t count;
  const float* points = lovrHeadsetDriver->getDisplayMask(&count);

  if (!points) {
    lua_pushnil(L);
    return 1;
  }

  lua_createtable(L, count, 0);
  for (uint32_t i = 0; i < count; i += 2) {
    lua_createtable(L, 2, 0);

    lua_pushnumber(L, points[i + 0]);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, points[i + 1]);
    lua_rawseti(L, -2, 2);

    lua_rawseti(L, -2, i / 2 + 1);
  }

  return 1;
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
  uint32_t count;
  const float* points = lovrHeadsetDriver->getBoundsGeometry(&count);

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
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getPose(device, position, orientation)) {
      lua_pushboolean(L, true);
      return 1;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

static int l_lovrHeadsetGetPose(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[4], orientation[4];
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
  for (int i = 0; i < 7; i++) {
    lua_pushnumber(L, 0.);
  }
  return 7;
}

static int l_lovrHeadsetGetPosition(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[4], orientation[4];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getPose(device, position, orientation)) {
      lua_pushnumber(L, position[0]);
      lua_pushnumber(L, position[1]);
      lua_pushnumber(L, position[2]);
      return 3;
    }
  }
  for (int i = 0; i < 3; i++) {
    lua_pushnumber(L, 0.);
  }
  return 3;
}

static int l_lovrHeadsetGetOrientation(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float position[4], orientation[4];
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
  for (int i = 0; i < 4; i++) {
    lua_pushnumber(L, 0.);
  }
  return 4;
}

static int l_lovrHeadsetGetVelocity(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float velocity[4], angularVelocity[4];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getVelocity(device, velocity, angularVelocity)) {
      lua_pushnumber(L, velocity[0]);
      lua_pushnumber(L, velocity[1]);
      lua_pushnumber(L, velocity[2]);
      return 3;
    }
  }
  for (int i = 0; i < 3; i++) {
    lua_pushnumber(L, 0.);
  }
  return 3;
}

static int l_lovrHeadsetGetAngularVelocity(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  float velocity[4], angularVelocity[4];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getVelocity(device, velocity, angularVelocity)) {
      lua_pushnumber(L, angularVelocity[0]);
      lua_pushnumber(L, angularVelocity[1]);
      lua_pushnumber(L, angularVelocity[2]);
      return 3;
    }
  }
  for (int i = 0; i < 3; i++) {
    lua_pushnumber(L, 0.);
  }
  return 3;
}

static int l_lovrHeadsetIsDown(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceButton button = luaL_checkoption(L, 2, NULL, DeviceButtons);
  bool down, changed;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->isDown(device, button, &down, &changed)) {
      lua_pushboolean(L, down);
      return 1;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

static int l_lovrHeadsetWasPressed(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceButton button = luaL_checkoption(L, 2, NULL, DeviceButtons);
  bool down, changed;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->isDown(device, button, &down, &changed)) {
      lua_pushboolean(L, down && changed);
      return 1;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

static int l_lovrHeadsetWasReleased(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceButton button = luaL_checkoption(L, 2, NULL, DeviceButtons);
  bool down, changed;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->isDown(device, button, &down, &changed)) {
      lua_pushboolean(L, !down && changed);
      return 1;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

static int l_lovrHeadsetIsTouched(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceButton button = luaL_checkoption(L, 2, NULL, DeviceButtons);
  bool touched;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->isTouched(device, button, &touched)) {
      lua_pushboolean(L, touched);
      return 1;
    }
  }
  lua_pushboolean(L, 1);
  return false;
}

static const int axisCounts[MAX_AXES] = {
  [AXIS_TRIGGER] = 1,
  [AXIS_THUMBSTICK] = 2,
  [AXIS_TOUCHPAD] = 2,
  [AXIS_GRIP] = 1
};

static int l_lovrHeadsetGetAxis(lua_State* L) {
  Device device = luax_optdevice(L, 1);
  DeviceAxis axis = luaL_checkoption(L, 2, NULL, DeviceAxes);
  int count = axisCounts[axis];
  float value[4];
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getAxis(device, axis, value)) {
      for (int i = 0; i < count; i++) {
        lua_pushnumber(L, value[i]);
      }
      return count;
    }
  }
  for (int i = 0; i < count; i++) {
    lua_pushnumber(L, 0.);
  }
  return count;
}

static int l_lovrHeadsetVibrate(lua_State* L) {
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

static int l_lovrHeadsetNewModel(lua_State* L) {
  Device device = luax_optdevice(L, 1);

  ModelData* modelData = NULL;
  FOREACH_TRACKING_DRIVER(driver) {
    if ((modelData = driver->newModelData(device)) != NULL) {
      break;
    }
  }

  if (modelData) {
    Model* model = lovrModelCreate(modelData);
    luax_pushtype(L, Model, model);
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
  luax_pushtype(L, Texture, texture);

  return 1;
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
  for (size_t i = 0; i < sizeof(hands) / sizeof(hands[0]); i++) {
    FOREACH_TRACKING_DRIVER(driver) {
      if (driver->getPose(hands[i], position, orientation)) {
        lua_pushstring(L, Devices[hands[i]]);
        lua_rawseti(L, -2, ++count);
      }
    }
  }
  lua_pushnil(L);
  lua_rawseti(L, -2, ++count);
  return 1;
}

static const luaL_Reg lovrHeadset[] = {
  { "getDriver", l_lovrHeadsetGetDriver },
  { "getName", l_lovrHeadsetGetName },
  { "getOriginType", l_lovrHeadsetGetOriginType },
  { "getDisplayWidth", l_lovrHeadsetGetDisplayWidth },
  { "getDisplayHeight", l_lovrHeadsetGetDisplayHeight },
  { "getDisplayDimensions", l_lovrHeadsetGetDisplayDimensions },
  { "getDisplayFrequency", l_lovrHeadsetGetDisplayFrequency },
  { "getDisplayMask", l_lovrHeadsetGetDisplayMask },
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
  { "renderTo", l_lovrHeadsetRenderTo },
  { "update", l_lovrHeadsetUpdate },
  { "getMirrorTexture", l_lovrHeadsetGetMirrorTexture },
  { "getHands", l_lovrHeadsetGetHands },
  { NULL, NULL }
};

int luaopen_lovr_headset(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrHeadset);

  luax_pushconf(L);
  lua_getfield(L, -1, "headset");

  size_t driverCount = 0;
  HeadsetDriver drivers[8];
  float offset = 1.7f;
  int msaa = 4;

  if (lua_istable(L, -1)) {

    // Drivers
    lua_getfield(L, -1, "drivers");
    int n = luax_len(L, -1);
    for (int i = 0; i < n; i++) {
      lua_rawgeti(L, -1, i + 1);
      drivers[driverCount++] = luaL_checkoption(L, -1, NULL, HeadsetDrivers);
      lovrAssert(driverCount < sizeof(drivers) / sizeof(drivers[0]), "Too many headset drivers specified in conf.lua");
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

  if (lovrHeadsetInit(drivers, driverCount, offset, msaa)) {
    luax_atexit(L, lovrHeadsetDestroy);
  }

  lua_pop(L, 2);
  headsetRenderData.ref = LUA_NOREF;
  return 1;
}
