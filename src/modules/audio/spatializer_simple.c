#include "spatializer.h"
#include "core/maf.h"
#include "core/util.h"
#include <math.h>

static struct {
  float listener[16];
  float gain[MAX_SOURCES][2];
} state;

bool simple_init(void) {
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

  float target[2] = { 1.f, 1.f };
  if (lovrSourceIsEffectEnabled(source, EFFECT_SPATIALIZATION)) {
    float leftEar[4] = { -0.1f, 0.0f, 0.0f, 1.0f };
    float rightEar[4] = { 0.1f, 0.0f, 0.0f, 1.0f };
    mat4_transform(state.listener, leftEar);
    mat4_transform(state.listener, rightEar);
    float ldistance = vec3_distance(sourcePos, leftEar);
    float rdistance = vec3_distance(sourcePos, rightEar);
    target[0] = .5f + (rdistance - ldistance) * 2.5f;
    target[1] = .5f + (ldistance - rdistance) * 2.5f;
  }

  float weight, power;
  lovrSourceGetDirectivity(source, &weight, &power);
  if (weight > 0.f && power > 0.f) {
    float sourceDirection[4];
    float sourceToListener[4];
    quat_getDirection(sourceOrientation, sourceDirection);
    vec3_normalize(vec3_sub(vec3_init(sourceToListener, listenerPos), sourcePos));
    float dot = vec3_dot(sourceToListener, sourceDirection);
    float factor = powf(fabsf(1.f - weight + weight * dot), power);
    target[0] *= factor;
    target[1] *= factor;
  }

  if (lovrSourceIsEffectEnabled(source, EFFECT_ATTENUATION)) {
    float distance = vec3_distance(sourcePos, listenerPos);
    float attenuation = 1.f / MAX(distance, 1.f);
    target[0] *= attenuation;
    target[1] *= attenuation;
  }

  uint32_t index = lovrSourceGetIndex(source);
  float* gain = state.gain[index];

  float lerpDuration = .05f;
  float lerpFrames = SAMPLE_RATE * lerpDuration;
  float lerpRate = 1.f / lerpFrames;

  for (uint32_t c = 0; c < 2; c++) {
    float sign = target[c] > gain[c] ? 1.f : -1.f;

    uint32_t lerpCount = fabsf(target[c] - gain[c]) / lerpRate;
    lerpCount = MIN(lerpCount, frames);

    for (uint32_t i = 0; i < lerpCount; i++) {
      output[i * 2 + c] = input[i] * gain[c];
      gain[c] += lerpRate * sign;
    }

    for (uint32_t i = lerpCount; i < frames; i++) {
      output[i * 2 + c] = input[i] * gain[c];
    }
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
  uint32_t index = lovrSourceGetIndex(source);
  state.gain[index][0] = 0.f;
  state.gain[index][1] = 0.f;
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
