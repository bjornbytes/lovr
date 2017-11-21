#include "loaders/model.h"
#include "loaders/animation.h"
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
  Mesh* mesh;
  Material** materials;
  Animator* animator;
  float pose[MAX_BONES * 16];
  float* localNodeTransforms;
  float* globalNodeTransforms;
  float aabb[6];
  bool aabbDirty;
} Model;

Model* lovrModelCreate(ModelData* modelData);
void lovrModelDestroy(const Ref* ref);
void lovrModelDraw(Model* model, mat4 transform);
Animator* lovrModelGetAnimator(Model* model);
void lovrModelSetAnimator(Model* model, Animator* animator);
int lovrModelGetAnimationCount(Model* model);
Mesh* lovrModelGetMesh(Model* model);
const float* lovrModelGetAABB(Model* model);
