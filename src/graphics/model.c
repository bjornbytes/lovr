#include "model.h"
#include <assimp/scene.h>

void luax_pushmodel(lua_State* L, Model* model) {
  Model** userdata = (Model**) lua_newuserdata(L, sizeof(Model*));
  luaL_getmetatable(L, "Model");
  lua_setmetatable(L, -2);
  *userdata = model;
}

Model* luax_checkmodel(lua_State* L, int index) {
  return *(Model**) luaL_checkudata(L, index, "Model");
}

int lovrModelDraw(lua_State* L) {
  Model* model = luax_checkmodel(L, 1);

  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);

  if (model) {
    printf("I just drew your fuckign model at %f %f %f\n", x, y, z);
  }

  return 0;
}

int lovrModelGetVertexCount(lua_State* L) {
  Model* model = luax_checkmodel(L, 1);
  lua_pushinteger(L, model->mNumVertices);
  return 1;
}

int lovrModelGetColors(lua_State* L) {
  Model* model = luax_checkmodel(L, 1);
  int colorSetIndex = 0;

  if (lua_gettop(L) > 1) {
    colorSetIndex = luaL_checkinteger(L, 2);
  }

  if (colorSetIndex >= AI_MAX_NUMBER_OF_COLOR_SETS || !model->mColors[colorSetIndex]) {
    lua_pushnil(L);
    return 1;
  }

  struct aiColor4D* colorSet = model->mColors[colorSetIndex];

  lua_createtable(L, model->mNumVertices, 0);

  for (int i = 0; i < model->mNumVertices; i++) {
    lua_createtable(L, 4, 0);

    lua_pushnumber(L, colorSet[i].r);
    lua_rawseti(L, -2, 1);

    lua_pushnumber(L, colorSet[i].g);
    lua_rawseti(L, -2, 2);

    lua_pushnumber(L, colorSet[i].b);
    lua_rawseti(L, -2, 3);

    lua_pushnumber(L, colorSet[i].a);
    lua_rawseti(L, -2, 4);

    lua_rawseti(L, -1, i);
  }

  return 1;
}

int lovrModelGetNormals(lua_State* L) {
  Model* model = luax_checkmodel(L, 1);

  if (!model->mNormals) {
    lua_pushnil(L);
    return 1;
  }

  lua_createtable(L, model->mNumVertices, 0);

  for (int i = 0; i < model->mNumVertices; i++) {
    lua_createtable(L, 3, 0);

    lua_pushnumber(L, model->mNormals[i].x);
    lua_rawseti(L, -2, 1);

    lua_pushnumber(L, model->mNormals[i].y);
    lua_rawseti(L, -2, 2);

    lua_pushnumber(L, model->mNormals[i].z);
    lua_rawseti(L, -2, 3);

    lua_rawseti(L, -2, i);
  }

  return 1;
}

int lovrModelGetTangents(lua_State* L) {
  Model* model = luax_checkmodel(L, 1);

  if (!model->mTangents) {
    lua_pushnil(L);
    return 1;
  }

  lua_createtable(L, model->mNumFaces, 0);

  for (int i = 0; i < model->mNumFaces; i++) {
    lua_createtable(L, 3, 0);

    lua_pushnumber(L, model->mTangents[i].x);
    lua_rawseti(L, -2, 1);

    lua_pushnumber(L, model->mTangents[i].y);
    lua_rawseti(L, -2, 2);

    lua_pushnumber(L, model->mTangents[i].z);
    lua_rawseti(L, -2, 3);

    lua_rawseti(L, -2, i);
  }

  return 1;
}

int lovrModelGetUVs(lua_State* L) {
  Model* model = luax_checkmodel(L, 1);
  int uvSetIndex = 0;

  if (lua_gettop(L) > 1) {
    uvSetIndex = luaL_checkinteger(L, 2);
  }

  if (uvSetIndex >= AI_MAX_NUMBER_OF_TEXTURECOORDS || !model->mTextureCoords[uvSetIndex]) {
    lua_pushnil(L);
    return 1;
  }

  struct aiVector3D* uvSet = model->mTextureCoords[uvSetIndex];

  lua_createtable(L, model->mNumVertices, 0);

  for (int i = 0; i < model->mNumVertices; i++) {
    lua_createtable(L, 4, 0);

    lua_pushnumber(L, uvSet->x);
    lua_rawseti(L, -2, 1);

    lua_pushnumber(L, uvSet->y);
    lua_rawseti(L, -2, 2);

    lua_pushnumber(L, uvSet->z);
    lua_rawseti(L, -2, 3);

    lua_rawseti(L, -1, i);
  }

  return 1;
}

int lovrModelGetVertex(lua_State* L) {
  Model* model = luax_checkmodel(L, 1);
  int index = luaL_checkint(L, 2);

  lua_pushnumber(L, model->mVertices[index].x);
  lua_pushnumber(L, model->mVertices[index].y);
  lua_pushnumber(L, model->mVertices[index].z);

  return 3;
}

const luaL_Reg lovrModel[] = {
  { "draw", lovrModelDraw },
  { "getVertexCount", lovrModelGetVertexCount },
  { "getColors", lovrModelGetColors },
  { "getNormals", lovrModelGetNormals },
  { "getTangents", lovrModelGetTangents },
  { "getUVs", lovrModelGetUVs },
  { "getVertex", lovrModelGetVertex },
  { NULL, NULL }
};
