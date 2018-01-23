#include "graphics/model.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include <stdlib.h>

static void renderNode(Model* model, int nodeIndex, int instances) {
  ModelNode* node = &model->modelData->nodes[nodeIndex];

  if (node->primitives.length > 0) {
    lovrGraphicsPush();
    lovrGraphicsMatrixTransform(MATRIX_MODEL, model->nodeTransforms[nodeIndex]);

    float globalInverse[16];
    if (model->animator) {
      mat4_set(globalInverse, model->nodeTransforms[nodeIndex]);
      mat4_invert(globalInverse);
    }

    for (int i = 0; i < node->primitives.length; i++) {
      ModelPrimitive* primitive = &model->modelData->primitives[node->primitives.data[i]];

      if (model->animator) {
        for (int i = 0; i < primitive->boneCount; i++) {
          Bone* bone = &primitive->bones[i];
          int nodeIndex = *(int*) map_get(&model->modelData->nodeMap, bone->name);

          mat4 bonePose = model->pose[i];
          mat4_identity(bonePose);
          mat4_set(bonePose, globalInverse);
          mat4_multiply(bonePose, model->nodeTransforms[nodeIndex]);
          mat4_multiply(bonePose, bone->offset);
        }
      }

      if (!model->material) {
        lovrMeshSetMaterial(model->mesh, model->materials[primitive->material]);
      }

      lovrMeshSetDrawRange(model->mesh, primitive->drawStart, primitive->drawCount);
      lovrMeshDraw(model->mesh, NULL, (float*) model->pose, instances);
    }

    lovrGraphicsPop();
  }

  for (int i = 0; i < node->children.length; i++) {
    renderNode(model, node->children.data[i], instances);
  }
}

Model* lovrModelCreate(ModelData* modelData) {
  Model* model = lovrAlloc(sizeof(Model), lovrModelDestroy);
  if (!model) return NULL;

  lovrRetain(&modelData->ref);
  model->modelData = modelData;
  model->aabbDirty = true;
  model->animator = NULL;
  model->material = NULL;

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

  if (modelData->skinned) {
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
    mat4_identity(model->pose[i]);
  }

  model->nodeTransforms = malloc(16 * modelData->nodeCount * sizeof(float));
  for (int i = 0; i < modelData->nodeCount; i++) {
    ModelNode* node = &model->modelData->nodes[i];
    mat4 transform = model->nodeTransforms[i];

    if (node->parent >= 0) {
      mat4_set(transform, model->nodeTransforms[node->parent]);
      mat4_multiply(transform, node->transform);
    } else {
      mat4_set(transform, node->transform);
    }
  }

  vec_deinit(&format);
  return model;
}

void lovrModelDestroy(const Ref* ref) {
  Model* model = containerof(ref, Model);
  for (int i = 0; i < model->modelData->materialCount; i++) {
    lovrRelease(&model->materials[i]->ref);
  }
  if (model->animator) {
    lovrRelease(&model->animator->ref);
  }
  if (model->material) {
    lovrRelease(&model->material->ref);
  }
  free(model->materials);
  lovrRelease(&model->modelData->ref);
  lovrRelease(&model->mesh->ref);
  free(model->nodeTransforms);
  free(model);
}

void lovrModelDraw(Model* model, mat4 transform, int instances) {
  if (model->modelData->nodeCount == 0) {
    return;
  }

  if (model->animator) {
    for (int i = 0; i < model->modelData->nodeCount; i++) {
      ModelNode* node = &model->modelData->nodes[i];

      float localTransform[16];
      mat4_identity(localTransform);
      if (!lovrAnimatorEvaluate(model->animator, node->name, localTransform)) {
        mat4_set(localTransform, node->transform);
      }

      mat4 globalTransform = model->nodeTransforms[i];
      if (node->parent >= 0) {
        mat4_set(globalTransform, model->nodeTransforms[node->parent]);
        mat4_multiply(globalTransform, localTransform);
      } else {
        mat4_set(globalTransform, localTransform);
      }
    }
  }

  if (model->material) {
    lovrMeshSetMaterial(model->mesh, model->material);
  }

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(MATRIX_MODEL, transform);
  renderNode(model, 0, instances);
  lovrGraphicsPop();
}

Animator* lovrModelGetAnimator(Model* model) {
  return model->animator;
}

void lovrModelSetAnimator(Model* model, Animator* animator) {
  if (model->animator != animator) {
    if (model->animator) {
      lovrRelease(&model->animator->ref);
    }

    model->animator = animator;

    if (animator) {
      lovrRetain(&animator->ref);
    }
  }
}

int lovrModelGetAnimationCount(Model* model) {
  return model->modelData->animationCount;
}

Material* lovrModelGetMaterial(Model* model) {
  return model->material;
}

void lovrModelSetMaterial(Model* model, Material* material) {
  if (model->material != material) {
    if (model->material) {
      lovrRelease(&model->material->ref);
    }

    model->material = material;

    if (material) {
      lovrRetain(&material->ref);
    }
  }
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
