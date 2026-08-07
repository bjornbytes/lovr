#include "api.h"
#include "graphics/graphics.h"
#include "data/modelData.h"
#include "math/math.h"
#include "core/maf.h"
#include "util.h"

uint32_t luax_checkblendshape(lua_State* L, int index, Model* model) {
  const ModelMetadata* meta = lovrModelGetMetadata(model);
  switch (lua_type(L, index)) {
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, index, &length);
      uint32_t hash = (uint32_t) hash64(name, length);
      for (uint32_t i = 0; i < meta->animationCount; i++) {
        if (meta->animationLookup[i] == hash) {
          return i;
        }
      }
      return luaL_error(L, "Model has no blend shape named '%s'", name);
    }
    case LUA_TNUMBER: {
      uint32_t blendShape = luax_checku32(L, index) - 1;
      luax_check(L, blendShape < meta->blendShapeCount, "Invalid blend shape index '%d'", blendShape + 1);
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

static int l_lovrModelIsNodeVisible(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetMetadata(model));
  bool visible = lovrModelIsNodeVisible(model, node);
  lua_pushboolean(L, visible);
  return 1;
}

static int l_lovrModelSetNodeVisible(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetMetadata(model));
  bool visible = lua_toboolean(L, 3);
  lovrModelSetNodeVisible(model, node, visible);
  return 0;
}

static int l_lovrModelGetNodePosition(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetMetadata(model));
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
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetMetadata(model));
  float position[3];
  int index = luax_readvec3(L, 3, position, NULL);
  float alpha = luax_optfloat(L, index, 1.f);
  lovrModelSetNodeTransform(model, node, position, NULL, NULL, alpha);
  return 0;
}

static int l_lovrModelGetNodeScale(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetMetadata(model));
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
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetMetadata(model));
  float scale[3];
  int index = luax_readscale(L, 3, scale, 3, NULL);
  float alpha = luax_optfloat(L, index, 1.f);
  lovrModelSetNodeTransform(model, node, NULL, scale, NULL, alpha);
  return 0;
}

static int l_lovrModelGetNodeOrientation(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetMetadata(model));
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
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetMetadata(model));
  float rotation[4];
  int index = luax_readquat(L, 3, rotation, NULL);
  float alpha = luax_optfloat(L, index, 1.f);
  lovrModelSetNodeTransform(model, node, NULL, NULL, rotation, alpha);
  return 0;
}

