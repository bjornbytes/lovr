#include "api.h"
#include "data/modelData.h"
#include "math/transform.h"
#include "math/mat4.h"
#include "math/quat.h"
#include "math/vec3.h"

int l_lovrModelDataGetVertexData(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  luax_pushtype(L, VertexData, modelData->vertexData);
  return 1;
}

int l_lovrModelDataGetTriangleCount(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, modelData->indexCount / 3);
  return 1;
}

int l_lovrModelDataGetNodeCount(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, modelData->nodeCount);
  return 1;
}

int l_lovrModelDataGetNodeName(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  int index = luaL_checkint(L, 2) - 1;
  lovrAssert(index >= 0 && index < modelData->nodeCount, "Invalid node index: %d", index);
  ModelNode* node = &modelData->nodes[index];
  lua_pushstring(L, node->name);
  return 1;
}

static int luax_writenodetransform(lua_State* L, mat4 m, int transformIndex) {
  Transform* transform;
  if ((transform = luax_totype(L, transformIndex, Transform)) != NULL) {
    lovrTransformSetMatrix(transform, m);
    lua_settop(L, transformIndex);
    return 1;
  } else {
    float x, y, z, sx, sy, sz, angle, ax, ay, az;
    mat4_getTransform(m, &x, &y, &z, &sx, &sy, &sz, &angle, &ax, &ay, &az);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    lua_pushnumber(L, sx);
    lua_pushnumber(L, sy);
    lua_pushnumber(L, sz);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
    return 10;
  }
}

int l_lovrModelDataGetLocalNodeTransform(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  int index = luaL_checkint(L, 2) - 1;
  lovrAssert(index >= 0 && index < modelData->nodeCount, "Invalid node index: %d", index);
  ModelNode* node = &modelData->nodes[index];
  return luax_writenodetransform(L, node->transform, 3);
}

int l_lovrModelDataGetGlobalNodeTransform(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  int index = luaL_checkint(L, 2) - 1;
  lovrAssert(index >= 0 && index < modelData->nodeCount, "Invalid node index: %d", index);
  ModelNode* node = &modelData->nodes[index];
  return luax_writenodetransform(L, node->globalTransform, 3);
}

int l_lovrModelDataGetNodeParent(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  int index = luaL_checkint(L, 2) - 1;
  lovrAssert(index >= 0 && index < modelData->nodeCount, "Invalid node index: %d", index);
  ModelNode* node = &modelData->nodes[index];
  if (node->parent == -1) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, node->parent);
  }
  return 1;
}

int l_lovrModelDataGetNodeChildren(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  int index = luaL_checkint(L, 2) - 1;
  lovrAssert(index >= 0 && index < modelData->nodeCount, "Invalid node index: %d", index);
  ModelNode* node = &modelData->nodes[index];

  if (lua_istable(L, 3)) {
    lua_settop(L, 3);
  } else {
    lua_settop(L, 2);
    lua_createtable(L, node->children.length, 0);
  }

  for (int i = 0; i < node->children.length; i++) {
    lua_pushinteger(L, node->children.data[i]);
    lua_rawseti(L, 3, i + 1);
  }

  return 1;
}

int l_lovrModelDataGetNodeComponentCount(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  int index = luaL_checkint(L, 2) - 1;
  lovrAssert(index >= 0 && index < modelData->nodeCount, "Invalid node index: %d", index);
  ModelNode* node = &modelData->nodes[index];
  lua_pushinteger(L, node->primitives.length);
  return 1;
}

