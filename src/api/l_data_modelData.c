#include "api.h"
#include "data/modelData.h"
#include "graphics/graphics.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>

ModelMetadata* luax_checkmodelmeta(lua_State* L, int index) {
  ModelData* modelData = luax_totype(L, index, ModelData);

  if (modelData) {
    return &modelData->meta;
  }

#ifndef LOVR_DISABLE_GRAPHICS
  Model* model = luax_totype(L, index, Model);

  if (model) {
    return lovrModelGetMetadata(model);
  }
#endif

  luax_typeerror(L, index, "Model or ModelData");
  return NULL;
}

uint32_t luax_checkanimationindex(lua_State* L, int index, ModelMetadata* meta) {
  switch (lua_type(L, index)) {
    case LUA_TNUMBER: {
      uint32_t animation = luax_checku32(L, index) - 1;
      luax_check(L, animation < meta->animationCount, "Invalid animation index '%d'", animation + 1);
      return animation;
    }
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, index, &length);
      uint32_t hash = (uint32_t) hash64(name, length);
      for (uint32_t i = 0; i < meta->animationCount; i++) {
        if (meta->animationLookup[i] == hash) {
          return i;
        }
      }
      return luaL_error(L, "Model has no animation named '%s'", name);
    }
    default: return luax_typeerror(L, index, "number or string"), ~0u;
  }
}

uint32_t luax_checkmaterialindex(lua_State* L, int index, ModelMetadata* meta) {
  switch (lua_type(L, index)) {
    case LUA_TNUMBER: {
      uint32_t material = luax_checku32(L, index) - 1;
      luax_check(L, material < meta->materialCount, "Invalid material index '%d'", material + 1);
      return material;
    }
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, index, &length);
      uint32_t hash = (uint32_t) hash64(name, length);
      for (uint32_t i = 0; i < meta->materialCount; i++) {
        if (meta->materialLookup[i] == hash) {
          return i;
        }
      }
      return luaL_error(L, "Model has no material named '%s'", name);
    }
    default: return luax_typeerror(L, index, "number or string"), ~0u;
  }
}

uint32_t luax_checknodeindex(lua_State* L, int index, ModelMetadata* meta) {
  switch (lua_type(L, index)) {
    case LUA_TNUMBER: {
      uint32_t node = luax_checku32(L, index) - 1;
      luax_check(L, node < meta->nodeCount, "Invalid node index '%d'", node + 1);
      return node;
    }
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, index, &length);
      uint32_t hash = (uint32_t) hash64(name, length);
      for (uint32_t i = 0; i < meta->nodeCount; i++) {
        if (meta->nodeLookup[i] == hash) {
          return i;
        }
      }
      return luaL_error(L, "Model has no node named '%s'", name);
    }
    default: return luax_typeerror(L, index, "number or string"), ~0u;
  }
}

static uint32_t luax_checkmeshindex(lua_State* L, int index, ModelMetadata* meta) {
  uint32_t mesh = luax_checku32(L, index) - 1;
  luax_check(L, mesh < meta->meshCount, "Invalid mesh index '%d'", mesh + 1);
  return mesh;
}

static ModelPart* luax_checkmeshpart(lua_State* L, int index, ModelMetadata* meta) {
  uint32_t mesh = luax_checkmeshindex(L, index, meta);
  uint32_t part = luax_optu32(L, index + 1, 1) - 1;
  luax_check(L, part < meta->meshes[mesh].partCount, "Invalid part index '%d'", part + 1);
  return &meta->meshes[mesh].parts[part];
}

int l_lovrModelMetaGetMetadata(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);

  if (!meta->comment || meta->commentLength == 0) {
    lua_pushnil(L);
  } else {
    lua_pushlstring(L, meta->comment, meta->commentLength);
  }

  return 1;
}

int l_lovrModelMetaGetRootNode(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  lua_pushinteger(L, meta->rootNode + 1);
  return 1;
}

int l_lovrModelMetaGetNodeCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  lua_pushinteger(L, meta->nodeCount);
  return 1;
}

int l_lovrModelMetaGetNodeName(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  uint32_t index = luax_checku32(L, 2) - 1;
  luax_check(L, index < meta->nodeCount, "Invalid node index '%d'", index + 1);
  lua_pushstring(L, meta->nodes[index].name);
  return 1;
}

int l_lovrModelMetaGetNodeChild(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelNode* node = &meta->nodes[luax_checknodeindex(L, 2, meta)];
  if (node->child != ~0u) {
    lua_pushinteger(L, node->child + 1);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

int l_lovrModelMetaGetNodeChildren(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelNode* node = &meta->nodes[luax_checknodeindex(L, 2, meta)];
  lua_newtable(L);
  for (uint32_t i = node->child; i != ~0u; i = meta->nodes[i].sibling) {
    lua_pushinteger(L, i + 1);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

int l_lovrModelMetaGetNodeSibling(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelNode* node = &meta->nodes[luax_checknodeindex(L, 2, meta)];
  if (node->sibling != ~0u) {
    lua_pushinteger(L, node->sibling + 1);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

int l_lovrModelMetaGetNodeParent(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelNode* node = &meta->nodes[luax_checknodeindex(L, 2, meta)];
  if (node->parent != ~0u) {
    lua_pushinteger(L, node->parent + 1);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

int l_lovrModelMetaGetNodePosition(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelNode* node = &meta->nodes[luax_checknodeindex(L, 2, meta)];
  if (node->hasMatrix) {
    float position[3];
    mat4_getPosition(node->transform.matrix, position);
    lua_pushnumber(L, position[0]);
    lua_pushnumber(L, position[1]);
    lua_pushnumber(L, position[2]);
    return 3;
  } else {
    lua_pushnumber(L, node->transform.translation[0]);
    lua_pushnumber(L, node->transform.translation[1]);
    lua_pushnumber(L, node->transform.translation[2]);
    return 3;
  }
}

int l_lovrModelMetaGetNodeOrientation(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelNode* node = &meta->nodes[luax_checknodeindex(L, 2, meta)];
  float angle, ax, ay, az;
  if (node->hasMatrix) {
    mat4_getAngleAxis(node->transform.matrix, &angle, &ax, &ay, &az);
  } else {
    quat_getAngleAxis(node->transform.rotation, &angle, &ax, &ay, &az);
  }
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

int l_lovrModelMetaGetNodeScale(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelNode* node = &meta->nodes[luax_checknodeindex(L, 2, meta)];
  if (node->hasMatrix) {
    float scale[3];
    mat4_getScale(node->transform.matrix, scale);
    lua_pushnumber(L, scale[0]);
    lua_pushnumber(L, scale[1]);
    lua_pushnumber(L, scale[2]);
  } else {
    lua_pushnumber(L, node->transform.scale[0]);
    lua_pushnumber(L, node->transform.scale[1]);
    lua_pushnumber(L, node->transform.scale[2]);
  }
  return 3;
}

int l_lovrModelMetaGetNodePose(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelNode* node = &meta->nodes[luax_checknodeindex(L, 2, meta)];
  if (node->hasMatrix) {
    float position[3], angle, ax, ay, az;
    mat4_getPosition(node->transform.matrix, position);
    mat4_getAngleAxis(node->transform.matrix, &angle, &ax, &ay, &az);
    lua_pushnumber(L, position[0]);
    lua_pushnumber(L, position[1]);
    lua_pushnumber(L, position[2]);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
  } else {
    float angle, ax, ay, az;
    quat_getAngleAxis(node->transform.rotation, &angle, &ax, &ay, &az);
    lua_pushnumber(L, node->transform.translation[0]);
    lua_pushnumber(L, node->transform.translation[1]);
    lua_pushnumber(L, node->transform.translation[2]);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
  }
  return 7;
}

int l_lovrModelMetaGetNodeTransform(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelNode* node = &meta->nodes[luax_checknodeindex(L, 2, meta)];
  if (node->hasMatrix) {
    float position[3], scale[4], angle, ax, ay, az;
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
    quat_getAngleAxis(node->transform.rotation, &angle, &ax, &ay, &az);
    lua_pushnumber(L, node->transform.translation[0]);
    lua_pushnumber(L, node->transform.translation[1]);
    lua_pushnumber(L, node->transform.translation[2]);
    lua_pushnumber(L, node->transform.scale[0]);
    lua_pushnumber(L, node->transform.scale[1]);
    lua_pushnumber(L, node->transform.scale[2]);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
  }
  return 10;
}

int l_lovrModelMetaGetNodeMesh(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelNode* node = &meta->nodes[luax_checknodeindex(L, 2, meta)];
  if (node->mesh == ~0u) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, node->mesh + 1);
  }
  return 1;
}

int l_lovrModelMetaGetNodeSkin(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelNode* node = &meta->nodes[luax_checknodeindex(L, 2, meta)];
  if (node->skin == ~0u) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, node->skin + 1);
  }
  return 1;
}

int l_lovrModelMetaGetMeshCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  lua_pushinteger(L, meta->meshCount);
  return 1;
}

int l_lovrModelMetaGetMeshBlendShapeCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  uint32_t mesh = luax_checkmeshindex(L, 2, meta);
  lua_pushinteger(L, meta->meshes[mesh].blendShapeCount);
  return 1;
}

int l_lovrModelMetaGetMeshBlendShapeName(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  uint32_t mesh = luax_checkmeshindex(L, 2, meta);
  uint32_t blendShape = luax_checku32(L, 3) - 1;
  luax_check(L, blendShape < meta->meshes[mesh].blendShapeCount, "Blend shape %d is out of range", blendShape + 1);
  lua_pushstring(L, meta->meshes[mesh].blendShapes[blendShape].name);
  return 1;
}

int l_lovrModelDataGetMeshBlendVertex(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelMetadata* meta = &model->meta;
  uint32_t meshIndex = luax_checkmeshindex(L, 2, meta);
  uint32_t blendShape = luax_checku32(L, 3) - 1;
  uint32_t vertex = luax_checku32(L, 4) - 1;
  ModelMesh* mesh = &meta->meshes[meshIndex];
  luax_check(L, blendShape < mesh->blendShapeCount, "Blend shape %d is out of range", blendShape + 1);
  luax_check(L, vertex < mesh->vertexCount, "Vertex %d is out of range", vertex + 1);
  BlendData* data = &model->blendData[mesh->blendDataOffset + blendShape * mesh->vertexCount + vertex];
  lua_pushnumber(L, data->x);
  lua_pushnumber(L, data->y);
  lua_pushnumber(L, data->z);
  lua_pushnumber(L, data->nx);
  lua_pushnumber(L, data->ny);
  lua_pushnumber(L, data->nz);
  lua_pushnumber(L, data->tx);
  lua_pushnumber(L, data->ty);
  lua_pushnumber(L, data->tz);
  return 9;
}

int l_lovrModelMetaGetMeshVertexCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  uint32_t mesh = luax_checkmeshindex(L, 2, meta);
  lua_pushinteger(L, meta->meshes[mesh].vertexCount);
  return 1;
}

int l_lovrModelMetaGetMeshIndexCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  uint32_t mesh = luax_checkmeshindex(L, 2, meta);
  lua_pushinteger(L, meta->meshes[mesh].indexCount);
  return 1;
}

static int l_lovrModelDataGetMeshVertex(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t mesh = luax_checkmeshindex(L, 2, &model->meta);
  uint32_t index = luax_checku32(L, 3) - 1;
  luax_check(L, index < model->meta.meshes[mesh].vertexCount, "Vertex %d is out of range", index + 1);
  ModelVertex* vertex = &model->vertices[model->meta.meshes[mesh].vertexOffset + index];
  lua_pushnumber(L, vertex->position.x);
  lua_pushnumber(L, vertex->position.y);
  lua_pushnumber(L, vertex->position.z);
#ifdef LOVR_WEBGPU
  lua_pushnumber(L, vertex->normal.x);
  lua_pushnumber(L, vertex->normal.y);
  lua_pushnumber(L, vertex->normal.z);
#else
  lua_pushnumber(L, MAX(((int32_t) (vertex->normal << 22) >> 22) / 511.f, -1.f));
  lua_pushnumber(L, MAX(((int32_t) (vertex->normal << 12) >> 22) / 511.f, -1.f));
  lua_pushnumber(L, MAX(((int32_t) (vertex->normal <<  2) >> 22) / 511.f, -1.f));
#endif
  lua_pushnumber(L, vertex->uv.u);
  lua_pushnumber(L, vertex->uv.v);
  lua_pushnumber(L, vertex->uv2.u / 65535.f);
  lua_pushnumber(L, vertex->uv2.v / 65535.f);
  lua_pushinteger(L, vertex->color.r);
  lua_pushinteger(L, vertex->color.g);
  lua_pushinteger(L, vertex->color.b);
  lua_pushinteger(L, vertex->color.a);
#ifdef LOVR_WEBGPU
  lua_pushnumber(L, vertex->tangent.x);
  lua_pushnumber(L, vertex->tangent.y);
  lua_pushnumber(L, vertex->tangent.z);
#else
  lua_pushnumber(L, MAX(((int32_t) (vertex->tangent << 22) >> 22) / 511.f, -1.f));
  lua_pushnumber(L, MAX(((int32_t) (vertex->tangent << 12) >> 22) / 511.f, -1.f));
  lua_pushnumber(L, MAX(((int32_t) (vertex->tangent <<  2) >> 22) / 511.f, -1.f));
#endif
  return 17;
}

static int l_lovrModelDataGetMeshIndex(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t meshIndex = luax_checkmeshindex(L, 2, &model->meta);
  ModelMesh* mesh = &model->meta.meshes[meshIndex];

  uint32_t index = luax_checku32(L, 3) - 1;
  luax_check(L, index < mesh->indexCount, "Index %d is out of range", index + 1);

  // Add the part's base vertex to the index value, so that everything is relative to the beginning
  // of the mesh and people don't have to worry about base vertex.  But we have to find the part...
  uint32_t part = 0;
  while (index < mesh->parts[part].start) {
    part++;
  }

  if (model->meta.indexSize == 4) {
    lua_pushinteger(L, ((uint32_t*) model->indices)[mesh->indexOffset + index] + mesh->parts[part].baseVertex + 1);
  } else {
    lua_pushinteger(L, ((uint16_t*) model->indices)[mesh->indexOffset + index] + mesh->parts[part].baseVertex + 1);
  }

  return 1;
}

int l_lovrModelMetaGetMeshPartCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  uint32_t mesh = luax_checkmeshindex(L, 2, meta);
  lua_pushinteger(L, meta->meshes[mesh].partCount);
  return 1;
}

