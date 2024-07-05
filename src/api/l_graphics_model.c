#include "api.h"
#include "graphics/graphics.h"
#include "data/modelData.h"
#include "core/maf.h"
#include "util.h"

// This adds about 2-3us of overhead, which sucks, but the reduction in complexity is large
static int luax_callmodeldata(lua_State* L, const char* method, int nrets) {
  int nargs = lua_gettop(L);
  Model* model = luax_checktype(L, 1, Model);
  ModelData* data = lovrModelGetInfo(model)->data;
  luax_pushtype(L, ModelData, data);
  lua_pushstring(L, method);
  lua_gettable(L, -2);
  lua_insert(L, 1);
  lua_replace(L, 2);
  lua_call(L, nargs, nrets);
  return nrets;
}

uint32_t luax_checkblendshape(lua_State* L, int index, Model* model) {
  switch (lua_type(L, index)) {
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, index, &length);
      ModelData* data = lovrModelGetInfo(model)->data;
      uint64_t blendShapeIndex = map_get(data->blendShapeMap, hash64(name, length));
      luax_check(L, blendShapeIndex != MAP_NIL, "ModelData has no blend shape named '%s'", name);
      return (uint32_t) blendShapeIndex;
    }
    case LUA_TNUMBER: {
      uint32_t blendShape = luax_checku32(L, index) - 1;
      ModelData* data = lovrModelGetInfo(model)->data;
      luax_check(L, blendShape < data->blendShapeCount, "Invalid blend shape index '%d'", blendShape + 1);
      return blendShape;
    }
    default: return luax_typeerror(L, index, "number or string"), ~0u;
  }
}

static int l_lovrModelClone(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Model* clone = lovrModelClone(model);
  luax_pushtype(L, Model, clone);
  lovrRelease(clone, lovrModelDestroy);
  return 1;
}

static int l_lovrModelGetData(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  ModelData* data = lovrModelGetInfo(model)->data;
  luax_pushtype(L, ModelData, data);
  return 1;
}

static int l_lovrModelGetMetadata(lua_State* L) {
  return luax_callmodeldata(L, "getMetadata", 1);
}

static int l_lovrModelGetRootNode(lua_State* L) {
  return luax_callmodeldata(L, "getRootNode", 1);
}

static int l_lovrModelGetNodeCount(lua_State* L) {
  return luax_callmodeldata(L, "getNodeCount", 1);
}

static int l_lovrModelGetNodeName(lua_State* L) {
  return luax_callmodeldata(L, "getNodeName", 1);
}

static int l_lovrModelGetNodeParent(lua_State* L) {
  return luax_callmodeldata(L, "getNodeParent", 1);
}

static int l_lovrModelGetNodeChildren(lua_State* L) {
  return luax_callmodeldata(L, "getNodeChildren", 1);
}

static int l_lovrModelGetNodePosition(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetInfo(model)->data);
  OriginType origin = luax_checkenum(L, 3, OriginType, "root");
  float position[3], scale[3], rotation[4];
  lovrModelGetNodeTransform(model, node, position, scale, rotation, origin);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrModelSetNodePosition(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetInfo(model)->data);
  float position[3];
  int index = luax_readvec3(L, 3, position, NULL);
  float alpha = luax_optfloat(L, index, 1.f);
  lovrModelSetNodeTransform(model, node, position, NULL, NULL, alpha);
  return 0;
}

static int l_lovrModelGetNodeScale(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetInfo(model)->data);
  OriginType origin = luax_checkenum(L, 3, OriginType, "root");
  float position[3], scale[3], rotation[4];
  lovrModelGetNodeTransform(model, node, position, scale, rotation, origin);
  lua_pushnumber(L, scale[0]);
  lua_pushnumber(L, scale[1]);
  lua_pushnumber(L, scale[2]);
  return 3;
}

static int l_lovrModelSetNodeScale(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetInfo(model)->data);
  float scale[3];
  int index = luax_readscale(L, 3, scale, 3, NULL);
  float alpha = luax_optfloat(L, index, 1.f);
  lovrModelSetNodeTransform(model, node, NULL, scale, NULL, alpha);
  return 0;
}

static int l_lovrModelGetNodeOrientation(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetInfo(model)->data);
  OriginType origin = luax_checkenum(L, 3, OriginType, "root");
  float position[3], scale[3], rotation[4], angle, ax, ay, az;
  lovrModelGetNodeTransform(model, node, position, scale, rotation, origin);
  quat_getAngleAxis(rotation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

static int l_lovrModelSetNodeOrientation(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetInfo(model)->data);
  float rotation[4];
  int index = luax_readquat(L, 3, rotation, NULL);
  float alpha = luax_optfloat(L, index, 1.f);
  lovrModelSetNodeTransform(model, node, NULL, NULL, rotation, alpha);
  return 0;
}

