#include "spatializer.h"
#include "core/maf.h"
#include "core/util.h"

static struct {
  float listener[16];
} state;

bool simple_init() {
  mat4_identity(state.listener);
  return true;
}

void simple_destroy(void) {
  //
}

uint32_t simple_apply(Source* source, const float* input, float* output, uint32_t frames, uint32_t _frames) {
  float sourcePos[4], sourceOrientation[4];
  lovrSourceGetPose(source, sourcePos, sourceOrientation);

  float listenerPos[4] = { 0.f };
  mat4_transform(state.listener, listenerPos);

  float distanceAttenuation = 1.f;
  if (lovrSourceIsEffectEnabled(source, EFFECT_FALLOFF)) {
    float distance = vec3_distance(sourcePos, listenerPos);
    distanceAttenuation = 1.f / MAX(distance, 1.f);
  }

  float leftAttenuation = 1.f;
  float rightAttenuation = 1.f;
  if (lovrSourceIsEffectEnabled(source, EFFECT_SPATIALIZATION)) {
    float leftEar[4] = { -0.1f, 0.0f, 0.0f, 1.0f };
    float rightEar[4] = { 0.1f, 0.0f, 0.0f, 1.0f };
    mat4_transform(state.listener, leftEar);
    mat4_transform(state.listener, rightEar);
    float ldistance = vec3_distance(sourcePos, leftEar);
    float rdistance = vec3_distance(sourcePos, rightEar);
    leftAttenuation = .5f + (rdistance - ldistance) * 2.5f;
    rightAttenuation = .5f + (ldistance - rdistance) * 2.5f;
  }

  for (unsigned int i = 0; i < frames; i++) {
    output[i * 2 + 0] = input[i] * distanceAttenuation * leftAttenuation;
    output[i * 2 + 1] = input[i] * distanceAttenuation * rightAttenuation;
  }

  return frames;
}

uint32_t simple_tail(float* scratch, float* output, uint32_t frames) {
  return 0;
}

void simple_setListenerPose(float position[4], float orientation[4]) {
  mat4_identity(state.listener);
  mat4_translate(state.listener, position[0], position[1], position[2]);
  mat4_rotateQuat(state.listener, orientation);
}

bool simple_setGeometry(float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial material) {
  return false;
}

void simple_sourceCreate(Source* source) {
  //
}

void simple_sourceDestroy(Source* source) {
  //
}

Spatializer simpleSpatializer = {
  .init = simple_init,
  .destroy = simple_destroy,
  .apply = simple_apply,
  .tail = simple_tail,
  .setListenerPose = simple_setListenerPose,
  .setGeometry = simple_setGeometry,
  .sourceCreate = simple_sourceCreate,
  .sourceDestroy = simple_sourceDestroy,
  .name = "simple"
};
