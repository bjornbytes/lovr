#include "api.h"
#include "data/model.h"

static int nextChild(lua_State* L) {
  ModelData* modelData = luax_checktype(L, lua_upvalueindex(1), ModelData);
  ModelNode* node = lua_touserdata(L, lua_upvalueindex(2));
  int index = lua_tonumber(L, lua_upvalueindex(3));
  if (index >= node->children.length) {
    lua_pushnil(L);
  } else {
    lua_pushlightuserdata(L, &modelData->nodes[node->children.data[index++]]);
    lua_pushnumber(L, index);
    lua_replace(L, lua_upvalueindex(3));
  }
  return 1;
}

static int nextNode(lua_State* L) {
  ModelData* modelData = luax_checktype(L, lua_upvalueindex(1), ModelData);
  int index = lua_tonumber(L, lua_upvalueindex(2));
  if (index >= modelData->nodeCount) {
    lua_pushnil(L);
  } else {
    lua_pushlightuserdata(L, &modelData->nodes[index++]);
    lua_pushnumber(L, index);
    lua_replace(L, lua_upvalueindex(2));
  }
  return 1;
}

int l_lovrModelDataGetVertexCount(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, modelData->vertexCount);
  return 1;
}

int l_lovrModelDataGetVertexFormat(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  return luax_pushvertexformat(L, &modelData->format);
}

int l_lovrModelDataGetVertex(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  int index = luaL_checkint(L, 2) - 1;
  VertexData vertex = modelData->vertices;
  vertex.bytes += index * modelData->format.stride;
  return luax_pushvertex(L, &vertex, &modelData->format);
}

int l_lovrModelDataGetFaceCount(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, modelData->indexCount / 3);
  return 1;
}

int l_lovrModelDataGetFace(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  int index = luaL_checkint(L, 2) - 1;
  if (modelData->indexSize == sizeof(uint32_t)) {
    lua_pushinteger(L, modelData->indices.ints[index * 3 + 0]);
    lua_pushinteger(L, modelData->indices.ints[index * 3 + 1]);
    lua_pushinteger(L, modelData->indices.ints[index * 3 + 2]);
  } else {
    lua_pushinteger(L, modelData->indices.shorts[index * 3 + 0]);
    lua_pushinteger(L, modelData->indices.shorts[index * 3 + 1]);
    lua_pushinteger(L, modelData->indices.shorts[index * 3 + 2]);
  }
  return 3;
}

int l_lovrModelDataGetNodeCount(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, modelData->nodeCount);
  return 1;
}

int l_lovrModelDataGetRoot(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  if (modelData->nodeCount == 0) {
    lua_pushnil(L);
  } else {
    ModelNode* root = &modelData->nodes[0];
    lua_pushlightuserdata(L, root);
  }
  return 1;
}

int l_lovrModelDataGetNodeName(lua_State* L) {
  ModelNode* node = lua_touserdata(L, 2);
  lua_pushstring(L, node->name);
  return 1;
}

int l_lovrModelDataGetNodeParent(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  ModelNode* node = lua_touserdata(L, 2);
  if (node->parent == -1) {
    lua_pushnil(L);
  } else {
    lua_pushlightuserdata(L, &modelData->nodes[node->parent]);
  }
  return 1;
}

int l_lovrModelDataChildren(lua_State* L) {
  luax_checktype(L, 1, ModelData);
  luaL_checktype(L, 2, LUA_TLIGHTUSERDATA);
  lua_settop(L, 2);
  lua_pushinteger(L, 0);
  lua_pushcclosure(L, nextChild, 3);
  return 1;
}

int l_lovrModelDataNodes(lua_State* L) {
  luax_checktype(L, 1, ModelData);
  lua_settop(L, 1);
  lua_pushinteger(L, 0);
  lua_pushcclosure(L, nextNode, 2);
  return 1;
}

const luaL_Reg lovrModelData[] = {
  { "getVertexFormat", l_lovrModelDataGetVertexFormat },
  { "getVertexCount", l_lovrModelDataGetVertexCount },
  { "getVertex", l_lovrModelDataGetVertex },
  { "getFaceCount", l_lovrModelDataGetFaceCount },
  { "getFace", l_lovrModelDataGetFace },
  { "getNodeCount", l_lovrModelDataGetNodeCount },
  { "getRoot", l_lovrModelDataGetRoot },
  { "getNodeName", l_lovrModelDataGetNodeName },
  { "getNodeParent", l_lovrModelDataGetNodeParent },
  { "children", l_lovrModelDataChildren },
  { "nodes", l_lovrModelDataNodes },
  { NULL, NULL }
};
