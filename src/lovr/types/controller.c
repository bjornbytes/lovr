#include "lovr/types/controller.h"
#include "lovr/headset.h"
#include "loaders/model.h"
#include "loaders/texture.h"
#include "graphics/model.h"
#include "util.h"

const luaL_Reg lovrController[] = {
  { "isPresent", l_lovrControllerIsPresent },
  { "getPosition", l_lovrControllerGetPosition },
  { "getOrientation", l_lovrControllerGetOrientation },
  { "getAxis", l_lovrControllerGetAxis },
  { "isDown", l_lovrControllerIsDown },
  { "vibrate", l_lovrControllerVibrate },
  { "newModel", l_lovrControllerNewModel },
  { NULL, NULL }
};

int l_lovrControllerIsPresent(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  lua_pushboolean(L, lovrHeadsetControllerIsPresent(controller));
  return 1;
}

int l_lovrControllerGetPosition(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  float x, y, z;
  lovrHeadsetControllerGetPosition(controller, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrControllerGetOrientation(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  float w, x, y, z;
  lovrHeadsetControllerGetOrientation(controller, &w, &x, &y, &z);
  lua_pushnumber(L, w);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 4;
}

int l_lovrControllerGetAxis(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ControllerAxis* axis = (ControllerAxis*) luax_checkenum(L, 2, &ControllerAxes, "controller axis");
  lua_pushnumber(L, lovrHeadsetControllerGetAxis(controller, *axis));
  return 1;
}

int l_lovrControllerIsDown(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ControllerButton* button = (ControllerButton*) luax_checkenum(L, 2, &ControllerButtons, "controller button");
  lua_pushboolean(L, lovrHeadsetControllerIsDown(controller, *button));
  return 1;
}

int l_lovrControllerVibrate(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  float duration = luaL_optnumber(L, 2, .5);
  lovrHeadsetControllerVibrate(controller, duration);
  return 0;
}

int l_lovrControllerNewModel(lua_State* L) {
  Controller* controller = luax_checktype(L, 1, Controller);
  ControllerModelFormat format;
  void* rawData = lovrHeadsetControllerGetModel(controller, &format);
#ifdef EMSCRIPTEN
  lua_pushnil(L);
#else
  if (rawData && format == CONTROLLER_MODEL_OPENVR) {
    ModelData* modelData = lovrModelDataFromOpenVRModel(rawData);
    TextureData* textureData = lovrTextureDataFromOpenVRModel(rawData);
    Model* model = lovrModelCreate(modelData);
    Texture* texture = lovrTextureCreate(textureData);
    lovrModelSetTexture(model, texture);
    luax_pushtype(L, Model, model);
  } else {
    lua_pushnil(L);
  }
#endif
  return 1;
}
