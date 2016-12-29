#include "loaders/model.h"
#include <stdlib.h>
#include <assimp/scene.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>

static void assimpNodeTraversal(ModelNode* node, struct aiNode* assimpNode) {

  // Transform
  struct aiMatrix4x4 m = assimpNode->mTransformation;
  aiTransposeMatrix4(&m);
  node->transform = mat4_copy((float*) &m);

  // Meshes
  vec_init(&node->meshes);
  vec_pusharr(&node->meshes, assimpNode->mMeshes, assimpNode->mNumMeshes);

  // Children
  vec_init(&node->children);
  for (unsigned int n = 0; n < assimpNode->mNumChildren; n++) {
    ModelNode* child = malloc(sizeof(ModelNode));
    assimpNodeTraversal(child, assimpNode->mChildren[n]);
    vec_push(&node->children, child);
  }
}

ModelData* lovrModelDataFromFile(void* data, int size) {
  ModelData* modelData = malloc(sizeof(ModelData));
  if (!modelData) return NULL;

  modelData->hasNormals = 0;
  modelData->hasTexCoords = 0;

  unsigned int flags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph | aiProcess_FlipUVs;
  const struct aiScene* scene = aiImportFileFromMemory(data, size, flags, NULL);

  // Meshes
  vec_init(&modelData->meshes);
  for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
    struct aiMesh* assimpMesh = scene->mMeshes[m];
    ModelMesh* mesh = malloc(sizeof(ModelMesh));
    vec_push(&modelData->meshes, mesh);

    // Faces
    vec_init(&mesh->faces);
    for (unsigned int f = 0; f < assimpMesh->mNumFaces; f++) {
      struct aiFace assimpFace = assimpMesh->mFaces[f];

      // Skip lines and points, polygons are triangulated
      if (assimpFace.mNumIndices != 3) {
        continue;
      }

      ModelFace face;
      for (unsigned int i = 0; i < assimpFace.mNumIndices; i++) {
        face.indices[i] = assimpFace.mIndices[i];
      }
      vec_push(&mesh->faces, face);
    }

    // Vertices
    vec_init(&mesh->vertices);
    for (unsigned int v = 0; v < assimpMesh->mNumVertices; v++) {
      ModelVertex vertex;
      vertex.x = assimpMesh->mVertices[v].x;
      vertex.y = assimpMesh->mVertices[v].y;
      vertex.z = assimpMesh->mVertices[v].z;
      vec_push(&mesh->vertices, vertex);
    }

    // Normals
    if (!assimpMesh->mNormals) {
      error("Model must have normals");
    }

    modelData->hasNormals = 1;
    vec_init(&mesh->normals);
    for (unsigned int n = 0; n < assimpMesh->mNumVertices; n++) {
      ModelVertex normal;
      normal.x = assimpMesh->mNormals[n].x;
      normal.y = assimpMesh->mNormals[n].y;
      normal.z = assimpMesh->mNormals[n].z;
      vec_push(&mesh->normals, normal);
    }

    modelData->hasTexCoords = modelData->hasTexCoords || assimpMesh->mTextureCoords[0] != NULL;
    if (assimpMesh->mTextureCoords[0]) {
      vec_init(&mesh->texCoords);
      for (unsigned int i = 0; i < assimpMesh->mNumVertices; i++) {
        ModelVertex texCoord;
        texCoord.x = assimpMesh->mTextureCoords[0][i].x;
        texCoord.y = assimpMesh->mTextureCoords[0][i].y;
        vec_push(&mesh->texCoords, texCoord);
      }
    }
  }

  // Nodes
  modelData->root = malloc(sizeof(ModelNode));
  assimpNodeTraversal(modelData->root, scene->mRootNode);

  aiReleaseImport(scene);
  return modelData;
}

#ifndef EMSCRIPTEN
ModelData* lovrModelDataFromOpenVRModel(OpenVRModel* vrModel) {
  ModelData* modelData = malloc(sizeof(ModelData));
  if (!modelData) return NULL;

  RenderModel_t* renderModel = vrModel->model;

  ModelMesh* mesh = malloc(sizeof(ModelMesh));
  vec_init(&modelData->meshes);
  vec_push(&modelData->meshes, mesh);

  vec_init(&mesh->faces);
  for (uint32_t i = 0; i < renderModel->unTriangleCount; i++) {
    ModelFace face;
    face.indices[0] = renderModel->rIndexData[3 * i + 0];
    face.indices[1] = renderModel->rIndexData[3 * i + 1];
    face.indices[2] = renderModel->rIndexData[3 * i + 2];
    vec_push(&mesh->faces, face);
  }

  vec_init(&mesh->vertices);
  vec_init(&mesh->normals);
  vec_init(&mesh->texCoords);
  for (size_t i = 0; i < renderModel->unVertexCount; i++) {
    float* position = renderModel->rVertexData[i].vPosition.v;
    float* normal = renderModel->rVertexData[i].vNormal.v;
    ModelVertex v;

    v.x = position[0];
    v.y = position[1];
    v.z = position[2];
    vec_push(&mesh->vertices, v);

    v.x = normal[0];
    v.y = normal[1];
    v.z = normal[2];
    vec_push(&mesh->normals, v);

    float* texCoords = renderModel->rVertexData[i].rfTextureCoord;
    v.x = texCoords[0];
    v.y = texCoords[1];
    v.z = 0.f;
    vec_push(&mesh->texCoords, v);
  }

  ModelNode* root = malloc(sizeof(ModelNode));
  root->transform = mat4_init();
  vec_init(&root->meshes);
  vec_push(&root->meshes, 0);
  vec_init(&root->children);

  modelData->root = root;
  modelData->hasNormals = 1;
  modelData->hasTexCoords = 1;

  return modelData;
}
#endif
