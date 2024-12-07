#include "api.h"
#include "headset/headset.h"
#include "core/maf.h"
#include "util.h"

static int l_lovrLayerGetPosition(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float position[3], orientation[4];
  lovrHeadsetInterface->getLayerPose(layer, position, orientation);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrLayerSetPosition(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float position[3], orientation[4];
  lovrHeadsetInterface->getLayerPose(layer, position, orientation);
  luax_readvec3(L, 2, position, NULL);
  lovrHeadsetInterface->setLayerPose(layer, position, orientation);
  return 0;
}

static int l_lovrLayerGetOrientation(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float position[3], orientation[4], angle, ax, ay, az;
  lovrHeadsetInterface->getLayerPose(layer, position, orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

static int l_lovrLayerSetOrientation(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float position[3], orientation[4];
  lovrHeadsetInterface->getLayerPose(layer, position, orientation);
  luax_readquat(L, 2, orientation, NULL);
  lovrHeadsetInterface->setLayerPose(layer, position, orientation);
  return 0;
}

static int l_lovrLayerGetPose(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float position[3], orientation[4], angle, ax, ay, az;
  lovrHeadsetInterface->getLayerPose(layer, position, orientation);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrLayerSetPose(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float position[3], orientation[4];
  int index = luax_readvec3(L, 2, position, NULL);
  luax_readquat(L, index, orientation, NULL);
  lovrHeadsetInterface->setLayerPose(layer, position, orientation);
  return 0;
}

static int l_lovrLayerGetDimensions(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float width, height;
  lovrHeadsetInterface->getLayerDimensions(layer, &width, &height);
  lua_pushnumber(L, width);
  lua_pushnumber(L, height);
  return 2;
}

static int l_lovrLayerSetDimensions(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float width = luax_checkfloat(L, 2);
  float height = luax_checkfloat(L, 3);
  lovrHeadsetInterface->setLayerDimensions(layer, width, height);
  return 0;
}

static int l_lovrLayerGetCurve(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float curve = lovrHeadsetInterface->getLayerCurve(layer);
  lua_pushnumber(L, curve);
  return 1;
}

static int l_lovrLayerSetCurve(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float curve = luax_optfloat(L, 2, 0.f);
  luax_assert(L, lovrHeadsetInterface->setLayerCurve(layer, curve));
  return 0;
}

static int l_lovrLayerGetColor(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float color[4];
  lovrHeadsetInterface->getLayerColor(layer, color);
  lua_pushnumber(L, color[0]);
  lua_pushnumber(L, color[1]);
  lua_pushnumber(L, color[2]);
  lua_pushnumber(L, color[3]);
  return 4;
}

static int l_lovrLayerSetColor(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float color[4];
  luax_readcolor(L, 1, color);
  lovrHeadsetInterface->setLayerColor(layer, color);
  return 0;
}

static int l_lovrLayerGetViewport(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  int32_t viewport[4];
  lovrHeadsetInterface->getLayerViewport(layer, viewport);
  lua_pushinteger(L, viewport[0]);
  lua_pushinteger(L, viewport[1]);
  lua_pushinteger(L, viewport[2]);
  lua_pushinteger(L, viewport[3]);
  return 4;
}

static int l_lovrLayerSetViewport(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  int32_t viewport[4];
  viewport[0] = luax_optu32(L, 2, 0);
  viewport[1] = luax_optu32(L, 3, 0);
  viewport[2] = luax_optu32(L, 4, 0);
  viewport[3] = luax_optu32(L, 5, 0);
  lovrHeadsetInterface->setLayerViewport(layer, viewport);
  return 0;
}

static int l_lovrLayerGetTexture(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  struct Texture* texture = lovrHeadsetInterface->getLayerTexture(layer);
  luax_assert(L, texture);
  luax_pushtype(L, Texture, texture);
  return 1;
}

static int l_lovrLayerGetPass(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  struct Pass* pass = lovrHeadsetInterface->getLayerPass(layer);
  luax_assert(L, pass);
  luax_pushtype(L, Pass, pass);
  return 1;
}

const luaL_Reg lovrLayer[] = {
  { "getPosition", l_lovrLayerGetPosition },
  { "setPosition", l_lovrLayerSetPosition },
  { "getOrientation", l_lovrLayerGetOrientation },
  { "setOrientation", l_lovrLayerSetOrientation },
  { "getPose", l_lovrLayerGetPose },
  { "setPose", l_lovrLayerSetPose },
  { "getDimensions", l_lovrLayerGetDimensions },
  { "setDimensions", l_lovrLayerSetDimensions },
  { "getCurve", l_lovrLayerGetCurve },
  { "setCurve", l_lovrLayerSetCurve },
  { "getColor", l_lovrLayerGetColor },
  { "setColor", l_lovrLayerSetColor },
  { "getViewport", l_lovrLayerGetViewport },
  { "setViewport", l_lovrLayerSetViewport },
  { "getTexture", l_lovrLayerGetTexture },
  { "getPass", l_lovrLayerGetPass },
  { NULL, NULL }
};
