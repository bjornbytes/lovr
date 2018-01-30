#include "api.h"
#include "data/model.h"

int l_lovrModelDataGetNodeCount(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, modelData->nodeCount);
  return 1;
}

int l_lovrModelDataGetAnimationCount(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, modelData->animationCount);
  return 1;
}

int l_lovrModelDataGetMaterialCount(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, modelData->materialCount);
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

int l_lovrModelDataGetTriangleCount(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, modelData->indexCount / 3);
  return 1;
}

const luaL_Reg lovrModelData[] = {
  { "getNodeCount", l_lovrModelDataGetNodeCount },
  { "getAnimationCount", l_lovrModelDataGetAnimationCount },
  { "getMaterialCount", l_lovrModelDataGetMaterialCount },
  { "getVertexFormat", l_lovrModelDataGetVertexFormat },
  { "getVertexCount", l_lovrModelDataGetVertexCount },
  { "getTriangleCount", l_lovrModelDataGetTriangleCount },
  { NULL, NULL }
};
