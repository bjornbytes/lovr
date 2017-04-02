#include "filesystem/blob.h"
#include "util.h"
#include "lib/vec/vec.h"

#pragma once

typedef struct {
  float x;
  float y;
  float z;
} ModelVertex;

typedef vec_t(ModelVertex) vec_model_vertex_t;

typedef struct {
  unsigned int indices[3];
} ModelFace;

typedef vec_t(ModelFace) vec_model_face_t;

typedef struct {
  vec_model_face_t faces;
  vec_model_vertex_t vertices;
  vec_model_vertex_t normals;
  vec_model_vertex_t texCoords;
} ModelMesh;

typedef vec_t(ModelMesh*) vec_model_mesh_t;

typedef struct ModelNode {
  float transform[16];
  vec_uint_t meshes;
  vec_void_t children;
} ModelNode;

typedef struct {
  ModelNode* root;
  vec_model_mesh_t meshes;
  int hasNormals;
  int hasTexCoords;
} ModelData;

ModelData* lovrModelDataCreate(Blob* blob);
void lovrModelDataDestroy(ModelData* modelData);