int l_lovrModelMetaGetMeshDrawMode(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelPart* part = luax_checkmeshpart(L, 2, meta);
  luax_pushenum(L, ModelDrawMode, part->mode);
  return 1;
}

int l_lovrModelMetaGetMeshDrawRange(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L,  1);
  ModelPart* part = luax_checkmeshpart(L, 2, meta);
  lua_pushinteger(L, part->start + 1);
  lua_pushinteger(L, part->count);
  return 2;
}

int l_lovrModelMetaGetMeshMaterial(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelPart* part = luax_checkmeshpart(L, 2, meta);
  if (part->material == ~0u) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, part->material + 1);
  }
  return 1;
}

static void luax_checkboundingbox(lua_State* L, int index, ModelMetadata* meta, float bounds[6]) {
  if (lua_type(L, index) == LUA_TNONE) {
    lovrModelMetadataGetBoundingBox(meta, bounds);
    return;
  }

  uint32_t mesh = luax_checku32(L, index) - 1;
  luax_check(L, mesh < meta->meshCount, "Invalid mesh index '%d'", mesh + 1);

  if (lua_type(L, index + 1) == LUA_TNONE) {
    lovrModelMetadataGetMeshBoundingBox(meta, mesh, bounds);
    return;
  }

  uint32_t part = luax_checku32(L, index + 1) - 1;
  luax_check(L, part < meta->meshes[mesh].partCount, "Invalid part index '%d'", part + 1);
  memcpy(bounds, meta->meshes[mesh].parts[part].bounds, 6 * sizeof(float));
}

