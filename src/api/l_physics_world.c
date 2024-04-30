#include "api.h"
#include "physics/physics.h"
#include "core/maf.h"
#include "util.h"
#include <float.h>
#include <stdbool.h>
#include <string.h>

static int luax_pushcastresult(lua_State* L, CastResult* hit) {
  luax_pushtype(L, Collider, hit->collider);
  luax_pushshape(L, hit->shape);
  lua_pushnumber(L, hit->position[0]);
  lua_pushnumber(L, hit->position[1]);
  lua_pushnumber(L, hit->position[2]);
  lua_pushnumber(L, hit->normal[0]);
  lua_pushnumber(L, hit->normal[1]);
  lua_pushnumber(L, hit->normal[2]);
  lua_pushnumber(L, hit->fraction);
  return 9;
}

static int luax_pushcollideresult(lua_State* L, CollideResult* hit) {
  luax_pushtype(L, Collider, hit->collider);
  luax_pushshape(L, hit->shape);
  lua_pushnumber(L, hit->position[0]);
  lua_pushnumber(L, hit->position[1]);
  lua_pushnumber(L, hit->position[2]);
  lua_pushnumber(L, hit->normal[0]);
  lua_pushnumber(L, hit->normal[1]);
  lua_pushnumber(L, hit->normal[2]);
  return 8;
}

static float castCallback(void* userdata, CastResult* hit) {
  lua_State* L = userdata;
  lua_pushvalue(L, -1);
  int n = luax_pushcastresult(L, hit);
  lua_call(L, n, 1);
  float fraction = lua_type(L, -1) == LUA_TNUMBER ? luax_tofloat(L, -1) : 1.f;
  lua_pop(L, 1);
  return fraction;
}

static float castClosestCallback(void* userdata, CastResult* hit) {
  *((CastResult*) userdata) = *hit;
  return hit->fraction;
}

static float collideCallback(void* userdata, CollideResult* hit) {
  lua_State* L = userdata;
  lua_pushvalue(L, -1);
  int n = luax_pushcollideresult(L, hit);
  lua_call(L, n, 1);
  bool stop = lua_type(L, -1) == LUA_TBOOLEAN && lua_toboolean(L, -1);
  lua_pop(L, 1);
  return stop ? -FLT_MAX : FLT_MAX;
}

static float collideFirstCallback(void* userdata, CollideResult* hit) {
  *((CollideResult*) userdata) = *hit;
  return -FLT_MAX;
}

static void queryCallback(void* userdata, Collider* collider) {
  lua_State* L = userdata;
  lua_pushvalue(L, -1);
  luax_pushtype(L, Collider, collider);
  lua_call(L, 2, 0);
}

static void queryNoCallback(void* userdata, Collider* collider) {
  *((Collider**) userdata) = collider;
}

static int l_lovrWorldNewCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  Shape* shape = luax_totype(L, 2, Shape);
  float position[3];
  luax_readvec3(L, 2 + !!shape, position, NULL);
  Collider* collider = lovrColliderCreate(world, shape, position);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  return 1;
}

static int l_lovrWorldNewBoxCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float position[3];
  int index = luax_readvec3(L, 2, position, NULL);
  BoxShape* shape = luax_newboxshape(L, index);
  Collider* collider = lovrColliderCreate(world, shape, position);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  lovrRelease(shape, lovrShapeDestroy);
  return 1;
}

static int l_lovrWorldNewCapsuleCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float position[3];
  int index = luax_readvec3(L, 2, position, NULL);
  CapsuleShape* shape = luax_newcapsuleshape(L, index);
  Collider* collider = lovrColliderCreate(world, shape, position);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  lovrRelease(shape, lovrShapeDestroy);
  return 1;
}

static int l_lovrWorldNewCylinderCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float position[3];
  int index = luax_readvec3(L, 2, position, NULL);
  CylinderShape* shape = luax_newcylindershape(L, index);
  Collider* collider = lovrColliderCreate(world, shape, position);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  lovrRelease(shape, lovrShapeDestroy);
  return 1;
}

static int l_lovrWorldNewConvexCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float position[3];
  int index = luax_readvec3(L, 2, position, NULL);
  ConvexShape* shape = luax_newconvexshape(L, index);
  Collider* collider = lovrColliderCreate(world, shape, position);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  lovrRelease(shape, lovrShapeDestroy);
  return 1;
}