static int l_lovrModelGetNodePose(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetMetadata(model));
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
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetMetadata(model));
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
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetMetadata(model));
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
  uint32_t node = luax_checknodeindex(L, 2, lovrModelGetMetadata(model));
  int index = 3;
  float position[3], scale[3], rotation[4];
  Mat4* matrix = luax_totype(L, index, Mat4);
  if (matrix) {
    float* m = lovrMat4GetData(matrix);
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

static int l_lovrModelHasJoints(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  const ModelMetadata* meta = lovrModelGetMetadata(model);
  lua_pushboolean(L, meta->skinCount > 0);
  return 1;
}

static int l_lovrModelAnimate(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t animation = luax_checkanimationindex(L, 2, lovrModelGetMetadata(model));
  float time = luax_checkfloat(L, 3);
  float alpha = luax_optfloat(L, 4, 1.f);
  luax_assert(L, lovrModelAnimate(model, animation, time, alpha));
  return 0;
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

static int l_lovrModelGetTexture(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t index = luax_checku32(L, 2) - 1;
  Texture* texture = lovrModelGetTexture(model, index);
  luax_assert(L, texture);
  luax_pushtype(L, Texture, texture);
  return 1;
}

static int l_lovrModelGetMaterial(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t index = luax_checkmaterialindex(L, 2, lovrModelGetMetadata(model));
  Material* material = lovrModelGetMaterial(model, index);
  luax_assert(L, material);
  luax_pushtype(L, Material, material);
  return 1;
}

static int l_lovrModelBuildRaytracer(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  luax_assert(L, lovrModelBuildRaytracer(model));
  return 0;
}

int luax_modelmeshiterator(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  ModelMetadata* meta = lovrModelGetMetadata(model);
  lua_settop(L, 2);
  uint32_t node = lua_type(L, 2) == LUA_TNIL ? ~0u : (uint32_t) lua_tonumber(L, 2) - 1;
  uint32_t next = lovrModelMetadataNextNodeWithMesh(meta, node);
  if (next == ~0u) {
    lua_pushnil(L);
    return 1;
  } else {
    lua_pushinteger(L, next + 1);
    lua_pushinteger(L, meta->nodes[next].mesh + 1);
    return 2;
  }
}

int l_lovrModelMeshes(lua_State* L) {
  luax_checktype(L, 1, Model);
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_pushvalue(L, 1);
  lua_pushnil(L);
  return 3;
}

// Deprecated

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

static int l_lovrModelGetMesh(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  uint32_t index = luax_checku32(L, 2) - 1;
  Mesh* mesh = lovrModelGetMesh(model, index);
  luax_assert(L, mesh);
  luax_pushtype(L, Mesh, mesh);
  return 1;
}

// Shared Model/ModelData methods
int l_lovrModelMetaGetMetadata(lua_State* L);
int l_lovrModelMetaGetRootNode(lua_State* L);
int l_lovrModelMetaGetNodeCount(lua_State* L);
int l_lovrModelMetaGetNodeName(lua_State* L);
int l_lovrModelMetaGetNodeChild(lua_State* L);
int l_lovrModelMetaGetNodeChildren(lua_State* L);
int l_lovrModelMetaGetNodeSibling(lua_State* L);
int l_lovrModelMetaGetNodeParent(lua_State* L);
int l_lovrModelMetaGetNodeMesh(lua_State* L);
int l_lovrModelMetaGetAnimationCount(lua_State* L);
int l_lovrModelMetaGetAnimationName(lua_State* L);
int l_lovrModelMetaGetAnimationDuration(lua_State* L);
int l_lovrModelMetaGetBlendShapeCount(lua_State* L);
int l_lovrModelMetaGetBlendShapeName(lua_State* L);
int l_lovrModelMetaGetWidth(lua_State* L);
int l_lovrModelMetaGetHeight(lua_State* L);
int l_lovrModelMetaGetDepth(lua_State* L);
int l_lovrModelMetaGetDimensions(lua_State* L);
int l_lovrModelMetaGetCenter(lua_State* L);
int l_lovrModelMetaGetBoundingBox(lua_State* L);
int l_lovrModelMetaGetMeshCount(lua_State* L);
int l_lovrModelMetaGetMeshBlendShapeCount(lua_State* L);
int l_lovrModelMetaGetMeshBlendShapeName(lua_State* L);
int l_lovrModelMetaGetMeshVertexCount(lua_State* L);
int l_lovrModelMetaGetMeshIndexCount(lua_State* L);
int l_lovrModelMetaGetMeshPartCount(lua_State* L);
int l_lovrModelMetaGetMeshDrawMode(lua_State* L);
int l_lovrModelMetaGetMeshDrawRange(lua_State* L);
int l_lovrModelMetaGetMeshMaterial(lua_State* L);
int l_lovrModelMetaGetImageCount(lua_State* L);
int l_lovrModelMetaGetMaterialCount(lua_State* L);
int l_lovrModelMetaGetMaterialName(lua_State* L);

const luaL_Reg lovrModel[] = {
  { "clone", l_lovrModelClone },
  { "getMetadata", l_lovrModelMetaGetMetadata },

  { "getRootNode", l_lovrModelMetaGetRootNode },
  { "getNodeCount", l_lovrModelMetaGetNodeCount },
  { "getNodeName", l_lovrModelMetaGetNodeName },
  { "getNodeChild", l_lovrModelMetaGetNodeChild },
  { "getNodeChildren", l_lovrModelMetaGetNodeChildren },
  { "getNodeSibling", l_lovrModelMetaGetNodeSibling },
  { "getNodeParent", l_lovrModelMetaGetNodeParent },
  { "getNodeMesh", l_lovrModelMetaGetNodeMesh },
  { "isNodeVisible", l_lovrModelIsNodeVisible },
  { "setNodeVisible", l_lovrModelSetNodeVisible },
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

  { "getAnimationCount", l_lovrModelMetaGetAnimationCount },
  { "getAnimationName", l_lovrModelMetaGetAnimationName },
  { "getAnimationDuration", l_lovrModelMetaGetAnimationDuration },
  { "hasJoints", l_lovrModelHasJoints },
  { "animate", l_lovrModelAnimate },

  { "getBlendShapeCount", l_lovrModelMetaGetBlendShapeCount },
  { "getBlendShapeName", l_lovrModelMetaGetBlendShapeName },
  { "getBlendShapeWeight", l_lovrModelGetBlendShapeWeight },
  { "setBlendShapeWeight", l_lovrModelSetBlendShapeWeight },
  { "resetBlendShapes", l_lovrModelResetBlendShapes },

  { "getWidth", l_lovrModelMetaGetWidth },
  { "getHeight", l_lovrModelMetaGetHeight },
  { "getDepth", l_lovrModelMetaGetDepth },
  { "getDimensions", l_lovrModelMetaGetDimensions },
  { "getCenter", l_lovrModelMetaGetCenter },
  { "getBoundingBox", l_lovrModelMetaGetBoundingBox },

  { "getMeshCount", l_lovrModelMetaGetMeshCount },
  { "getMeshBlendShapeCount", l_lovrModelMetaGetMeshBlendShapeCount },
  { "getMeshBlendShapeName", l_lovrModelMetaGetMeshBlendShapeName },
  { "getMeshVertexCount", l_lovrModelMetaGetMeshVertexCount },
  { "getMeshIndexCount", l_lovrModelMetaGetMeshIndexCount },
  { "getMeshPartCount", l_lovrModelMetaGetMeshPartCount },
  { "getMeshDrawMode", l_lovrModelMetaGetMeshDrawMode },
  { "getMeshDrawRange", l_lovrModelMetaGetMeshDrawRange },
  { "getMeshMaterial", l_lovrModelMetaGetMeshMaterial },

  { "getTextureCount", l_lovrModelMetaGetImageCount },
  { "getTexture", l_lovrModelGetTexture },
  { "getMaterialCount", l_lovrModelMetaGetMaterialCount },
  { "getMaterialName", l_lovrModelMetaGetMaterialName },
  { "getMaterial", l_lovrModelGetMaterial },

  { "buildRaytracer", l_lovrModelBuildRaytracer },

  { "getVertexBuffer", l_lovrModelGetVertexBuffer }, // Deprecated
  { "getIndexBuffer", l_lovrModelGetIndexBuffer }, // Deprecated
  { "getMesh", l_lovrModelGetMesh }, // Deprecated

  { NULL, NULL }
};
