#include "graphics/model.h"
#include "graphics/graphics.h"
#include "resources/shaders.h"

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
      }
    }
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
  free(ref);
}

void lovrModelDraw(Model* model, mat4 transform, int instances) {
  //
}
