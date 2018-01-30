#include "data/model.h"
#include "graphics/animator.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/texture.h"
#include "math/math.h"
#include "lib/glfw.h"
#include "util.h"
#include <stdbool.h>

#pragma once

typedef struct {
  Ref ref;
  ModelData* modelData;
  Texture** textures;
  Material** materials;
  Material* material;
  Animator* animator;
  Mesh* mesh;
  float pose[MAX_BONES][16];
  float (*nodeTransforms)[16];
  float aabb[6];
  bool aabbDirty;
} Model;

Model* lovrModelCreate(ModelData* modelData);
void lovrModelDestroy(const Ref* ref);
void lovrModelDraw(Model* model, mat4 transform, int instances);
Animator* lovrModelGetAnimator(Model* model);
void lovrModelSetAnimator(Model* model, Animator* animator);
int lovrModelGetAnimationCount(Model* model);
Material* lovrModelGetMaterial(Model* model);
void lovrModelSetMaterial(Model* model, Material* material);
Mesh* lovrModelGetMesh(Model* model);
const float* lovrModelGetAABB(Model* model);
