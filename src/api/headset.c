#include "api.h"
#include "api/headset.h"
#include "api/math.h"
#include "headset/headset.h"
#include "data/modelData.h"
#include "graphics/model.h"
#include "graphics/texture.h"
#include "lib/math.h"

#if defined(EMSCRIPTEN) || defined(LOVR_USE_OCULUS_MOBILE)
#define LOVR_HEADSET_HELPER_USES_REGISTRY
#endif

const char* ControllerAxes[] = {
  [CONTROLLER_AXIS_TRIGGER] = "trigger",
  [CONTROLLER_AXIS_GRIP] = "grip",
  [CONTROLLER_AXIS_TOUCHPAD_X] = "touchx",
  [CONTROLLER_AXIS_TOUCHPAD_Y] = "touchy",
  NULL
};

const char* ControllerButtons[] = {
  [CONTROLLER_BUTTON_SYSTEM] = "system",
  [CONTROLLER_BUTTON_MENU] = "menu",
  [CONTROLLER_BUTTON_TRIGGER] = "trigger",
  [CONTROLLER_BUTTON_GRIP] = "grip",
  [CONTROLLER_BUTTON_TOUCHPAD] = "touchpad",
  [CONTROLLER_BUTTON_A] = "a",
  [CONTROLLER_BUTTON_B] = "b",
  [CONTROLLER_BUTTON_X] = "x",
  [CONTROLLER_BUTTON_Y] = "y",
  NULL
};

const char* ControllerHands[] = {
  [HAND_UNKNOWN] = "unknown",
  [HAND_LEFT] = "left",
  [HAND_RIGHT] = "right",
  NULL
};

const char* HeadsetDrivers[] = {
  [DRIVER_DESKTOP] = "desktop",
  [DRIVER_OCULUS] = "oculus",
  [DRIVER_OCULUS_MOBILE] = "oculusmobile",
  [DRIVER_OPENVR] = "openvr",
  [DRIVER_WEBVR] = "webvr",
  NULL
};

const char* HeadsetEyes[] = {
  [EYE_LEFT] = "left",
  [EYE_RIGHT] = "right",
  NULL
};

const char* HeadsetOrigins[] = {
  [ORIGIN_HEAD] = "head",
  [ORIGIN_FLOOR] = "floor",
  NULL
};

const char* HeadsetTypes[] = {
  [HEADSET_UNKNOWN] = "unknown",
  [HEADSET_VIVE] = "vive",
  [HEADSET_RIFT] = "rift",
  [HEADSET_GEAR] = "gear",
  [HEADSET_GO] = "go",
  [HEADSET_WINDOWS_MR] = "windowsmr",
  NULL
};

const char* Subpaths[] = {
  [PATH_NONE] = "",
  [PATH_HEAD] = "head",
  [PATH_HANDS] = "hands",
  [PATH_LEFT] = "left",
  [PATH_RIGHT] = "right",
  [PATH_TRIGGER] = "trigger",
  [PATH_GRIP] = "grip",
  [PATH_TOUCHPAD] = "touchpad"
};

typedef struct {
  lua_State* L;
  int ref;
} HeadsetRenderData;

static HeadsetRenderData headsetRenderData;

Path luax_optpath(lua_State* L, int index, const char* fallback) {
  char* str = (char*) luaL_optstring(L, index, fallback);
  Path path = { { PATH_NONE } };
  int count = 0;

  if (str[0] == '/') {
    str++;
  }

  while (1) {
    char* slash = strchr(str, '/');

    if (slash) {
      *slash = '\0';
    }

    Subpath subpath = PATH_NONE;
    for (size_t i = 0; i < sizeof(Subpaths) / sizeof(Subpaths[0]); i++) {
      if (!strcmp(str, Subpaths[0])) {
        subpath = i;
        break;
      }
    }

    lovrAssert(subpath != PATH_NONE, "Unknown path component '%s'", str);
    path.pieces[count++] = subpath;

    if (slash) {
      *slash = '/';
      str = slash + 1;
    } else {
      break;
    }
  }

  return path;
}

void luax_pushpath(lua_State* L, Path path) {
  for (int i = 0; i < 8; i++) {
    if (path.pieces[i] == PATH_NONE) {
      lua_concat(L, i + 1);
      break;
    }

    lua_pushstring(L, Subpaths[path.pieces[i]]);
  }

  lovrThrow("Unreachable");
}

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

static int l_lovrHeadsetGetDriver(lua_State* L) {
  lua_pushstring(L, HeadsetDrivers[lovrHeadsetDriver->driverType]);
  return 1;
}

static int l_lovrHeadsetGetType(lua_State* L) {
  lua_pushstring(L, HeadsetTypes[lovrHeadsetDriver->getType()]);
  return 1;
}

static int l_lovrHeadsetGetOriginType(lua_State* L) {
  lua_pushstring(L, HeadsetOrigins[lovrHeadsetDriver->getOriginType()]);
  return 1;
}

