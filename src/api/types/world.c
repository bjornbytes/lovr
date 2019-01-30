#include "api.h"
#include "physics/physics.h"
#include <stdbool.h>

static void collisionResolver(World* world, void* userdata) {
  lua_State* L = userdata;
  luaL_checktype(L, -1, LUA_TFUNCTION);
  luax_pushobject(L, world);
  lua_call(L, 1, 0);
}

static int nextOverlap(lua_State* L) {
  World* world = luax_checktype(L, lua_upvalueindex(1), World);
  Shape* a;
  Shape* b;
  if (lovrWorldGetNextOverlap(world, &a, &b)) {
    luax_pushobject(L, a);
    luax_pushobject(L, b);
    return 2;
  } else {
    lua_pushnil(L);
    return 1;
  }
}

static void raycastCallback(Shape* shape, float x, float y, float z, float nx, float ny, float nz, void* userdata) {
  lua_State* L = userdata;
  luaL_checktype(L, -1, LUA_TFUNCTION);
  lua_pushvalue(L, -1);
  luax_pushobject(L, shape);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  lua_pushnumber(L, nx);
  lua_pushnumber(L, ny);
  lua_pushnumber(L, nz);
  lua_call(L, 7, 0);
}

int l_lovrWorldNewCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float x = luax_optfloat(L, 2, 0.f);
  float y = luax_optfloat(L, 3, 0.f);
  float z = luax_optfloat(L, 4, 0.f);
  Collider* collider = lovrColliderCreate(world, x, y, z);
  luax_pushobject(L, collider);
  lovrRelease(collider);
  return 1;
}

int l_lovrWorldNewBoxCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float x = luax_optfloat(L, 2, 0.f);
  float y = luax_optfloat(L, 3, 0.f);
  float z = luax_optfloat(L, 4, 0.f);
  float sx = luax_optfloat(L, 5, 1.f);
  float sy = luax_optfloat(L, 6, sx);
  float sz = luax_optfloat(L, 7, sx);
  Collider* collider = lovrColliderCreate(world, x, y, z);
  BoxShape* shape = lovrBoxShapeCreate(sx, sy, sz);
  lovrColliderAddShape(collider, shape);
  luax_pushobject(L, collider);
  lovrRelease(collider);
  lovrRelease(shape);
  return 1;
}

int l_lovrWorldNewCapsuleCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float x = luax_optfloat(L, 2, 0.f);
  float y = luax_optfloat(L, 3, 0.f);
  float z = luax_optfloat(L, 4, 0.f);
  float radius = luax_optfloat(L, 5, 1.f);
  float length = luax_optfloat(L, 6, 1.f);
  Collider* collider = lovrColliderCreate(world, x, y, z);
  CapsuleShape* shape = lovrCapsuleShapeCreate(radius, length);
  lovrColliderAddShape(collider, shape);
  luax_pushobject(L, collider);
  lovrRelease(collider);
  lovrRelease(shape);
  return 1;
}

int l_lovrWorldNewCylinderCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float x = luax_optfloat(L, 2, 0.f);
  float y = luax_optfloat(L, 3, 0.f);
  float z = luax_optfloat(L, 4, 0.f);
  float radius = luax_optfloat(L, 5, 1.f);
  float length = luax_optfloat(L, 6, 1.f);
  Collider* collider = lovrColliderCreate(world, x, y, z);
  CylinderShape* shape = lovrCylinderShapeCreate(radius, length);
  lovrColliderAddShape(collider, shape);
  luax_pushobject(L, collider);
  lovrRelease(collider);
  lovrRelease(shape);
  return 1;
}

int l_lovrWorldNewSphereCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float x = luax_optfloat(L, 2, 0.f);
  float y = luax_optfloat(L, 3, 0.f);
  float z = luax_optfloat(L, 4, 0.f);
  float radius = luax_optfloat(L, 5, 1.f);
  Collider* collider = lovrColliderCreate(world, x, y, z);
  SphereShape* shape = lovrSphereShapeCreate(radius);
  lovrColliderAddShape(collider, shape);
  luax_pushobject(L, collider);
  lovrRelease(collider);
  lovrRelease(shape);
  return 1;
}

int l_lovrWorldDestroy(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  lovrWorldDestroyData(world);
  return 0;
}

int l_lovrWorldUpdate(lua_State* L) {
  lua_settop(L, 3);
  World* world = luax_checktype(L, 1, World);
  float dt = luax_checkfloat(L, 2);
  CollisionResolver resolver = lua_type(L, 3) == LUA_TFUNCTION ? collisionResolver : NULL;
  lovrWorldUpdate(world, dt, resolver, L);
  return 0;
}

int l_lovrWorldComputeOverlaps(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  lovrWorldComputeOverlaps(world);
  return 0;
}

int l_lovrWorldOverlaps(lua_State* L) {
  luax_checktype(L, 1, World);
  lua_settop(L, 1);
  lua_pushcclosure(L, nextOverlap, 1);
  return 1;
}

