#include "graphics/model.h"
#include "graphics/graphics.h"
#include "graphics/shader.h"
#include "data/blob.h"
#include "data/textureData.h"
#include "data/vertexData.h"
#include "math/mat4.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void renderNode(Model* model, int nodeIndex, int instances) {
  ModelNode* node = &model->modelData->nodes[nodeIndex];

  if (node->primitives.length > 0) {
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
      lovrMeshSetPose(model->mesh, (float*) model->pose);
      lovrGraphicsDraw(&(DrawCommand) {
        .transform = model->nodeTransforms[nodeIndex],
        .mesh = model->mesh,
        .material = lovrMeshGetMaterial(model->mesh),
        .instances = instances
      });
    }
  }

  for (int i = 0; i < node->children.length; i++) {
    renderNode(model, node->children.data[i], instances);
  }
}

Model* lovrModelCreate(ModelData* modelData) {
  Model* model = lovrAlloc(Model, lovrModelDestroy);
  if (!model) return NULL;

  lovrRetain(modelData);
  model->modelData = modelData;
  model->aabbDirty = true;

  model->mesh = lovrMeshCreate(modelData->vertexData->count, modelData->vertexData->format, MESH_TRIANGLES, USAGE_STATIC);
  VertexPointer vertices = lovrMeshMapVertices(model->mesh, 0, modelData->vertexData->count, false, true);
  memcpy(vertices.raw, modelData->vertexData->blob.data, modelData->vertexData->count * modelData->vertexData->format.stride);

  IndexPointer indices = lovrMeshWriteIndices(model->mesh, modelData->indexCount, modelData->indexSize);
  memcpy(indices.raw, modelData->indices.raw, modelData->indexCount * modelData->indexSize);

  if (modelData->textures.length > 0) {
    model->textures = calloc(modelData->textures.length, sizeof(Texture*));
  }

  if (modelData->materialCount > 0) {
    model->materials = calloc(modelData->materialCount, sizeof(Material*));
    for (int i = 0; i < modelData->materialCount; i++) {
      ModelMaterial* materialData = &modelData->materials[i];
      Material* material = lovrMaterialCreate();
      lovrMaterialSetScalar(material, SCALAR_METALNESS, materialData->metalness);
      lovrMaterialSetScalar(material, SCALAR_ROUGHNESS, materialData->roughness);
      lovrMaterialSetColor(material, COLOR_DIFFUSE, materialData->diffuseColor);
      lovrMaterialSetColor(material, COLOR_EMISSIVE, materialData->emissiveColor);
      for (MaterialTexture textureType = 0; textureType < MAX_MATERIAL_TEXTURES; textureType++) {
        int textureIndex = 0;

        switch (textureType) {
          case TEXTURE_DIFFUSE: textureIndex = materialData->diffuseTexture; break;
          case TEXTURE_EMISSIVE: textureIndex = materialData->emissiveTexture; break;
          case TEXTURE_METALNESS: textureIndex = materialData->metalnessTexture; break;
          case TEXTURE_ROUGHNESS: textureIndex = materialData->roughnessTexture; break;
          case TEXTURE_OCCLUSION: textureIndex = materialData->occlusionTexture; break;
          case TEXTURE_NORMAL: textureIndex = materialData->normalTexture; break;
          default: break;
        }

        if (textureIndex) {
          if (!model->textures[textureIndex]) {
            TextureData* textureData = modelData->textures.data[textureIndex];
            bool srgb = textureType == TEXTURE_DIFFUSE || textureType == TEXTURE_EMISSIVE;
            model->textures[textureIndex] = lovrTextureCreate(TEXTURE_2D, &textureData, 1, srgb, true, 0);
          }

          lovrMaterialSetTexture(material, textureType, model->textures[textureIndex]);
        }
      }

      model->materials[i] = material;
    }
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

void lovrModelDestroy(void* ref) {
  Model* model = ref;
  for (int i = 0; i < model->modelData->textures.length; i++) {
    lovrRelease(model->textures[i]);
  }
  for (int i = 0; i < model->modelData->materialCount; i++) {
    lovrRelease(model->materials[i]);
  }
  lovrRelease(model->animator);
  lovrRelease(model->material);
  free(model->textures);
  free(model->materials);
  lovrRelease(model->modelData);
  lovrRelease(model->mesh);
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
  lovrGraphicsMatrixTransform(transform);
  renderNode(model, 0, instances);
  lovrGraphicsPop();
  lovrMeshSetPose(model->mesh, NULL);
}

Animator* lovrModelGetAnimator(Model* model) {
  return model->animator;
}

void lovrModelSetAnimator(Model* model, Animator* animator) {
  if (model->animator != animator) {
    lovrRetain(animator);
    lovrRelease(model->animator);
    model->animator = animator;
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
    lovrRetain(material);
    lovrRelease(model->material);
    model->material = material;
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
