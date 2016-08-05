#include <stdlib.h>
#include "device.h"
#include "interface.h"
#include "osvr.h"
#include "util.h"

extern OSVR_ClientContext ctx;

int lovrDeviceGetByName(lua_State* L) {
  const char* name = luaL_checkstring(L, 1);

  Interface* interface = (Interface*) malloc(sizeof(Interface));
  osvrClientGetInterface(ctx, name, interface);

  if (interface) {
    luax_pushinterface(L, interface);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

int lovrDeviceGetHeadset(lua_State* L) {
  const char* name = "/me/head";

  Interface* headset = (Interface*) malloc(sizeof(Interface));
  osvrClientGetInterface(ctx, name, headset);

  if (headset) {
    luax_pushinterface(L, headset);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

int lovrDeviceGetControllers(lua_State* L) {
  const char* leftHandPath = "/me/hands/left";
  const char* rightHandPath = "/me/hands/right";

  Interface* leftHand = (Interface*) malloc(sizeof(Interface));
  osvrClientGetInterface(ctx, leftHandPath, leftHand);

  if (leftHand) {
    luax_pushinterface(L, leftHand);
  } else {
    lua_pushnil(L);
  }

  Interface* rightHand = (Interface*) malloc(sizeof(Interface));
  osvrClientGetInterface(ctx, rightHandPath, rightHand);

  if (rightHand) {
    luax_pushinterface(L, rightHand);
  } else {
    lua_pushnil(L);
  }

  return 2;
}

const luaL_Reg lovrDevice[] = {
  { "getByName", lovrDeviceGetByName },
  { "getHeadset", lovrDeviceGetHeadset },
  { "getControllers", lovrDeviceGetControllers },
  { NULL, NULL }
};

int lovrInitDevice(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrDevice);
  luaRegisterType(L, "Interface", lovrInterface);
  initOSVR();
  return 1;
}
