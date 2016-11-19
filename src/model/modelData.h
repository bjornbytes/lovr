#include "vendor/vec/vec.h"
#include "util.h"
#include "matrix.h"

#ifndef LOVR_MODEL_DATA_TYPES
#define LOVR_MODEL_DATA_TYPES

typedef struct {
  float x;
  float y;
  float z;
} ModelVertex;

typedef vec_t(ModelVertex) vec_model_vertex_t;

typedef struct {
  vec_uint_t indices;
} ModelFace;

typedef vec_t(ModelFace) vec_model_face_t;

typedef struct {
  vec_model_face_t faces;
  vec_model_vertex_t vertices;
  vec_model_vertex_t normals;
} ModelMesh;

typedef vec_t(ModelMesh*) vec_model_mesh_t;

typedef struct ModelNode {
  mat4 transform;
  vec_uint_t meshes;
  vec_void_t children;
} ModelNode;

typedef struct {
  ModelNode* root;
  vec_model_mesh_t meshes;
  int hasNormals;
} ModelData;

#endif

ModelData* lovrModelDataCreate(void* data, int size);
void lovrModelDataDestroy(ModelData* modelData);
