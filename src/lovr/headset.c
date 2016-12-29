#include "lovr/headset.h"
#include "lovr/types/controller.h"
#include "headset/headset.h"
#include "util.h"

typedef struct RenderData {
  lua_State* L;
  int ref;
} RenderData;

static RenderData renderData;

void renderHelper(int eyeIndex, void* userdata) {
  RenderData* data = (RenderData*) userdata;
  lua_State* L = data->L;
  lua_rawgeti(L, LUA_REGISTRYINDEX, data->ref);
  luaL_checktype(L, -1, LUA_TFUNCTION);
  lua_pushstring(L, eyeIndex == 0 ? "left" : "right");
  lua_call(L, 1, 0);
}

const luaL_Reg lovrHeadset[] = {
  { "isPresent", l_lovrHeadsetIsPresent },
  { "getType", l_lovrHeadsetGetType },
  { "getDisplayWidth", l_lovrHeadsetGetDisplayWidth },
  { "getDisplayHeight", l_lovrHeadsetGetDisplayHeight },
  { "getDisplayDimensions", l_lovrHeadsetGetDisplayDimensions },
  { "getClipDistance", l_lovrHeadsetGetClipDistance },
  { "setClipDistance", l_lovrHeadsetSetClipDistance },
  { "getBoundsWidth", l_lovrHeadsetGetBoundsWidth },
  { "getBoundsDepth", l_lovrHeadsetGetBoundsDepth },
  { "getBoundsDimensions", l_lovrHeadsetGetBoundsDimensions },
  { "getBoundsGeometry", l_lovrHeadsetGetBoundsGeometry },
  { "isBoundsVisible", l_lovrHeadsetIsBoundsVisible },
  { "setBoundsVisible", l_lovrHeadsetSetBoundsVisible },
  { "getPosition", l_lovrHeadsetGetPosition },
  { "getOrientation", l_lovrHeadsetGetOrientation },
  { "getVelocity", l_lovrHeadsetGetVelocity },
  { "getAngularVelocity", l_lovrHeadsetGetAngularVelocity },
  { "getControllers", l_lovrHeadsetGetControllers },
  { "getControllerCount", l_lovrHeadsetGetControllerCount },
  { "renderTo", l_lovrHeadsetRenderTo },
  { NULL, NULL }
};

int l_lovrHeadsetInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrHeadset);
  luax_registertype(L, "Controller", lovrController);

  map_init(&ControllerAxes);
  map_set(&ControllerAxes, "trigger", CONTROLLER_AXIS_TRIGGER);
  map_set(&ControllerAxes, "touchx", CONTROLLER_AXIS_TOUCHPAD_X);
  map_set(&ControllerAxes, "touchy", CONTROLLER_AXIS_TOUCHPAD_Y);

  map_init(&ControllerButtons);
  map_set(&ControllerButtons, "system", CONTROLLER_BUTTON_SYSTEM);
  map_set(&ControllerButtons, "menu", CONTROLLER_BUTTON_MENU);
  map_set(&ControllerButtons, "grip", CONTROLLER_BUTTON_GRIP);
  map_set(&ControllerButtons, "touchpad", CONTROLLER_BUTTON_TOUCHPAD);

  lovrHeadsetInit();

  return 1;
}

int l_lovrHeadsetIsPresent(lua_State* L) {
  lua_pushboolean(L, lovrHeadsetIsPresent());
  return 1;
}

int l_lovrHeadsetGetType(lua_State* L) {
  lua_pushstring(L, lovrHeadsetGetType());
  return 1;
}

int l_lovrHeadsetGetDisplayWidth(lua_State* L) {
  int width;
  lovrHeadsetGetDisplayDimensions(&width, NULL);
  lua_pushnumber(L, width);
  return 1;
}

int l_lovrHeadsetGetDisplayHeight(lua_State* L) {
  int height;
  lovrHeadsetGetDisplayDimensions(NULL, &height);
  lua_pushnumber(L, height);
  return 1;
}

int l_lovrHeadsetGetDisplayDimensions(lua_State* L) {
  int width, height;
  lovrHeadsetGetDisplayDimensions(&width, &height);
  lua_pushnumber(L, width);
  lua_pushnumber(L, height);
  return 2;
}

int l_lovrHeadsetGetClipDistance(lua_State* L) {
  float near, far;
  lovrHeadsetGetClipDistance(&near, &far);
  lua_pushnumber(L, near);
  lua_pushnumber(L, far);
  return 2;
}

int l_lovrHeadsetSetClipDistance(lua_State* L) {
  float near = luaL_checknumber(L, 1);
  float far = luaL_checknumber(L, 2);
  lovrHeadsetSetClipDistance(near, far);
  return 0;
}

int l_lovrHeadsetGetBoundsWidth(lua_State* L) {
  lua_pushnumber(L, lovrHeadsetGetBoundsWidth());
  return 1;
}

int l_lovrHeadsetGetBoundsDepth(lua_State* L) {
  lua_pushnumber(L, lovrHeadsetGetBoundsDepth());
  return 1;
}

int l_lovrHeadsetGetBoundsDimensions(lua_State* L) {
  lua_pushnumber(L, lovrHeadsetGetBoundsWidth());
  lua_pushnumber(L, lovrHeadsetGetBoundsDepth());
  return 2;
}

int l_lovrHeadsetGetBoundsGeometry(lua_State* L) {
  float geometry[12];
  lovrHeadsetGetBoundsGeometry(geometry);
  lua_newtable(L);
  for (int i = 0; i < 4; i++) {
    lua_newtable(L);
    for (int j = 0; j < 3; j++) {
      lua_pushnumber(L, geometry[3 * i + j]);
      lua_rawseti(L, -2, j + 1);
    }
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

int l_lovrHeadsetIsBoundsVisible(lua_State* L) {
  lua_pushboolean(L, lovrHeadsetIsBoundsVisible());
  return 1;
}

int l_lovrHeadsetSetBoundsVisible(lua_State* L) {
  char visible = lua_toboolean(L, 1);
  lovrHeadsetSetBoundsVisible(visible);
  return 0;
}

int l_lovrHeadsetGetPosition(lua_State* L) {
  float x, y, z;
  lovrHeadsetGetPosition(&x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrHeadsetGetOrientation(lua_State* L) {
  float w, x, y, z;
  lovrHeadsetGetOrientation(&w, &x, &y, &z);
  lua_pushnumber(L, w);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 4;
}

int l_lovrHeadsetGetVelocity(lua_State* L) {
  float x, y, z;
  lovrHeadsetGetVelocity(&x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrHeadsetGetAngularVelocity(lua_State* L) {
  float x, y, z;
  lovrHeadsetGetAngularVelocity(&x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrHeadsetGetControllers(lua_State* L) {
  vec_controller_t* controllers = lovrHeadsetGetControllers();
  if (!controllers) {
    lua_newtable(L);
    return 1;
  }

  lua_newtable(L);
  Controller* controller; int i;
  vec_foreach(controllers, controller, i) {
    lovrRetain(&controller->ref);
    luax_pushtype(L, Controller, controller);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

int l_lovrHeadsetGetControllerCount(lua_State* L) {
  vec_controller_t* controllers = lovrHeadsetGetControllers();
  if (controllers) {
    lua_pushnumber(L, controllers->length);
  } else {
    lua_pushnumber(L, 0);
  }
  return 1;
}

int l_lovrHeadsetRenderTo(lua_State* L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  renderData.L = L;
  renderData.ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lovrHeadsetRenderTo(renderHelper, &renderData);
  return 0;
}
