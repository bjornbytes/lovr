#include "headset.h"
#include "types/controller.h"
#include "../headset/headset.h"
#include "../util.h"

void renderHelper(int eyeIndex, void* userdata) {
  lua_State* L = (lua_State*)userdata;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lua_pushvalue(L, 1);
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
  { "getTrackingSize", l_lovrHeadsetGetTrackingSize },
  { "isBoundsVisible", l_lovrHeadsetIsBoundsVisible },
  { "setBoundsVisible", l_lovrHeadsetSetBoundsVisible },
  { "getPosition", l_lovrHeadsetGetPosition },
  { "getOrientation", l_lovrHeadsetGetOrientation },
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

int l_lovrHeadsetGetController(lua_State* L) {
  ControllerHand* hand = (ControllerHand*) luax_checkenum(L, 1, &ControllerHands, "controller hand");
  luax_pushcontroller(L, lovrHeadsetGetController(*hand));
  return 1;
}

int l_lovrHeadsetRenderTo(lua_State* L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lovrHeadsetRenderTo(renderHelper, L);
  return 0;
}