int l_lovrModelMetaGetWidth(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  float bounds[6];
  luax_checkboundingbox(L, 2, meta, bounds);
  lua_pushnumber(L, bounds[1] - bounds[0]);
  return 1;
}

int l_lovrModelMetaGetHeight(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  float bounds[6];
  luax_checkboundingbox(L, 2, meta, bounds);
  lua_pushnumber(L, bounds[3] - bounds[2]);
  return 1;
}

int l_lovrModelMetaGetDepth(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  float bounds[6];
  luax_checkboundingbox(L, 2, meta, bounds);
  lua_pushnumber(L, bounds[5] - bounds[4]);
  return 1;
}

int l_lovrModelMetaGetDimensions(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  float bounds[6];
  luax_checkboundingbox(L, 2, meta, bounds);
  lua_pushnumber(L, bounds[1] - bounds[0]);
  lua_pushnumber(L, bounds[3] - bounds[2]);
  lua_pushnumber(L, bounds[5] - bounds[4]);
  return 3;
}

int l_lovrModelMetaGetCenter(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  float bounds[6];
  luax_checkboundingbox(L, 2, meta, bounds);
  lua_pushnumber(L, (bounds[0] + bounds[1]) / 2.f);
  lua_pushnumber(L, (bounds[2] + bounds[3]) / 2.f);
  lua_pushnumber(L, (bounds[4] + bounds[5]) / 2.f);
  return 3;
}