int l_lovrWorldCollide(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  Shape* a = luax_checktype(L, 2, Shape);
  Shape* b = luax_checktype(L, 3, Shape);
  float friction = luax_optfloat(L, 4, -1.f);
  float restitution = luax_optfloat(L, 5, -1.f);
  lua_pushboolean(L, lovrWorldCollide(world, a, b, friction, restitution));
  return 1;
}

int l_lovrWorldGetGravity(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float x, y, z;
  lovrWorldGetGravity(world, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrWorldSetGravity(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float x = luax_checkfloat(L, 2);
  float y = luax_checkfloat(L, 3);
  float z = luax_checkfloat(L, 4);
  lovrWorldSetGravity(world, x, y, z);
  return 0;
}

int l_lovrWorldGetLinearDamping(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float damping, threshold;
  lovrWorldGetLinearDamping(world, &damping, &threshold);
  lua_pushnumber(L, damping);
  lua_pushnumber(L, threshold);
  return 2;
}

int l_lovrWorldSetLinearDamping(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float damping = luax_checkfloat(L, 2);
  float threshold = luax_optfloat(L, 3, .01f);
  lovrWorldSetLinearDamping(world, damping, threshold);
  return 0;
}

int l_lovrWorldGetAngularDamping(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float damping, threshold;
  lovrWorldGetAngularDamping(world, &damping, &threshold);
  lua_pushnumber(L, damping);
  lua_pushnumber(L, threshold);
  return 2;
}

int l_lovrWorldSetAngularDamping(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float damping = luax_checkfloat(L, 2);
  float threshold = luax_optfloat(L, 3, .01f);
  lovrWorldSetAngularDamping(world, damping, threshold);
  return 0;
}

int l_lovrWorldIsSleepingAllowed(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  lua_pushboolean(L, lovrWorldIsSleepingAllowed(world));
  return 1;
}

int l_lovrWorldSetSleepingAllowed(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  bool allowed = lua_toboolean(L, 2);
  lovrWorldSetSleepingAllowed(world, allowed);
  return 0;
}

int l_lovrWorldRaycast(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float x1 = luax_checkfloat(L, 2);
  float y1 = luax_checkfloat(L, 3);
  float z1 = luax_checkfloat(L, 4);
  float x2 = luax_checkfloat(L, 5);
  float y2 = luax_checkfloat(L, 6);
  float z2 = luax_checkfloat(L, 7);
  luaL_checktype(L, 8, LUA_TFUNCTION);
  lua_settop(L, 8);
  lovrWorldRaycast(world, x1, y1, z1, x2, y2, z2, raycastCallback, L);
  return 0;
}

int l_lovrWorldDisableCollisionBetween(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  const char* tag1 = luaL_checkstring(L, 2);
  const char* tag2 = luaL_checkstring(L, 3);
  lovrWorldDisableCollisionBetween(world, tag1, tag2);
  return 0;
}

int l_lovrWorldEnableCollisionBetween(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  const char* tag1 = luaL_checkstring(L, 2);
  const char* tag2 = luaL_checkstring(L, 3);
  lovrWorldEnableCollisionBetween(world, tag1, tag2);
  return 0;
}

int l_lovrWorldIsCollisionEnabledBetween(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  const char* tag1 = luaL_checkstring(L, 2);
  const char* tag2 = luaL_checkstring(L, 3);
  lua_pushboolean(L, lovrWorldIsCollisionEnabledBetween(world, tag1, tag2));
  return 1;
}

const luaL_Reg lovrWorld[] = {
  { "newCollider", l_lovrWorldNewCollider },
  { "newBoxCollider", l_lovrWorldNewBoxCollider },
  { "newCapsuleCollider", l_lovrWorldNewCapsuleCollider },
  { "newCylinderCollider", l_lovrWorldNewCylinderCollider },
  { "newSphereCollider", l_lovrWorldNewSphereCollider },
  { "destroy", l_lovrWorldDestroy },
  { "update", l_lovrWorldUpdate },
  { "computeOverlaps", l_lovrWorldComputeOverlaps },
  { "overlaps", l_lovrWorldOverlaps },
  { "collide", l_lovrWorldCollide },
  { "getGravity", l_lovrWorldGetGravity },
  { "setGravity", l_lovrWorldSetGravity },
  { "getLinearDamping", l_lovrWorldGetLinearDamping },
  { "setLinearDamping", l_lovrWorldSetLinearDamping },
  { "getAngularDamping", l_lovrWorldGetAngularDamping },
  { "setAngularDamping", l_lovrWorldSetAngularDamping },
  { "isSleepingAllowed", l_lovrWorldIsSleepingAllowed },
  { "setSleepingAllowed", l_lovrWorldSetSleepingAllowed },
  { "raycast", l_lovrWorldRaycast },
  { "disableCollisionBetween", l_lovrWorldDisableCollisionBetween },
  { "enableCollisionBetween", l_lovrWorldEnableCollisionBetween },
  { "isCollisionEnabledBetween", l_lovrWorldIsCollisionEnabledBetween },
  { NULL, NULL }
};
