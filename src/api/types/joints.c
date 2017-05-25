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

int l_lovrBallJointGetAnchors(lua_State* L) {
  BallJoint* ball = luax_checktype(L, 1, BallJoint);
  float x1, y1, z1, x2, y2, z2;
  lovrBallJointGetAnchors(ball, &x1, &y1, &z1, &x2, &y2, &z2);
  lua_pushnumber(L, x1);
  lua_pushnumber(L, y1);
  lua_pushnumber(L, z1);
  lua_pushnumber(L, x2);
  lua_pushnumber(L, y2);
  lua_pushnumber(L, z2);
  return 6;
}

int l_lovrBallJointSetAnchor(lua_State* L) {
  BallJoint* ball = luax_checktype(L, 1, BallJoint);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrBallJointSetAnchor(ball, x, y, z);
  return 0;
}

const luaL_Reg lovrBallJoint[] = {
  { "getAnchors", l_lovrBallJointGetAnchors },
  { "setAnchor", l_lovrBallJointSetAnchor },
  { NULL, NULL }
};

int l_lovrHingeJointGetAnchors(lua_State* L) {
  HingeJoint* hinge = luax_checktype(L, 1, HingeJoint);
  float x1, y1, z1, x2, y2, z2;
  lovrHingeJointGetAnchors(hinge, &x1, &y1, &z1, &x2, &y2, &z2);
  lua_pushnumber(L, x1);
  lua_pushnumber(L, y1);
  lua_pushnumber(L, z1);
  lua_pushnumber(L, x2);
  lua_pushnumber(L, y2);
  lua_pushnumber(L, z2);
  return 6;
}

int l_lovrHingeJointSetAnchor(lua_State* L) {
  HingeJoint* hinge = luax_checktype(L, 1, HingeJoint);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrHingeJointSetAnchor(hinge, x, y, z);
  return 0;
}

int l_lovrHingeJointGetAxis(lua_State* L) {
  HingeJoint* hinge = luax_checktype(L, 1, HingeJoint);
  float x, y, z;
  lovrHingeJointGetAxis(hinge, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrHingeJointSetAxis(lua_State* L) {
  HingeJoint* hinge = luax_checktype(L, 1, HingeJoint);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrHingeJointSetAxis(hinge, x, y, z);
  return 0;
}

int l_lovrHingeJointGetAngle(lua_State* L) {
  HingeJoint* hinge = luax_checktype(L, 1, HingeJoint);
  lua_pushnumber(L, lovrHingeJointGetAngle(hinge));
  return 1;
}

const luaL_Reg lovrHingeJoint[] = {
  { "getAnchors", l_lovrHingeJointGetAnchors },
  { "setAnchor", l_lovrHingeJointSetAnchor },
  { "getAxis", l_lovrHingeJointGetAxis },
  { "setAxis", l_lovrHingeJointSetAxis },
  { "getAngle", l_lovrHingeJointGetAngle },
  { NULL, NULL }
};