int l_lovrModelMetaGetBoundingBox(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  float bounds[6];
  luax_checkboundingbox(L, 2, meta, bounds);
  lua_pushnumber(L, bounds[0]);
  lua_pushnumber(L, bounds[1]);
  lua_pushnumber(L, bounds[2]);
  lua_pushnumber(L, bounds[3]);
  lua_pushnumber(L, bounds[4]);
  lua_pushnumber(L, bounds[5]);
  return 6;
}

int l_lovrModelMetaGetImageCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  lua_pushinteger(L, meta->imageCount);
  return 1;
}

static int l_lovrModelDataGetImage(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  luax_check(L, index < model->meta.imageCount, "Invalid image index '%d'", index + 1);
  luax_pushtype(L, Image, model->images[index]);
  return 1;
}

int l_lovrModelMetaGetMaterialCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  lua_pushinteger(L, meta->materialCount);
  return 1;
}

int l_lovrModelMetaGetMaterialName(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  uint32_t index = luax_checku32(L, 2) - 1;
  luax_check(L, index < meta->materialCount, "Invalid material index '%d'", index + 1);
  lua_pushstring(L, meta->materials[index].name);
  return 1;
}

static int l_lovrModelDataGetMaterial(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelMaterial* material = &model->meta.materials[luax_checkmaterialindex(L, 2, &model->meta)];

  lua_newtable(L);

  lua_createtable(L, 4, 0);
  lua_pushnumber(L, material->color[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, material->color[1]);
  lua_rawseti(L, -2, 2);
  lua_pushnumber(L, material->color[2]);
  lua_rawseti(L, -2, 3);
  lua_pushnumber(L, material->color[3]);
  lua_rawseti(L, -2, 4);
  lua_setfield(L, -2, "color");

  lua_createtable(L, 4, 0);
  lua_pushnumber(L, material->glow[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, material->glow[1]);
  lua_rawseti(L, -2, 2);
  lua_pushnumber(L, material->glow[2]);
  lua_rawseti(L, -2, 3);
  lua_pushnumber(L, material->glow[3]);
  lua_rawseti(L, -2, 4);
  lua_setfield(L, -2, "glow");

  lua_createtable(L, 2, 0);
  lua_pushnumber(L, material->uvShift[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, material->uvShift[1]);
  lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "uvShift");

  lua_createtable(L, 2, 0);
  lua_pushnumber(L, material->uvScale[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, material->uvScale[1]);
  lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "uvScale");

  lua_pushnumber(L, material->metalness), lua_setfield(L, -2, "metalness");
  lua_pushnumber(L, material->roughness), lua_setfield(L, -2, "roughness");
  lua_pushnumber(L, material->clearcoat), lua_setfield(L, -2, "clearcoat");
  lua_pushnumber(L, material->clearcoatRoughness), lua_setfield(L, -2, "clearcoatRoughness");
  lua_pushnumber(L, material->occlusionStrength), lua_setfield(L, -2, "occlusionStrength");
  lua_pushnumber(L, material->normalScale), lua_setfield(L, -2, "normalScale");
  lua_pushnumber(L, material->alphaCutoff), lua_setfield(L, -2, "alphaCutoff");

#define PUSH_IMAGE(t) if (material->t != ~0u) luax_pushtype(L, Image, model->images[material->t]), lua_setfield(L, -2, #t)
  PUSH_IMAGE(texture);
  PUSH_IMAGE(glowTexture);
  PUSH_IMAGE(metalnessTexture);
  PUSH_IMAGE(roughnessTexture);
  PUSH_IMAGE(clearcoatTexture);
  PUSH_IMAGE(occlusionTexture);
  PUSH_IMAGE(normalTexture);

  return 1;
}

int l_lovrModelMetaGetAnimationCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  lua_pushinteger(L, meta->animationCount);
  return 1;
}

int l_lovrModelMetaGetAnimationName(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  uint32_t index = luax_checku32(L, 2) - 1;
  luax_check(L, index < meta->animationCount, "Invalid animation index '%d'", index + 1);
  lua_pushstring(L, meta->animations[index].name);
  return 1;
}

int l_lovrModelMetaGetAnimationDuration(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelAnimation* animation = &meta->animations[luax_checkanimationindex(L, 2, meta)];
  lua_pushnumber(L, animation->duration);
  return 1;
}

int l_lovrModelMetaGetAnimationChannelCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelAnimation* animation = &meta->animations[luax_checkanimationindex(L, 2, meta)];
  lua_pushinteger(L, animation->channelCount);
  return 1;
}

