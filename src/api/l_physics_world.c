#include "api.h"
#include "physics/physics.h"
#include "core/maf.h"
#include "util.h"
#include <float.h>
#include <stdbool.h>
#include <string.h>

static World* luax_checkworld(lua_State* L, int index) {
  World* world = luax_checktype(L, index, World);
  luax_check(L, !lovrWorldIsDestroyed(world), "Attempt to use a destroyed World");
  return world;
}

static int luax_pushcastresult(lua_State* L, CastResult* hit) {
  luax_pushtype(L, Collider, hit->collider);
  luax_pushshape(L, hit->shape);
  lua_pushnumber(L, hit->position[0]);
  lua_pushnumber(L, hit->position[1]);
  lua_pushnumber(L, hit->position[2]);
  lua_pushnumber(L, hit->normal[0]);
  lua_pushnumber(L, hit->normal[1]);
  lua_pushnumber(L, hit->normal[2]);
  if (hit->triangle == ~0u) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, hit->triangle + 1);
  }
  lua_pushnumber(L, hit->fraction);
  return 10;
}

static int luax_pushoverlapresult(lua_State* L, OverlapResult* hit) {
  luax_pushtype(L, Collider, hit->collider);
  luax_pushshape(L, hit->shape);
  lua_pushnumber(L, hit->position[0]);
  lua_pushnumber(L, hit->position[1]);
  lua_pushnumber(L, hit->position[2]);
  lua_pushnumber(L, hit->normal[0]);
  lua_pushnumber(L, hit->normal[1]);
  lua_pushnumber(L, hit->normal[2]);
  if (hit->triangle == ~0u) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, hit->triangle + 1);
  }
  return 9;
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

static float overlapCallback(void* userdata, OverlapResult* hit) {
  lua_State* L = userdata;
  lua_pushvalue(L, -1);
  int n = luax_pushoverlapresult(L, hit);
  lua_call(L, n, 1);
  bool stop = lua_type(L, -1) == LUA_TBOOLEAN && lua_toboolean(L, -1);
  lua_pop(L, 1);
  return stop ? -FLT_MAX : FLT_MAX;
}

static float overlapFirstCallback(void* userdata, OverlapResult* hit) {
  *((OverlapResult*) userdata) = *hit;
  return -FLT_MAX;
}

static void queryCallback(void* userdata, Collider* collider) {
  lua_State* L = userdata;
  lua_pushvalue(L, -1);
  luax_pushtype(L, Collider, collider);
  lua_call(L, 1, 0);
}

static void queryNoCallback(void* userdata, Collider* collider) {
  *((Collider**) userdata) = collider;
}

static bool filterCallback(void* userdata, World* world, Collider* a, Collider* b) {
  lua_State* L = userdata;
  luax_pushstash(L, "lovr.world.filter");
  luax_pushtype(L, World, world);
  lua_rawget(L, -2);
  lua_remove(L, -2);
  luax_pushtype(L, Collider, a);
  luax_pushtype(L, Collider, b);
  if (lua_pcall(L, 2, 1, 0)) {
    lua_settop(L, 3); // Only keep first error
  } else {
    bool accept = lua_type(L, -1) != LUA_TBOOLEAN || lua_toboolean(L, -1);
    lua_pop(L, 1);
    return accept;
  }
  return true;
}

static void enterCallback(void* userdata, World* world, Collider* a, Collider* b, Contact* contact) {
  lua_State* L = userdata;
  luax_pushstash(L, "lovr.world.enter");
  luax_pushtype(L, World, world);
  lua_rawget(L, -2);
  lua_remove(L, -2);
  luax_pushtype(L, Collider, a);
  luax_pushtype(L, Collider, b);
  luax_pushtype(L, Contact, contact);
  if (lua_pcall(L, 3, 0, 0)) {
    lua_settop(L, 3); // Only keep first error
  }
}

static void exitCallback(void* userdata, World* world, Collider* a, Collider* b) {
  lua_State* L = userdata;
  luax_pushstash(L, "lovr.world.exit");
  luax_pushtype(L, World, world);
  lua_rawget(L, -2);
  lua_remove(L, -2);
  luax_pushtype(L, Collider, a);
  luax_pushtype(L, Collider, b);
  if (lua_pcall(L, 2, 0, 0)) {
    lua_settop(L, 3); // Only keep first error
  }
}

static void contactCallback(void* userdata, World* world, Collider* a, Collider* b, Contact* contact) {
  lua_State* L = userdata;
  luax_pushstash(L, "lovr.world.contact");
  luax_pushtype(L, World, world);
  lua_rawget(L, -2);
  lua_remove(L, -2);
  luax_pushtype(L, Collider, a);
  luax_pushtype(L, Collider, b);
  luax_pushtype(L, Contact, contact);
  if (lua_pcall(L, 3, 0, 0)) {
    lua_settop(L, 3); // Only keep first error
  }
}

