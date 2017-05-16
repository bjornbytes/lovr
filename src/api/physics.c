#include "api/lovr.h"
#include "physics/physics.h"

int l_lovrPhysicsInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrPhysics);
  luax_registertype(L, "World", lovrWorld);
  luax_registertype(L, "Body", lovrBody);
  lovrPhysicsInit();
  return 1;
}

int l_lovrPhysicsNewWorld(lua_State* L) {
  luax_pushtype(L, World, lovrWorldCreate());
  return 1;
}

int l_lovrPhysicsNewBody(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  luax_pushtype(L, Body, lovrBodyCreate(world));
  return 1;
}

const luaL_Reg lovrPhysics[] = {
  { "newWorld", l_lovrPhysicsNewWorld },
  { "newBody", l_lovrPhysicsNewBody },
  { NULL, NULL }
};
