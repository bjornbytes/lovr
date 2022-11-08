#include "api.h"
#include "physics/physics.h"
#include "data/image.h"
#include "util.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

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

static float terrainCallback(lua_State * L, int fn_index, float x, float z) {
  lua_pushvalue(L, fn_index);
  lua_pushnumber(L, x);
  lua_pushnumber(L, z);
  lua_call(L, 2, 1);
  float height = luax_checkfloat(L, -1);
  lua_remove(L, -1);
  return height;
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

static int l_lovrWorldNewTerrainCollider(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  TerrainShape* shape;
  float horizontalScale = luax_checkfloat(L, 2);
  int type = lua_type(L, 3);
  if (type == LUA_TNIL || type == LUA_TNONE) {
    float vertices[4] = {0.f};
    shape = lovrTerrainShapeCreate(vertices, 2, 2, horizontalScale, 1.f);
  } else if (type == LUA_TFUNCTION) {
    unsigned samples = luax_optu32(L, 4, 100);
    float* vertices = malloc(sizeof(float) * samples * samples);
    for (unsigned i = 0; i < samples * samples; i++) {
      float x = horizontalScale * (-0.5f + ((float) (i % samples)) / samples);
      float z = horizontalScale * (-0.5f + ((float) (i / samples)) / samples);
      vertices[i] = terrainCallback(L, 3, x, z);
    }
    shape = lovrTerrainShapeCreate(vertices, samples, samples, horizontalScale, 1.f);
    free(vertices);
  } else if (type == LUA_TUSERDATA) {
    Image* image = luax_checktype(L, 3, Image);
    uint32_t imageWidth = lovrImageGetWidth(image, 0);
    uint32_t imageHeight = lovrImageGetHeight(image, 0);
    float verticalScale = luax_optfloat(L, 4, 1.f);
    float* vertices = malloc(sizeof(float) * imageWidth * imageHeight);
    for (int y = 0; y < imageHeight; y++) {
      for (int x = 0; x < imageWidth; x++) {
        float pixel[4];
        lovrImageGetPixel(image, x, y, pixel);
        vertices[x + y * imageWidth] = pixel[0];
      }
    }
    shape = lovrTerrainShapeCreate(vertices, imageWidth, imageHeight, horizontalScale, verticalScale);
    free(vertices);
  }
  Collider* collider = lovrColliderCreate(world, 0, 0, 0);
  lovrColliderAddShape(collider, shape);
  lovrColliderSetKinematic(collider, true);
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
    collider = lovrColliderGetNext(collider);
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

static int l_lovrWorldGetContacts(lua_State* L) {
  World* world = luax_checktype(L, 1, World);
  Shape* a = luax_checkshape(L, 2);
  Shape* b = luax_checkshape(L, 3);
  uint32_t count;
  Contact contacts[MAX_CONTACTS];
  lovrWorldGetContacts(world, a, b, contacts, &count);
  lua_createtable(L, count, 0);
  for (uint32_t i = 0; i < count; i++) {
    lua_createtable(L, 7, 0);
    lua_pushnumber(L, contacts[i].x);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, contacts[i].y);
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, contacts[i].z);
    lua_rawseti(L, -2, 3);
    lua_pushnumber(L, contacts[i].nx);
    lua_rawseti(L, -2, 4);
    lua_pushnumber(L, contacts[i].ny);
    lua_rawseti(L, -2, 5);
    lua_pushnumber(L, contacts[i].nz);
    lua_rawseti(L, -2, 6);
    lua_pushnumber(L, contacts[i].depth);
    lua_rawseti(L, -2, 7);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
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
  { "newSphereCollider", l_lovrWorldNewSphereCollider },
  { "newMeshCollider", l_lovrWorldNewMeshCollider },
  { "newTerrainCollider", l_lovrWorldNewTerrainCollider },
  { "getColliders", l_lovrWorldGetColliders },
  { "destroy", l_lovrWorldDestroy },
  { "update", l_lovrWorldUpdate },
  { "computeOverlaps", l_lovrWorldComputeOverlaps },
  { "overlaps", l_lovrWorldOverlaps },
  { "collide", l_lovrWorldCollide },
  { "getContacts", l_lovrWorldGetContacts },
  { "raycast", l_lovrWorldRaycast },
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
  { "disableCollisionBetween", l_lovrWorldDisableCollisionBetween },
  { "enableCollisionBetween", l_lovrWorldEnableCollisionBetween },
  { "isCollisionEnabledBetween", l_lovrWorldIsCollisionEnabledBetween },
  { "getStepCount", l_lovrWorldGetStepCount },
  { "setStepCount", l_lovrWorldSetStepCount },
  { NULL, NULL }
};
