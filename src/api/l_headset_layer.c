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

static int l_lovrLayerGetSize(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float width, height;
  lovrHeadsetInterface->getLayerSize(layer, &width, &height);
  lua_pushnumber(L, width);
  lua_pushnumber(L, height);
  return 2;
}

static int l_lovrLayerSetSize(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  float width = luax_checkfloat(L, 2);
  float height = luax_checkfloat(L, 3);
  lovrHeadsetInterface->setLayerSize(layer, width, height);
  return 0;
}

static int l_lovrLayerGetEyeMask(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  EyeMask mask = lovrHeadsetInterface->getLayerEyeMask(layer);
  luax_pushenum(L, EyeMask, mask);
  return 1;
}

static int l_lovrLayerSetEyeMask(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  EyeMask mask = luax_checkenum(L, 2, EyeMask, "both");
  lovrHeadsetInterface->setLayerEyeMask(layer, mask);
  return 0;
}

static int l_lovrLayerGetViewport(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  uint32_t viewport[4];
  lovrHeadsetInterface->getLayerViewport(layer, viewport);
  lua_pushinteger(L, viewport[0]);
  lua_pushinteger(L, viewport[1]);
  lua_pushinteger(L, viewport[2]);
  lua_pushinteger(L, viewport[3]);
  return 4;
}

static int l_lovrLayerSetViewport(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  uint32_t viewport[4];
  viewport[0] = luax_optu32(L, 2, 0);
  viewport[1] = luax_optu32(L, 3, 0);
  viewport[2] = luax_optu32(L, 4, ~0u);
  viewport[3] = luax_optu32(L, 5, ~0u);
  lovrHeadsetInterface->setLayerViewport(layer, viewport);
  return 0;
}

static int l_lovrLayerGetSupersample(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  bool supersample = lovrHeadsetInterface->getLayerOption(layer, LAYER_SUPERSAMPLE);
  lua_pushboolean(L, supersample);
  return 1;
}

static int l_lovrLayerSetSupersample(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  bool supersample = lua_toboolean(L, 2);
  lovrHeadsetInterface->setLayerOption(layer, LAYER_SUPERSAMPLE, supersample);
  return 0;
}

static int l_lovrLayerGetSharpen(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  bool sharpen = lovrHeadsetInterface->getLayerOption(layer, LAYER_SHARPEN);
  lua_pushboolean(L, sharpen);
  return 1;
}

static int l_lovrLayerSetSharpen(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  bool sharpen = lua_toboolean(L, 2);
  lovrHeadsetInterface->setLayerOption(layer, LAYER_SHARPEN, sharpen);
  return 0;
}

static int l_lovrLayerGetTexture(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  struct Texture* texture = lovrHeadsetInterface->getLayerTexture(layer);
  luax_pushtype(L, Texture, texture);
  return 1;
}

static int l_lovrLayerGetPass(lua_State* L) {
  Layer* layer = luax_checktype(L, 1, Layer);
  struct Pass* pass = lovrHeadsetInterface->getLayerPass(layer);
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
  { "getSize", l_lovrLayerGetSize },
  { "setSize", l_lovrLayerSetSize },
  { "getEyeMask", l_lovrLayerGetEyeMask },
  { "setEyeMask", l_lovrLayerSetEyeMask },
  { "getViewport", l_lovrLayerGetViewport },
  { "setViewport", l_lovrLayerSetViewport },
  { "getSupersample", l_lovrLayerGetSupersample },
  { "setSupersample", l_lovrLayerSetSupersample },
  { "getSharpen", l_lovrLayerGetSharpen },
  { "setSharpen", l_lovrLayerSetSharpen },
  { "getTexture", l_lovrLayerGetTexture },
  { "getPass", l_lovrLayerGetPass },
  { NULL, NULL }
};
