#include "api/lovr.h"
#include "graphics/animator.h"

int l_lovrAnimatorGetAnimationCount(lua_State* L) {
  Animator* animator = luax_checktype(L, 1, Animator);
  lua_pushnumber(L, lovrAnimatorGetAnimationCount(animator));
  return 1;
}

const luaL_Reg lovrAnimator[] = {
  { "getAnimationCount", l_lovrAnimatorGetAnimationCount },
  { NULL, NULL }
};
