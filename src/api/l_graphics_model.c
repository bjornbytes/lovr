#include "api.h"
#include "graphics/graphics.h"
#include "data/modelData.h"
#include "core/maf.h"
#include <lua.h>
#include <lauxlib.h>

static uint32_t luax_checkanimation(lua_State* L, int index, Model* model) {
  switch (lua_type(L, index)) {
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, index, &length);
      ModelData* modelData = lovrModelGetModelData(model);
      uint64_t animationIndex = map_get(&modelData->animationMap, hash64(name, length));
      lovrCheck(animationIndex != MAP_NIL, "ModelData has no animation named '%s'", name);
      return (uint32_t) animationIndex;
    }
    case LUA_TNUMBER: return lua_tointeger(L, index) - 1;
    default: return luax_typeerror(L, index, "number or string"), ~0u;
  }
}

static uint32_t luax_checknode(lua_State* L, int index, Model* model) {
  switch (lua_type(L, index)) {
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, index, &length);
      ModelData* modelData = lovrModelGetModelData(model);
      uint64_t nodeIndex = map_get(&modelData->nodeMap, hash64(name, length));
      lovrCheck(nodeIndex != MAP_NIL, "ModelData has no node named '%s'", name);
      return (uint32_t) nodeIndex;
    }
    case LUA_TNUMBER: return lua_tointeger(L, index) - 1;
    default: return luax_typeerror(L, index, "number or string"), ~0u;
  }
}

static int l_lovrModelGetModelData(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  luax_pushtype(L, ModelData, lovrModelGetModelData(model));
  return 1;
}

static int l_lovrModelAnimate(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t animation = luax_checkanimation(L, 2, model);
  float time = luax_checkfloat(L, 3);
  float alpha = luax_optfloat(L, 4, 1.f);
  lovrModelAnimate(model, animation, time, alpha);
  return 0;
}

static int l_lovrModelPose(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);

  uint32_t node;
  switch (lua_type(L, 2)) {
    case LUA_TNONE:
    case LUA_TNIL:
      lovrModelResetPose(model);
      return 0;
    case LUA_TNUMBER:
    case LUA_TSTRING:
      node = luax_checknode(L, 2, model);
      break;
    default: return luax_typeerror(L, 2, "nil, number, or string");
  }

  int index = 3;
  float position[4], rotation[4];
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readquat(L, index, rotation, NULL);
  float alpha = luax_optfloat(L, index, 1.f);
  lovrModelPose(model, node, position, rotation, alpha);
  return 0;
}

