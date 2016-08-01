#include "interface.h"
#include "util.h"
#include "osvr.h"

void luax_pushinterface(lua_State* L, Interface* interface) {
  Interface** userdata = (Interface**) lua_newuserdata(L, sizeof(Interface*));

  luaL_getmetatable(L, "Interface");
  lua_setmetatable(L, -2);

  *userdata = interface;
}

Interface* luax_checkinterface(lua_State* L, int index) {
  return *(Interface**) luaL_checkudata(L, index, "Interface");
}

int lovrInterfaceGetPosition(lua_State* L) {
  Interface* interface = luax_checkinterface(L, 1);
  OSVR_TimeValue t;
  OSVR_PositionState position;

  OSVR_ReturnCode res = osvrGetPositionState(*interface, &t, &position);

  if (res != OSVR_RETURN_SUCCESS) {
    lua_pushnil(L);
    return 1;
  }

  lua_pushnumber(L, position.data[0]);
  lua_pushnumber(L, position.data[1]);
  lua_pushnumber(L, position.data[2]);

  return 3;
}

int lovrInterfaceGetOrientation(lua_State* L) {
  Interface* interface = luax_checkinterface(L, 1);
  OSVR_TimeValue t;
  OSVR_OrientationState orientation;

  osvrClientUpdate(ctx);

  OSVR_ReturnCode res = osvrGetOrientationState(*interface, &t, &orientation);

  if (res != OSVR_RETURN_SUCCESS) {
    lua_pushnil(L);
    return 1;
  }

  lua_pushnumber(L, orientation.data[0]);
  lua_pushnumber(L, orientation.data[1]);
  lua_pushnumber(L, orientation.data[2]);

  return 3;
}

const luaL_Reg lovrInterface[] = {
  { "getPosition", lovrInterfaceGetPosition },
  { "getOrientation", lovrInterfaceGetOrientation },
  { NULL, NULL }
};
