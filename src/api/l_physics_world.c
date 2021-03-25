#include "api.h"
#include "physics/physics.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdbool.h>

static void collisionResolver(World* world, void* userdata) {
  lua_State* L = userdata;
  luaL_checktype(L, -1, LUA_TFUNCTION);
  luax_pushtype(L, World, world);
  lua_call(L, 1, 0);
}

static int nextOverlap(lua_State* L) {
  World* world = luax_checktype(L, lua_upvalueindex(1), World);
  Shape* a;
  Shape* b;
  if (lovrWorldGetNextOverlap(world, &a, &b)) {
    luax_pushshape(L, a);
    luax_pushshape(L, b);
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
  luax_pushshape(L, shape);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  lua_pushnumber(L, nx);
  lua_pushnumber(L, ny);
  lua_pushnumber(L, nz);
  lua_call(L, 7, 0);
}

static int l_lovrWorldNewCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float position[4];
  luax_readvec3(L, 2, position, NULL);
  Collider* collider = lovrColliderCreate(world, position[0], position[1], position[2]);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  return 1;
}

static int l_lovrWorldNewBoxCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float position[4], size[4];
  int index = luax_readvec3(L, 2, position, NULL);
  luax_readscale(L, index, size, 3, NULL);
  Collider* collider = lovrColliderCreate(world, position[0], position[1], position[2]);
  BoxShape* shape = lovrBoxShapeCreate(size[0], size[1], size[2]);
  lovrColliderAddShape(collider, shape);
  lovrColliderInitInertia(collider, shape);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  lovrRelease(shape, lovrShapeDestroy);
  return 1;
}

static int l_lovrWorldNewCapsuleCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float position[4];
  int index = luax_readvec3(L, 2, position, NULL);
  float radius = luax_optfloat(L, index++, 1.f);
  float length = luax_optfloat(L, index, 1.f);
  Collider* collider = lovrColliderCreate(world, position[0], position[1], position[2]);
  CapsuleShape* shape = lovrCapsuleShapeCreate(radius, length);
  lovrColliderAddShape(collider, shape);
  lovrColliderInitInertia(collider, shape);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  lovrRelease(shape, lovrShapeDestroy);
  return 1;
}

static int l_lovrWorldNewCylinderCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float position[4];
  int index = luax_readvec3(L, 2, position, NULL);
  float radius = luax_optfloat(L, index++, 1.f);
  float length = luax_optfloat(L, index, 1.f);
  Collider* collider = lovrColliderCreate(world, position[0], position[1], position[2]);
  CylinderShape* shape = lovrCylinderShapeCreate(radius, length);
  lovrColliderAddShape(collider, shape);
  lovrColliderInitInertia(collider, shape);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  lovrRelease(shape, lovrShapeDestroy);
  return 1;
}

static int l_lovrWorldNewSphereCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float position[4];
  int index = luax_readvec3(L, 2, position, NULL);
  float radius = luax_optfloat(L, index, 1.f);
  Collider* collider = lovrColliderCreate(world, position[0], position[1], position[2]);
  SphereShape* shape = lovrSphereShapeCreate(radius);
  lovrColliderAddShape(collider, shape);
  lovrColliderInitInertia(collider, shape);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  lovrRelease(shape, lovrShapeDestroy);
  return 1;
}

static int l_lovrWorldNewMeshCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);

  float* vertices;
  uint32_t* indices;
  uint32_t vertexCount;
  uint32_t indexCount;
  bool shouldFree;
  luax_readmesh(L, 2, &vertices, &vertexCount, &indices, &indexCount, &shouldFree);

  // If we do not own the mesh data, we must make a copy
  // ode's trimesh collider needs to own the triangle info for the lifetime of the geom
  // Note that if shouldFree is true, we don't free the data and let the physics module do it when
  // the collider/shape is destroyed
  if (!shouldFree) {
    float* v = vertices;
    uint32_t* i = indices;
    vertices = malloc(3 * vertexCount * sizeof(float));
    indices = malloc(indexCount * sizeof(uint32_t));
    lovrAssert(vertices && indices, "Out of memory");
    memcpy(vertices, v, 3 * vertexCount * sizeof(float));
    memcpy(indices, i, indexCount * sizeof(uint32_t));
  }

  Collider* collider = lovrColliderCreate(world, 0, 0, 0);
  MeshShape* shape = lovrMeshShapeCreate(vertexCount, vertices, indexCount, indices);
  lovrColliderAddShape(collider, shape);
  lovrColliderInitInertia(collider, shape);
  luax_pushtype(L, Collider, collider);
  lovrRelease(collider, lovrColliderDestroy);
  lovrRelease(shape, lovrShapeDestroy);
  return 1;
}

