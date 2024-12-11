#include "api.h"
#include "physics/physics.h"
#include "core/maf.h"
#include "util.h"
#include <string.h>

StringEntry lovrShapeType[] = {
  [SHAPE_BOX] = ENTRY("box"),
  [SHAPE_SPHERE] = ENTRY("sphere"),
  [SHAPE_CAPSULE] = ENTRY("capsule"),
  [SHAPE_CYLINDER] = ENTRY("cylinder"),
  [SHAPE_CONVEX] = ENTRY("convex"),
  [SHAPE_MESH] = ENTRY("mesh"),
  [SHAPE_TERRAIN] = ENTRY("terrain"),
  { 0 }
};

StringEntry lovrJointType[] = {
  [JOINT_WELD] = ENTRY("weld"),
  [JOINT_BALL] = ENTRY("ball"),
  [JOINT_DISTANCE] = ENTRY("distance"),
  [JOINT_HINGE] = ENTRY("hinge"),
  [JOINT_SLIDER] = ENTRY("slider"),
  { 0 }
};

StringEntry lovrMotorMode[] = {
  [MOTOR_OFF] = ENTRY("off"),
  [MOTOR_VELOCITY] = ENTRY("velocity"),
  [MOTOR_POSITION] = ENTRY("position"),
  { 0 }
};

