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

int l_lovrAnimatorPlay(lua_State* L) {
  Animator* animator = luax_checktype(L, 1, Animator);
  const char* animation = luaL_checkstring(L, 2);
  lovrAnimatorPlay(animator, animation);
  return 0;
}

int l_lovrAnimatorStop(lua_State* L) {
  Animator* animator = luax_checktype(L, 1, Animator);
  const char* animation = luaL_checkstring(L, 2);
  lovrAnimatorStop(animator, animation);
  return 0;
}

int l_lovrAnimatorPause(lua_State* L) {
  Animator* animator = luax_checktype(L, 1, Animator);
  const char* animation = luaL_checkstring(L, 2);
  lovrAnimatorPause(animator, animation);
  return 0;
}

int l_lovrAnimatorResume(lua_State* L) {
  Animator* animator = luax_checktype(L, 1, Animator);
  const char* animation = luaL_checkstring(L, 2);
  lovrAnimatorResume(animator, animation);
  return 0;
}

const luaL_Reg lovrAnimator[] = {
  { "getAnimationCount", l_lovrAnimatorGetAnimationCount },
  { "update", l_lovrAnimatorUpdate },
  { "play", l_lovrAnimatorPlay },
  { "stop", l_lovrAnimatorStop },
  { "pause", l_lovrAnimatorPause },
  { "resume", l_lovrAnimatorResume },
  { NULL, NULL }
};