static int l_lovrWorldGetColliders(lua_State* L) {
  World* world = luax_checktype(L, 1, World);

  if (lua_istable(L, 2)) {
    lua_settop(L, 2);
  } else {
    lua_newtable(L);
  }

  Collider* collider = lovrWorldGetFirstCollider(world);
  int index = 1;

  while (collider) {
    luax_pushtype(L, Collider, collider);
    lua_rawseti(L, -2, index++);
    collider = collider->next;
  }

  return 1;
}

static int l_lovrWorldDestroy(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  lovrWorldDestroyData(world);
  return 0;
}

static int l_lovrWorldUpdate(lua_State* L) {
  lua_settop(L, 3);
  World* world = luax_checktype(L, 1, World);
  float dt = luax_checkfloat(L, 2);
  CollisionResolver resolver = lua_type(L, 3) == LUA_TFUNCTION ? collisionResolver : NULL;
  lovrWorldUpdate(world, dt, resolver, L);
  return 0;
}

static int l_lovrWorldComputeOverlaps(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  lovrWorldComputeOverlaps(world);
  return 0;
}

static int l_lovrWorldOverlaps(lua_State* L) {
  luax_checktype(L, 1, World);
  lua_settop(L, 1);
  lua_pushcclosure(L, nextOverlap, 1);
  return 1;
}

static int l_lovrWorldCollide(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  Shape* a = luax_checkshape(L, 2);
  Shape* b = luax_checkshape(L, 3);
  float friction = luax_optfloat(L, 4, -1.f);
  float restitution = luax_optfloat(L, 5, -1.f);
  lua_pushboolean(L, lovrWorldCollide(world, a, b, friction, restitution));
  return 1;
}

static int l_lovrWorldGetGravity(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float x, y, z;
  lovrWorldGetGravity(world, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

static int l_lovrWorldSetGravity(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float gravity[4];
  luax_readvec3(L, 2, gravity, NULL);
  lovrWorldSetGravity(world, gravity[0], gravity[1], gravity[2]);
  return 0;
}

static int l_lovrWorldGetTightness(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float tightness = lovrWorldGetTightness(world);
  lovrAssert(tightness >= 0, "Negative tightness factor causes simulation instability");
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
  lovrAssert(responseTime >= 0, "Negative response time causes simulation instability");
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

static int l_lovrWorldRaycast(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  float start[4], end[4];
  int index;
  index = luax_readvec3(L, 2, start, NULL);
  index = luax_readvec3(L, index, end, NULL);
  luaL_checktype(L, index, LUA_TFUNCTION);
  lua_settop(L, index);
  lovrWorldRaycast(world, start[0], start[1], start[2], end[0], end[1], end[2], raycastCallback, L);
  return 0;
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
  { "newMeshCollider", l_lovrWorldNewMeshCollider },
  { "getColliders", l_lovrWorldGetColliders },
  { "destroy", l_lovrWorldDestroy },
  { "update", l_lovrWorldUpdate },
  { "computeOverlaps", l_lovrWorldComputeOverlaps },
  { "overlaps", l_lovrWorldOverlaps },
  { "collide", l_lovrWorldCollide },
  { "getGravity", l_lovrWorldGetGravity },
  { "setGravity", l_lovrWorldSetGravity },
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
  { "raycast", l_lovrWorldRaycast },
  { "disableCollisionBetween", l_lovrWorldDisableCollisionBetween },
  { "enableCollisionBetween", l_lovrWorldEnableCollisionBetween },
  { "isCollisionEnabledBetween", l_lovrWorldIsCollisionEnabledBetween },
  { NULL, NULL }
};