static int l_lovrWorldNewSphereCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float position[3];
  int index = luax_readvec3(L, 2, position, NULL);
  SphereShape* shape = luax_newsphereshape(L, index);
  Collider* collider = lovrColliderCreate(world, shape, position);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  lovrRelease(shape, lovrShapeDestroy);
  return 1;
}

static int l_lovrWorldNewMeshCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  MeshShape* shape = luax_newmeshshape(L, 2);
  float position[3] = { 0.f, 0.f, 0.f };
  Collider* collider = lovrColliderCreate(world, shape, position);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  lovrRelease(shape, lovrShapeDestroy);
  return 1;
}

static int l_lovrWorldNewTerrainCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  TerrainShape* shape = luax_newterrainshape(L, 2);
  float position[3] = { 0.f, 0.f, 0.f };
  Collider* collider = lovrColliderCreate(world, shape, position);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  lovrRelease(shape, lovrShapeDestroy);
  return 1;
}

static int l_lovrWorldDestroy(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  lovrWorldDestroyData(world);
  return 0;
}

static int l_lovrWorldGetTags(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  uint32_t count;
  char** tags = lovrWorldGetTags(world, &count);
  lua_createtable(L, (int) count, 0);
  for (uint32_t i = 0; i < count; i++) {
    lua_pushstring(L, tags[i]);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int l_lovrWorldGetColliderCount(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  uint32_t count = lovrWorldGetColliderCount(world);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrWorldGetJointCount(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  uint32_t count = lovrWorldGetJointCount(world);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrWorldGetColliders(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  int index = 1;
  Collider* collider = NULL;
  lua_createtable(L, (int) lovrWorldGetColliderCount(world), 0);
  while ((collider = lovrWorldGetColliders(world, collider)) != NULL) {
    luax_pushtype(L, Collider, collider);
    lua_rawseti(L, -2, index++);
  }
  return 1;
}

static int l_lovrWorldGetJoints(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  int index = 1;
  Joint* joint = NULL;
  lua_createtable(L, (int) lovrWorldGetJointCount(world), 0);
  while ((joint = lovrWorldGetJoints(world, joint)) != NULL) {
    luax_pushjoint(L, joint);
    lua_rawseti(L, -2, index++);
  }
  return 1;
}

static int l_lovrWorldGetGravity(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float gravity[3];
  lovrWorldGetGravity(world, gravity);
  lua_pushnumber(L, gravity[0]);
  lua_pushnumber(L, gravity[1]);
  lua_pushnumber(L, gravity[2]);
  return 3;
}

static int l_lovrWorldSetGravity(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float gravity[3];
  luax_readvec3(L, 2, gravity, NULL);
  lovrWorldSetGravity(world, gravity);
  return 0;
}

static int l_lovrWorldUpdate(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float dt = luax_checkfloat(L, 2);
  lovrWorldUpdate(world, dt);
  return 0;
}

static uint32_t luax_checktagmask(lua_State* L, int index, World* world) {
  if (lua_isnoneornil(L, index)) {
    return ~0u;
  } else {
    size_t length;
    const char* string = luaL_checklstring(L, index, &length);
    return lovrWorldGetTagMask(world, string, length);
  }
}

static int l_lovrWorldRaycast(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  int index = 2;
  float start[3], end[3];
  index = luax_readvec3(L, index, start, NULL);
  index = luax_readvec3(L, index, end, NULL);
  uint32_t filter = luax_checktagmask(L, index++, world);
  if (lua_isnoneornil(L, index)) {
    CastResult hit;
    if (lovrWorldRaycast(world, start, end, filter, castClosestCallback, &hit)) {
      return luax_pushcastresult(L, &hit);
    }
  } else {
    luaL_checktype(L, index, LUA_TFUNCTION);
    lua_settop(L, index);
    lovrWorldRaycast(world, start, end, filter, castCallback, L);
  }
  return 0;
}

static int l_lovrWorldShapecast(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  int index = 2;
  float pose[7], scale, end[3];
  Shape* shape = luax_checkshape(L, index++);
  index = luax_readvec3(L, index, pose, NULL);
  index = luax_readvec3(L, index, end, NULL);
  scale = luax_optfloat(L, index++, 1.f);
  index = luax_readquat(L, index, pose + 3, NULL);
  uint32_t filter = luax_checktagmask(L, index++, world);
  if (lua_isnoneornil(L, index)) {
    CastResult hit;
    if (lovrWorldShapecast(world, shape, pose, scale, end, filter, castClosestCallback, &hit)) {
      return luax_pushcastresult(L, &hit);
    }
  } else {
    luaL_checktype(L, index, LUA_TFUNCTION);
    lua_settop(L, index);
    lovrWorldShapecast(world, shape, pose, scale, end, filter, castCallback, L);
  }
  return 0;
}

static int l_lovrWorldCollideShape(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  int index;
  Shape* shape;
  float pose[7], scale;
  Collider* collider = luax_totype(L, 2, Collider);
  if (collider) {
    shape = lovrColliderGetShape(collider);
    lovrColliderGetPosition(collider, pose);
    lovrColliderGetOrientation(collider, pose + 3);
    scale = 1.f;
    index = 3;
  } else {
    shape = luax_checkshape(L, 2);
    index = luax_readvec3(L, 3, pose, NULL);
    scale = luax_optfloat(L, index++, 1.f);
    index = luax_readquat(L, index, pose + 3, NULL);
  }
  uint32_t filter = luax_checktagmask(L, index++, world);
  if (lua_isnoneornil(L, index)) {
    CollideResult hit;
    if (lovrWorldCollideShape(world, shape, pose, scale, filter, collideFirstCallback, &hit)) {
      return luax_pushcollideresult(L, &hit);
    }
  } else {
    luaL_checktype(L, index, LUA_TFUNCTION);
    lua_settop(L, index);
    lovrWorldCollideShape(world, shape, pose, scale, filter, collideCallback, L);
  }
  return 0;
}

static int l_lovrWorldQueryBox(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float position[3], size[3];
  int index = 2;
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readvec3(L, index, size, NULL);
  uint32_t filter = luax_checktagmask(L, index++, world);
  if (lua_isnoneornil(L, index)) {
    Collider* collider = NULL;
    lovrWorldQueryBox(world, position, size, filter, queryNoCallback, &collider);
    luax_pushtype(L, Collider, collider);
    return 1;
  } else {
    luaL_checktype(L, index, LUA_TFUNCTION);
    lua_settop(L, index);
    lovrWorldQueryBox(world, position, size, filter, queryCallback, L);
    return 0;
  }
}

static int l_lovrWorldQuerySphere(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float position[3];
  int index = luax_readvec3(L, 2, position, NULL);
  float radius = luax_checkfloat(L, index++);
  uint32_t filter = luax_checktagmask(L, index++, world);
  if (lua_isnoneornil(L, index)) {
    Collider* collider = NULL;
    lovrWorldQuerySphere(world, position, radius, filter, queryNoCallback, &collider);
    luax_pushtype(L, Collider, collider);
    return 1;
  } else {
    luaL_checktype(L, index, LUA_TFUNCTION);
    lua_settop(L, index);
    lovrWorldQuerySphere(world, position, radius, filter, queryCallback, L);
    return 0;
  }
}

static int l_lovrWorldDisableCollisionBetween(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  const char* tag1 = luaL_checkstring(L, 2);
  const char* tag2 = luaL_checkstring(L, 3);
  lovrWorldDisableCollisionBetween(world, tag1, tag2);
  return 0;
}

static int l_lovrWorldEnableCollisionBetween(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  const char* tag1 = luaL_checkstring(L, 2);
  const char* tag2 = luaL_checkstring(L, 3);
  lovrWorldEnableCollisionBetween(world, tag1, tag2);
  return 0;
}

static int l_lovrWorldIsCollisionEnabledBetween(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  const char* tag1 = lua_tostring(L, 2);
  const char* tag2 = lua_tostring(L, 3);
  lua_pushboolean(L, lovrWorldIsCollisionEnabledBetween(world, tag1, tag2));
  return 1;
}

// Deprecated

static int l_lovrWorldGetTightness(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float tightness = lovrWorldGetTightness(world);
  lovrCheck(tightness >= 0, "Negative tightness factor causes simulation instability");
  lua_pushnumber(L, tightness);
  return 1;
}

static int l_lovrWorldSetTightness(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float tightness = luax_checkfloat(L, 2);
  lovrWorldSetTightness(world, tightness);
  return 0;
}

static int l_lovrWorldGetResponseTime(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float responseTime = lovrWorldGetResponseTime(world);
  lua_pushnumber(L, responseTime);
  return 1;
}

static int l_lovrWorldSetResponseTime(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float responseTime = luax_checkfloat(L, 2);
  lovrCheck(responseTime >= 0, "Negative response time causes simulation instability");
  lovrWorldSetResponseTime(world, responseTime);
  return 0;
}

static int l_lovrWorldGetLinearDamping(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float damping, threshold;
  lovrWorldGetLinearDamping(world, &damping, &threshold);
  lua_pushnumber(L, damping);
  lua_pushnumber(L, threshold);
  return 2;
}

static int l_lovrWorldSetLinearDamping(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float damping = luax_checkfloat(L, 2);
  float threshold = luax_optfloat(L, 3, 0.0f);
  lovrWorldSetLinearDamping(world, damping, threshold);
  return 0;
}

static int l_lovrWorldGetAngularDamping(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float damping, threshold;
  lovrWorldGetAngularDamping(world, &damping, &threshold);
  lua_pushnumber(L, damping);
  lua_pushnumber(L, threshold);
  return 2;
}

static int l_lovrWorldSetAngularDamping(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float damping = luax_checkfloat(L, 2);
  float threshold = luax_optfloat(L, 3, 0.0f);
  lovrWorldSetAngularDamping(world, damping, threshold);
  return 0;
}

static int l_lovrWorldIsSleepingAllowed(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  lua_pushboolean(L, lovrWorldIsSleepingAllowed(world));
  return 1;
}

static int l_lovrWorldSetSleepingAllowed(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  bool allowed = lua_toboolean(L, 2);
  lovrWorldSetSleepingAllowed(world, allowed);
  return 0;
}

static int l_lovrWorldGetStepCount(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  int iterations = lovrWorldGetStepCount(world);
  lua_pushnumber(L, iterations);
  return 1;
}

static int l_lovrWorldSetStepCount(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  int iterations = luaL_checkinteger(L, 2);
  lovrWorldSetStepCount(world, iterations);
  return 0;
}

const luaL_Reg lovrWorld[] = {
  { "newCollider", l_lovrWorldNewCollider },
  { "newBoxCollider", l_lovrWorldNewBoxCollider },
  { "newCapsuleCollider", l_lovrWorldNewCapsuleCollider },
  { "newCylinderCollider", l_lovrWorldNewCylinderCollider },
  { "newConvexCollider", l_lovrWorldNewConvexCollider },
  { "newSphereCollider", l_lovrWorldNewSphereCollider },
  { "newMeshCollider", l_lovrWorldNewMeshCollider },
  { "newTerrainCollider", l_lovrWorldNewTerrainCollider },
  { "destroy", l_lovrWorldDestroy },
  { "getTags", l_lovrWorldGetTags },
  { "getColliderCount", l_lovrWorldGetColliderCount },
  { "getJointCount", l_lovrWorldGetJointCount },
  { "getColliders", l_lovrWorldGetColliders },
  { "getJoints", l_lovrWorldGetJoints },
  { "update", l_lovrWorldUpdate },
  { "raycast", l_lovrWorldRaycast },
  { "shapecast", l_lovrWorldShapecast },
  { "collideShape", l_lovrWorldCollideShape },
  { "queryBox", l_lovrWorldQueryBox },
  { "querySphere", l_lovrWorldQuerySphere },
  { "getGravity", l_lovrWorldGetGravity },
  { "setGravity", l_lovrWorldSetGravity },
  { "disableCollisionBetween", l_lovrWorldDisableCollisionBetween },
  { "enableCollisionBetween", l_lovrWorldEnableCollisionBetween },
  { "isCollisionEnabledBetween", l_lovrWorldIsCollisionEnabledBetween },

  // Deprecated
  { "getTightness", l_lovrWorldGetTightness },
  { "setTightness", l_lovrWorldSetTightness },
  { "getResponseTime", l_lovrWorldGetResponseTime },
  { "setResponseTime", l_lovrWorldSetResponseTime },
  { "getLinearDamping", l_lovrWorldGetLinearDamping },
  { "setLinearDamping", l_lovrWorldSetLinearDamping },
  { "getAngularDamping", l_lovrWorldGetAngularDamping },
  { "setAngularDamping", l_lovrWorldSetAngularDamping },
  { "isSleepingAllowed", l_lovrWorldIsSleepingAllowed },
  { "setSleepingAllowed", l_lovrWorldSetSleepingAllowed },
  { "getStepCount", l_lovrWorldGetStepCount },
  { "setStepCount", l_lovrWorldSetStepCount },

  { NULL, NULL }
};
