#include "api.h"
#include "data/modelData.h"
#include "core/maf.h"
#include "util.h"

static ModelNode* luax_checknode(lua_State* L, int index, ModelData* model) {
  switch (lua_type(L, index)) {
    case LUA_TNUMBER: {
      uint32_t node = luax_checku32(L, index) - 1;
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

static ModelMaterial* luax_checkmaterial(lua_State* L, int index, ModelData* model) {
  switch (lua_type(L, index)) {
    case LUA_TNUMBER: {
      uint32_t material = luax_checku32(L, index) - 1;
      lovrCheck(material < model->materialCount, "Invalid material index '%d'", material + 1);
      return &model->materials[material];
    }
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, index, &length);
      uint64_t hash = hash64(name, length);
      uint64_t entry = map_get(&model->materialMap, hash);
      lovrCheck(entry != MAP_NIL, "ModelData has no material named '%s'", name);
      return &model->materials[(uint32_t) entry];
    }
    default: return luax_typeerror(L, index, "number or string"), NULL;
  }
}

static ModelAnimation* luax_checkanimation(lua_State* L, int index, ModelData* model) {
  switch (lua_type(L, index)) {
    case LUA_TNUMBER: {
      uint32_t animation = luax_checku32(L, index) - 1;
      lovrCheck(animation < model->animationCount, "Invalid animation index '%d'", animation + 1);
      return &model->animations[animation];
    }
    case LUA_TSTRING: {
      size_t length;
      const char* name = lua_tolstring(L, index, &length);
      uint64_t hash = hash64(name, length);
      uint64_t entry = map_get(&model->animationMap, hash);
      lovrCheck(entry != MAP_NIL, "ModelData has no animation named '%s'", name);
      return &model->animations[(uint32_t) entry];
    }
    default: return luax_typeerror(L, index, "number or string"), NULL;
  }
}

static int l_lovrModelDataGetMetadata(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);

  if (!model->metadata || model->metadataSize == 0) {
    lua_pushnil(L);
  } else {
    lua_pushlstring(L, model->metadata, model->metadataSize);
  }

  return 1;
}

static int l_lovrModelDataGetBlobCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->blobCount);
  return 1;
}

static int l_lovrModelDataGetBlob(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
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
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->imageCount, "Invalid image index '%d'", index + 1);
  luax_pushtype(L, Image, model->images[index]);
  return 1;
}

static int l_lovrModelDataGetRootNode(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->rootNode + 1);
  return 1;
}

static int l_lovrModelDataGetNodeCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->nodeCount);
  return 1;
}

static int l_lovrModelDataGetNodeName(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->nodeCount, "Invalid node index '%d'", index + 1);
  lua_pushstring(L, model->nodes[index].name);
  return 1;
}

static int l_lovrModelDataGetNodeParent(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
  if (node->parent == ~0u) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, node->parent + 1);
  }
  return 1;
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

static int l_lovrModelDataGetNodePosition(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
  if (node->hasMatrix) {
    float position[4];
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

static int l_lovrModelDataGetNodeOrientation(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
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

static int l_lovrModelDataGetNodeScale(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
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

static int l_lovrModelDataGetNodePose(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
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

static int l_lovrModelDataGetNodeTransform(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelNode* node = luax_checknode(L, 2, model);
  if (node->hasMatrix) {
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

static int l_lovrModelDataGetMeshCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->primitiveCount);
  return 1;
}

static int l_lovrModelDataGetMeshDrawMode(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->primitiveCount, "Invalid mesh index '%d'", index + 1);
  ModelPrimitive* mesh = &model->primitives[index];
  luax_pushenum(L, DrawMode, mesh->mode);
  return 1;
}

static int l_lovrModelDataGetMeshMaterial(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->primitiveCount, "Invalid mesh index '%d'", index + 1);
  ModelPrimitive* mesh = &model->primitives[index];
  if (mesh->material == ~0u) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, mesh->material + 1);
  }
  return 1;
}

static int l_lovrModelDataGetMeshVertexCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->primitiveCount, "Invalid mesh index '%d'", index + 1);
  ModelPrimitive* mesh = &model->primitives[index];
  lua_pushinteger(L, mesh->attributes[ATTR_POSITION] ? mesh->attributes[ATTR_POSITION]->count : 0);
  return 1;
}

static int l_lovrModelDataGetMeshIndexCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->primitiveCount, "Invalid mesh index '%d'", index + 1);
  ModelPrimitive* mesh = &model->primitives[index];
  lua_pushinteger(L, mesh->indices ? mesh->indices->count : 0);
  return 1;
}

static int l_lovrModelDataGetMeshVertexFormat(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->primitiveCount, "Invalid mesh index '%d'", index + 1);
  ModelPrimitive* mesh = &model->primitives[index];
  lua_newtable(L);
  uint32_t count = 0;
  for (uint32_t i = 0; i < MAX_DEFAULT_ATTRIBUTES; i++) {
    ModelAttribute* attribute = mesh->attributes[i];
    if (!attribute) continue;

    lua_createtable(L, 6, 0);

    luax_pushenum(L, DefaultAttribute, i);
    lua_rawseti(L, -2, 1);

    luax_pushenum(L, AttributeType, attribute->type);
    lua_rawseti(L, -2, 2);

    lua_pushinteger(L, attribute->components);
    lua_rawseti(L, -2, 3);

    lua_pushinteger(L, model->buffers[attribute->buffer].blob + 1);
    lua_rawseti(L, -2, 4);

    lua_pushinteger(L, model->buffers[attribute->buffer].offset + attribute->offset);
    lua_rawseti(L, -2, 5);

    lua_pushinteger(L, model->buffers[attribute->buffer].stride);
    lua_rawseti(L, -2, 6);

    lua_rawseti(L, -2, ++count);
  }
  return 1;
}

static int l_lovrModelDataGetMeshIndexFormat(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->primitiveCount, "Invalid mesh index '%d'", index + 1);
  ModelPrimitive* mesh = &model->primitives[index];
  if (!mesh->indices) return lua_pushnil(L), 1;
  luax_pushenum(L, AttributeType, mesh->indices->type);
  lua_pushinteger(L, model->buffers[mesh->indices->buffer].blob + 1);
  lua_pushinteger(L, model->buffers[mesh->indices->buffer].offset + mesh->indices->offset);
  lua_pushinteger(L, model->buffers[mesh->indices->buffer].stride);
  return 4;
}

static int l_lovrModelDataGetMeshVertex(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->primitiveCount, "Invalid mesh index '%d'", index + 1);
  ModelPrimitive* mesh = &model->primitives[index];
  uint32_t vertex = luax_checku32(L, 3) - 1;
  uint32_t vertexCount = mesh->attributes[ATTR_POSITION] ? mesh->attributes[ATTR_POSITION]->count : 0;
  lovrCheck(vertex < vertexCount, "Invalid vertex index '%d'", vertex + 1);
  uint32_t total = 0;
  for (uint32_t i = 0; i < MAX_DEFAULT_ATTRIBUTES; i++) {
    ModelAttribute* attribute = mesh->attributes[i];
    if (!attribute) continue;

    size_t stride = model->buffers[attribute->buffer].stride;

    if (!stride) {
      switch (attribute->type) {
        case I8: case U8: stride = attribute->components * 1; break;
        case I16: case U16: stride = attribute->components * 2; break;
        case I32: case U32: case F32: stride = attribute->components * 4; break;
        default: break;
      }
    }

    AttributeData data = { .raw = model->buffers[attribute->buffer].data };
    data.u8 += attribute->offset;
    data.u8 += vertex * stride;

    for (uint32_t j = 0; j < attribute->components; j++) {
      switch (attribute->type) {
        case I8: lua_pushinteger(L, data.i8[j]); break;
        case U8: lua_pushinteger(L, data.u8[j]); break;
        case I16: lua_pushinteger(L, data.i16[j]); break;
        case U16: lua_pushinteger(L, data.u16[j]); break;
        case I32: lua_pushinteger(L, data.i32[j]); break;
        case U32: lua_pushinteger(L, data.u32[j]); break;
        case F32: lua_pushnumber(L, data.f32[j]); break;
        default: break;
      }
    }

    total += attribute->components;
  }
  return total;
}

