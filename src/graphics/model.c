#include "graphics/model.h"
#include "graphics/graphics.h"
#include "resources/shaders.h"

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
    float pose[16 * MAX_BONES];
    if (node->skin >= 0 && model->animator) {
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
          .pose = node->skin >= 0 ? pose : NULL
        },
        .transform = globalTransform,
        .material = model->materials[primitive->material]
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
      model->meshes[i] = lovrMeshCreateEmpty(primitive->mode);

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
            .enabled = true
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
        .integer = true,
        .enabled = true
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

    if (data->imageCount > 0) {
      model->textures = calloc(data->imageCount, sizeof(Texture*));
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

        if (!model->textures[index]) {
          ModelTexture* texture = &data->textures[index];
          TextureData* image = data->images[texture->imageIndex];
          bool srgb = j == TEXTURE_DIFFUSE || j == TEXTURE_EMISSIVE;
          model->textures[index] = lovrTextureCreate(TEXTURE_2D, &image, 1, srgb, texture->mipmaps, 0);
          lovrTextureSetFilter(model->textures[index], texture->filter);
          lovrTextureSetWrap(model->textures[index], texture->wrap);
        }

        lovrMaterialSetTexture(material, j, model->textures[index]);
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
    lovrRelease(model->buffers[i]);
  }
  for (int i = 0; i < model->data->primitiveCount; i++) {
    lovrRelease(model->meshes[i]);
  }
  lovrRelease(model->data);
}

void lovrModelDraw(Model* model, mat4 transform, int instances) {
  updateGlobalNodeTransform(model, 0, transform);
  renderNode(model, 0, instances); // TODO use root
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
