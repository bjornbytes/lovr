#include "../spatializer.h"

struct {
  float listener[16];
} state;

bool dummy_spatializer_init(void)
{
  mat4_identity(state.listener);
  return true;
}
void dummy_spatializer_destroy(void)
{

}
void dummy_spatializer_apply(Source* source, mat4 transform, const float* input, float* output, uint32_t frames) {
  float sourcePos[4] = {0};
  mat4_transform(transform, sourcePos);

  float listenerPos[4] = {0};
  mat4_transform(state.listener, listenerPos);

  float distance = vec3_distance(sourcePos, listenerPos);
  float leftEar[4] = {-0.1,0,0,1};
  float rightEar[4] = {0.1,0,0,1};
  mat4_transform(state.listener, leftEar);
  mat4_transform(state.listener, rightEar);
  float ldistance = vec3_distance(sourcePos, leftEar);
  float rdistance = vec3_distance(sourcePos, rightEar);
  float distanceAttenuation = MAX(1.0 - distance/10.0, 0.0);
  float leftAttenuation = 0.5 + (rdistance-ldistance)*2.5;
  float rightAttenuation = 0.5 + (ldistance-rdistance)*2.5;

  for(int i = 0; i < frames; i++) {
    output[i*2] = input[i] * distanceAttenuation * leftAttenuation;
    output[i*2+1] = input[i] * distanceAttenuation * rightAttenuation;
  }
}
void dummy_spatializer_setListenerPose(float position[4], float orientation[4]) {
  mat4_identity(state.listener);
  mat4_translate(state.listener, position[0], position[1], position[2]);
  mat4_rotate(state.listener, orientation[0], orientation[1], orientation[2], orientation[3]);
}
Spatializer dummySpatializer = {
  dummy_spatializer_init,
  dummy_spatializer_destroy,
  dummy_spatializer_apply,
  dummy_spatializer_setListenerPose,
  "dummy"
};