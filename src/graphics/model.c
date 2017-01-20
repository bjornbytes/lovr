#include "graphics/model.h"
#include "graphics/graphics.h"
#include <stdlib.h>

static void visitNode(ModelData* modelData, ModelNode* node, mat4 transform, vec_float_t* vertices, vec_uint_t* indices) {
  float newTransform[16];

  if (transform) {
    mat4_set(newTransform, transform);
  } else {
    mat4_identity(newTransform);
  }

  mat4_multiply(newTransform, node->transform);

  int indexOffset = vertices->length / 3;

  // Meshes
  for (int m = 0; m < node->meshes.length; m++) {
    ModelMesh* mesh = modelData->meshes.data[node->meshes.data[m]];

    // Transformed vertices
    for (int v = 0; v < mesh->vertices.length; v++) {
      ModelVertex vertex = mesh->vertices.data[v];

      float vec[3];
      vec3_set(vec, vertex.x, vertex.y, vertex.z);
      vec3_transform(vec, newTransform);
      vec_pusharr(vertices, vec, 3);

      if (modelData->hasNormals) {
        ModelVertex normal = mesh->normals.data[v];
        vec_push(vertices, normal.x);
        vec_push(vertices, normal.y);
        vec_push(vertices, normal.z);
      }

      if (modelData->hasTexCoords) {
        ModelVertex texCoord = mesh->texCoords.data[v];
        vec_push(vertices, texCoord.x);
        vec_push(vertices, texCoord.y);
      }
    }

    // Face vertex indices
    for (int f = 0; f < mesh->faces.length; f++) {
      ModelFace face = mesh->faces.data[f];
      vec_push(indices, face.indices[0] + indexOffset);
      vec_push(indices, face.indices[1] + indexOffset);
      vec_push(indices, face.indices[2] + indexOffset);
    }
  }

  for (int c = 0; c < node->children.length; c++) {
    visitNode(modelData, node->children.data[c], newTransform, vertices, indices);
  }
}

Model* lovrModelCreate(ModelData* modelData) {
  Model* model = lovrAlloc(sizeof(Model), lovrModelDestroy);
  if (!model) return NULL;

  model->modelData = modelData;

  vec_float_t vertices;
  vec_init(&vertices);

  vec_uint_t indices;
  vec_init(&indices);

  visitNode(modelData, modelData->root, NULL, &vertices, &indices);

  BufferFormat format;
  vec_init(&format);

  int components = 3;
  BufferAttribute position = { .name = "lovrPosition", .type = BUFFER_FLOAT, .count = 3 };
  vec_push(&format, position);

  if (modelData->hasNormals) {
    BufferAttribute normal = { .name = "lovrNormal", .type = BUFFER_FLOAT, .count = 3 };
    vec_push(&format, normal);
    components += 3;
  }

  if (modelData->hasTexCoords) {
    BufferAttribute texCoord = { .name = "lovrTexCoord", .type = BUFFER_FLOAT, .count = 2 };
    vec_push(&format, texCoord);
    components += 2;
  }

  model->buffer = lovrBufferCreate(vertices.length / components, &format, BUFFER_TRIANGLES, BUFFER_STATIC);
  lovrBufferSetVertices(model->buffer, (void*) vertices.data, vertices.length * sizeof(float));
  lovrBufferSetVertexMap(model->buffer, indices.data, indices.length);

  model->texture = NULL;

  vec_deinit(&format);
  vec_deinit(&vertices);
  vec_deinit(&indices);
  return model;
}

void lovrModelDestroy(const Ref* ref) {
  Model* model = containerof(ref, Model);
  if (model->texture) {
    lovrRelease(&model->texture->ref);
  }
  lovrModelDataDestroy(model->modelData);
  lovrRelease(&model->buffer->ref);
  free(model);
}

void lovrModelDataDestroy(ModelData* modelData) {
  for (int i = 0; i < modelData->meshes.length; i++) {
    ModelMesh* mesh = modelData->meshes.data[i];
    vec_deinit(&mesh->faces);
    vec_deinit(&mesh->vertices);
    vec_deinit(&mesh->normals);
    if (modelData->hasTexCoords) {
      vec_deinit(&mesh->texCoords);
    }
    free(mesh);
  }

  vec_void_t nodes;
  vec_init(&nodes);
  vec_push(&nodes, modelData->root);
  while (nodes.length > 0) {
    ModelNode* node = vec_first(&nodes);
    vec_extend(&nodes, &node->children);
    vec_deinit(&node->meshes);
    vec_deinit(&node->children);
    vec_splice(&nodes, 0, 1);
    free(node);
  }

  vec_deinit(&modelData->meshes);
  vec_deinit(&nodes);
  free(modelData);
}

void lovrModelDraw(Model* model, float x, float y, float z, float scale, float angle, float ax, float ay, float az) {
  lovrGraphicsPush();
  lovrGraphicsTransform(x, y, z, scale, scale, scale, angle, ax, ay, az);
  lovrBufferDraw(model->buffer);
  lovrGraphicsPop();
}

Texture* lovrModelGetTexture(Model* model) {
  return model->texture;
}

void lovrModelSetTexture(Model* model, Texture* texture) {
  if (model->texture) {
    lovrRelease(&model->texture->ref);
  }

  model->texture = texture;
  lovrBufferSetTexture(model->buffer, model->texture);

  if (model->texture) {
    lovrRetain(&model->texture->ref);
  }
}
