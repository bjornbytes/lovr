#include "api.h"
#include "api/headset.h"
#include "api/math.h"
#include "headset/headset.h"
#include "data/modelData.h"
#include "graphics/model.h"

static int l_lovrControllerIsConnected(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  lua_pushboolean(L, lovrHeadsetDriver->controllerIsConnected(controller));
  return 1;
}

static int l_lovrControllerGetHand(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ControllerHand hand = lovrHeadsetDriver->controllerGetHand(controller);
  lua_pushstring(L, ControllerHands[hand]);
  return 1;
}

static int l_lovrControllerGetPose(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  luax_pushpath(L, controller->path);
  lua_replace(L, 1);
  return l_lovrHeadsetGetPose(L);
}

static int l_lovrControllerGetPosition(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  luax_pushpath(L, controller->path);
  return l_lovrHeadsetGetPosition(L);
}

static int l_lovrControllerGetOrientation(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  luax_pushpath(L, controller->path);
  return l_lovrHeadsetGetOrientation(L);
}

static int l_lovrControllerGetDirection(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  float x, y, z, angle, ax, ay, az;
  lovrHeadsetDriver->controllerGetPose(controller, &x, &y, &z, &angle, &ax, &ay, &az);
  float q[4];
  quat_fromAngleAxis(q, angle, ax, ay, az);
  float v[3] = { 0.f, 0.f, -1.f };
  quat_rotate(q, v);
  lua_pushnumber(L, v[0]);
  lua_pushnumber(L, v[1]);
  lua_pushnumber(L, v[2]);
  return 3;
}

static int l_lovrControllerGetVelocity(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  luax_pushpath(L, controller->path);
  return l_lovrHeadsetGetVelocity(L);
}

static int l_lovrControllerGetAngularVelocity(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  luax_pushpath(L, controller->path);
  return l_lovrHeadsetGetAngularVelocity(L);
}

static int l_lovrControllerGetAxis(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ControllerAxis axis = luaL_checkoption(L, 2, NULL, ControllerAxes);
  lua_pushnumber(L, lovrHeadsetDriver->controllerGetAxis(controller, axis));
  return 1;
}

static int l_lovrControllerIsDown(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ControllerButton button = luaL_checkoption(L, 2, NULL, ControllerButtons);
  lua_pushboolean(L, lovrHeadsetDriver->controllerIsDown(controller, button));
  return 1;
}

static int l_lovrControllerIsTouched(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ControllerButton button = luaL_checkoption(L, 2, NULL, ControllerButtons);
  lua_pushboolean(L, lovrHeadsetDriver->controllerIsTouched(controller, button));
  return 1;
}

static int l_lovrControllerVibrate(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  float duration = luax_optfloat(L, 2, .5f);
  float power = luax_optfloat(L, 3, 1.f);
  lovrHeadsetDriver->controllerVibrate(controller, duration, power);
  return 0;
}

static int l_lovrControllerNewModel(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ModelData* modelData = lovrHeadsetDriver->controllerNewModelData(controller);
  if (modelData) {
    Model* model = lovrModelCreate(modelData);
    luax_pushobject(L, model);
    lovrRelease(ModelData, modelData);
    lovrRelease(Model, model);
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
  { "getDirection", l_lovrControllerGetDirection },
  { "getVelocity", l_lovrControllerGetVelocity },
  { "getAngularVelocity", l_lovrControllerGetAngularVelocity },
  { "getAxis", l_lovrControllerGetAxis },
  { "isDown", l_lovrControllerIsDown },
  { "isTouched", l_lovrControllerIsTouched },
  { "vibrate", l_lovrControllerVibrate },
  { "newModel", l_lovrControllerNewModel },
  { NULL, NULL }
};
