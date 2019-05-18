#include "graphics/model.h"
#include "graphics/animator.h"
#include "graphics/buffer.h"
#include "graphics/graphics.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "resources/shaders.h"
#include <float.h>

static void updateGlobalNodeTransform(Model* model, uint32_t nodeIndex, mat4 transform) {
  ModelNode* node = &model->data->nodes[nodeIndex];

  mat4 globalTransform = model->globalNodeTransforms + 16 * nodeIndex;
  mat4_set(globalTransform, transform);

  if (!model->animator || !lovrAnimatorEvaluate(model->animator, nodeIndex, globalTransform)) {
    mat4_multiply(globalTransform, node->transform);
  }

  for (uint32_t i = 0; i < node->childCount; i++) {
    updateGlobalNodeTransform(model, node->children[i], globalTransform);
  }
}

static void renderNode(Model* model, uint32_t nodeIndex, int instances) {
  ModelNode* node = &model->data->nodes[nodeIndex];
  mat4 globalTransform = model->globalNodeTransforms + 16 * nodeIndex;

  if (node->primitiveCount > 0) {
    bool animated = node->skin >= 0 && model->animator;
    float pose[16 * MAX_BONES];

    if (animated) {
      ModelSkin* skin = &model->data->skins[node->skin];

      for (uint32_t j = 0; j < skin->jointCount; j++) {
        mat4 globalJointTransform = model->globalNodeTransforms + 16 * skin->joints[j];
        mat4 inverseBindMatrix = skin->inverseBindMatrices + 16 * j;
        mat4 jointPose = pose + 16 * j;

        mat4_set(jointPose, globalTransform);
        mat4_invert(jointPose);
        mat4_multiply(jointPose, globalJointTransform);
        mat4_multiply(jointPose, inverseBindMatrix);
      }
    }

    for (uint32_t i = 0; i < node->primitiveCount; i++) {
      ModelPrimitive* primitive = &model->data->primitives[node->primitiveIndex + i];
      Mesh* mesh = model->meshes[node->primitiveIndex + i];
      Material* material = primitive->material >= 0 ? model->materials[primitive->material] : NULL;

      if (model->userMaterial) {
        material = model->userMaterial;
      }

      uint32_t rangeStart, rangeCount;
      lovrMeshGetDrawRange(mesh, &rangeStart, &rangeCount);

      lovrGraphicsBatch(&(BatchRequest) {
        .type = BATCH_MESH,
        .params.mesh = {
          .object = mesh,
          .mode = primitive->mode,
          .rangeStart = rangeStart,
          .rangeCount = rangeCount,
          .instances = instances,
          .pose = animated ? pose : NULL
        },
        .drawMode = primitive->mode,
        .transform = globalTransform,
        .material = material
      });
    }
  }

  for (uint32_t i = 0; i < node->childCount; i++) {
    renderNode(model, node->children[i], instances);
  }
}

Model* lovrModelInit(Model* model, ModelData* data) {
  model->data = data;
  lovrRetain(data);

  // Geometry
  if (data->primitiveCount > 0) {
    if (data->bufferCount > 0) {
      model->buffers = calloc(data->bufferCount, sizeof(Buffer*));
    }

    model->meshes = calloc(data->primitiveCount, sizeof(Mesh*));
    for (int i = 0; i < data->primitiveCount; i++) {
      ModelPrimitive* primitive = &data->primitives[i];
      model->meshes[i] = lovrMeshCreate(primitive->mode, NULL, 0);

      bool setDrawRange = false;
      for (int j = 0; j < MAX_DEFAULT_ATTRIBUTES; j++) {
        if (primitive->attributes[j]) {
          ModelAttribute* attribute = primitive->attributes[j];

          if (!model->buffers[attribute->buffer]) {
            ModelBuffer* buffer = &data->buffers[attribute->buffer];
            model->buffers[attribute->buffer] = lovrBufferCreate(buffer->size, buffer->data, BUFFER_VERTEX, USAGE_STATIC, false);
          }

          lovrMeshAttachAttribute(model->meshes[i], lovrShaderAttributeNames[j], &(MeshAttribute) {
            .buffer = model->buffers[attribute->buffer],
            .offset = attribute->offset,
            .stride = data->buffers[attribute->buffer].stride,
            .type = attribute->type,
            .components = attribute->components,
            .integer = j == ATTR_BONES,
            .normalized = attribute->normalized
          });

          if (!setDrawRange && !primitive->indices) {
            lovrMeshSetDrawRange(model->meshes[i], 0, attribute->count);
            setDrawRange = true;
          }
        }
      }

      lovrMeshAttachAttribute(model->meshes[i], "lovrDrawID", &(MeshAttribute) {
        .buffer = lovrGraphicsGetIdentityBuffer(),
        .type = U8,
        .components = 1,
        .divisor = 1,
        .integer = true
      });

      if (primitive->indices) {
        ModelAttribute* attribute = primitive->indices;

        if (!model->buffers[attribute->buffer]) {
          ModelBuffer* buffer = &data->buffers[attribute->buffer];
          model->buffers[attribute->buffer] = lovrBufferCreate(buffer->size, buffer->data, BUFFER_INDEX, USAGE_STATIC, false);
        }

        size_t indexSize = attribute->type == U16 ? 2 : 4;
        lovrMeshSetIndexBuffer(model->meshes[i], model->buffers[attribute->buffer], attribute->count, indexSize, attribute->offset);
        lovrMeshSetDrawRange(model->meshes[i], 0, attribute->count);
      }
    }
  }

  // Materials
  if (data->materialCount > 0) {
    model->materials = malloc(data->materialCount * sizeof(Material*));

    if (data->textureCount > 0) {
      model->textures = calloc(data->textureCount, sizeof(Texture*));
    }

    for (int i = 0; i < data->materialCount; i++) {
      Material* material = lovrMaterialCreate();

      for (int j = 0; j < MAX_MATERIAL_SCALARS; j++) {
        lovrMaterialSetScalar(material, j, data->materials[i].scalars[j]);
      }

      for (int j = 0; j < MAX_MATERIAL_COLORS; j++) {
        lovrMaterialSetColor(material, j, data->materials[i].colors[j]);
      }

      for (int j = 0; j < MAX_MATERIAL_TEXTURES; j++) {
        int index = data->materials[i].textures[j];

        if (index != -1) {
          if (!model->textures[index]) {
            TextureData* textureData = data->textures[index];
            bool srgb = j == TEXTURE_DIFFUSE || j == TEXTURE_EMISSIVE;
            model->textures[index] = lovrTextureCreate(TEXTURE_2D, &textureData, 1, srgb, true, 0);
            lovrTextureSetFilter(model->textures[index], data->materials[i].filters[j]);
            lovrTextureSetWrap(model->textures[index], data->materials[i].wraps[j]);
          }

          lovrMaterialSetTexture(material, j, model->textures[index]);
        }
      }

      model->materials[i] = material;
    }
  }

  model->globalNodeTransforms = malloc(16 * sizeof(float) * model->data->nodeCount);
  for (int i = 0; i < model->data->nodeCount; i++) {
    mat4_identity(model->globalNodeTransforms + 16 * i);
  }

  return model;
}