static int l_lovrModelDataGetMeshIndex(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t meshIndex = luax_checku32(L, 2) - 1;
  lovrCheck(meshIndex < model->primitiveCount, "Invalid mesh index '%d'", meshIndex + 1);
  ModelPrimitive* mesh = &model->primitives[meshIndex];
  uint32_t index = luax_checku32(L, 3) - 1;
  uint32_t indexCount = mesh->indices ? mesh->indices->count : 0;
  lovrCheck(index < indexCount, "Invalid index index '%d'", index + 1);
  AttributeData data = { .raw = model->buffers[mesh->indices->buffer].data };
  data.u8 += mesh->indices->offset;
  switch (mesh->indices->type) {
    case U16: lua_pushinteger(L, data.u16[index] + 1); return 1;
    case U32: lua_pushinteger(L, data.u32[index] + 1); return 1;
    default: lovrUnreachable();
  }
}

static int l_lovrModelDataGetTriangles(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);

  float* vertices = NULL;
  uint32_t* indices = NULL;
  uint32_t vertexCount = 0;
  uint32_t indexCount = 0;
  lovrModelDataGetTriangles(model, &vertices, &indices, &vertexCount, &indexCount);

  lua_createtable(L, vertexCount * 3, 0);
  for (uint32_t i = 0; i < vertexCount * 3; i++) {
    lua_pushnumber(L, vertices[i]);
    lua_rawseti(L, -2, i + 1);
  }

  lua_createtable(L, indexCount, 0);
  for (uint32_t i = 0; i < indexCount; i++) {
    lua_pushinteger(L, indices[i] + 1);
    lua_rawseti(L, -2, i + 1);
  }

  return 2;
}

static int l_lovrModelDataGetTriangleCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t vertexCount, indexCount;
  lovrModelDataGetTriangles(model, NULL, NULL, &vertexCount, &indexCount);
  lua_pushinteger(L, indexCount / 3);
  return 1;
}

static int l_lovrModelDataGetVertexCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t vertexCount, indexCount;
  lovrModelDataGetTriangles(model, NULL, NULL, &vertexCount, &indexCount);
  lua_pushinteger(L, vertexCount);
  return 1;
}

static int l_lovrModelDataGetWidth(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  float bounds[6];
  lovrModelDataGetBoundingBox(model, bounds);
  lua_pushnumber(L, bounds[1] - bounds[0]);
  return 1;
}

static int l_lovrModelDataGetHeight(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  float bounds[6];
  lovrModelDataGetBoundingBox(model, bounds);
  lua_pushnumber(L, bounds[3] - bounds[2]);
  return 1;
}

static int l_lovrModelDataGetDepth(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  float bounds[6];
  lovrModelDataGetBoundingBox(model, bounds);
  lua_pushnumber(L, bounds[5] - bounds[4]);
  return 1;
}

static int l_lovrModelDataGetDimensions(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  float bounds[6];
  lovrModelDataGetBoundingBox(model, bounds);
  lua_pushnumber(L, bounds[1] - bounds[0]);
  lua_pushnumber(L, bounds[3] - bounds[2]);
  lua_pushnumber(L, bounds[5] - bounds[4]);
  return 3;
}

static int l_lovrModelDataGetCenter(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  float bounds[6];
  lovrModelDataGetBoundingBox(model, bounds);
  lua_pushnumber(L, (bounds[0] + bounds[1]) / 2.f);
  lua_pushnumber(L, (bounds[2] + bounds[3]) / 2.f);
  lua_pushnumber(L, (bounds[4] + bounds[5]) / 2.f);
  return 3;
}

static int l_lovrModelDataGetBoundingBox(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  float bounds[6];
  lovrModelDataGetBoundingBox(model, bounds);
  lua_pushnumber(L, bounds[0]);
  lua_pushnumber(L, bounds[1]);
  lua_pushnumber(L, bounds[2]);
  lua_pushnumber(L, bounds[3]);
  lua_pushnumber(L, bounds[4]);
  lua_pushnumber(L, bounds[5]);
  return 6;
}

static int l_lovrModelDataGetBoundingSphere(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  float sphere[4];
  lovrModelDataGetBoundingSphere(model, sphere);
  lua_pushnumber(L, sphere[0]);
  lua_pushnumber(L, sphere[1]);
  lua_pushnumber(L, sphere[2]);
  lua_pushnumber(L, sphere[3]);
  return 4;
}

static int l_lovrModelDataGetMaterialCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->materialCount);
  return 1;
}