static int l_lovrPhysicsNewWorld(lua_State* L) {
  WorldInfo info = {
    .maxColliders = 16384,
    .threadSafe = true,
    .allowSleep = true,
    .stabilization = .2f,
    .maxOverlap = .01f,
    .restitutionThreshold = 1.f,
    .velocitySteps = 10,
    .positionSteps = 2
  };

  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "maxColliders");
    if (!lua_isnil(L, -1)) info.maxColliders = luax_checku32(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "threadSafe");
    if (!lua_isnil(L, -1)) info.threadSafe = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "allowSleep");
    if (!lua_isnil(L, -1)) info.allowSleep = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "stabilization");
    if (!lua_isnil(L, -1)) info.stabilization = luax_checkfloat(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "maxOverlap");
    if (!lua_isnil(L, -1)) info.maxOverlap = luax_checkfloat(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "restitutionThreshold");
    if (!lua_isnil(L, -1)) info.restitutionThreshold = luax_checkfloat(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "velocitySteps");
    if (!lua_isnil(L, -1)) info.velocitySteps = luax_checku32(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "positionSteps");
    if (!lua_isnil(L, -1)) info.positionSteps = luax_checku32(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "tags");
    if (!lua_isnil(L, -1)) {
      luax_check(L, lua_istable(L, -1), "World tag list should be a table");
      luax_check(L, info.tagCount <= MAX_TAGS, "Max number of world tags is %d", MAX_TAGS);
      info.tagCount = luax_len(L, -1);
      for (uint32_t i = 0; i < info.tagCount; i++) {
        lua_rawgeti(L, -1, (int) i + 1);
        if (lua_isstring(L, -1)) {
          info.tags[i] = lua_tostring(L, -1);
        } else {
          luaL_error(L, "World tags must be a table of strings");
          return 0;
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "staticTags");
    if (!lua_isnil(L, -1)) {
      luax_check(L, lua_istable(L,  -1), "World static tag list should be a table");
      int length = luax_len(L, -1);
      for (int i = 0; i < length; i++) {
        lua_rawgeti(L, -1, i + 1);
        const char* string = lua_tostring(L, -1);
        luax_check(L, string, "Static tag list must be a table of strings");
        for (uint32_t j = 0; j < info.tagCount; j++) {
          if (!strcmp(string, info.tags[j])) {
            info.staticTagMask |= (1 << j);
            break;
          }
          luax_check(L, j < info.tagCount - 1, "Static tag '%s' does not exist", string);
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
  } else { // Deprecated
    info.allowSleep = lua_gettop(L) < 4 || lua_toboolean(L, 4);
    if (lua_type(L, 5) == LUA_TTABLE) {
      info.tagCount = luax_len(L, 5);
      luax_check(L, info.tagCount <= MAX_TAGS, "Max number of world tags is %d", MAX_TAGS);
      for (uint32_t i = 0; i < info.tagCount; i++) {
        lua_rawgeti(L, -1, (int) i + 1);
        if (lua_isstring(L, -1)) {
          info.tags[i] = lua_tostring(L, -1);
        } else {
          luaL_error(L, "World tags must be a table of strings");
          return 0;
        }
        lua_pop(L, 1);
      }
    } else {
      info.tagCount = 0;
    }
  }

  World* world = lovrWorldCreate(&info);

  if (!lua_istable(L, 1)) {
    float gravity[3];
    gravity[0] = luax_optfloat(L, 1, 0.f);
    gravity[1] = luax_optfloat(L, 2, -9.81f);
    gravity[2] = luax_optfloat(L, 3, 0.f);
    lovrWorldSetGravity(world, gravity);
  }

  luax_pushtype(L, World, world);
  lovrRelease(world, lovrWorldDestroy);
  return 1;
}

static int l_lovrPhysicsNewBoxShape(lua_State* L) {
  BoxShape* box = luax_newboxshape(L, 1);
  luax_pushtype(L, BoxShape, box);
  lovrRelease(box, lovrShapeDestroy);
  return 1;
}

static int l_lovrPhysicsNewSphereShape(lua_State* L) {
  SphereShape* sphere = luax_newsphereshape(L, 1);
  luax_pushtype(L, SphereShape, sphere);
  lovrRelease(sphere, lovrShapeDestroy);
  return 1;
}

static int l_lovrPhysicsNewCapsuleShape(lua_State* L) {
  CapsuleShape* capsule = luax_newcapsuleshape(L, 1);
  luax_pushtype(L, CapsuleShape, capsule);
  lovrRelease(capsule, lovrShapeDestroy);
  return 1;
}

static int l_lovrPhysicsNewCylinderShape(lua_State* L) {
  CylinderShape* cylinder = luax_newcylindershape(L, 1);
  luax_pushtype(L, CylinderShape, cylinder);
  lovrRelease(cylinder, lovrShapeDestroy);
  return 1;
}

static int l_lovrPhysicsNewConvexShape(lua_State* L) {
  ConvexShape* convex = luax_newconvexshape(L, 1);
  luax_pushtype(L, ConvexShape, convex);
  lovrRelease(convex, lovrShapeDestroy);
  return 1;
}

static int l_lovrPhysicsNewMeshShape(lua_State* L) {
  MeshShape* mesh = luax_newmeshshape(L, 1);
  luax_pushtype(L, MeshShape, mesh);
  lovrRelease(mesh, lovrShapeDestroy);
  return 1;
}

static int l_lovrPhysicsNewTerrainShape(lua_State* L) {
  TerrainShape* terrain = luax_newterrainshape(L, 1);
  luax_pushtype(L, TerrainShape, terrain);
  lovrRelease(terrain, lovrShapeDestroy);
  return 1;
}

static int l_lovrPhysicsNewWeldJoint(lua_State* L) {
  Collider* a = luax_totype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  WeldJoint* joint = lovrWeldJointCreate(a, b);
  luax_assert(L, joint);
  luax_pushtype(L, WeldJoint, joint);
  lovrRelease(joint, lovrJointDestroy);
  return 1;
}

static int l_lovrPhysicsNewBallJoint(lua_State* L) {
  Collider* a = luax_totype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float anchor[3];
  if (lua_isnoneornil(L, 3)) {
    lovrColliderGetRawPosition(a ? a : b, anchor);
  } else {
    luax_readvec3(L, 3, anchor, NULL);
  }
  BallJoint* joint = lovrBallJointCreate(a, b, anchor);
  luax_assert(L, joint);
  luax_pushtype(L, BallJoint, joint);
  lovrRelease(joint, lovrJointDestroy);
  return 1;
}

static int l_lovrPhysicsNewConeJoint(lua_State* L) {
  Collider* a = luax_totype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);

  int index = 3;
  float anchor[3];
  if (lua_isnoneornil(L, index)) {
    lovrColliderGetRawPosition(a ? a : b, anchor);
  } else {
    index = luax_readvec3(L, index, anchor, NULL);
  }

  float axis[3];
  if (lua_isnoneornil(L, index)) {
    lovrColliderGetRawPosition(b, axis);
    vec3_sub(axis, anchor);
    vec3_normalize(axis);
  } else {
    luax_readvec3(L, index, axis, NULL);
  }

  ConeJoint* joint = lovrConeJointCreate(a, b, anchor, axis);
  luax_assert(L, joint);
  luax_pushtype(L, ConeJoint, joint);
  lovrRelease(joint, lovrJointDestroy);
  return 1;
}

static int l_lovrPhysicsNewDistanceJoint(lua_State* L) {
  Collider* a = luax_totype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float anchor1[3], anchor2[3];
  if (lua_isnoneornil(L, 3)) {
    lovrColliderGetRawPosition(a ? a : b, anchor1);
    lovrColliderGetRawPosition(b, anchor2);
  } else {
    int index = luax_readvec3(L, 3, anchor1, NULL);
    luax_readvec3(L, index, anchor2, NULL);
  }
  DistanceJoint* joint = lovrDistanceJointCreate(a, b, anchor1, anchor2);
  luax_assert(L, joint);
  luax_pushtype(L, DistanceJoint, joint);
  lovrRelease(joint, lovrJointDestroy);
  return 1;
}

static int l_lovrPhysicsNewHingeJoint(lua_State* L) {
  Collider* a = luax_totype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);

  int index = 3;
  float anchor[3];
  if (lua_isnoneornil(L, index)) {
    lovrColliderGetRawPosition(a ? a : b, anchor);
  } else {
    index = luax_readvec3(L, index, anchor, NULL);
  }

  float axis[3];
  if (lua_isnoneornil(L, index)) {
    lovrColliderGetRawPosition(b, axis);
    vec3_sub(axis, anchor);
    vec3_normalize(axis);
  } else {
    luax_readvec3(L, index, axis, NULL);
  }

  HingeJoint* joint = lovrHingeJointCreate(a, b, anchor, axis);
  luax_assert(L, joint);
  luax_pushtype(L, HingeJoint, joint);
  lovrRelease(joint, lovrJointDestroy);
  return 1;
}

static int l_lovrPhysicsNewSliderJoint(lua_State* L) {
  Collider* a = luax_totype(L, 1, Collider);
  Collider* b = luax_checktype(L, 2, Collider);
  float axis[3];
  luax_readvec3(L, 3, axis, NULL);
  SliderJoint* joint = lovrSliderJointCreate(a, b, axis);
  luax_assert(L, joint);
  luax_pushtype(L, SliderJoint, joint);
  lovrRelease(joint, lovrJointDestroy);
  return 1;
}

static const luaL_Reg lovrPhysics[] = {
  { "newWorld", l_lovrPhysicsNewWorld },
  { "newBoxShape", l_lovrPhysicsNewBoxShape },
  { "newSphereShape", l_lovrPhysicsNewSphereShape },
  { "newCapsuleShape", l_lovrPhysicsNewCapsuleShape },
  { "newCylinderShape", l_lovrPhysicsNewCylinderShape },
  { "newConvexShape", l_lovrPhysicsNewConvexShape },
  { "newMeshShape", l_lovrPhysicsNewMeshShape },
  { "newTerrainShape", l_lovrPhysicsNewTerrainShape },
  { "newWeldJoint", l_lovrPhysicsNewWeldJoint },
  { "newBallJoint", l_lovrPhysicsNewBallJoint },
  { "newConeJoint", l_lovrPhysicsNewConeJoint },
  { "newDistanceJoint", l_lovrPhysicsNewDistanceJoint },
  { "newHingeJoint", l_lovrPhysicsNewHingeJoint },
  { "newSliderJoint", l_lovrPhysicsNewSliderJoint },
  { NULL, NULL }
};

extern const luaL_Reg lovrWorld[];
extern const luaL_Reg lovrCollider[];
extern const luaL_Reg lovrContact[];
extern const luaL_Reg lovrBoxShape[];
extern const luaL_Reg lovrSphereShape[];
extern const luaL_Reg lovrCapsuleShape[];
extern const luaL_Reg lovrCylinderShape[];
extern const luaL_Reg lovrConvexShape[];
extern const luaL_Reg lovrMeshShape[];
extern const luaL_Reg lovrTerrainShape[];
extern const luaL_Reg lovrWeldJoint[];
extern const luaL_Reg lovrBallJoint[];
extern const luaL_Reg lovrConeJoint[];
extern const luaL_Reg lovrDistanceJoint[];
extern const luaL_Reg lovrHingeJoint[];
extern const luaL_Reg lovrSliderJoint[];

static void luax_unref(void* object, uintptr_t userdata) {
  if (!userdata) return;
  lua_State* L = (lua_State*) userdata;
  lua_pushlightuserdata(L, object);
  lua_pushnil(L);
  lua_rawset(L, LUA_REGISTRYINDEX);
}

int luaopen_lovr_physics(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrPhysics);
  luax_registertype(L, World);
  luax_registertype(L, Collider);
  luax_registertype(L, Contact);
  luax_registertype(L, BoxShape);
  luax_registertype(L, SphereShape);
  luax_registertype(L, CapsuleShape);
  luax_registertype(L, CylinderShape);
  luax_registertype(L, ConvexShape);
  luax_registertype(L, MeshShape);
  luax_registertype(L, TerrainShape);
  luax_registertype(L, WeldJoint);
  luax_registertype(L, BallJoint);
  luax_registertype(L, ConeJoint);
  luax_registertype(L, DistanceJoint);
  luax_registertype(L, HingeJoint);
  luax_registertype(L, SliderJoint);
  lovrPhysicsInit(luax_unref);
  luax_atexit(L, lovrPhysicsDestroy);
  return 1;
}
