#include "controller.h"
#include "../headset.h"
#include "../../util.h"

void luax_pushcontroller(lua_State* L, Controller* controller) {
  if (controller == NULL) {
    return lua_pushnil(L);
  }

  Controller** userdata = (Controller**) lua_newuserdata(L, sizeof(Controller*));
  luaL_getmetatable(L, "Controller");
  lua_setmetatable(L, -2);
  *userdata = controller;
}

Controller* luax_checkcontroller(lua_State* L, int index) {
  return *(Controller**) luaL_checkudata(L, index, "Controller");
}

const luaL_Reg lovrController[] = {
  { "isPresent", l_lovrControllerIsPresent },
  { "getPosition", l_lovrControllerGetPosition },
  { "getOrientation", l_lovrControllerGetOrientation },
  { "getHand", l_lovrControllerGetHand },
  { NULL, NULL }
};

int l_lovrControllerIsPresent(lua_State* L) {
  Controller* controller = luax_checkcontroller(L, 1);
  lua_pushboolean(L, lovrHeadsetIsControllerPresent(controller));
  return 1;
}

int l_lovrControllerGetPosition(lua_State* L) {
  Controller* controller = luax_checkcontroller(L, 1);
  float x, y, z;
  lovrHeadsetGetControllerPosition(controller, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrControllerGetOrientation(lua_State* L) {
  Controller* controller = luax_checkcontroller(L, 1);
  float w, x, y, z;
  lovrHeadsetGetControllerOrientation(controller, &w, &x, &y, &z);
  lua_pushnumber(L, w);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 4;
}

int l_lovrControllerGetHand(lua_State* L) {
  Controller* controller = luax_checkcontroller(L, 1);
  lua_pushstring(L, map_int_find(&ControllerHands, lovrHeadsetGetControllerHand(controller)));
  return 1;
}
