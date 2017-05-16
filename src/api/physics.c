#include "api/lovr.h"
#include "physics/physics.h"

int l_lovrPhysicsInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrPhysics);
  lovrPhysicsInit();
  return 1;
}

const luaL_Reg lovrPhysics[] = {
  { NULL, NULL }
};