static int l_lovrModelGetNodePose(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknode(L, 2, model);
  float position[4], rotation[4], angle, ax, ay, az;
  CoordinateSpace space = luax_checkenum(L, 3, CoordinateSpace, "global");
  lovrModelGetNodePose(model, node, position, rotation, space);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  quat_getAngleAxis(rotation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrModelGetTexture(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t index = luaL_checkinteger(L, 2);
  Texture* texture = lovrModelGetTexture(model, index);
  luax_pushtype(L, Texture, texture);
  return 1;
}

static int l_lovrModelGetMaterial(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t index = luaL_checkinteger(L, 2);
  Material* material = lovrModelGetMaterial(model, index);
  luax_pushtype(L, Material, material);
  return 1;
}

static int l_lovrModelGetVertexBuffer(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Buffer* buffer = lovrModelGetVertexBuffer(model);
  luax_pushtype(L, Buffer, buffer);
  return 1;
}

static int l_lovrModelGetIndexBuffer(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Buffer* buffer = lovrModelGetIndexBuffer(model);
  luax_pushtype(L, Buffer, buffer);
  return 1;
}

static int l_lovrModelGetTriangles(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float* vertices = NULL;
  uint32_t* indices = NULL;
  uint32_t vertexCount;
  uint32_t indexCount;
  lovrModelGetTriangles(model, &vertices, &vertexCount, &indices, &indexCount);

  lua_createtable(L, vertexCount * 3, 0);
  for (uint32_t i = 0; i < vertexCount; i++) {
    lua_pushnumber(L, vertices[i]);
    lua_rawseti(L, -2, i + 1);
  }

  lua_createtable(L, indexCount, 0);
  for (uint32_t i = 0; i < indexCount; i++) {
    lua_pushinteger(L, indices[i] + 1);
    lua_rawseti(L, -2, i + 1);
  }

  return 2;
}

static int l_lovrModelGetTriangleCount(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t count = lovrModelGetTriangleCount(model);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrModelGetVertexCount(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t count = lovrModelGetVertexCount(model);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrModelGetWidth(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float bounds[6];
  lovrModelGetBoundingBox(model, bounds);
  lua_pushnumber(L, bounds[1] - bounds[0]);
  return 1;
}

static int l_lovrModelGetHeight(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float bounds[6];
  lovrModelGetBoundingBox(model, bounds);
  lua_pushnumber(L, bounds[3] - bounds[2]);
  return 1;
}

static int l_lovrModelGetDepth(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float bounds[6];
  lovrModelGetBoundingBox(model, bounds);
  lua_pushnumber(L, bounds[5] - bounds[4]);
  return 1;
}

static int l_lovrModelGetDimensions(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float bounds[6];
  lovrModelGetBoundingBox(model, bounds);
  lua_pushnumber(L, bounds[1] - bounds[0]);
  lua_pushnumber(L, bounds[3] - bounds[2]);
  lua_pushnumber(L, bounds[5] - bounds[4]);
  return 3;
}

static int l_lovrModelGetCenter(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float bounds[6];
  lovrModelGetBoundingBox(model, bounds);
  lua_pushnumber(L, (bounds[0] + bounds[1]) / 2.f);
  lua_pushnumber(L, (bounds[2] + bounds[3]) / 2.f);
  lua_pushnumber(L, (bounds[4] + bounds[5]) / 2.f);
  return 1;
}

static int l_lovrModelGetBoundingBox(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float bounds[6];
  lovrModelGetBoundingBox(model, bounds);
  lua_pushnumber(L, bounds[0]);
  lua_pushnumber(L, bounds[1]);
  lua_pushnumber(L, bounds[2]);
  lua_pushnumber(L, bounds[3]);
  lua_pushnumber(L, bounds[4]);
  lua_pushnumber(L, bounds[5]);
  return 6;
}

static int l_lovrModelGetBoundingSphere(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float sphere[4];
  lovrModelGetBoundingSphere(model, sphere);
  lua_pushnumber(L, sphere[0]);
  lua_pushnumber(L, sphere[1]);
  lua_pushnumber(L, sphere[2]);
  lua_pushnumber(L, sphere[3]);
  return 4;
}

const luaL_Reg lovrModel[] = {
  { "getModelData", l_lovrModelGetModelData },
  { "animate", l_lovrModelAnimate },
  { "pose", l_lovrModelPose },
  { "getNodePose", l_lovrModelGetNodePose },
  { "getTexture", l_lovrModelGetTexture },
  { "getMaterial", l_lovrModelGetMaterial },
  { "getVertexBuffer", l_lovrModelGetVertexBuffer },
  { "getIndexBuffer", l_lovrModelGetIndexBuffer },
  { "getTriangles", l_lovrModelGetTriangles },
  { "getTriangleCount", l_lovrModelGetTriangleCount },
  { "getVertexCount", l_lovrModelGetVertexCount },
  { "getWidth", l_lovrModelGetWidth },
  { "getHeight", l_lovrModelGetHeight },
  { "getDepth", l_lovrModelGetDepth },
  { "getDimensions", l_lovrModelGetDimensions },
  { "getCenter", l_lovrModelGetCenter },
  { "getBoundingBox", l_lovrModelGetBoundingBox },
  { "getBoundingSphere", l_lovrModelGetBoundingSphere },
  { NULL, NULL }
};
