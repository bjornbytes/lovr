#include "api.h"
#include "physics/physics.h"
#include "util.h"
#include <string.h>

void luax_pushjoint(lua_State* L, Joint* joint) {
  switch (lovrJointGetType(joint)) {
    case JOINT_BALL: luax_pushtype(L, BallJoint, joint); break;
    case JOINT_DISTANCE: luax_pushtype(L, DistanceJoint, joint); break;
    case JOINT_HINGE: luax_pushtype(L, HingeJoint, joint); break;
    case JOINT_SLIDER: luax_pushtype(L, SliderJoint, joint); break;
    default: lovrUnreachable();
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

    for (size_t i = 0; i < COUNTOF(hashes); i++) {
      if (p->hash == hashes[i]) {
        return p->object;
      }
    }
  }

  luax_typeerror(L, index, "Joint");
  return NULL;
}

static int l_lovrJointDestroy(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  lovrJointDestroyData(joint);
  return 0;
}

static int l_lovrJointIsDestroyed(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  bool destroyed = lovrJointIsDestroyed(joint);
  lua_pushboolean(L, destroyed);
  return 1;
}

static int l_lovrJointGetType(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  luax_pushenum(L, JointType, lovrJointGetType(joint));
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

static void luax_pushjointstash(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrjointstash");

  if (lua_isnil(L, -1)) {
    lua_newtable(L);
    lua_replace(L, -2);

    // metatable
    lua_newtable(L);
    lua_pushliteral(L, "k");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);

    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, "_lovrjointstash");
  }
}

static int l_lovrJointGetUserData(lua_State* L) {
  luax_checktype(L, 1, Joint);
  luax_pushjointstash(L);
  lua_pushvalue(L, 1);
  lua_rawget(L, -2);
  return 1;
}

static int l_lovrJointSetUserData(lua_State* L) {
  luax_checktype(L, 1, Joint);
  luax_pushjointstash(L);
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_rawset(L, -3);
  return 0;
}

static int l_lovrJointIsEnabled(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  lua_pushboolean(L, lovrJointIsEnabled(joint));
  return 1;
}

static int l_lovrJointSetEnabled(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  bool enable = lua_toboolean(L, 2);
  lovrJointSetEnabled(joint, enable);
  return 0;
}

#define lovrJoint \
  { "destroy", l_lovrJointDestroy }, \
  { "isDestroyed", l_lovrJointIsDestroyed }, \
  { "getType", l_lovrJointGetType }, \
  { "getColliders", l_lovrJointGetColliders }, \
  { "getUserData", l_lovrJointGetUserData }, \
  { "setUserData", l_lovrJointSetUserData }, \
  { "isEnabled", l_lovrJointIsEnabled }, \
  { "setEnabled", l_lovrJointSetEnabled }

static int l_lovrBallJointGetAnchors(lua_State* L) {
  BallJoint* joint = luax_checktype(L, 1, BallJoint);
  float anchor1[3], anchor2[3];
  lovrBallJointGetAnchors(joint, anchor1, anchor2);
  lua_pushnumber(L, anchor1[0]);
  lua_pushnumber(L, anchor1[1]);
  lua_pushnumber(L, anchor1[2]);
  lua_pushnumber(L, anchor2[0]);
  lua_pushnumber(L, anchor2[1]);
  lua_pushnumber(L, anchor2[2]);
  return 6;
}

static int l_lovrBallJointSetAnchor(lua_State* L) {
  BallJoint* joint = luax_checktype(L, 1, BallJoint);
  float anchor[3];
  luax_readvec3(L, 2, anchor, NULL);
  lovrBallJointSetAnchor(joint, anchor);
  return 0;
}

static int l_lovrBallJointGetResponseTime(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  float responseTime = lovrBallJointGetResponseTime(joint);
  lua_pushnumber(L, responseTime);
  return 1;
}

static int l_lovrBallJointSetResponseTime(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  float responseTime = luax_checkfloat(L, 2);
  lovrCheck(responseTime >= 0, "Negative response time causes simulation instability");
  lovrBallJointSetResponseTime(joint, responseTime);
  return 0;
}

static int l_lovrBallJointGetTightness(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  float tightness = lovrBallJointGetTightness(joint);
  lua_pushnumber(L, tightness);
  return 1;
}

static int l_lovrBallJointSetTightness(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  float tightness = luax_checkfloat(L, 2);
  lovrCheck(tightness >= 0, "Negative tightness factor causes simulation instability");
  lovrBallJointSetTightness(joint, tightness);
  return 0;
}

