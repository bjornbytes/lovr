#include "api.h"
#include "headset/headset.h"
#include "data/modelData.h"
#include "graphics/model.h"

int l_lovrControllerIsConnected(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  lua_pushboolean(L, lovrHeadsetDriver->controllerIsConnected(controller));
  return 1;
}

int l_lovrControllerGetHand(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ControllerHand hand = lovrHeadsetDriver->controllerGetHand(controller);
  lua_pushstring(L, ControllerHands[hand]);
  return 1;
}

int l_lovrControllerGetPose(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  float x, y, z, angle, ax, ay, az;
  lovrHeadsetDriver->controllerGetPose(controller, &x, &y, &z, &angle, &ax, &ay, &az);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

int l_lovrControllerGetPosition(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  float x, y, z, angle, ax, ay, az;
  lovrHeadsetDriver->controllerGetPose(controller, &x, &y, &z, &angle, &ax, &ay, &az);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrControllerGetOrientation(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  float x, y, z, angle, ax, ay, az;
  lovrHeadsetDriver->controllerGetPose(controller, &x, &y, &z, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

int l_lovrControllerGetAxis(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ControllerAxis axis = luaL_checkoption(L, 2, NULL, ControllerAxes);
  lua_pushnumber(L, lovrHeadsetDriver->controllerGetAxis(controller, axis));
  return 1;
}

int l_lovrControllerIsDown(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ControllerButton button = luaL_checkoption(L, 2, NULL, ControllerButtons);
  lua_pushboolean(L, lovrHeadsetDriver->controllerIsDown(controller, button));
  return 1;
}

int l_lovrControllerIsTouched(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ControllerButton button = luaL_checkoption(L, 2, NULL, ControllerButtons);
  lua_pushboolean(L, lovrHeadsetDriver->controllerIsTouched(controller, button));
  return 1;
}

int l_lovrControllerVibrate(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  float duration = luaL_optnumber(L, 2, .5);
  float power = luaL_optnumber(L, 3, 1);
  lovrHeadsetDriver->controllerVibrate(controller, duration, power);
  return 0;
}

int l_lovrControllerNewModel(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ModelData* modelData = lovrHeadsetDriver->controllerNewModelData(controller);
  if (modelData) {
    Model* model = lovrModelCreate(modelData);
    luax_pushtype(L, Model, model);
    lovrRelease(modelData);
    lovrRelease(model);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

const luaL_Reg lovrController[] = {
  { "isConnected", l_lovrControllerIsConnected },
  { "getHand", l_lovrControllerGetHand },
  { "getPose", l_lovrControllerGetPose },
  { "getPosition", l_lovrControllerGetPosition },
  { "getOrientation", l_lovrControllerGetOrientation },
  { "getAxis", l_lovrControllerGetAxis },
  { "isDown", l_lovrControllerIsDown },
  { "isTouched", l_lovrControllerIsTouched },
  { "vibrate", l_lovrControllerVibrate },
  { "newModel", l_lovrControllerNewModel },
  { NULL, NULL }
};