void lovrModelDestroy(void* ref) {
  Model* model = ref;
  for (int i = 0; i < model->data->bufferCount; i++) {
    lovrRelease(Buffer, model->buffers[i]);
  }
  for (int i = 0; i < model->data->primitiveCount; i++) {
    lovrRelease(Mesh, model->meshes[i]);
  }
  lovrRelease(ModelData, model->data);
}

void lovrModelDraw(Model* model, mat4 transform, int instances) {
  updateGlobalNodeTransform(model, model->data->rootNode, transform);
  renderNode(model, model->data->rootNode, instances);
}

Animator* lovrModelGetAnimator(Model* model) {
  return model->animator;
}

void lovrModelSetAnimator(Model* model, Animator* animator) {
  if (model->animator != animator) {
    lovrRetain(animator);
    lovrRelease(Animator, model->animator);
    model->animator = animator;
  }
}

Material* lovrModelGetMaterial(Model* model) {
  return model->userMaterial;
}

void lovrModelSetMaterial(Model* model, Material* material) {
  lovrRetain(material);
  lovrRelease(Material, model->userMaterial);
  model->userMaterial = material;
}

static void applyAABB(Model* model, int nodeIndex, float aabb[6]) {
  ModelNode* node = &model->data->nodes[nodeIndex];

  for (uint32_t i = 0; i < node->primitiveCount; i++) {
    ModelAttribute* position = model->data->primitives[node->primitiveIndex + i].attributes[ATTR_POSITION];
    if (position && position->hasMin && position->hasMax) {
      mat4 m = model->globalNodeTransforms + 16 * nodeIndex;

      float xa[3] = { position->min[0] * m[0], position->min[0] * m[1], position->min[0] * m[2] };
      float xb[3] = { position->max[0] * m[0], position->max[0] * m[1], position->max[0] * m[2] };

      float ya[3] = { position->min[1] * m[4], position->min[1] * m[5], position->min[1] * m[6] };
      float yb[3] = { position->max[1] * m[4], position->max[1] * m[5], position->max[1] * m[6] };

      float za[3] = { position->min[2] * m[8], position->min[2] * m[9], position->min[2] * m[10] };
      float zb[3] = { position->max[2] * m[8], position->max[2] * m[9], position->max[2] * m[10] };

      float min[3] = {
        MIN(xa[0], xb[0]) + MIN(ya[0], yb[0]) + MIN(za[0], zb[0]) + m[12],
        MIN(xa[1], xb[1]) + MIN(ya[1], yb[1]) + MIN(za[1], zb[1]) + m[13],
        MIN(xa[2], xb[2]) + MIN(ya[2], yb[2]) + MIN(za[2], zb[2]) + m[14]
      };

      float max[3] = {
        MAX(xa[0], xb[0]) + MAX(ya[0], yb[0]) + MAX(za[0], zb[0]) + m[12],
        MAX(xa[1], xb[1]) + MAX(ya[1], yb[1]) + MAX(za[1], zb[1]) + m[13],
        MAX(xa[2], xb[2]) + MAX(ya[2], yb[2]) + MAX(za[2], zb[2]) + m[14]
      };

      aabb[0] = MIN(aabb[0], min[0]);
      aabb[1] = MAX(aabb[1], max[0]);
      aabb[2] = MIN(aabb[2], min[1]);
      aabb[3] = MAX(aabb[3], max[1]);
      aabb[4] = MIN(aabb[4], min[2]);
      aabb[5] = MAX(aabb[5], max[2]);
    }
  }

  for (uint32_t i = 0; i < node->childCount; i++) {
    applyAABB(model, node->children[i], aabb);
  }
}

void lovrModelGetAABB(Model* model, float aabb[6]) {
  aabb[0] = aabb[2] = aabb[4] = FLT_MAX;
  aabb[1] = aabb[3] = aabb[5] = -FLT_MAX;
  updateGlobalNodeTransform(model, model->data->rootNode, (float[]) MAT4_IDENTITY);
  applyAABB(model, model->data->rootNode, aabb);
}
