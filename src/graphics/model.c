#include "graphics/model.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include <stdlib.h>

static void renderNode(Model* model, int nodeIndex) {
  ModelNode* node = &model->modelData->nodes[nodeIndex];

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(MATRIX_MODEL, node->transform);

  for (int i = 0; i < node->primitives.length; i++) {
    ModelPrimitive* primitive = &model->modelData->primitives[node->primitives.data[i]];
    lovrMeshSetDrawRange(model->mesh, primitive->drawStart, primitive->drawCount);
    lovrMeshDraw(model->mesh, NULL);
  }

  for (int i = 0; i < node->children.length; i++) {
    renderNode(model, node->children.data[i]);
  }

  lovrGraphicsPop();
}

Model* lovrModelCreate(ModelData* modelData) {
  Model* model = lovrAlloc(sizeof(Model), lovrModelDestroy);
  if (!model) return NULL;

  model->modelData = modelData;

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

  model->mesh = lovrMeshCreate(modelData->vertexCount, &format, MESH_TRIANGLES, MESH_STATIC);
  void* data = lovrMeshMap(model->mesh, 0, modelData->vertexCount, 0, 1);
  memcpy(data, modelData->vertices, modelData->vertexCount * modelData->vertexSize * sizeof(float));
  lovrMeshUnmap(model->mesh);
  lovrMeshSetVertexMap(model->mesh, modelData->indices, modelData->indexCount);
  lovrMeshSetRangeEnabled(model->mesh, 1);

  vec_deinit(&format);
  return model;
}

void lovrModelDestroy(const Ref* ref) {
  Model* model = containerof(ref, Model);
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
