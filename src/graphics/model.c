#include "graphics/model.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include <stdlib.h>
#include <float.h>

static void visitNode(Model* model, ModelData* modelData, ModelNode* node, mat4 transform, vec_float_t* vertices, vec_uint_t* indices) {
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

      float vec[3] = { vertex.x, vertex.y, vertex.z };
      mat4_transform(newTransform, vec);
      vec_pusharr(vertices, vec, 3);

      model->aabb[0] = MIN(model->aabb[0], vec[0]);
      model->aabb[1] = MAX(model->aabb[1], vec[0]);
      model->aabb[2] = MIN(model->aabb[2], vec[1]);
      model->aabb[3] = MAX(model->aabb[3], vec[1]);
      model->aabb[4] = MIN(model->aabb[4], vec[2]);
      model->aabb[5] = MAX(model->aabb[5], vec[2]);

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
    visitNode(model, modelData, node->children.data[c], newTransform, vertices, indices);
  }
}

Model* lovrModelCreate(ModelData* modelData) {
  Model* model = lovrAlloc(sizeof(Model), lovrModelDestroy);
  if (!model) return NULL;

  model->modelData = modelData;
  model->aabb[0] = FLT_MAX;
  model->aabb[1] = FLT_MIN;
  model->aabb[2] = FLT_MAX;
  model->aabb[3] = FLT_MIN;
  model->aabb[4] = FLT_MAX;
  model->aabb[5] = FLT_MIN;

  vec_float_t vertices;
  vec_init(&vertices);

  vec_uint_t indices;
  vec_init(&indices);

  visitNode(model, modelData, modelData->root, NULL, &vertices, &indices);

  MeshFormat format;
  vec_init(&format);

  int components = 3;
  MeshAttribute position = { .name = "lovrPosition", .type = MESH_FLOAT, .count = 3 };
  vec_push(&format, position);

  if (modelData->hasNormals) {
    MeshAttribute normal = { .name = "lovrNormal", .type = MESH_FLOAT, .count = 3 };
    vec_push(&format, normal);
    components += 3;
  }

  if (modelData->hasTexCoords) {
    MeshAttribute texCoord = { .name = "lovrTexCoord", .type = MESH_FLOAT, .count = 2 };
    vec_push(&format, texCoord);
    components += 2;
  }

  model->mesh = lovrMeshCreate(vertices.length / components, &format, MESH_TRIANGLES, MESH_STATIC);
  void* data = lovrMeshMap(model->mesh, 0, vertices.length / components);
  memcpy(data, vertices.data, vertices.length * sizeof(float));
  lovrMeshUnmap(model->mesh);
  lovrMeshSetVertexMap(model->mesh, indices.data, indices.length);

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
  lovrRelease(&model->mesh->ref);
  free(model);
}

void lovrModelDraw(Model* model, mat4 transform) {
  lovrMeshDraw(model->mesh, transform);
}

Texture* lovrModelGetTexture(Model* model) {
  return model->texture;
}

void lovrModelSetTexture(Model* model, Texture* texture) {
  if (model->texture) {
    lovrRelease(&model->texture->ref);
  }

  model->texture = texture;
  lovrMeshSetTexture(model->mesh, model->texture);

  if (model->texture) {
    lovrRetain(&model->texture->ref);
  }
}

float* lovrModelGetAABB(Model* model) {
  return model->aabb;
}