static int l_lovrModelDataGetMaterialName(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->nodeCount, "Invalid material index '%d'", index + 1);
  lua_pushstring(L, model->materials[index].name);
  return 1;
}

static int l_lovrModelDataGetMaterial(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelMaterial* material = luax_checkmaterial(L, 2, model);

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
  lua_pushnumber(L, material->uvShift[0]);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, material->uvShift[1]);
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

static int l_lovrModelDataGetAnimationCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->animationCount);
  return 1;
}

static int l_lovrModelDataGetAnimationName(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->animationCount, "Invalid animation index '%d'", index + 1);
  lua_pushstring(L, model->animations[index].name);
  return 1;
}

static int l_lovrModelDataGetAnimationDuration(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelAnimation* animation = luax_checkanimation(L, 2, model);
  lua_pushnumber(L, animation->duration);
  return 1;
}

static int l_lovrModelDataGetAnimationChannelCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelAnimation* animation = luax_checkanimation(L, 2, model);
  lua_pushinteger(L, animation->channelCount);
  return 1;
}

static int l_lovrModelDataGetAnimationNode(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelAnimation* animation = luax_checkanimation(L, 2, model);
  uint32_t index = luax_checku32(L, 3) - 1;
  lovrCheck(index < animation->channelCount, "Invalid channel index '%d'", index + 1);
  ModelAnimationChannel* channel = &animation->channels[index];
  lua_pushinteger(L, channel->nodeIndex);
  return 1;
}

static int l_lovrModelDataGetAnimationProperty(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelAnimation* animation = luax_checkanimation(L, 2, model);
  uint32_t index = luax_checku32(L, 3) - 1;
  lovrCheck(index < animation->channelCount, "Invalid channel index '%d'", index + 1);
  ModelAnimationChannel* channel = &animation->channels[index];
  luax_pushenum(L, AnimationProperty, channel->property);
  return 1;
}

static int l_lovrModelDataGetAnimationSmoothMode(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelAnimation* animation = luax_checkanimation(L, 2, model);
  uint32_t index = luax_checku32(L, 3) - 1;
  lovrCheck(index < animation->channelCount, "Invalid channel index '%d'", index + 1);
  ModelAnimationChannel* channel = &animation->channels[index];
  luax_pushenum(L, SmoothMode, channel->smoothing);
  return 1;
}

static int l_lovrModelDataGetAnimationKeyframeCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelAnimation* animation = luax_checkanimation(L, 2, model);
  uint32_t index = luax_checku32(L, 3) - 1;
  lovrCheck(index < animation->channelCount, "Invalid channel index '%d'", index + 1);
  ModelAnimationChannel* channel = &animation->channels[index];
  lua_pushinteger(L, channel->keyframeCount);
  return 1;
}

static int l_lovrModelDataGetAnimationKeyframe(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  ModelAnimation* animation = luax_checkanimation(L, 2, model);
  uint32_t index = luax_checku32(L, 3) - 1;
  lovrCheck(index < animation->channelCount, "Invalid channel index '%d'", index + 1);
  ModelAnimationChannel* channel = &animation->channels[index];
  uint32_t keyframe = luax_checku32(L, 4) - 1;
  lovrCheck(keyframe < channel->keyframeCount, "Invalid keyframe index '%d'", keyframe + 1);
  lua_pushnumber(L, channel->times[keyframe]);
  int counts[] = { [PROP_TRANSLATION] = 3, [PROP_ROTATION] = 4, [PROP_SCALE] = 3 };
  int count = counts[channel->property];
  for (int i = 0; i < count; i++) {
    lua_pushnumber(L, channel->data[keyframe * count + i]);
  }
  return count + 1;
}

static int l_lovrModelDataGetSkinCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->skinCount);
  return 1;
}

