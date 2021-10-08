#include "api.h"
#include "data/modelData.h"
#include "core/maf.h"
#include "core/map.h"
#include <lua.h>
#include <lauxlib.h>

static ModelNode* luax_checknode(lua_State* L, int index, ModelData* model) {
  switch (lua_type(L, index)) {
    case LUA_TNUMBER: {
      uint32_t node = lua_tointeger(L, index) - 1;
      lovrCheck(node < model->nodeCount, "Invalid node index '%d'", node + 1);
      return &model->nodes[node];
    }
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, index, &length);
      uint64_t hash = hash64(name, length);
      uint64_t entry = map_get(&model->nodeMap, hash);
      lovrCheck(entry != MAP_NIL, "ModelData has no node named '%s'", name);
      return &model->nodes[(uint32_t) entry];
    }
    default: return luax_typeerror(L, index, "number or string"), NULL;
  }
}

static int l_lovrModelDataGetBlobCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->blobCount);
  return 1;
}

static int l_lovrModelDataGetBlob(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luaL_checkinteger(L, 2) - 1;
  lovrCheck(index < model->blobCount, "Invalid blob index '%d'", index + 1);
  luax_pushtype(L, Blob, model->blobs[index]);
  return 1;
}

static int l_lovrModelDataGetImageCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->imageCount);
  return 1;
}

static int l_lovrModelDataGetImage(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luaL_checkinteger(L, 2) - 1;
  lovrCheck(index < model->imageCount, "Invalid image index '%d'", index + 1);
  luax_pushtype(L, Image, model->images[index]);
  return 1;
}

static int l_lovrModelDataGetNodeCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->nodeCount);
  return 1;
}

static int l_lovrModelDataGetRootNode(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->rootNode + 1);
  return 1;
}

static int l_lovrModelDataGetNodeName(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luaL_checkinteger(L, 2) - 1;
  lovrCheck(index < model->nodeCount, "Invalid node index '%d'", index + 1);
  lua_pushstring(L, model->nodes[index].name);
  return 1;
}

static int l_lovrModelDataGetNodePosition(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
  if (node->matrix) {
    float position[4];
    mat4_getPosition(node->transform.matrix, position);
    lua_pushnumber(L, position[0]);
    lua_pushnumber(L, position[1]);
    lua_pushnumber(L, position[2]);
    return 3;
  } else {
    lua_pushnumber(L, node->transform.properties.translation[0]);
    lua_pushnumber(L, node->transform.properties.translation[1]);
    lua_pushnumber(L, node->transform.properties.translation[2]);
    return 3;
  }
}

static int l_lovrModelDataGetNodeOrientation(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
  float angle, ax, ay, az;
  if (node->matrix) {
    mat4_getAngleAxis(node->transform.matrix, &angle, &ax, &ay, &az);
  } else {
    quat_getAngleAxis(node->transform.properties.rotation, &angle, &ax, &ay, &az);
  }
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

static int l_lovrModelDataGetNodeScale(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
  if (node->matrix) {
    float scale[3];
    mat4_getScale(node->transform.matrix, scale);
    lua_pushnumber(L, scale[0]);
    lua_pushnumber(L, scale[1]);
    lua_pushnumber(L, scale[2]);
  } else {
    lua_pushnumber(L, node->transform.properties.scale[0]);
    lua_pushnumber(L, node->transform.properties.scale[1]);
    lua_pushnumber(L, node->transform.properties.scale[2]);
  }
  return 3;
}

static int l_lovrModelDataGetNodeTransform(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
  if (node->matrix) {
    float position[4], scale[4], angle, ax, ay, az;
    mat4_getPosition(node->transform.matrix, position);
    mat4_getScale(node->transform.matrix, scale);
    mat4_getAngleAxis(node->transform.matrix, &angle, &ax, &ay, &az);
    lua_pushnumber(L, position[0]);
    lua_pushnumber(L, position[1]);
    lua_pushnumber(L, position[2]);
    lua_pushnumber(L, scale[0]);
    lua_pushnumber(L, scale[1]);
    lua_pushnumber(L, scale[2]);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
  } else {
    float angle, ax, ay, az;
    quat_getAngleAxis(node->transform.properties.rotation, &angle, &ax, &ay, &az);
    lua_pushnumber(L, node->transform.properties.translation[0]);
    lua_pushnumber(L, node->transform.properties.translation[1]);
    lua_pushnumber(L, node->transform.properties.translation[2]);
    lua_pushnumber(L, node->transform.properties.scale[0]);
    lua_pushnumber(L, node->transform.properties.scale[1]);
    lua_pushnumber(L, node->transform.properties.scale[2]);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
  }
  return 10;
}

static int l_lovrModelDataGetNodeChildren(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
  lua_createtable(L, node->childCount, 0);
  for (uint32_t i = 0; i < node->childCount; i++) {
    lua_pushinteger(L, node->children[i] + 1);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int l_lovrModelDataGetNodeMeshes(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
  lua_createtable(L, node->primitiveCount, 0);
  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    lua_pushinteger(L, node->primitiveIndex + i + 1);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int l_lovrModelDataGetNodeSkin(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
  if (node->skin == ~0u) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, node->skin + 1);
  }
  return 1;
}

const luaL_Reg lovrModelData[] = {
  { "getBlobCount", l_lovrModelDataGetBlobCount },
  { "getBlob", l_lovrModelDataGetBlob },
  { "getImageCount", l_lovrModelDataGetImageCount },
  { "getImage", l_lovrModelDataGetImage },
  { "getNodeCount", l_lovrModelDataGetNodeCount },
  { "getRootNode", l_lovrModelDataGetRootNode },
  { "getNodeName", l_lovrModelDataGetNodeName },
  { "getNodePosition", l_lovrModelDataGetNodePosition },
  { "getNodeOrientation", l_lovrModelDataGetNodeOrientation },
  { "getNodeScale", l_lovrModelDataGetNodeScale },
  { "getNodeTransform", l_lovrModelDataGetNodeTransform },
  { "getNodeChildren", l_lovrModelDataGetNodeChildren },
  { "getNodeMeshes", l_lovrModelDataGetNodeMeshes },
  { "getNodeSkin", l_lovrModelDataGetNodeSkin },
  { NULL, NULL }
};