int l_lovrModelMetaGetAnimationNode(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelAnimation* animation = &meta->animations[luax_checkanimationindex(L, 2, meta)];
  uint32_t index = luax_checku32(L, 3) - 1;
  luax_check(L, index < animation->channelCount, "Invalid channel index '%d'", index + 1);
  ModelAnimationChannel* channel = &animation->channels[index];
  lua_pushinteger(L, channel->nodeIndex + 1);
  return 1;
}

int l_lovrModelMetaGetAnimationProperty(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelAnimation* animation = &meta->animations[luax_checkanimationindex(L, 2, meta)];
  uint32_t index = luax_checku32(L, 3) - 1;
  luax_check(L, index < animation->channelCount, "Invalid channel index '%d'", index + 1);
  ModelAnimationChannel* channel = &animation->channels[index];
  luax_pushenum(L, AnimationProperty, channel->property);
  return 1;
}

int l_lovrModelMetaGetAnimationSmoothMode(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelAnimation* animation = &meta->animations[luax_checkanimationindex(L, 2, meta)];
  uint32_t index = luax_checku32(L, 3) - 1;
  luax_check(L, index < animation->channelCount, "Invalid channel index '%d'", index + 1);
  ModelAnimationChannel* channel = &animation->channels[index];
  luax_pushenum(L, SmoothMode, channel->smoothing);
  return 1;
}

