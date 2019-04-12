#include "api.h"
#include "api/headset.h"
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
  [DRIVER_OCULUS] = "oculus",
  [DRIVER_OCULUS_MOBILE] = "oculusmobile",
  [DRIVER_OPENVR] = "openvr",
  [DRIVER_WEBVR] = "webvr",
  NULL
};

const char* HeadsetOrigins[] = {
  [ORIGIN_HEAD] = "head",
  [ORIGIN_FLOOR] = "floor",
  NULL
};

const char* Subpaths[] = {
  [P_NONE] = "",
  [P_HEAD] = "head",
  [P_HAND] = "hand",
  [P_EYE] = "eye",
  [P_LEFT] = "left",
  [P_RIGHT] = "right",
  [P_PROXIMITY] = "proximity",
  [P_TRIGGER] = "trigger",
  [P_TRACKPAD] = "trackpad",
  [P_JOYSTICK] = "joystick",
  [P_MENU] = "menu",
  [P_GRIP] = "grip",
  [P_A] = "a",
  [P_B] = "b",
  [P_X] = "x",
  [P_Y] = "y"
};

typedef struct {
  lua_State* L;
  int ref;
} HeadsetRenderData;

static HeadsetRenderData headsetRenderData;

Path luax_optpath(lua_State* L, int index, const char* fallback) {
  char* str = (char*) luaL_optstring(L, index, fallback);
  Path path = { { P_NONE } };
  int count = 0;

  if (str[0] == '/') {
    str++;
  }

  while (1) {
    char* slash = strchr(str, '/');

    if (slash) {
      *slash = '\0';
    }

    Subpath subpath = P_NONE;
    for (size_t i = 0; i < sizeof(Subpaths) / sizeof(Subpaths[0]); i++) {
      if (!strcmp(str, Subpaths[i])) {
        subpath = i;
        break;
      }
    }

    lovrAssert(subpath != P_NONE, "Unknown path component '%s'", str);
    path.p[count++] = subpath;

    if (slash) {
      *slash = '/';
      str = slash + 1;
    } else {
      break;
    }
  }

  return path;
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

static int l_lovrHeadsetGetName(lua_State* L) {
  lua_pushstring(L, lovrHeadsetDriver->getName());
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
  Path path = luax_optpath(L, 1, "head");
  float x, y, z;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getPose(path, &x, &y, &z, NULL, NULL, NULL, NULL)) {
      lua_pushnumber(L, x);
      lua_pushnumber(L, y);
      lua_pushnumber(L, z);
      return 3;
    }
  }
  return 0;
}

int l_lovrHeadsetGetOrientation(lua_State* L) {
  Path path = luax_optpath(L, 1, "head");
  float angle, ax, ay, az;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->getPose(path, NULL, NULL, NULL, &angle, &ax, &ay, &az)) {
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
  Path path = luax_optpath(L, 1, "head");
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
  Path path = luax_optpath(L, 1, "head");
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

int l_lovrHeadsetIsDown(lua_State* L) {
  Path path = luax_optpath(L, 1, "head");
  bool down;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->isDown(path, &down)) {
      lua_pushboolean(L, down);
      return 1;
    }
  }
  return 0;
}

int l_lovrHeadsetIsTouched(lua_State* L) {
  Path path = luax_optpath(L, 1, "head");
  bool touched;
  FOREACH_TRACKING_DRIVER(driver) {
    if (driver->isDown(path, &touched)) {
      lua_pushboolean(L, touched);
      return 1;
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
  { "getVelocity", l_lovrHeadsetGetVelocity },
  { "getAngularVelocity", l_lovrHeadsetGetAngularVelocity },
  { "isDown", l_lovrHeadsetIsDown },
  { "isTouched", l_lovrHeadsetIsTouched },
  { "getAxis", l_lovrHeadsetGetAxis },
  { "vibrate", l_lovrHeadsetVibrate },
  { "newModel", l_lovrHeadsetNewModel },
  { "renderTo", l_lovrHeadsetRenderTo },
  { "update", l_lovrHeadsetUpdate },
  { "getMirrorTexture", l_lovrHeadsetGetMirrorTexture },
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