static int l_lovrModelGetNodePose(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetInfo(model)->data);
  OriginType origin = luax_checkenum(L, 3, OriginType, "root");
  float position[3], scale[3], rotation[4], angle, ax, ay, az;
  lovrModelGetNodeTransform(model, node, position, scale, rotation, origin);
  quat_getAngleAxis(rotation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrModelSetNodePose(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetInfo(model)->data);
  int index = 3;
  float position[3], rotation[4];
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readquat(L, index, rotation, NULL);
  float alpha = luax_optfloat(L, index, 1.f);
  lovrModelSetNodeTransform(model, node, position, NULL, rotation, alpha);
  return 0;
}

static int l_lovrModelGetNodeTransform(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetInfo(model)->data);
  OriginType origin = luax_checkenum(L, 3, OriginType, "root");
  float position[3], scale[3], rotation[4], angle, ax, ay, az;
  lovrModelGetNodeTransform(model, node, position, scale, rotation, origin);
  quat_getAngleAxis(rotation, &angle, &ax, &ay, &az);
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
  return 10;
}

static int l_lovrModelSetNodeTransform(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetInfo(model)->data);
  int index = 3;
  VectorType type;
  float position[3], scale[3], rotation[4];
  float* m = luax_tovector(L, index, &type);
  if (m && type == V_MAT4) {
    mat4_getPosition(m, position);
    mat4_getScale(m, scale);
    mat4_getOrientation(m, rotation);
    index = 4;
  } else {
    index = luax_readvec3(L, index, position, NULL);
    index = luax_readscale(L, index, scale, 3, NULL);
    index = luax_readquat(L, index, rotation, NULL);
  }
  float alpha = luax_optfloat(L, index, 1.f);
  lovrModelSetNodeTransform(model, node, position, scale, rotation, alpha);
  return 0;
}

static int l_lovrModelResetNodeTransforms(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  lovrModelResetNodeTransforms(model);
  return 0;
}

static int l_lovrModelGetAnimationCount(lua_State* L) {
  return luax_callmodeldata(L, "getAnimationCount", 1);
}

static int l_lovrModelGetAnimationName(lua_State* L) {
  return luax_callmodeldata(L, "getAnimationName", 1);
}

static int l_lovrModelGetAnimationDuration(lua_State* L) {
  return luax_callmodeldata(L, "getAnimationDuration", 1);
}

static int l_lovrModelHasJoints(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  ModelData* data = lovrModelGetInfo(model)->data;
  lua_pushboolean(L, data->skinCount > 0);
  return 1;
}

static int l_lovrModelAnimate(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t animation = luax_checkanimationindex(L, 2, lovrModelGetInfo(model)->data);
  float time = luax_checkfloat(L, 3);
  float alpha = luax_optfloat(L, 4, 1.f);
  luax_assert(L, lovrModelAnimate(model, animation, time, alpha));
  return 0;
}

static int l_lovrModelGetBlendShapeCount(lua_State* L) {
  return luax_callmodeldata(L, "getBlendShapeCount", 1);
}

static int l_lovrModelGetBlendShapeName(lua_State* L) {
  return luax_callmodeldata(L, "getBlendShapeName", 1);
}

static int l_lovrModelGetBlendShapeWeight(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t blendShape = luax_checkblendshape(L, 2, model);
  float weight = lovrModelGetBlendShapeWeight(model, blendShape);
  lua_pushnumber(L, weight);
  return 1;
}

static int l_lovrModelSetBlendShapeWeight(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t blendShape = luax_checkblendshape(L, 2, model);
  float weight = luax_checkfloat(L, 3);
  lovrModelSetBlendShapeWeight(model, blendShape, weight);
  return 0;
}

static int l_lovrModelResetBlendShapes(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  lovrModelResetBlendShapes(model);
  return 0;
}

static int l_lovrModelGetTriangles(lua_State* L) {
  return luax_callmodeldata(L, "getTriangles", 2);
}

static int l_lovrModelGetTriangleCount(lua_State* L) {
  return luax_callmodeldata(L, "getTriangleCount", 1);
}

static int l_lovrModelGetVertexCount(lua_State* L) {
  return luax_callmodeldata(L, "getVertexCount", 1);
}

static int l_lovrModelGetWidth(lua_State* L) {
  return luax_callmodeldata(L, "getWidth", 1);
}

static int l_lovrModelGetHeight(lua_State* L) {
  return luax_callmodeldata(L, "getHeight", 1);
}

