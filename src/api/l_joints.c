#include "api.h"
#include "physics/physics.h"

void luax_pushjoint(lua_State* L, Joint* joint) {
  switch (joint->type) {
    case JOINT_BALL: luax_pushtype(L, BallJoint, joint); break;
    case JOINT_DISTANCE: luax_pushtype(L, DistanceJoint, joint); break;
    case JOINT_HINGE: luax_pushtype(L, HingeJoint, joint); break;
    case JOINT_SLIDER: luax_pushtype(L, SliderJoint, joint); break;
  }
}

Joint* luax_checkjoint(lua_State* L, int index) {
  Proxy* p = lua_touserdata(L, index);

  if (p) {
    const uint64_t hashes[] = {
      hash64("BallJoint", strlen("BallJoint")),
      hash64("DistanceJoint", strlen("DistanceJoint")),
      hash64("HingeJoint", strlen("HingeJoint")),
      hash64("SliderJoint", strlen("SliderJoint"))
    };

    for (size_t i = 0; i < sizeof(hashes) / sizeof(hashes[0]); i++) {
      if (p->hash == hashes[i]) {
        return p->object;
      }
    }
  }

  luaL_typerror(L, index, "Joint");
  return NULL;
}

static int l_lovrJointDestroy(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  lovrJointDestroyData(joint);
  return 0;
}

static int l_lovrJointGetType(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  lua_pushstring(L, JointTypes[lovrJointGetType(joint)]);
  return 1;
}

static int l_lovrJointGetColliders(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  Collider* a;
  Collider* b;
  lovrJointGetColliders(joint, &a, &b);
  luax_pushtype(L, Collider, a);
  luax_pushtype(L, Collider, b);
  return 2;
}

static int l_lovrJointGetUserData(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  union { int i; void* p; } ref = { .p = lovrJointGetUserData(joint) };
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref.i);
  return 1;
}

static int l_lovrJointSetUserData(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  union { int i; void* p; } ref = { .p = lovrJointGetUserData(joint) };
  if (ref.i) {
    luaL_unref(L, LUA_REGISTRYINDEX, ref.i);
  }

  if (lua_gettop(L) < 2) {
    lua_pushnil(L);
  }

  lua_settop(L, 2);
  ref.i = luaL_ref(L, LUA_REGISTRYINDEX);
  lovrJointSetUserData(joint, ref.p);
  return 0;
}

#define lovrJoint \
  { "destroy", l_lovrJointDestroy }, \
  { "getType", l_lovrJointGetType }, \
  { "getColliders", l_lovrJointGetColliders }, \
  { "getUserData", l_lovrJointGetUserData }, \
  { "setUserData", l_lovrJointSetUserData }

static int l_lovrBallJointGetAnchors(lua_State* L) {
  BallJoint* joint = luax_checktype(L, 1, BallJoint);
  float x1, y1, z1, x2, y2, z2;
  lovrBallJointGetAnchors(joint, &x1, &y1, &z1, &x2, &y2, &z2);
  lua_pushnumber(L, x1);
  lua_pushnumber(L, y1);
  lua_pushnumber(L, z1);
  lua_pushnumber(L, x2);
  lua_pushnumber(L, y2);
  lua_pushnumber(L, z2);
  return 6;
}

static int l_lovrBallJointSetAnchor(lua_State* L) {
  BallJoint* joint = luax_checktype(L, 1, BallJoint);
  float x = luax_checkfloat(L, 2);
  float y = luax_checkfloat(L, 3);
  float z = luax_checkfloat(L, 4);
  lovrBallJointSetAnchor(joint, x, y, z);
  return 0;
}

const luaL_Reg lovrBallJoint[] = {
  lovrJoint,
  { "getAnchors", l_lovrBallJointGetAnchors },
  { "setAnchor", l_lovrBallJointSetAnchor },
  { NULL, NULL }
};

