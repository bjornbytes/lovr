#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <osvr/ClientKit/InterfaceC.h>
#include <osvr/ClientKit/InterfaceStateC.h>

typedef OSVR_ClientInterface Interface;
void luax_pushinterface(lua_State* L, Interface* interface);
Interface* luax_checkinterface(lua_State* L, int index);

extern const luaL_Reg lovrInterface[];
