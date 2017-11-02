#include "graphics/model.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include <stdlib.h>

static void renderNode(Model* model, int nodeIndex) {
  ModelNode* node = &model->modelData->nodes[nodeIndex];
  Material* currentMaterial = lovrGraphicsGetMaterial();
  bool useMaterials = currentMaterial->isDefault;

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(MATRIX_MODEL, node->transform);

  for (int i = 0; i < node->primitives.length; i++) {
    ModelPrimitive* primitive = &model->modelData->primitives[node->primitives.data[i]];
    if (useMaterials) {
      lovrGraphicsSetMaterial(model->materials[primitive->material]);
    }
    lovrMeshSetDrawRange(model->mesh, primitive->drawStart, primitive->drawCount);
    lovrMeshDraw(model->mesh, NULL);
  }

  for (int i = 0; i < node->children.length; i++) {
    renderNode(model, node->children.data[i]);
  }

  lovrGraphicsPop();
  if (useMaterials) {
    lovrGraphicsSetMaterial(currentMaterial);
  }
}

Model* lovrModelCreate(ModelData* modelData) {
  Model* model = lovrAlloc(sizeof(Model), lovrModelDestroy);
  if (!model) return NULL;

  model->modelData = modelData;
  model->aabbDirty = true;

  MeshFormat format;
  vec_init(&format);

  MeshAttribute attribute = { .name = "lovrPosition", .type = MESH_FLOAT, .count = 3 };
  vec_push(&format, attribute);

  if (modelData->hasNormals) {
    MeshAttribute attribute = { .name = "lovrNormal", .type = MESH_FLOAT, .count = 3 };
    vec_push(&format, attribute);
  }

  if (modelData->hasUVs) {
    MeshAttribute attribute = { .name = "lovrTexCoord", .type = MESH_FLOAT, .count = 2 };
    vec_push(&format, attribute);
  }

  if (modelData->hasVertexColors) {
    MeshAttribute attribute = { .name = "lovrVertexColor", .type = MESH_BYTE, .count = 4 };
    vec_push(&format, attribute);
  }

  model->mesh = lovrMeshCreate(modelData->vertexCount, &format, MESH_TRIANGLES, MESH_STATIC);
  void* data = lovrMeshMap(model->mesh, 0, modelData->vertexCount, false, true);
  memcpy(data, modelData->vertices.data, modelData->vertexCount * modelData->stride);
  lovrMeshUnmap(model->mesh);
  lovrMeshSetVertexMap(model->mesh, modelData->indices.data, modelData->indexCount);
  lovrMeshSetRangeEnabled(model->mesh, true);

  model->materials = malloc(modelData->materialCount * sizeof(Material*));
  for (int i = 0; i < modelData->materialCount; i++) {
    model->materials[i] = lovrMaterialCreate(&modelData->materials[i], false);
  }

  vec_deinit(&format);
  return model;
}

void lovrModelDestroy(const Ref* ref) {
  Model* model = containerof(ref, Model);
  for (int i = 0; i < model->modelData->materialCount; i++) {
    lovrRelease(&model->materials[i]->ref);
  }
  free(model->materials);
  lovrModelDataDestroy(model->modelData);
  lovrRelease(&model->mesh->ref);
  free(model);
}

void lovrModelDraw(Model* model, mat4 transform) {
  if (model->modelData->nodeCount == 0) {
    return;
  }

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(MATRIX_MODEL, transform);
  renderNode(model, 0);
  lovrGraphicsPop();
}

Mesh* lovrModelGetMesh(Model* model) {
  return model->mesh;
}

const float* lovrModelGetAABB(Model* model) {
  if (model->aabbDirty) {
    lovrModelDataGetAABB(model->modelData, model->aabb);
    model->aabbDirty = false;
  }

  return model->aabb;
}
