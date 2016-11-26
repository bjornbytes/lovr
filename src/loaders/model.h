#include "vendor/vec/vec.h"
#include "util.h"
#include "matrix.h"
#include <stdbool.h>
#ifndef _WIN32
#define __stdcall
#endif
#include <openvr_capi.h>

#ifndef LOVR_MODEL_DATA_TYPES
#define LOVR_MODEL_DATA_TYPES

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
  mat4 transform;
  vec_uint_t meshes;
  vec_void_t children;
} ModelNode;

typedef struct {
  Ref ref;
  ModelNode* root;
  vec_model_mesh_t meshes;
  int hasNormals;
  int hasTexCoords;
} ModelData;

#endif

ModelData* lovrModelDataCreateFromFile(void* data, int size);
ModelData* lovrModelDataCreateFromOpenVRModel(RenderModel_t* renderModel);
void lovrModelDataDestroy(const Ref* ref);