static int l_lovrModelGetDepth(lua_State* L) {
  return luax_callmodeldata(L, "getDepth", 1);
}

static int l_lovrModelGetDimensions(lua_State* L) {
  return luax_callmodeldata(L, "getDimensions", 3);
}

static int l_lovrModelGetCenter(lua_State* L) {
  return luax_callmodeldata(L, "getCenter", 3);
}

static int l_lovrModelGetBoundingBox(lua_State* L) {
  return luax_callmodeldata(L, "getBoundingBox", 6);
}

static int l_lovrModelGetBoundingSphere(lua_State* L) {
  return luax_callmodeldata(L, "getBoundingSphere", 4);
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

static int l_lovrModelGetMeshCount(lua_State* L) {
  return luax_callmodeldata(L, "getMeshCount", 1);
}

static int l_lovrModelGetMesh(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t index = luax_checku32(L, 2) - 1;
  Mesh* mesh = lovrModelGetMesh(model, index);
  luax_assert(L, mesh);
  luax_pushtype(L, Mesh, mesh);
  return 1;
}

static int l_lovrModelGetTextureCount(lua_State* L) {
  return luax_callmodeldata(L, "getImageCount", 1);
}

static int l_lovrModelGetTexture(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t index = luax_checku32(L, 2) - 1;
  Texture* texture = lovrModelGetTexture(model, index);
  luax_assert(L, texture);
  luax_pushtype(L, Texture, texture);
  return 1;
}

static int l_lovrModelGetMaterialCount(lua_State* L) {
  return luax_callmodeldata(L, "getMaterialCount", 1);
}

static int l_lovrModelGetMaterialName(lua_State* L) {
  return luax_callmodeldata(L, "getMaterialName", 1);
}

static int l_lovrModelGetMaterial(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t index = luax_checkmaterialindex(L, 2, lovrModelGetInfo(model)->data);
  Material* material = lovrModelGetMaterial(model, index);
  luax_assert(L, material);
  luax_pushtype(L, Material, material);
  return 1;
}

const luaL_Reg lovrModel[] = {
  { "clone", l_lovrModelClone },
  { "getData", l_lovrModelGetData },
  { "getMetadata", l_lovrModelGetMetadata },
  { "getRootNode", l_lovrModelGetRootNode },
  { "getNodeCount", l_lovrModelGetNodeCount },
  { "getNodeName", l_lovrModelGetNodeName },
  { "getNodeParent", l_lovrModelGetNodeParent },
  { "getNodeChildren", l_lovrModelGetNodeChildren },
  { "getNodePosition", l_lovrModelGetNodePosition },
  { "setNodePosition", l_lovrModelSetNodePosition },
  { "getNodeOrientation", l_lovrModelGetNodeOrientation },
  { "setNodeOrientation", l_lovrModelSetNodeOrientation },
  { "getNodeScale", l_lovrModelGetNodeScale },
  { "setNodeScale", l_lovrModelSetNodeScale },
  { "getNodePose", l_lovrModelGetNodePose },
  { "setNodePose", l_lovrModelSetNodePose },
  { "getNodeTransform", l_lovrModelGetNodeTransform },
  { "setNodeTransform", l_lovrModelSetNodeTransform },
  { "resetNodeTransforms", l_lovrModelResetNodeTransforms },
  { "getAnimationCount", l_lovrModelGetAnimationCount },
  { "getAnimationName", l_lovrModelGetAnimationName },
  { "getAnimationDuration", l_lovrModelGetAnimationDuration },
  { "hasJoints", l_lovrModelHasJoints },
  { "animate", l_lovrModelAnimate },
  { "getBlendShapeCount", l_lovrModelGetBlendShapeCount },
  { "getBlendShapeName", l_lovrModelGetBlendShapeName },
  { "getBlendShapeWeight", l_lovrModelGetBlendShapeWeight },
  { "setBlendShapeWeight", l_lovrModelSetBlendShapeWeight },
  { "resetBlendShapes", l_lovrModelResetBlendShapes },
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
  { "getVertexBuffer", l_lovrModelGetVertexBuffer },
  { "getIndexBuffer", l_lovrModelGetIndexBuffer },
  { "getMeshCount", l_lovrModelGetMeshCount },
  { "getMesh", l_lovrModelGetMesh },
  { "getTextureCount", l_lovrModelGetTextureCount },
  { "getTexture", l_lovrModelGetTexture },
  { "getMaterialCount", l_lovrModelGetMaterialCount },
  { "getMaterialName", l_lovrModelGetMaterialName },
  { "getMaterial", l_lovrModelGetMaterial },
  { NULL, NULL }
};
