#include "api.h"
#include "graphics/material.h"
#include "graphics/model.h"
#include "data/modelData.h"
#include "core/maf.h"

static uint32_t luax_checkanimation(lua_State* L, int index, Model* model) {
  switch (lua_type(L, index)) {
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, index, &length);
      ModelData* modelData = lovrModelGetModelData(model);
      uint64_t index = map_get(&modelData->animationMap, hash64(name, length));
      lovrAssert(index != MAP_NIL, "Model has no animation named '%s'", name);
      return (uint32_t) index;
    }
    case LUA_TNUMBER: return lua_tointeger(L, index) - 1;
    default: return luaL_typerror(L, index, "number or string");
  }
}

static int l_lovrModelDraw(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1);
  int instances = luaL_optinteger(L, index, 1);
  lovrModelDraw(model, transform, instances);
  return 0;
}

static int l_lovrModelAnimate(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t animation = luax_checkanimation(L, 2, model);
  float time = luaL_checknumber(L, 3);
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
      node = lua_tointeger(L, 2) - 1;
      break;
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, 2, &length);
      ModelData* modelData = lovrModelGetModelData(model);
      uint64_t index = map_get(&modelData->nodeMap, hash64(name, length));
      lovrAssert(index != MAP_NIL, "Model has no node named '%s'", name);
      node = (uint32_t) index;
      break;
    }
    default:
      return luaL_typerror(L, 2, "nil, number, or string");
  }

  int index = 3;
  float position[4], rotation[4];
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readquat(L, index, rotation, NULL);
  float alpha = luax_optfloat(L, index, 1.f);
  lovrModelPose(model, node, position, rotation, alpha);
  return 0;
}

static int l_lovrModelGetMaterial(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);

  uint32_t material;
  switch (lua_type(L, 2)) {
    case LUA_TNUMBER:
      material = lua_tointeger(L, 2) - 1;
      break;
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, 2, &length);
      ModelData* modelData = lovrModelGetModelData(model);
      uint64_t index = map_get(&modelData->materialMap, hash64(name, length));
      lovrAssert(index != MAP_NIL, "Model has no material named '%s'", name);
      material = (uint32_t) index;
      break;
    }
    default:
      return luaL_typerror(L, 2, "nil, number, or string");
  }

  luax_pushtype(L, Material, lovrModelGetMaterial(model, material));
  return 1;
}

static int l_lovrModelGetAABB(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  float aabb[6];
  lovrModelGetAABB(model, aabb);
  for (int i = 0; i < 6; i++) {
    lua_pushnumber(L, aabb[i]);
  }
  return 6;
}

static int l_lovrModelGetNodePose(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node;
  switch (lua_type(L, 2)) {
    case LUA_TNUMBER:
      node = lua_tointeger(L, 2) - 1;
      break;
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, 2, &length);
      ModelData* modelData = lovrModelGetModelData(model);
      uint64_t index = map_get(&modelData->nodeMap, hash64(name, length));
      lovrAssert(index != MAP_NIL, "Model has no node named '%s'", name);
      node = (uint32_t) index;
      break;
    }
    default:
      return luaL_typerror(L, 2, "number or string");
  }

  float position[4], rotation[4], angle, ax, ay, az;
  CoordinateSpace space = luaL_checkoption(L, 3, "global", CoordinateSpaces);
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

static int l_lovrModelGetAnimationName(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t index = luaL_checkinteger(L, 2);
  ModelData* modelData = lovrModelGetModelData(model);
  lovrAssert(index > 0 && index <= modelData->animationCount, "Model has no animation at index %d", index);
  lua_pushstring(L, modelData->animations[index - 1].name);
  return 1;
}

static int l_lovrModelGetMaterialName(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t index = luaL_checkinteger(L, 2);
  ModelData* modelData = lovrModelGetModelData(model);
  lovrAssert(index > 0 && index <= modelData->materialCount, "Model has no material at index %d", index);
  lua_pushstring(L, modelData->materials[index - 1].name);
  return 1;
}

static int l_lovrModelGetNodeName(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t index = luaL_checkinteger(L, 2);
  ModelData* modelData = lovrModelGetModelData(model);
  lovrAssert(index > 0 && index <= modelData->nodeCount, "Model has no node at index %d", index);
  lua_pushstring(L, modelData->nodes[index - 1].name);
  return 1;
}

static int l_lovrModelGetAnimationCount(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  lua_pushinteger(L, lovrModelGetModelData(model)->animationCount);
  return 1;
}

static int l_lovrModelGetMaterialCount(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  lua_pushinteger(L, lovrModelGetModelData(model)->materialCount);
  return 1;
}

static int l_lovrModelGetNodeCount(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  lua_pushinteger(L, lovrModelGetModelData(model)->nodeCount);
  return 1;
}

static int l_lovrModelGetAnimationDuration(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t animation = luax_checkanimation(L, 2, model);
  lua_pushnumber(L, lovrModelGetModelData(model)->animations[animation].duration);
  return 1;
}

const luaL_Reg lovrModel[] = {
  { "draw", l_lovrModelDraw },
  { "animate", l_lovrModelAnimate },
  { "pose", l_lovrModelPose },
  { "getMaterial", l_lovrModelGetMaterial },
  { "getAABB", l_lovrModelGetAABB },
  { "getNodePose", l_lovrModelGetNodePose },
  { "getAnimationName", l_lovrModelGetAnimationName },
  { "getMaterialName", l_lovrModelGetMaterialName },
  { "getNodeName", l_lovrModelGetNodeName },
  { "getAnimationCount", l_lovrModelGetAnimationCount },
  { "getMaterialCount", l_lovrModelGetMaterialCount },
  { "getNodeCount", l_lovrModelGetNodeCount },
  { "getAnimationDuration", l_lovrModelGetAnimationDuration },
  { NULL, NULL }
};
