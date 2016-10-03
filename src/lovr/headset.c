#include "headset.h"
#include "controller.h"
#include "../headset/headset.h"

void renderHelper(int eyeIndex, void* userdata) {
  lua_State* L = (lua_State*)userdata;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, eyeIndex);
  lua_call(L, 1, 0);
}

const luaL_Reg lovrHeadset[] = {
  { "isPresent", l_lovrHeadsetIsPresent },
  { "getType", l_lovrHeadsetGetType },
  { "getClipDistance", l_lovrHeadsetGetClipDistance },
  { "setClipDistance", l_lovrHeadsetSetClipDistance },
  { "getTrackingSize", l_lovrHeadsetGetTrackingSize },
  { "getPosition", l_lovrHeadsetGetPosition },
  { "getOrientation", l_lovrHeadsetGetPosition },
  { "getVelocity", l_lovrHeadsetGetVelocity },
  { "getAngularVelocity", l_lovrHeadsetGetAngularVelocity },
  { "getController", l_lovrHeadsetGetController },
  { "renderTo", l_lovrHeadsetRenderTo },
  { NULL, NULL }
};

int l_lovrHeadsetInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrHeadset);
  luaRegisterType(L, "Controller", lovrController, NULL);

  map_init(&ControllerHands);
  map_set(&ControllerHands, "left", CONTROLLER_HAND_LEFT);
  map_set(&ControllerHands, "right", CONTROLLER_HAND_RIGHT);

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

int l_lovrHeadsetGetTrackingSize(lua_State* L) {
  float width, depth;
  lovrHeadsetGetClipDistance(&width, &depth);
  lua_pushnumber(L, width);
  lua_pushnumber(L, depth);
  return 2;
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
  float x, y, z, w;
  lovrHeadsetGetOrientation(&x, &y, &z, &w);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  lua_pushnumber(L, w);
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

int l_lovrHeadsetGetController(lua_State* L) {
  const char* userHand = luaL_checkstring(L, 1);
  ControllerHand* hand = (ControllerHand*) map_get(&ControllerHands, userHand);
  if (!hand) {
    return luaL_error(L, "Invalid controller hand: '%s'", userHand);
  }

  luax_pushcontroller(L, lovrHeadsetGetController(*hand));
  return 1;
}

int l_lovrHeadsetRenderTo(lua_State* L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lovrHeadsetRenderTo(renderHelper, L);
  return 0;
}
