#include "graphics/model.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include <stdlib.h>

static void poseNode(Model* model, int nodeIndex, mat4 transform) {
  ModelNode* node = &model->modelData->nodes[nodeIndex];

  float globalTransform[16];
  mat4_set(globalTransform, transform);

  float localTransform[16];
  mat4_identity(localTransform);
  lovrAnimatorEvaluate(model->animator, node->name, localTransform);

  mat4_set(model->nodeTransforms[nodeIndex], localTransform);

  int* boneIndex = map_get(&model->modelData->boneMap, node->name);
  if (!boneIndex) {
    mat4_multiply(globalTransform, node->transform);
    mat4_multiply(globalTransform, localTransform);
  } else {
    Bone* bone = &model->modelData->bones.data[*boneIndex];
    mat4 finalTransform = model->pose + (*boneIndex * 16);

    mat4_multiply(globalTransform, localTransform);

    mat4_identity(finalTransform);
    mat4_multiply(finalTransform, model->modelData->inverseRootTransform);
    mat4_multiply(finalTransform, globalTransform);
    mat4_multiply(finalTransform, bone->offset);
  }

  for (int i = 0; i < node->children.length; i++) {
    poseNode(model, node->children.data[i], globalTransform);
  }
}

static void renderNode(Model* model, int nodeIndex) {
  ModelNode* node = &model->modelData->nodes[nodeIndex];
  Material* currentMaterial = lovrGraphicsGetMaterial();
  bool useMaterials = currentMaterial->isDefault;

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(MATRIX_MODEL, node->transform);
  lovrGraphicsMatrixTransform(MATRIX_MODEL, model->nodeTransforms[nodeIndex]);

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

  if (modelData->hasBones) {
    MeshAttribute bones = { .name = "lovrBones", .type = MESH_INT, .count = 4 };
    vec_push(&format, bones);

    MeshAttribute weights = { .name = "lovrBoneWeights", .type = MESH_FLOAT, .count = 4 };
    vec_push(&format, weights);
  }

  model->mesh = lovrMeshCreate(modelData->vertexCount, &format, MESH_TRIANGLES, MESH_STATIC);
  void* data = lovrMeshMap(model->mesh, 0, modelData->vertexCount, false, true);
  memcpy(data, modelData->vertices.data, modelData->vertexCount * modelData->stride);
  lovrMeshUnmap(model->mesh);
  lovrMeshSetVertexMap(model->mesh, modelData->indices.data, modelData->indexCount);
  lovrMeshSetRangeEnabled(model->mesh, true);

  model->materials = malloc(modelData->materialCount * sizeof(Material*));
  for (int i = 0; i < modelData->materialCount; i++) {
    model->materials[i] = lovrMaterialCreate(modelData->materials[i], false);
  }

  for (int i = 0; i < MAX_BONES; i++) {
    mat4_identity(model->pose + (16 * i));
  }

  for (int i = 0; i < 640; i++) {
    mat4_identity(model->nodeTransforms[i]);
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

  if (model->animator) {
    float transform[16];
    mat4_identity(transform);
    poseNode(model, 0, transform);
    Shader* shader = lovrGraphicsGetActiveShader();
    if (shader) {
      lovrShaderSetMatrix(shader, "lovrBoneTransforms", model->pose, MAX_BONES * 16);
    }
  }

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(MATRIX_MODEL, transform);
  renderNode(model, 0);
  lovrGraphicsPop();
}

Animator* lovrModelGetAnimator(Model* model) {
  return model->animator;
}

void lovrModelSetAnimator(Model* model, Animator* animator) {
  model->animator = animator;
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