static int l_lovrWorldNewCollider(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float position[3];
  luax_readvec3(L, 2, position, NULL);
  Collider* collider = lovrColliderCreate(world, position, NULL);
  luax_assert(L, collider);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  return 1;
}

static int l_lovrWorldNewBoxCollider(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float position[3];
  int index = luax_readvec3(L, 2, position, NULL);
  BoxShape* shape = luax_newboxshape(L, index);
  Collider* collider = lovrColliderCreate(world, position, shape);
  lovrRelease(shape, lovrShapeDestroy);
  luax_assert(L, collider);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  return 1;
}

static int l_lovrWorldNewCapsuleCollider(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float position[3];
  int index = luax_readvec3(L, 2, position, NULL);
  CapsuleShape* shape = luax_newcapsuleshape(L, index);
  Collider* collider = lovrColliderCreate(world, position, shape);
  lovrRelease(shape, lovrShapeDestroy);
  luax_assert(L, collider);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  return 1;
}

static int l_lovrWorldNewCylinderCollider(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float position[3];
  int index = luax_readvec3(L, 2, position, NULL);
  CylinderShape* shape = luax_newcylindershape(L, index);
  Collider* collider = lovrColliderCreate(world, position, shape);
  lovrRelease(shape, lovrShapeDestroy);
  luax_assert(L, collider);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  return 1;
}

static int l_lovrWorldNewConvexCollider(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float position[3];
  int index = luax_readvec3(L, 2, position, NULL);
  ConvexShape* shape = luax_newconvexshape(L, index);
  Collider* collider = lovrColliderCreate(world, position, shape);
  lovrRelease(shape, lovrShapeDestroy);
  luax_assert(L, collider);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  return 1;
}

static int l_lovrWorldNewSphereCollider(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float position[3];
  int index = luax_readvec3(L, 2, position, NULL);
  SphereShape* shape = luax_newsphereshape(L, index);
  Collider* collider = lovrColliderCreate(world, position, shape);
  lovrRelease(shape, lovrShapeDestroy);
  luax_assert(L, collider);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  return 1;
}

static int l_lovrWorldNewMeshCollider(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  MeshShape* shape = luax_newmeshshape(L, 2);
  float position[3] = { 0.f, 0.f, 0.f };
  Collider* collider = lovrColliderCreate(world, position, shape);
  lovrRelease(shape, lovrShapeDestroy);
  luax_assert(L, collider);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  return 1;
}

static int l_lovrWorldNewTerrainCollider(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  TerrainShape* shape = luax_newterrainshape(L, 2);
  float position[3] = { 0.f, 0.f, 0.f };
  Collider* collider = lovrColliderCreate(world, position, shape);
  lovrRelease(shape, lovrShapeDestroy);
  luax_assert(L, collider);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  return 1;
}

static int l_lovrWorldDestroy(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  lovrWorldDestruct(world);
  return 0;
}

static int l_lovrWorldIsDestroyed(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  bool destroyed = lovrWorldIsDestroyed(world);
  lua_pushboolean(L, destroyed);
  return 1;
}