int l_lovrModelMetaGetAnimationKeyframeCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  ModelAnimation* animation = &meta->animations[luax_checkanimationindex(L, 2, meta)];
  uint32_t index = luax_checku32(L, 3) - 1;
  luax_check(L, index < animation->channelCount, "Invalid channel index '%d'", index + 1);
  ModelAnimationChannel* channel = &animation->channels[index];
  lua_pushinteger(L, channel->keyframeCount);
  return 1;
}

static int l_lovrModelDataGetAnimationKeyframe(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelAnimation* animation = &model->meta.animations[luax_checkanimationindex(L, 2, &model->meta)];
  uint32_t index = luax_checku32(L, 3) - 1;
  luax_check(L, index < animation->channelCount, "Invalid channel index '%d'", index + 1);
  ModelAnimationChannel* channel = &animation->channels[index];
  uint32_t keyframe = luax_checku32(L, 4) - 1;
  luax_check(L, keyframe < channel->keyframeCount, "Invalid keyframe index '%d'", keyframe + 1);
  lua_pushnumber(L, channel->times[keyframe]);
  int count;
  switch (channel->property) {
    case PROP_TRANSLATION: count = 3; break;
    case PROP_ROTATION: count = 4; break;
    case PROP_SCALE: count = 3; break;
    case PROP_WEIGHTS: count = model->meta.meshes[model->meta.nodes[channel->nodeIndex].mesh].blendShapeCount; break;
  }
  for (int i = 0; i < count; i++) {
    lua_pushnumber(L, channel->data[keyframe * count + i]);
  }
  return count + 1;
}

int l_lovrModelMetaGetSkinCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  lua_pushinteger(L, meta->skinCount);
  return 1;
}