static int l_lovrDistanceJointGetAnchors(lua_State* L) {
  DistanceJoint* joint = luax_checktype(L, 1, DistanceJoint);
  float x1, y1, z1, x2, y2, z2;
  lovrDistanceJointGetAnchors(joint, &x1, &y1, &z1, &x2, &y2, &z2);
  lua_pushnumber(L, x1);
  lua_pushnumber(L, y1);
  lua_pushnumber(L, z1);
  lua_pushnumber(L, x2);
  lua_pushnumber(L, y2);
  lua_pushnumber(L, z2);
  return 6;
}

static int l_lovrDistanceJointSetAnchors(lua_State* L) {
  DistanceJoint* joint = luax_checktype(L, 1, DistanceJoint);
  float x1 = luax_checkfloat(L, 2);
  float y1 = luax_checkfloat(L, 3);
  float z1 = luax_checkfloat(L, 4);
  float x2 = luax_checkfloat(L, 5);
  float y2 = luax_checkfloat(L, 6);
  float z2 = luax_checkfloat(L, 7);
  lovrDistanceJointSetAnchors(joint, x1, y1, z1, x2, y2, z2);
  return 0;
}

static int l_lovrDistanceJointGetDistance(lua_State* L) {
  DistanceJoint* joint = luax_checktype(L, 1, DistanceJoint);
  lua_pushnumber(L, lovrDistanceJointGetDistance(joint));
  return 1;
}

static int l_lovrDistanceJointSetDistance(lua_State* L) {
  DistanceJoint* joint = luax_checktype(L, 1, DistanceJoint);
  float distance = luax_checkfloat(L, 2);
  lovrDistanceJointSetDistance(joint, distance);
  return 0;
}

const luaL_Reg lovrDistanceJoint[] = {
  lovrJoint,
  { "getAnchors", l_lovrDistanceJointGetAnchors },
  { "setAnchors", l_lovrDistanceJointSetAnchors },
  { "getDistance", l_lovrDistanceJointGetDistance },
  { "setDistance", l_lovrDistanceJointSetDistance },
  { NULL, NULL }
};

static int l_lovrHingeJointGetAnchors(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float x1, y1, z1, x2, y2, z2;
  lovrHingeJointGetAnchors(joint, &x1, &y1, &z1, &x2, &y2, &z2);
  lua_pushnumber(L, x1);
  lua_pushnumber(L, y1);
  lua_pushnumber(L, z1);
  lua_pushnumber(L, x2);
  lua_pushnumber(L, y2);
  lua_pushnumber(L, z2);
  return 6;
}

static int l_lovrHingeJointSetAnchor(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float x = luax_checkfloat(L, 2);
  float y = luax_checkfloat(L, 3);
  float z = luax_checkfloat(L, 4);
  lovrHingeJointSetAnchor(joint, x, y, z);
  return 0;
}

