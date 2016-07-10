#include "device.h"
#include "interface.h"

extern OSVR_ClientContext ctx;

int lovrDeviceGetInterface(lua_State* L) {
  const char* name = luaL_checkstring(L, 1);

  Interface interface;
  osvrClientGetInterface(ctx, name, &interface);

  printf("%p\n", interface);

  if (interface) {
    luax_pushinterface(L, &interface);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

const luaL_Reg lovrDevice[] = {
  { "getInterface", lovrDeviceGetInterface },
  { NULL, NULL }
};
