#include "api.h"
#include "physics/physics.h"
#include "util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void luax_pushjoint(lua_State* L, Joint* joint) {
  switch (lovrJointGetType(joint)) {
    case JOINT_WELD: luax_pushtype(L, WeldJoint, joint); break;
    case JOINT_BALL: luax_pushtype(L, BallJoint, joint); break;
    case JOINT_CONE: luax_pushtype(L, ConeJoint, joint); break;
    case JOINT_DISTANCE: luax_pushtype(L, DistanceJoint, joint); break;
    case JOINT_HINGE: luax_pushtype(L, HingeJoint, joint); break;
    case JOINT_SLIDER: luax_pushtype(L, SliderJoint, joint); break;
    default: lovrUnreachable();
  }
}

static Joint* luax_tojoint(lua_State* L, int index) {
  Proxy* p = lua_touserdata(L, index);

  if (p) {
    const uint64_t hashes[] = {
      hash64("WeldJoint", strlen("WeldJoint")),
      hash64("BallJoint", strlen("BallJoint")),
      hash64("ConeJoint", strlen("ConeJoint")),
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

  return NULL;
}

Joint* luax_checkjoint(lua_State* L, int index) {
  Joint* joint = luax_tojoint(L, index);
  if (joint) {
    luax_check(L, !lovrJointIsDestroyed(joint), "Attempt to use a destroyed Joint");
    return joint;
  } else {
    luax_typeerror(L, index, "Joint");
    return NULL;
  }
}

static int l_lovrJointDestroy(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  lovrJointDestruct(joint);
  return 0;
}

static int l_lovrJointIsDestroyed(lua_State* L) {
  Joint* joint = luax_tojoint(L, 1);
  if (!joint) luax_typeerror(L, 1, "Joint");
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
  Collider* a = lovrJointGetColliderA(joint);
  Collider* b = lovrJointGetColliderB(joint);
  luax_pushtype(L, Collider, a);
  luax_pushtype(L, Collider, b);
  return 2;
}

static int l_lovrJointGetAnchors(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  float anchor1[3], anchor2[3];
  lovrJointGetAnchors(joint, anchor1, anchor2);
  lua_pushnumber(L, anchor1[0]);
  lua_pushnumber(L, anchor1[1]);
  lua_pushnumber(L, anchor1[2]);
  lua_pushnumber(L, anchor2[0]);
  lua_pushnumber(L, anchor2[1]);
  lua_pushnumber(L, anchor2[2]);
  return 6;
}

static int l_lovrJointGetUserData(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  lua_pushlightuserdata(L, joint);
  lua_rawget(L, LUA_REGISTRYINDEX);
  return 1;
}

static int l_lovrJointSetUserData(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  lovrJointSetUserData(joint, (uintptr_t) lua_tothread(L, -1));
  lua_pushlightuserdata(L, joint);
  lua_pushvalue(L, 2);
  lua_rawset(L, LUA_REGISTRYINDEX);
  return 0;
}

static int l_lovrJointGetPriority(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  uint32_t priority = lovrJointGetPriority(joint);
  lua_pushinteger(L, priority);
  return 1;
}

static int l_lovrJointSetPriority(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  uint32_t priority = luax_checku32(L, 2);
  lovrJointSetPriority(joint, priority);
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

static int l_lovrJointGetForce(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  float force = lovrJointGetForce(joint);
  lua_pushnumber(L, force);
  return 1;
}

static int l_lovrJointGetTorque(lua_State* L) {
  Joint* joint = luax_checkjoint(L, 1);
  float torque = lovrJointGetTorque(joint);
  lua_pushnumber(L, torque);
  return 1;
}

#define lovrJoint \
  { "destroy", l_lovrJointDestroy }, \
  { "isDestroyed", l_lovrJointIsDestroyed }, \
  { "getType", l_lovrJointGetType }, \
  { "getColliders", l_lovrJointGetColliders }, \
  { "getAnchors", l_lovrJointGetAnchors }, \
  { "getUserData", l_lovrJointGetUserData }, \
  { "setUserData", l_lovrJointSetUserData }, \
  { "getPriority", l_lovrJointGetPriority }, \
  { "setPriority", l_lovrJointSetPriority }, \
  { "isEnabled", l_lovrJointIsEnabled }, \
  { "setEnabled", l_lovrJointSetEnabled }, \
  { "getForce", l_lovrJointGetForce }, \
  { "getTorque", l_lovrJointGetTorque }

const luaL_Reg lovrWeldJoint[] = {
  lovrJoint,
  { NULL, NULL }
};

const luaL_Reg lovrBallJoint[] = {
  lovrJoint,
  { NULL, NULL }
};

static int l_lovrConeJointGetAxis(lua_State* L) {
  ConeJoint* joint = luax_checktype(L, 1, ConeJoint);
  float axis[3];
  lovrConeJointGetAxis(joint, axis);
  lua_pushnumber(L, axis[0]);
  lua_pushnumber(L, axis[1]);
  lua_pushnumber(L, axis[2]);
  return 3;
}

static int l_lovrConeJointGetLimit(lua_State* L) {
  ConeJoint* joint = luax_checktype(L, 1, ConeJoint);
  float limit = lovrConeJointGetLimit(joint);
  lua_pushnumber(L, limit);
  return 1;
}

static int l_lovrConeJointSetLimit(lua_State* L) {
  ConeJoint* joint = luax_checktype(L, 1, ConeJoint);
  float limit = luax_checkfloat(L, 2);
  luax_assert(L, lovrConeJointSetLimit(joint, limit));
  return 0;
}

const luaL_Reg lovrConeJoint[] = {
  lovrJoint,
  { "getAxis", l_lovrConeJointGetAxis },
  { "getLimit", l_lovrConeJointGetLimit },
  { "setLimit", l_lovrConeJointSetLimit },
  { NULL, NULL }
};

static int l_lovrDistanceJointGetLimits(lua_State* L) {
  DistanceJoint* joint = luax_checktype(L, 1, DistanceJoint);
  float min, max;
  lovrDistanceJointGetLimits(joint, &min, &max);
  lua_pushnumber(L, min);
  lua_pushnumber(L, max);
  return 2;
}

static int l_lovrDistanceJointSetLimits(lua_State* L) {
  DistanceJoint* joint = luax_checktype(L, 1, DistanceJoint);
  float min = luax_optfloat(L, 2, 0.f);
  float max = luax_optfloat(L, 3, lua_type(L, 2) == LUA_TNUMBER ? min : HUGE_VALF);
  luax_assert(L, lovrDistanceJointSetLimits(joint, min, max));
  return 0;
}

static int l_lovrDistanceJointGetSpring(lua_State* L) {
  DistanceJoint* joint = luax_checktype(L, 1, DistanceJoint);
  float frequency, damping;
  lovrDistanceJointGetSpring(joint, &frequency, &damping);
  lua_pushnumber(L, frequency);
  lua_pushnumber(L, damping);
  return 2;
}

static int l_lovrDistanceJointSetSpring(lua_State* L) {
  DistanceJoint* joint = luax_checktype(L, 1, DistanceJoint);
  float frequency = luax_optfloat(L, 2, 0.f);
  float damping = luax_optfloat(L, 3, 1.f);
  lovrDistanceJointSetSpring(joint, frequency, damping);
  return 0;
}

const luaL_Reg lovrDistanceJoint[] = {
  lovrJoint,
  { "getLimits", l_lovrDistanceJointGetLimits },
  { "setLimits", l_lovrDistanceJointSetLimits },
  { "getSpring", l_lovrDistanceJointGetSpring },
  { "setSpring", l_lovrDistanceJointSetSpring },
  { NULL, NULL }
};

static int l_lovrHingeJointGetAxis(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float axis[3];
  lovrHingeJointGetAxis(joint, axis);
  lua_pushnumber(L, axis[0]);
  lua_pushnumber(L, axis[1]);
  lua_pushnumber(L, axis[2]);
  return 3;
}

static int l_lovrHingeJointGetAngle(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  lua_pushnumber(L, lovrHingeJointGetAngle(joint));
  return 1;
}

static int l_lovrHingeJointGetLimits(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float min, max;
  lovrHingeJointGetLimits(joint, &min, &max);
  lua_pushnumber(L, min);
  lua_pushnumber(L, max);
  return 2;
}

static int l_lovrHingeJointSetLimits(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  if (lua_isnoneornil(L, 2)) {
    luax_assert(L, lovrHingeJointSetLimits(joint, (float) -M_PI, (float) M_PI));
  } else {
    float min = luax_checkfloat(L, 2);
    float max = luax_checkfloat(L, 3);
    luax_assert(L, lovrHingeJointSetLimits(joint, min, max));
  }
  return 0;
}

static int l_lovrHingeJointGetFriction(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float friction = lovrHingeJointGetFriction(joint);
  lua_pushnumber(L, friction);
  return 1;
}

static int l_lovrHingeJointSetFriction(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float friction = luax_optfloat(L, 2, 0.f);
  lovrHingeJointSetFriction(joint, friction);
  return 0;
}

static int l_lovrHingeJointGetMotorMode(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  MotorMode mode = lovrHingeJointGetMotorMode(joint);
  if (mode == MOTOR_OFF) {
    lua_pushnil(L);
  } else {
    luax_pushenum(L, MotorMode, mode);
  }
  return 1;
}

static int l_lovrHingeJointSetMotorMode(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  MotorMode mode = luax_checkenum(L, 2, MotorMode, "off");
  lovrHingeJointSetMotorMode(joint, mode);
  return 0;
}

static int l_lovrHingeJointGetMotorTarget(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float target = lovrHingeJointGetMotorTarget(joint);
  lua_pushnumber(L, target);
  return 1;
}

static int l_lovrHingeJointSetMotorTarget(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float target = luax_checkfloat(L, 2);
  lovrHingeJointSetMotorTarget(joint, target);
  return 0;
}

static int l_lovrHingeJointGetMotorSpring(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float frequency, damping;
  lovrHingeJointGetMotorSpring(joint, &frequency, &damping);
  lua_pushnumber(L, frequency);
  lua_pushnumber(L, damping);
  return 2;
}

static int l_lovrHingeJointSetMotorSpring(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float frequency = luax_optfloat(L, 2, 0.f);
  float damping = luax_optfloat(L, 3, 1.f);
  lovrHingeJointSetMotorSpring(joint, frequency, damping);
  return 0;
}

static int l_lovrHingeJointGetMaxMotorTorque(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float positive, negative;
  lovrHingeJointGetMaxMotorTorque(joint, &positive, &negative);
  lua_pushnumber(L, positive);
  lua_pushnumber(L, negative);
  return 2;
}

static int l_lovrHingeJointSetMaxMotorTorque(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  if (lua_isnoneornil(L, 2)) {
    lovrHingeJointSetMaxMotorTorque(joint, HUGE_VALF, HUGE_VALF);
  } else {
    float positive = luax_checkfloat(L, 2);
    float negative = luax_optfloat(L, 3, positive);
    lovrHingeJointSetMaxMotorTorque(joint, positive, negative);
  }
  return 0;
}

static int l_lovrHingeJointGetMotorTorque(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float force = lovrHingeJointGetMotorTorque(joint);
  lua_pushnumber(L, force);
  return 1;
}

static int l_lovrHingeJointGetSpring(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float frequency, damping;
  lovrHingeJointGetSpring(joint, &frequency, &damping);
  lua_pushnumber(L, frequency);
  lua_pushnumber(L, damping);
  return 2;
}

static int l_lovrHingeJointSetSpring(lua_State* L) {
  HingeJoint* joint = luax_checktype(L, 1, HingeJoint);
  float frequency = luax_optfloat(L, 2, 0.f);
  float damping = luax_optfloat(L, 3, 1.f);
  lovrHingeJointSetSpring(joint, frequency, damping);
  return 0;
}

const luaL_Reg lovrHingeJoint[] = {
  lovrJoint,
  { "getAxis", l_lovrHingeJointGetAxis },
  { "getAngle", l_lovrHingeJointGetAngle },
  { "getLimits", l_lovrHingeJointGetLimits },
  { "setLimits", l_lovrHingeJointSetLimits },
  { "getFriction", l_lovrHingeJointGetFriction },
  { "setFriction", l_lovrHingeJointSetFriction },
  { "getMotorMode", l_lovrHingeJointGetMotorMode },
  { "setMotorMode", l_lovrHingeJointSetMotorMode },
  { "getMotorTarget", l_lovrHingeJointGetMotorTarget },
  { "setMotorTarget", l_lovrHingeJointSetMotorTarget },
  { "getMotorSpring", l_lovrHingeJointGetMotorSpring },
  { "setMotorSpring", l_lovrHingeJointSetMotorSpring },
  { "getMaxMotorTorque", l_lovrHingeJointGetMaxMotorTorque },
  { "setMaxMotorTorque", l_lovrHingeJointSetMaxMotorTorque },
  { "getMotorTorque", l_lovrHingeJointGetMotorTorque },
  { "getSpring", l_lovrHingeJointGetSpring },
  { "setSpring", l_lovrHingeJointSetSpring },
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

static int l_lovrSliderJointGetPosition(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  lua_pushnumber(L, lovrSliderJointGetPosition(joint));
  return 1;
}

static int l_lovrSliderJointGetLimits(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float min, max;
  lovrSliderJointGetLimits(joint, &min, &max);
  lua_pushnumber(L, min);
  lua_pushnumber(L, max);
  return 2;
}

static int l_lovrSliderJointSetLimits(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  if (lua_isnoneornil(L, 2)) {
    luax_assert(L, lovrSliderJointSetLimits(joint, (float) -HUGE_VALF, (float) HUGE_VALF));
  } else {
    float min = luax_checkfloat(L, 2);
    float max = luax_checkfloat(L, 3);
    luax_assert(L, lovrSliderJointSetLimits(joint, min, max));
  }
  return 0;
}

static int l_lovrSliderJointGetFriction(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float friction = lovrSliderJointGetFriction(joint);
  lua_pushnumber(L, friction);
  return 1;
}

static int l_lovrSliderJointSetFriction(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float friction = luax_optfloat(L, 2, 0.f);
  lovrSliderJointSetFriction(joint, friction);
  return 0;
}

static int l_lovrSliderJointGetMotorMode(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  MotorMode mode = lovrSliderJointGetMotorMode(joint);
  if (mode == MOTOR_OFF) {
    lua_pushnil(L);
  } else {
    luax_pushenum(L, MotorMode, mode);
  }
  return 1;
}

static int l_lovrSliderJointSetMotorMode(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  MotorMode mode = luax_checkenum(L, 2, MotorMode, "off");
  lovrSliderJointSetMotorMode(joint, mode);
  return 0;
}

static int l_lovrSliderJointGetMotorTarget(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float target = lovrSliderJointGetMotorTarget(joint);
  lua_pushnumber(L, target);
  return 1;
}

static int l_lovrSliderJointSetMotorTarget(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float target = luax_checkfloat(L, 2);
  lovrSliderJointSetMotorTarget(joint, target);
  return 0;
}

static int l_lovrSliderJointGetMotorSpring(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float frequency, damping;
  lovrSliderJointGetMotorSpring(joint, &frequency, &damping);
  lua_pushnumber(L, frequency);
  lua_pushnumber(L, damping);
  return 2;
}

static int l_lovrSliderJointSetMotorSpring(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float frequency = luax_optfloat(L, 2, 0.f);
  float damping = luax_optfloat(L, 3, 1.f);
  lovrSliderJointSetMotorSpring(joint, frequency, damping);
  return 0;
}

static int l_lovrSliderJointGetMaxMotorForce(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float positive, negative;
  lovrSliderJointGetMaxMotorForce(joint, &positive, &negative);
  lua_pushnumber(L, positive);
  lua_pushnumber(L, negative);
  return 2;
}

static int l_lovrSliderJointSetMaxMotorForce(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  if (lua_isnoneornil(L, 2)) {
    lovrSliderJointSetMaxMotorForce(joint, HUGE_VALF, HUGE_VALF);
  } else {
    float positive = luax_checkfloat(L, 2);
    float negative = luax_optfloat(L, 3, positive);
    lovrSliderJointSetMaxMotorForce(joint, positive, negative);
  }
  return 0;
}

static int l_lovrSliderJointGetMotorForce(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float force = lovrSliderJointGetMotorForce(joint);
  lua_pushnumber(L, force);
  return 1;
}

static int l_lovrSliderJointGetSpring(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float frequency, damping;
  lovrSliderJointGetSpring(joint, &frequency, &damping);
  lua_pushnumber(L, frequency);
  lua_pushnumber(L, damping);
  return 2;
}

static int l_lovrSliderJointSetSpring(lua_State* L) {
  SliderJoint* joint = luax_checktype(L, 1, SliderJoint);
  float frequency = luax_optfloat(L, 2, 0.f);
  float damping = luax_optfloat(L, 3, 1.f);
  lovrSliderJointSetSpring(joint, frequency, damping);
  return 0;
}

const luaL_Reg lovrSliderJoint[] = {
  lovrJoint,
  { "getAxis", l_lovrSliderJointGetAxis },
  { "getPosition", l_lovrSliderJointGetPosition },
  { "getLimits", l_lovrSliderJointGetLimits },
  { "setLimits", l_lovrSliderJointSetLimits },
  { "getFriction", l_lovrSliderJointGetFriction },
  { "setFriction", l_lovrSliderJointSetFriction },
  { "getMotorMode", l_lovrSliderJointGetMotorMode },
  { "setMotorMode", l_lovrSliderJointSetMotorMode },
  { "getMotorTarget", l_lovrSliderJointGetMotorTarget },
  { "setMotorTarget", l_lovrSliderJointSetMotorTarget },
  { "getMotorSpring", l_lovrSliderJointGetMotorSpring },
  { "setMotorSpring", l_lovrSliderJointSetMotorSpring },
  { "getMaxMotorForce", l_lovrSliderJointGetMaxMotorForce },
  { "setMaxMotorForce", l_lovrSliderJointSetMaxMotorForce },
  { "getMotorForce", l_lovrSliderJointGetMotorForce },
  { "getSpring", l_lovrSliderJointGetSpring },
  { "setSpring", l_lovrSliderJointSetSpring },
  { NULL, NULL }
};
