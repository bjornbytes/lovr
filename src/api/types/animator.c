#include "api/lovr.h"
#include "graphics/animator.h"

int l_lovrAnimatorGetAnimationCount(lua_State* L) {
  Animator* animator = luax_checktype(L, 1, Animator);
  lua_pushnumber(L, lovrAnimatorGetAnimationCount(animator));
  return 1;
}

int l_lovrAnimatorUpdate(lua_State* L) {
  Animator* animator = luax_checktype(L, 1, Animator);
  float dt = luaL_checknumber(L, 2);
  lovrAnimatorUpdate(animator, dt);
  return 0;
}

const luaL_Reg lovrAnimator[] = {
  { "getAnimationCount", l_lovrAnimatorGetAnimationCount },
  { "update", l_lovrAnimatorUpdate },
  { NULL, NULL }
};