static int l_lovrModelDataGetSkinJoints(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->skinCount, "Invalid skin index '%d'", index + 1);
  ModelSkin* skin = &model->skins[index];
  lua_createtable(L, skin->jointCount, 0);
  for (uint32_t i = 0; i < skin->jointCount; i++) {
    lua_pushinteger(L, skin->joints[i]);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int l_lovrModelDataGetSkinInverseBindMatrix(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luax_checku32(L, 2) - 1;
  lovrCheck(index < model->skinCount, "Invalid skin index '%d'", index + 1);
  ModelSkin* skin = &model->skins[index];
  uint32_t joint = luax_checku32(L, 3) - 1;
  lovrCheck(index < skin->jointCount, "Invalid joint index '%d'", joint + 1);
  float* m = skin->inverseBindMatrices + joint * 16;
  for (uint32_t i = 0; i < 16; i++) {
    lua_pushnumber(L, m[i]);
  }
  return 16;
}

const luaL_Reg lovrModelData[] = {
  { "getMetadata", l_lovrModelDataGetMetadata },
  { "getBlobCount", l_lovrModelDataGetBlobCount },
  { "getBlob", l_lovrModelDataGetBlob },
  { "getImageCount", l_lovrModelDataGetImageCount },
  { "getImage", l_lovrModelDataGetImage },
  { "getRootNode", l_lovrModelDataGetRootNode },
  { "getNodeCount", l_lovrModelDataGetNodeCount },
  { "getNodeName", l_lovrModelDataGetNodeName },
  { "getNodeParent", l_lovrModelDataGetNodeParent },
  { "getNodeChildren", l_lovrModelDataGetNodeChildren },
  { "getNodePosition", l_lovrModelDataGetNodePosition },
  { "getNodeOrientation", l_lovrModelDataGetNodeOrientation },
  { "getNodeScale", l_lovrModelDataGetNodeScale },
  { "getNodePose", l_lovrModelDataGetNodePose },
  { "getNodeTransform", l_lovrModelDataGetNodeTransform },
  { "getNodeMeshes", l_lovrModelDataGetNodeMeshes },
  { "getNodeSkin", l_lovrModelDataGetNodeSkin },
  { "getMeshCount", l_lovrModelDataGetMeshCount },
  { "getMeshDrawMode", l_lovrModelDataGetMeshDrawMode },
  { "getMeshMaterial", l_lovrModelDataGetMeshMaterial },
  { "getMeshVertexCount", l_lovrModelDataGetMeshVertexCount },
  { "getMeshIndexCount", l_lovrModelDataGetMeshIndexCount },
  { "getMeshVertexFormat", l_lovrModelDataGetMeshVertexFormat },
  { "getMeshIndexFormat", l_lovrModelDataGetMeshIndexFormat },
  { "getMeshVertex", l_lovrModelDataGetMeshVertex },
  { "getMeshIndex", l_lovrModelDataGetMeshIndex },
  { "getTriangles", l_lovrModelDataGetTriangles },
  { "getTriangleCount", l_lovrModelDataGetTriangleCount },
  { "getVertexCount", l_lovrModelDataGetVertexCount },
  { "getWidth", l_lovrModelDataGetWidth },
  { "getHeight", l_lovrModelDataGetHeight },
  { "getDepth", l_lovrModelDataGetDepth },
  { "getDimensions", l_lovrModelDataGetDimensions },
  { "getCenter", l_lovrModelDataGetCenter },
  { "getBoundingBox", l_lovrModelDataGetBoundingBox },
  { "getBoundingSphere", l_lovrModelDataGetBoundingSphere },
  { "getMaterialCount", l_lovrModelDataGetMaterialCount },
  { "getMaterialName", l_lovrModelDataGetMaterialName },
  { "getMaterial", l_lovrModelDataGetMaterial },
  { "getAnimationCount", l_lovrModelDataGetAnimationCount },
  { "getAnimationName", l_lovrModelDataGetAnimationName },
  { "getAnimationDuration", l_lovrModelDataGetAnimationDuration },
  { "getAnimationChannelCount", l_lovrModelDataGetAnimationChannelCount },
  { "getAnimationNode", l_lovrModelDataGetAnimationNode },
  { "getAnimationProperty", l_lovrModelDataGetAnimationProperty },
  { "getAnimationSmoothMode", l_lovrModelDataGetAnimationSmoothMode },
  { "getAnimationKeyframeCount", l_lovrModelDataGetAnimationKeyframeCount },
  { "getAnimationKeyframe", l_lovrModelDataGetAnimationKeyframe },
  { "getSkinCount", l_lovrModelDataGetSkinCount },
  { "getSkinJoints", l_lovrModelDataGetSkinJoints },
  { "getSkinInverseBindMatrix", l_lovrModelDataGetSkinInverseBindMatrix },
  { NULL, NULL }
};