static int l_lovrHeadsetIsMounted(lua_State* L) {
  lua_pushboolean(L, lovrHeadsetDriver->isMounted());
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
  Path path = luax_optpath(L, 1, "head");
  float x, y, z, angle, ax, ay, az;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getPose(path, &x, &y, &z, &angle, &ax, &ay, &az)) {
      lua_pushnumber(L, x);
      lua_pushnumber(L, y);
      lua_pushnumber(L, z);
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
  Path path = { { PATH_NONE } };
  float position[3], angle, ax, ay, az;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getPose(path, &position[0], &position[1], &position[2], &angle, &ax, &ay, &az)) {
      lua_pushnumber(L, position[0]);
      lua_pushnumber(L, position[1]);
      lua_pushnumber(L, position[2]);
      return 3;
    }
  }
  return 0;
}

int l_lovrHeadsetGetOrientation(lua_State* L) {
  Path path = { { PATH_HEAD } };
  float x, y, z, angle, ax, ay, az;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getPose(path, &x, &y, &z, &angle, &ax, &ay, &az)) {
      lua_pushnumber(L, angle);
      lua_pushnumber(L, ax);
      lua_pushnumber(L, ay);
      lua_pushnumber(L, az);
      return 4;
    }
  }
  return 0;
}

int l_lovrHeadsetGetVelocity(lua_State* L) {
  Path path = { { PATH_HEAD } };
  float vx, vy, vz;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getVelocity(path, &vx, &vy, &vz)) {
      lua_pushnumber(L, vx);
      lua_pushnumber(L, vy);
      lua_pushnumber(L, vz);
      return 3;
    }
  }
  return 0;
}

int l_lovrHeadsetGetAngularVelocity(lua_State* L) {
  Path path = { { PATH_HEAD } };
  float vx, vy, vz;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getAngularVelocity(path, &vx, &vy, &vz)) {
      lua_pushnumber(L, vx);
      lua_pushnumber(L, vy);
      lua_pushnumber(L, vz);
      return 3;
    }
  }
  return 0;
}

int l_lovrHeadsetGetAxis(lua_State* L) {
  Path path = luax_optpath(L, 1, "head");
  float x, y, z;
  int count;
  FOREACH_TRACKING_DRIVER(driver) {
    if ((count = driver->getAxis(path, &x, &y, &z)) > 0) {
      lua_pushnumber(L, x);
      lua_pushnumber(L, y);
      lua_pushnumber(L, z);
      lua_pop(L, 3 - count);
      return count;
    }
  }
  return 0;
}

int l_lovrHeadsetVibrate(lua_State* L) {
  Path path = luax_optpath(L, 1, "head");
  float strength = luax_optfloat(L, 2, 1.f);
  float duration = luax_optfloat(L, 3, .5f);
  float frequency = luax_optfloat(L, 4, 0.f);
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->vibrate(path, strength, duration, frequency)) {
      lua_pushboolean(L, true);
      return 1;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

int l_lovrHeadsetNewModel(lua_State* L) {
  Path path = luax_optpath(L, 1, "head");

  ModelData* modelData = NULL;
  FOREACH_TRACKING_DRIVER(driver) {
    if ((modelData = driver->newModelData(path)) != NULL) {
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

static int l_lovrHeadsetGetControllers(lua_State* L) {
  uint8_t count;
  Controller** controllers = lovrHeadsetDriver->getControllers(&count);
  lua_createtable(L, count, 0);
  for (uint8_t i = 0; i < count; i++) {
    luax_pushobject(L, controllers[i]);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int l_lovrHeadsetGetControllerCount(lua_State* L) {
  uint8_t count;
  lovrHeadsetDriver->getControllers(&count);
  lua_pushnumber(L, count);
  return 1;
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
  if (lovrHeadsetDriver->update) {
    lovrHeadsetDriver->update(luax_checkfloat(L, 1));
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

static const luaL_Reg lovrHeadset[] = {
  { "getDriver", l_lovrHeadsetGetDriver },
  { "getType", l_lovrHeadsetGetType },
  { "getOriginType", l_lovrHeadsetGetOriginType },
  { "isMounted", l_lovrHeadsetIsMounted },
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
  { "getVelocity", l_lovrHeadsetGetVelocity },
  { "getAngularVelocity", l_lovrHeadsetGetAngularVelocity },
  { "getAxis", l_lovrHeadsetGetAxis },
  { "vibrate", l_lovrHeadsetVibrate },
  { "newModel", l_lovrHeadsetNewModel },
  { "getControllers", l_lovrHeadsetGetControllers },
  { "getControllerCount", l_lovrHeadsetGetControllerCount },
  { "renderTo", l_lovrHeadsetRenderTo },
  { "update", l_lovrHeadsetUpdate },
  { "getMirrorTexture", l_lovrHeadsetGetMirrorTexture },
  { NULL, NULL }
};

int luaopen_lovr_headset(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrHeadset);
  luax_registertype(L, Controller);

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
