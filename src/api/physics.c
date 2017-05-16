#include "api/lovr.h"
#include "physics/physics.h"

int l_lovrPhysicsInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrPhysics);
  luax_registertype(L, "World", lovrWorld);
  lovrPhysicsInit();
  return 1;
}

int l_lovrPhysicsNewWorld(lua_State* L) {
  luax_pushtype(L, World, lovrWorldCreate());
  return 1;
}

const luaL_Reg lovrPhysics[] = {
  { "newWorld", l_lovrPhysicsNewWorld },
  { NULL, NULL }
};