const luaL_Reg lovrBallJoint[] = {
  lovrJoint,
  { "getAnchors", l_lovrBallJointGetAnchors },
  { "setAnchor", l_lovrBallJointSetAnchor },
  { "getResponseTime", l_lovrBallJointGetResponseTime},
  { "setResponseTime", l_lovrBallJointSetResponseTime},
  { "getTightness", l_lovrBallJointGetTightness},
  { "setTightness", l_lovrBallJointSetTightness},
  { NULL, NULL }
};

static int l_lovrDistanceJointGetAnchors(lua_State* L) {
  DistanceJoint* joint = luax_checktype(L, 1, DistanceJoint);
  float anchor1[3], anchor2[3];
  lovrDistanceJointGetAnchors(joint, anchor1, anchor2);
  lua_pushnumber(L, anchor1[0]);
  lua_pushnumber(L, anchor1[1]);
  lua_pushnumber(L, anchor1[2]);
  lua_pushnumber(L, anchor2[0]);
  lua_pushnumber(L, anchor2[1]);
  lua_pushnumber(L, anchor2[2]);
  return 6;
}

static int l_lovrDistanceJointSetAnchors(lua_State* L) {
  DistanceJoint* joint = luax_checktype(L, 1, DistanceJoint);
  float anchor1[3], anchor2[3];
  int index = luax_readvec3(L, 2, anchor1, NULL);
  luax_readvec3(L, index, anchor2, NULL);
  lovrDistanceJointSetAnchors(joint, anchor1, anchor2);
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

static int l_lovrDistanceJointGetResponseTime(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  float responseTime = lovrDistanceJointGetResponseTime(joint);
  lua_pushnumber(L, responseTime);
  return 1;
}

static int l_lovrDistanceJointSetResponseTime(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  float responseTime = luax_checkfloat(L, 2);
  lovrCheck(responseTime >= 0, "Negative response time causes simulation instability");
  lovrDistanceJointSetResponseTime(joint, responseTime);
  return 0;
}

static int l_lovrDistanceJointGetTightness(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  float tightness = lovrDistanceJointGetTightness(joint);
  lua_pushnumber(L, tightness);
  return 1;
}

static int l_lovrDistanceJointSetTightness(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  float tightness = luax_checkfloat(L, 2);
  lovrCheck(tightness >= 0, "Negative tightness factor causes simulation instability");
  lovrDistanceJointSetTightness(joint, tightness);
  return 0;
}

const luaL_Reg lovrDistanceJoint[] = {
  lovrJoint,
  { "getAnchors", l_lovrDistanceJointGetAnchors },
  { "setAnchors", l_lovrDistanceJointSetAnchors },
  { "getDistance", l_lovrDistanceJointGetDistance },
  { "setDistance", l_lovrDistanceJointSetDistance },
  { "getResponseTime", l_lovrDistanceJointGetResponseTime},
  { "setResponseTime", l_lovrDistanceJointSetResponseTime},
  { "getTightness", l_lovrDistanceJointGetTightness},
  { "setTightness", l_lovrDistanceJointSetTightness},
  { NULL, NULL }
};

static int l_lovrHingeJointGetAnchors(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float anchor1[3], anchor2[3];
  lovrHingeJointGetAnchors(joint, anchor1, anchor2);
  lua_pushnumber(L, anchor1[0]);
  lua_pushnumber(L, anchor1[1]);
  lua_pushnumber(L, anchor1[2]);
  lua_pushnumber(L, anchor2[0]);
  lua_pushnumber(L, anchor2[1]);
  lua_pushnumber(L, anchor2[2]);
  return 6;
}

static int l_lovrHingeJointSetAnchor(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float anchor[3];
  luax_readvec3(L, 2, anchor, NULL);
  lovrHingeJointSetAnchor(joint, anchor);
  return 0;
}

static int l_lovrHingeJointGetAxis(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float axis[3];
  lovrHingeJointGetAxis(joint, axis);
  lua_pushnumber(L, axis[0]);
  lua_pushnumber(L, axis[1]);
  lua_pushnumber(L, axis[2]);
  return 3;
}

static int l_lovrHingeJointSetAxis(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float axis[3];
  luax_readvec3(L, 2, axis, NULL);
  lovrHingeJointSetAxis(joint, axis);
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
  float axis[3];
  lovrSliderJointGetAxis(joint, axis);
  lua_pushnumber(L, axis[0]);
  lua_pushnumber(L, axis[1]);
  lua_pushnumber(L, axis[2]);
  return 3;
}

static int l_lovrSliderJointSetAxis(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float axis[3];
  luax_readvec3(L, 2, axis, NULL);
  lovrSliderJointSetAxis(joint, axis);
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