int l_lovrModelMetaGetSkinJoints(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  uint32_t index = luax_checku32(L, 2) - 1;
  luax_check(L, index < meta->skinCount, "Invalid skin index '%d'", index + 1);
  ModelSkin* skin = &meta->skins[index];
  lua_createtable(L, skin->jointCount, 0);
  for (uint32_t i = 0; i < skin->jointCount; i++) {
    lua_pushinteger(L, skin->joints[i] + 1);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

int l_lovrModelMetaGetSkinInverseBindMatrix(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  uint32_t index = luax_checku32(L, 2) - 1;
  luax_check(L, index < meta->skinCount, "Invalid skin index '%d'", index + 1);
  ModelSkin* skin = &meta->skins[index];
  uint32_t joint = luax_checku32(L, 3) - 1;
  luax_check(L, joint < skin->jointCount, "Invalid joint index '%d'", joint + 1);
  if (!skin->inverseBindMatrices) return lua_pushnil(L), 1;
  float* m = skin->inverseBindMatrices + joint * 16;
  for (uint32_t i = 0; i < 16; i++) {
    lua_pushnumber(L, m[i]);
  }
  return 16;
}

int l_lovrModelMetaGetBlendShapeCount(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  lua_pushinteger(L, meta->blendShapeCount);
  return 1;
}

int l_lovrModelMetaGetBlendShapeName(lua_State* L) {
  ModelMetadata* meta = luax_checkmodelmeta(L, 1);
  uint32_t index = luax_checku32(L, 2) - 1;
  luax_check(L, index < meta->blendShapeCount, "Invalid blend shape index '%d'", index + 1);
  lua_pushstring(L, meta->blendShapes[index].name);
  return 1;
}

const luaL_Reg lovrModelData[] = {
  { "getMetadata", l_lovrModelMetaGetMetadata },

  { "getRootNode", l_lovrModelMetaGetRootNode },
  { "getNodeCount", l_lovrModelMetaGetNodeCount },
  { "getNodeName", l_lovrModelMetaGetNodeName },
  { "getNodeChild", l_lovrModelMetaGetNodeChild },
  { "getNodeChildren", l_lovrModelMetaGetNodeChildren }, // Deprecated
  { "getNodeSibling", l_lovrModelMetaGetNodeSibling },
  { "getNodeParent", l_lovrModelMetaGetNodeParent },
  { "getNodePosition", l_lovrModelMetaGetNodePosition },
  { "getNodeOrientation", l_lovrModelMetaGetNodeOrientation },
  { "getNodeScale", l_lovrModelMetaGetNodeScale },
  { "getNodePose", l_lovrModelMetaGetNodePose },
  { "getNodeTransform", l_lovrModelMetaGetNodeTransform },
  { "getNodeMesh", l_lovrModelMetaGetNodeMesh },
  { "getNodeSkin", l_lovrModelMetaGetNodeSkin },

  { "getMeshCount", l_lovrModelMetaGetMeshCount },
  { "getMeshBlendShapeCount", l_lovrModelMetaGetMeshBlendShapeCount },
  { "getMeshBlendShapeName", l_lovrModelMetaGetMeshBlendShapeName },
  { "getMeshBlendVertex", l_lovrModelDataGetMeshBlendVertex },
  { "getMeshVertexCount", l_lovrModelMetaGetMeshVertexCount },
  { "getMeshIndexCount", l_lovrModelMetaGetMeshIndexCount },
  { "getMeshVertex", l_lovrModelDataGetMeshVertex },
  { "getMeshIndex", l_lovrModelDataGetMeshIndex },
  { "getMeshPartCount", l_lovrModelMetaGetMeshPartCount },
  { "getMeshDrawMode", l_lovrModelMetaGetMeshDrawMode },
  { "getMeshDrawRange", l_lovrModelMetaGetMeshDrawRange },
  { "getMeshMaterial", l_lovrModelMetaGetMeshMaterial },

  { "getWidth", l_lovrModelMetaGetWidth },
  { "getHeight", l_lovrModelMetaGetHeight },
  { "getDepth", l_lovrModelMetaGetDepth },
  { "getDimensions", l_lovrModelMetaGetDimensions },
  { "getCenter", l_lovrModelMetaGetCenter },
  { "getBoundingBox", l_lovrModelMetaGetBoundingBox },

  { "getImageCount", l_lovrModelMetaGetImageCount },
  { "getImage", l_lovrModelDataGetImage },
  { "getMaterialCount", l_lovrModelMetaGetMaterialCount },
  { "getMaterialName", l_lovrModelMetaGetMaterialName },
  { "getMaterial", l_lovrModelDataGetMaterial },

  { "getAnimationCount", l_lovrModelMetaGetAnimationCount },
  { "getAnimationName", l_lovrModelMetaGetAnimationName },
  { "getAnimationDuration", l_lovrModelMetaGetAnimationDuration },
  { "getAnimationChannelCount", l_lovrModelMetaGetAnimationChannelCount },
  { "getAnimationNode", l_lovrModelMetaGetAnimationNode },
  { "getAnimationProperty", l_lovrModelMetaGetAnimationProperty },
  { "getAnimationSmoothMode", l_lovrModelMetaGetAnimationSmoothMode },
  { "getAnimationKeyframeCount", l_lovrModelMetaGetAnimationKeyframeCount },
  { "getAnimationKeyframe", l_lovrModelDataGetAnimationKeyframe },
  { "getSkinCount", l_lovrModelMetaGetSkinCount },
  { "getSkinJoints", l_lovrModelMetaGetSkinJoints },
  { "getSkinInverseBindMatrix", l_lovrModelMetaGetSkinInverseBindMatrix },

  { "getBlendShapeCount", l_lovrModelMetaGetBlendShapeCount },
  { "getBlendShapeName", l_lovrModelMetaGetBlendShapeName },

  { NULL, NULL }
};