static int l_lovrWorldGetTags(lua_State* L) {
  World* world = luax_checkworld(L, 1);
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
  World* world = luax_checkworld(L, 1);
  uint32_t count = lovrWorldGetColliderCount(world);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrWorldGetJointCount(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  uint32_t count = lovrWorldGetJointCount(world);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrWorldGetColliders(lua_State* L) {
  World* world = luax_checkworld(L, 1);
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
  World* world = luax_checkworld(L, 1);
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
  World* world = luax_checkworld(L, 1);
  float gravity[3];
  lovrWorldGetGravity(world, gravity);
  lua_pushnumber(L, gravity[0]);
  lua_pushnumber(L, gravity[1]);
  lua_pushnumber(L, gravity[2]);
  return 3;
}

static int l_lovrWorldSetGravity(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float gravity[3];
  luax_readvec3(L, 2, gravity, NULL);
  lovrWorldSetGravity(world, gravity);
  return 0;
}

static int l_lovrWorldUpdate(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float dt = luax_checkfloat(L, 2);
  lua_settop(L, 2);
  lovrWorldUpdate(world, dt);
  if (lua_type(L, 3) == LUA_TSTRING) {
    lua_error(L);
  }
  return 0;
}

static int l_lovrWorldInterpolate(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float alpha = luax_checkfloat(L, 2);
  lovrWorldInterpolate(world, alpha);
  return 0;
}

static uint32_t luax_checktagmask(lua_State* L, int index, World* world) {
  if (lua_isnoneornil(L, index)) {
    return ~0u;
  } else {
    size_t length;
    const char* string = luaL_checklstring(L, index, &length);
    uint32_t mask = lovrWorldGetTagMask(world, string, length);
    luax_assert(L, mask);
    return mask;
  }
}

static int l_lovrWorldRaycast(lua_State* L) {
  World* world = luax_checkworld(L, 1);
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
  World* world = luax_checkworld(L, 1);
  int index = 2;
  float pose[7], end[3];
  Shape* shape = luax_checkshape(L, index++);
  index = luax_readvec3(L, index, pose, NULL);
  index = luax_readvec3(L, index, end, NULL);
  index = luax_readquat(L, index, pose + 3, NULL);
  uint32_t filter = luax_checktagmask(L, index++, world);
  if (lua_isnoneornil(L, index)) {
    CastResult hit;
    if (lovrWorldShapecast(world, shape, pose, end, filter, castClosestCallback, &hit)) {
      return luax_pushcastresult(L, &hit);
    }
  } else {
    luaL_checktype(L, index, LUA_TFUNCTION);
    lua_settop(L, index);
    lovrWorldShapecast(world, shape, pose, end, filter, castCallback, L);
  }
  return 0;
}

static int l_lovrWorldOverlapShape(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  int index;
  float pose[7];
  Shape* shape = luax_checkshape(L, 2);
  index = luax_readvec3(L, 3, pose, NULL);
  index = luax_readquat(L, index, pose + 3, NULL);
  float maxDistance = luax_optfloat(L, index++, 0.f);
  uint32_t filter = luax_checktagmask(L, index++, world);
  if (lua_isnoneornil(L, index)) {
    OverlapResult hit;
    if (lovrWorldOverlapShape(world, shape, pose, maxDistance, filter, overlapFirstCallback, &hit)) {
      return luax_pushoverlapresult(L, &hit);
    }
  } else {
    luaL_checktype(L, index, LUA_TFUNCTION);
    lua_settop(L, index);
    lovrWorldOverlapShape(world, shape, pose, maxDistance, filter, overlapCallback, L);
  }
  return 0;
}

static int l_lovrWorldQueryBox(lua_State* L) {
  World* world = luax_checkworld(L, 1);
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
  World* world = luax_checkworld(L, 1);
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
  World* world = luax_checkworld(L, 1);
  const char* tag1 = luaL_checkstring(L, 2);
  const char* tag2 = luaL_checkstring(L, 3);
  luax_assert(L, lovrWorldDisableCollisionBetween(world, tag1, tag2));
  return 0;
}

static int l_lovrWorldEnableCollisionBetween(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  const char* tag1 = luaL_checkstring(L, 2);
  const char* tag2 = luaL_checkstring(L, 3);
  luax_assert(L, lovrWorldEnableCollisionBetween(world, tag1, tag2));
  return 0;
}

static int l_lovrWorldIsCollisionEnabledBetween(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  const char* tag1 = lua_tostring(L, 2);
  const char* tag2 = lua_tostring(L, 3);
  bool enabled;
  luax_assert(L, lovrWorldIsCollisionEnabledBetween(world, tag1, tag2, &enabled));
  lua_pushboolean(L, enabled);
  return 1;
}

static int l_lovrWorldGetCallbacks(lua_State* L) {
  luax_checkworld(L, 1);
  lua_settop(L, 1);
  lua_createtable(L, 0, 3);

  luax_pushstash(L, "lovr.world.filter");
  lua_pushvalue(L, 1);
  lua_rawget(L, -2);
  lua_setfield(L, 2, "filter");
  lua_pop(L, 1);

  luax_pushstash(L, "lovr.world.enter");
  lua_pushvalue(L, 1);
  lua_rawget(L, -2);
  lua_setfield(L, 2, "enter");
  lua_pop(L, 1);

  luax_pushstash(L, "lovr.world.exit");
  lua_pushvalue(L, 1);
  lua_rawget(L, -2);
  lua_setfield(L, 2, "exit");
  lua_pop(L, 1);

  luax_pushstash(L, "lovr.world.contact");
  lua_pushvalue(L, 1);
  lua_rawget(L, -2);
  lua_setfield(L, 2, "exit");
  lua_pop(L, 1);

  return 1;
}

static int l_lovrWorldSetCallbacks(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  if (lua_isnoneornil(L, 2)) {
    lovrWorldSetCallbacks(world, &(WorldCallbacks) { 0 });
    return 0;
  }

  luaL_checktype(L, 2, LUA_TTABLE);

  luax_pushstash(L, "lovr.world.filter");
  lua_pushvalue(L, 1);
  lua_getfield(L, 2, "filter");
  bool filter = lua_type(L, -1) == LUA_TFUNCTION;
  lua_rawset(L, -3);
  lua_pop(L, 1);

  luax_pushstash(L, "lovr.world.enter");
  lua_pushvalue(L, 1);
  lua_getfield(L, 2, "enter");
  bool enter = lua_type(L, -1) == LUA_TFUNCTION;
  lua_rawset(L, -3);
  lua_pop(L, 1);

  luax_pushstash(L, "lovr.world.exit");
  lua_pushvalue(L, 1);
  lua_getfield(L, 2, "exit");
  bool exit = lua_type(L, -1) == LUA_TFUNCTION;
  lua_rawset(L, -3);
  lua_pop(L, 1);

  luax_pushstash(L, "lovr.world.contact");
  lua_pushvalue(L, 1);
  lua_getfield(L, 2, "contact");
  bool contact = lua_type(L, -1) == LUA_TFUNCTION;
  lua_rawset(L, -3);
  lua_pop(L, 1);

  lovrWorldSetCallbacks(world, &(WorldCallbacks) {
    .filter = filter ? filterCallback : NULL,
    .enter = enter ? enterCallback : NULL,
    .exit = exit ? exitCallback : NULL,
    .contact = contact ? contactCallback : NULL,
    .userdata = L
  });

  return 0;
}

// Deprecated

static int l_lovrWorldGetTightness(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float tightness = lovrWorldGetTightness(world);
  luax_check(L, tightness >= 0, "Negative tightness factor causes simulation instability");
  lua_pushnumber(L, tightness);
  return 1;
}

static int l_lovrWorldSetTightness(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float tightness = luax_checkfloat(L, 2);
  lovrWorldSetTightness(world, tightness);
  return 0;
}

static int l_lovrWorldGetResponseTime(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float responseTime = lovrWorldGetResponseTime(world);
  lua_pushnumber(L, responseTime);
  return 1;
}

static int l_lovrWorldSetResponseTime(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float responseTime = luax_checkfloat(L, 2);
  luax_check(L, responseTime >= 0, "Negative response time causes simulation instability");
  lovrWorldSetResponseTime(world, responseTime);
  return 0;
}

static int l_lovrWorldGetLinearDamping(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float damping, threshold;
  lovrWorldGetLinearDamping(world, &damping, &threshold);
  lua_pushnumber(L, damping);
  lua_pushnumber(L, threshold);
  return 2;
}

static int l_lovrWorldSetLinearDamping(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float damping = luax_checkfloat(L, 2);
  float threshold = luax_optfloat(L, 3, 0.0f);
  lovrWorldSetLinearDamping(world, damping, threshold);
  return 0;
}

static int l_lovrWorldGetAngularDamping(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float damping, threshold;
  lovrWorldGetAngularDamping(world, &damping, &threshold);
  lua_pushnumber(L, damping);
  lua_pushnumber(L, threshold);
  return 2;
}

static int l_lovrWorldSetAngularDamping(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  float damping = luax_checkfloat(L, 2);
  float threshold = luax_optfloat(L, 3, 0.0f);
  lovrWorldSetAngularDamping(world, damping, threshold);
  return 0;
}

static int l_lovrWorldIsSleepingAllowed(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  lua_pushboolean(L, lovrWorldIsSleepingAllowed(world));
  return 1;
}

static int l_lovrWorldSetSleepingAllowed(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  bool allowed = lua_toboolean(L, 2);
  lovrWorldSetSleepingAllowed(world, allowed);
  return 0;
}

static int l_lovrWorldGetStepCount(lua_State* L) {
  World* world = luax_checkworld(L, 1);
  int iterations = lovrWorldGetStepCount(world);
  lua_pushnumber(L, iterations);
  return 1;
}

static int l_lovrWorldSetStepCount(lua_State* L) {
  World* world = luax_checkworld(L, 1);
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
  { "isDestroyed", l_lovrWorldIsDestroyed },
  { "getTags", l_lovrWorldGetTags },
  { "getColliderCount", l_lovrWorldGetColliderCount },
  { "getJointCount", l_lovrWorldGetJointCount },
  { "getColliders", l_lovrWorldGetColliders },
  { "getJoints", l_lovrWorldGetJoints },
  { "update", l_lovrWorldUpdate },
  { "interpolate", l_lovrWorldInterpolate },
  { "raycast", l_lovrWorldRaycast },
  { "shapecast", l_lovrWorldShapecast },
  { "overlapShape", l_lovrWorldOverlapShape },
  { "queryBox", l_lovrWorldQueryBox },
  { "querySphere", l_lovrWorldQuerySphere },
  { "getGravity", l_lovrWorldGetGravity },
  { "setGravity", l_lovrWorldSetGravity },
  { "disableCollisionBetween", l_lovrWorldDisableCollisionBetween },
  { "enableCollisionBetween", l_lovrWorldEnableCollisionBetween },
  { "isCollisionEnabledBetween", l_lovrWorldIsCollisionEnabledBetween },
  { "getCallbacks", l_lovrWorldGetCallbacks },
  { "setCallbacks", l_lovrWorldSetCallbacks },

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