static int l_lovrHingeJointGetAxis(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float x, y, z;
  lovrHingeJointGetAxis(joint, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

static int l_lovrHingeJointSetAxis(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float x = luax_checkfloat(L, 2);
  float y = luax_checkfloat(L, 3);
  float z = luax_checkfloat(L, 4);
  lovrHingeJointSetAxis(joint, x, y, z);
  return 0;
}

static int l_lovrHingeJointGetAngle(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  lua_pushnumber(L, lovrHingeJointGetAngle(joint));
  return 1;
}

static int l_lovrHingeJointGetLowerLimit(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  lua_pushnumber(L, lovrHingeJointGetLowerLimit(joint));
  return 1;
}

static int l_lovrHingeJointSetLowerLimit(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float limit = luax_checkfloat(L, 2);
  lovrHingeJointSetLowerLimit(joint, limit);
  return 0;
}

static int l_lovrHingeJointGetUpperLimit(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  lua_pushnumber(L, lovrHingeJointGetUpperLimit(joint));
  return 1;
}

static int l_lovrHingeJointSetUpperLimit(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float limit = luax_checkfloat(L, 2);
  lovrHingeJointSetUpperLimit(joint, limit);
  return 0;
}

static int l_lovrHingeJointGetLimits(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  lua_pushnumber(L, lovrHingeJointGetLowerLimit(joint));
  lua_pushnumber(L, lovrHingeJointGetUpperLimit(joint));
  return 2;
}

static int l_lovrHingeJointSetLimits(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float lower = luax_checkfloat(L, 2);
  float upper = luax_checkfloat(L, 3);
  lovrHingeJointSetLowerLimit(joint, lower);
  lovrHingeJointSetUpperLimit(joint, upper);
  return 0;
}

const luaL_Reg lovrHingeJoint[] = {
  lovrJoint,
  { "getAnchors", l_lovrHingeJointGetAnchors },
  { "setAnchor", l_lovrHingeJointSetAnchor },
  { "getAxis", l_lovrHingeJointGetAxis },
  { "setAxis", l_lovrHingeJointSetAxis },
  { "getAngle", l_lovrHingeJointGetAngle },
  { "getLowerLimit", l_lovrHingeJointGetLowerLimit },
  { "setLowerLimit", l_lovrHingeJointSetLowerLimit },
  { "getUpperLimit", l_lovrHingeJointGetUpperLimit },
  { "setUpperLimit", l_lovrHingeJointSetUpperLimit },
  { "getLimits", l_lovrHingeJointGetLimits },
  { "setLimits", l_lovrHingeJointSetLimits },
  { NULL, NULL }
};

static int l_lovrSliderJointGetAxis(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float x, y, z;
  lovrSliderJointGetAxis(joint, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

static int l_lovrSliderJointSetAxis(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float x = luax_checkfloat(L, 2);
  float y = luax_checkfloat(L, 3);
  float z = luax_checkfloat(L, 4);
  lovrSliderJointSetAxis(joint, x, y, z);
  return 0;
}

static int l_lovrSliderJointGetPosition(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  lua_pushnumber(L, lovrSliderJointGetPosition(joint));
  return 1;
}

static int l_lovrSliderJointGetLowerLimit(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  lua_pushnumber(L, lovrSliderJointGetLowerLimit(joint));
  return 1;
}

static int l_lovrSliderJointSetLowerLimit(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float limit = luax_checkfloat(L, 2);
  lovrSliderJointSetLowerLimit(joint, limit);
  return 0;
}

static int l_lovrSliderJointGetUpperLimit(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  lua_pushnumber(L, lovrSliderJointGetUpperLimit(joint));
  return 1;
}

static int l_lovrSliderJointSetUpperLimit(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float limit = luax_checkfloat(L, 2);
  lovrSliderJointSetUpperLimit(joint, limit);
  return 0;
}

static int l_lovrSliderJointGetLimits(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  lua_pushnumber(L, lovrSliderJointGetLowerLimit(joint));
  lua_pushnumber(L, lovrSliderJointGetUpperLimit(joint));
  return 2;
}

static int l_lovrSliderJointSetLimits(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float lower = luax_checkfloat(L, 2);
  float upper = luax_checkfloat(L, 3);
  lovrSliderJointSetLowerLimit(joint, lower);
  lovrSliderJointSetUpperLimit(joint, upper);
  return 0;
}

const luaL_Reg lovrSliderJoint[] = {
  lovrJoint,
  { "getAxis", l_lovrSliderJointGetAxis },
  { "setAxis", l_lovrSliderJointSetAxis },
  { "getPosition", l_lovrSliderJointGetPosition },
  { "getLowerLimit", l_lovrSliderJointGetLowerLimit },
  { "setLowerLimit", l_lovrSliderJointSetLowerLimit },
  { "getUpperLimit", l_lovrSliderJointGetUpperLimit },
  { "setUpperLimit", l_lovrSliderJointSetUpperLimit },
  { "getLimits", l_lovrSliderJointGetLimits },
  { "setLimits", l_lovrSliderJointSetLimits },
  { NULL, NULL }
};
