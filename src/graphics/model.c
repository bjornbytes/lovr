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

  if (node->mesh >= 0) {
    ModelMesh* modelMesh = &model->data->meshes[node->mesh];

    float pose[16 * MAX_BONES];
    if (node->skin >= 0 && model->animator) {
      ModelSkin* skin = &model->data->skins[node->skin];

      for (uint32_t j = 0; j < skin->jointCount; j++) {
        mat4 globalJointTransform = model->globalNodeTransforms + 16 * skin->joints[j];

        ModelAccessor* inverseBindMatrices = &model->data->accessors[skin->inverseBindMatrices];
        ModelView* view = &model->data->views[inverseBindMatrices->view];
        mat4 inverseBindMatrix = (float*) ((uint8_t*) model->data->blobs[view->blob].data + view->offset + inverseBindMatrices->offset + 16 * j * sizeof(float));

        mat4 jointPose = pose + 16 * j;
        mat4_set(jointPose, globalTransform);
        mat4_invert(jointPose);
        mat4_multiply(jointPose, globalJointTransform);
        mat4_multiply(jointPose, inverseBindMatrix);
      }
    }

    for (uint32_t i = 0; i < modelMesh->primitiveCount; i++) {
      uint32_t primitiveIndex = modelMesh->firstPrimitive + i;
      ModelPrimitive* primitive = &model->data->primitives[primitiveIndex];
      Mesh* mesh = model->meshes[primitiveIndex];

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
        .transform = globalTransform
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

  if (data->viewCount > 0) {
    model->buffers = calloc(data->viewCount, sizeof(Buffer*));
    for (int i = 0; i < data->viewCount; i++) {
      ModelView* view = &data->views[i];
      ModelBlob* blob = &data->blobs[view->blob];
      model->buffers[i] = lovrBufferCreate(view->length, (uint8_t*) blob->data + view->offset, BUFFER_GENERIC, USAGE_STATIC, false);
    }
  }

  if (data->primitiveCount > 0) {
    model->meshes = calloc(data->primitiveCount, sizeof(Mesh*));
    for (int i = 0; i < data->primitiveCount; i++) {
      ModelPrimitive* primitive = &data->primitives[i];
      model->meshes[i] = lovrMeshCreateEmpty(primitive->mode);

      bool setDrawRange = false;
      for (int j = 0; j < MAX_DEFAULT_ATTRIBUTES; j++) {
        if (primitive->attributes[j] >= 0) {
          ModelAccessor* accessor = &data->accessors[primitive->attributes[j]];

          lovrMeshAttachAttribute(model->meshes[i], lovrShaderAttributeNames[j], &(MeshAttribute) {
            .buffer = model->buffers[accessor->view],
            .offset = accessor->offset,
            .stride = data->views[accessor->view].stride,
            .type = accessor->type,
            .components = accessor->components,
            .enabled = true
          });

          if (!setDrawRange && primitive->indices == -1) {
            lovrMeshSetDrawRange(model->meshes[i], 0, accessor->count);
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

      if (primitive->indices >= 0) {
        ModelAccessor* accessor = &data->accessors[primitive->indices];
        lovrMeshSetIndexBuffer(model->meshes[i], model->buffers[accessor->view], accessor->count, accessor->type == U16 ? 2 : 4);
        lovrMeshSetDrawRange(model->meshes[i], 0, accessor->count);
      }
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
  for (int i = 0; i < model->data->viewCount; i++) {
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
