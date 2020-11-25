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
void dummy_spatializer_apply(Source* source, mat4 pose, const float* input, float* output, uint32_t frames) {
  float source_pos[4] = {0};
  mat4_transform(pose, source_pos);

  float listener_pos[4] = {0};
  mat4_transform(state.listener, listener_pos);

  float distance = vec3_distance(source_pos, listener_pos);
  float left_ear[4] = {-0.1,0,0,1};
  float right_ear[4] = {0.1,0,0,1};
  mat4_transform(state.listener, left_ear);
  mat4_transform(state.listener, right_ear);
  float ldistance = vec3_distance(source_pos, left_ear);
  float rdistance = vec3_distance(source_pos, right_ear);
  float distance_attenuation = MAX(1.0 - distance/10.0, 0.0);
  float left_attenuation = 0.5 + (rdistance-ldistance)*2.5;
  float right_attenuation = 0.5 + (ldistance-rdistance)*2.5;

  for(int i = 0; i < frames; i++) {
    output[i*2] = input[i] * distance_attenuation * left_attenuation;
    output[i*2+1] = input[i] * distance_attenuation * right_attenuation;
  }
}
void dummy_spatializer_setListenerPose(float position[4], float orientation[4]) {
  mat4_identity(state.listener);
  mat4_translate(state.listener, position[0], position[1], position[2]);
  mat4_rotate(state.listener, orientation[0], orientation[1], orientation[2], orientation[3]);
}
Spatializer dummy_spatializer = {
  dummy_spatializer_init,
  dummy_spatializer_destroy,
  dummy_spatializer_apply,
  dummy_spatializer_setListenerPose,
  "dummy"
};