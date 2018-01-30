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

      if (!model->material && model->materials) {
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

  model->mesh = lovrMeshCreate(modelData->vertexCount, &modelData->format, MESH_TRIANGLES, MESH_STATIC);
  VertexData vertices = lovrMeshMap(model->mesh, 0, modelData->vertexCount, false, true);
  memcpy(vertices.data, modelData->vertices.data, modelData->vertexCount * modelData->format.stride);
  lovrMeshUnmap(model->mesh);
  lovrMeshSetVertexMap(model->mesh, modelData->indices.data, modelData->indexCount);
  lovrMeshSetRangeEnabled(model->mesh, true);

  if (modelData->textures.length > 0) {
    model->textures = malloc(modelData->textures.length * sizeof(Texture*));
    for (int i = 0; i < modelData->textures.length; i++) {
      if (modelData->textures.data[i]) {
        model->textures[i] = lovrTextureCreate(TEXTURE_2D, (TextureData**) &modelData->textures.data[i], 1, true);
      } else {
        model->textures[i] = NULL;
      }
    }
  } else {
    model->textures = NULL;
  }

  if (modelData->materialCount > 0) {
    model->materials = malloc(modelData->materialCount * sizeof(Material*));
    for (int i = 0; i < modelData->materialCount; i++) {
      ModelMaterial* materialData = &modelData->materials[i];
      Material* material = lovrMaterialCreate(false);
      lovrMaterialSetColor(material, COLOR_DIFFUSE, materialData->diffuseColor);
      lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, model->textures[materialData->diffuseTexture]);
      model->materials[i] = material;
    }
  } else {
    model->materials = NULL;
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

  return model;
}

void lovrModelDestroy(const Ref* ref) {
  Model* model = containerof(ref, Model);
  for (int i = 0; i < model->modelData->textures.length; i++) {
    if (model->textures[i]) {
      lovrRelease(&model->textures[i]->ref);
    }
  }
  for (int i = 0; i < model->modelData->materialCount; i++) {
    lovrRelease(&model->materials[i]->ref);
  }
  if (model->animator) {
    lovrRelease(&model->animator->ref);
  }
  if (model->material) {
    lovrRelease(&model->material->ref);
  }
  free(model->textures);
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