int l_lovrModelDataGetNodeComponent(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  int nodeIndex = luaL_checkint(L, 2) - 1;
  int primitiveIndex = luaL_checkint(L, 3) - 1;
  lovrAssert(nodeIndex >= 0 && nodeIndex < modelData->nodeCount, "Invalid node index: %d", nodeIndex + 1);
  ModelNode* node = &modelData->nodes[nodeIndex];
  lovrAssert(primitiveIndex >= 0 && primitiveIndex < node->primitives.length, "Invalid component index: %d", primitiveIndex + 1);
  ModelPrimitive* primitive = &modelData->primitives[node->primitives.data[primitiveIndex]];
  lua_pushinteger(L, primitive->drawStart);
  lua_pushinteger(L, primitive->drawCount);
  lua_pushinteger(L, primitive->material);
  return 3;
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

static ModelMaterial* luax_checkmodelmaterial(lua_State* L, int index) {
  ModelData* modelData = luax_checktype(L, index, ModelData);
  int materialIndex = luaL_checkint(L, index + 1) - 1;
  lovrAssert(materialIndex >= 0 && materialIndex < modelData->materialCount, "Invalid material index: %d", materialIndex + 1);
  return &modelData->materials[materialIndex];
}

int l_lovrModelDataGetMetalness(lua_State* L) {
  ModelMaterial* material = luax_checkmodelmaterial(L, 1);
  lua_pushnumber(L, material->metalness);
  return 1;
}

int l_lovrModelDataGetRoughness(lua_State* L) {
  ModelMaterial* material = luax_checkmodelmaterial(L, 1);
  lua_pushnumber(L, material->roughness);
  return 1;
}

int l_lovrModelDataGetDiffuseColor(lua_State* L) {
  ModelMaterial* material = luax_checkmodelmaterial(L, 1);
  Color color = material->diffuseColor;
  lua_pushnumber(L, color.r);
  lua_pushnumber(L, color.g);
  lua_pushnumber(L, color.b);
  lua_pushnumber(L, color.a);
  return 4;
}

int l_lovrModelDataGetEmissiveColor(lua_State* L) {
  ModelMaterial* material = luax_checkmodelmaterial(L, 1);
  Color color = material->emissiveColor;
  lua_pushnumber(L, color.r);
  lua_pushnumber(L, color.g);
  lua_pushnumber(L, color.b);
  lua_pushnumber(L, color.a);
  return 4;
}

int l_lovrModelDataGetDiffuseTexture(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  ModelMaterial* material = luax_checkmodelmaterial(L, 1);
  TextureData* textureData = modelData->textures.data[material->diffuseTexture];
  luax_pushtype(L, TextureData, textureData);
  return 1;
}

int l_lovrModelDataGetEmissiveTexture(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  ModelMaterial* material = luax_checkmodelmaterial(L, 1);
  TextureData* textureData = modelData->textures.data[material->emissiveTexture];
  luax_pushtype(L, TextureData, textureData);
  return 1;
}

int l_lovrModelDataGetMetalnessTexture(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  ModelMaterial* material = luax_checkmodelmaterial(L, 1);
  TextureData* textureData = modelData->textures.data[material->metalnessTexture];
  luax_pushtype(L, TextureData, textureData);
  return 1;
}

int l_lovrModelDataGetRoughnessTexture(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  ModelMaterial* material = luax_checkmodelmaterial(L, 1);
  TextureData* textureData = modelData->textures.data[material->roughnessTexture];
  luax_pushtype(L, TextureData, textureData);
  return 1;
}

int l_lovrModelDataGetOcclusionTexture(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  ModelMaterial* material = luax_checkmodelmaterial(L, 1);
  TextureData* textureData = modelData->textures.data[material->occlusionTexture];
  luax_pushtype(L, TextureData, textureData);
  return 1;
}

int l_lovrModelDataGetNormalTexture(lua_State* L) {
  ModelData* modelData = luax_checktype(L, 1, ModelData);
  ModelMaterial* material = luax_checkmodelmaterial(L, 1);
  TextureData* textureData = modelData->textures.data[material->normalTexture];
  luax_pushtype(L, TextureData, textureData);
  return 1;
}

const luaL_Reg lovrModelData[] = {
  { "getVertexData", l_lovrModelDataGetVertexData },
  { "getTriangleCount", l_lovrModelDataGetTriangleCount },
  { "getNodeCount", l_lovrModelDataGetNodeCount },
  { "getNodeName", l_lovrModelDataGetNodeName },
  { "getLocalNodeTransform", l_lovrModelDataGetLocalNodeTransform },
  { "getGlobalNodeTransform", l_lovrModelDataGetGlobalNodeTransform },
  { "getNodeParent", l_lovrModelDataGetNodeParent },
  { "getNodeChildren", l_lovrModelDataGetNodeChildren },
  { "getNodeComponentCount", l_lovrModelDataGetNodeComponentCount },
  { "getNodeComponent", l_lovrModelDataGetNodeComponent },
  { "getAnimationCount", l_lovrModelDataGetAnimationCount },
  { "getMaterialCount", l_lovrModelDataGetMaterialCount },
  { "getMetalness", l_lovrModelDataGetMetalness },
  { "getRoughness", l_lovrModelDataGetRoughness },
  { "getDiffuseColor", l_lovrModelDataGetDiffuseColor },
  { "getEmissiveColor", l_lovrModelDataGetEmissiveColor },
  { "getDiffuseTexture", l_lovrModelDataGetDiffuseTexture },
  { "getEmissiveTexture", l_lovrModelDataGetEmissiveTexture },
  { "getMetalnessTexture", l_lovrModelDataGetMetalnessTexture },
  { "getRoughnessTexture", l_lovrModelDataGetRoughnessTexture },
  { "getOcclusionTexture", l_lovrModelDataGetOcclusionTexture },
  { "getNormalTexture", l_lovrModelDataGetNormalTexture },
  { NULL, NULL }
};
