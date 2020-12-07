#include "audio.h"
#include "core/maf.h"

typedef struct {
  bool (*init)(void);
  void (*destroy)(void);
  void (*apply)(Source* source, mat4 transform, const float* input /*mono*/, float* output/*stereo*/, uint32_t frames);
  void (*setListenerPose)(float position[4], float orientation[4]);
  const char *name;
} Spatializer;

bool dummy_spatializer_init(void);
void dummy_spatializer_destroy(void);
void dummy_spatializer_apply(Source* source, mat4 transform, const float* input, float* output, uint32_t frames);
extern Spatializer dummySpatializer;