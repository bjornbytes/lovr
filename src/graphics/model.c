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
  lovrGraphicsMatrixTransform(MATRIX_MODEL, model->globalNodeTransforms + 16 * nodeIndex);

  float globalInverse[16];
  mat4_set(globalInverse, model->globalNodeTransforms + nodeIndex * 16);
  mat4_invert(globalInverse);

  for (int i = 0; i < model->modelData->bones.length; i++) {
    Bone* bone = &model->modelData->bones.data[i];

    int nodeIndex = -1;
    for (int j = 0; j < model->modelData->nodeCount; j++) {
      if (!strcmp(model->modelData->nodes[j].name, bone->name)) {
        nodeIndex = j;
        break;
      }
    }

    mat4 bonePose = model->pose + 16 * i;
    mat4_identity(bonePose);
    mat4_set(bonePose, globalInverse);
    mat4_multiply(bonePose, &model->globalNodeTransforms[16 * nodeIndex]);
    mat4_multiply(bonePose, bone->offset);
  }

  Shader* shader = lovrGraphicsGetActiveShader();
  if (shader) {
    lovrShaderSetMatrix(shader, "lovrPose", model->pose, MAX_BONES * 16);
  }

  for (int i = 0; i < node->primitives.length; i++) {
    ModelPrimitive* primitive = &model->modelData->primitives[node->primitives.data[i]];
    if (useMaterials) {
      lovrGraphicsSetMaterial(model->materials[primitive->material]);
    }
    lovrMeshSetDrawRange(model->mesh, primitive->drawStart, primitive->drawCount);
    lovrMeshDraw(model->mesh, NULL);
  }

  lovrGraphicsPop();

  for (int i = 0; i < node->children.length; i++) {
    renderNode(model, node->children.data[i]);
  }

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

  model->localNodeTransforms = malloc(16 * modelData->nodeCount * sizeof(float));
  for (int i = 0; i < modelData->nodeCount; i++) {
    mat4 transform = model->localNodeTransforms + 16 * i;
    mat4_identity(transform);
  }

  model->globalNodeTransforms = malloc(16 * modelData->nodeCount * sizeof(float));
  for (int i = 0; i < modelData->nodeCount; i++) {
    mat4 transform = &model->globalNodeTransforms[16 * i];
    mat4_identity(transform);
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

    // Compute local bone transform info (should be done once in animator prolly)
    for (int i = 0; i < model->modelData->nodeCount; i++) {
      ModelNode* node = &model->modelData->nodes[i];
      mat4 transform = &model->localNodeTransforms[16 * i];
      mat4_identity(transform);
      if (!lovrAnimatorEvaluate(model->animator, node->name, transform)) {
        mat4_set(transform, node->transform);
      }
    }

    // Compute global transforms
    for (int i = 0; i < model->modelData->nodeCount; i++) {
      ModelNode* node = &model->modelData->nodes[i];
      mat4 transform = &model->globalNodeTransforms[16 * i];
      if (node->parent >= 0) {
        mat4_set(transform, model->globalNodeTransforms + 16 * node->parent);
        mat4_multiply(transform, model->localNodeTransforms + 16 * i);
      } else {
        mat4_set(transform, model->localNodeTransforms + 16 * i);
      }
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

int lovrModelGetAnimationCount(Model* model) {
  return model->modelData->animationCount;
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
