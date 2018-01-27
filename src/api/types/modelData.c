#include "api.h"
#include "data/model.h"

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

const luaL_Reg lovrModelData[] = {
  { "getVertexFormat", l_lovrModelDataGetVertexFormat },
  { "getVertexCount", l_lovrModelDataGetVertexCount },
  { "getVertex", l_lovrModelDataGetVertex },
  { "getFaceCount", l_lovrModelDataGetFaceCount },
  { "getFace", l_lovrModelDataGetFace },
  { NULL, NULL }
};
