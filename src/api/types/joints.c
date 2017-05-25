#include "api/lovr.h"
#include "physics/physics.h"

int luax_pushjoint(lua_State* L, Joint* joint) {
  switch (lovrJointGetType(joint)) {
    case JOINT_BALL: luax_pushtype(L, BallJoint, joint); return 1;
    default: return 0;
  }
}

int l_lovrJointDestroy(lua_State* L) {
  Joint* joint = luax_checktypeof(L, 1, Joint);
  lovrJointDestroyData(joint);
  return 0;
}

int l_lovrJointGetType(lua_State* L) {
  Joint* joint = luax_checktypeof(L, 1, Joint);
  luax_pushenum(L, &JointTypes, lovrJointGetType(joint));
  return 1;
}

int l_lovrJointGetColliders(lua_State* L) {
  Joint* joint = luax_checktype(L, 1, Joint);
  Collider* a;
  Collider* b;
  lovrJointGetColliders(joint, &a, &b);
  luax_pushtype(L, Collider, a);
  luax_pushtype(L, Collider, b);
  return 2;
}

int l_lovrJointGetUserData(lua_State* L) {
  Joint* joint = luax_checktypeof(L, 1, Joint);
  int ref = (int) lovrJointGetUserData(joint);
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  return 1;
}

int l_lovrJointSetUserData(lua_State* L) {
  Joint* joint = luax_checktypeof(L, 1, Joint);
  uint64_t ref = (int) lovrJointGetUserData(joint);
  if (ref) {
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
  }

  if (lua_gettop(L) < 2) {
    lua_pushnil(L);
  }

  lua_settop(L, 2);
  ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lovrJointSetUserData(joint, (void*) ref);
  return 0;
}

const luaL_Reg lovrJoint[] = {
  { "destroy", l_lovrJointDestroy },
  { "getType", l_lovrJointGetType },
  { "getColliders", l_lovrJointGetColliders },
  { "getUserData", l_lovrJointGetUserData },
  { "setUserData", l_lovrJointSetUserData },
  { NULL, NULL }
};

const luaL_Reg lovrBallJoint[] = {
  { NULL, NULL }
};
