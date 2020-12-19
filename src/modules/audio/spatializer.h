#include "audio.h"
#include "core/maf.h"

typedef struct {
	int maxSourcesHint;
	int fixedBuffer;
	int sampleRate;
} SpatializerConfigIn;

typedef struct {
	bool needFixedBuffer;
} SpatializerConfigOut;

typedef struct {
  // return true on success
  bool (*init)(SpatializerConfigIn configIn, SpatializerConfigOut *configOut);
  void (*destroy)(void);

  // input is mono, output is interleaved stereo, framesIn is mono frames, framesOut is stereo frames.
  // Safe to assume framesIn == framesOut unless spatializer requests needFixedBuffer.
  // Return value is number of samples written into output.
  uint32_t (*apply)(Source* source, const float* input, float* output, uint32_t framesIn, uint32_t framesOut);
  // called at end of frame for any "additional noise", like echo.
  // output is stereo, frames is stereo frames, scratch is a buffer the length of output (in case that helps)
  // return value is number of stereo frames written.
  uint32_t (*tail)(float *scratch, float* output, uint32_t frames);

  void (*setListenerPose)(float position[4], float orientation[4]);

  void (*sourceCreate)(Source *source);
  void (*sourceDestroy)(Source *source);

  const char *name;
} Spatializer;

bool dummy_spatializer_init(SpatializerConfigIn configIn, SpatializerConfigOut *configOut);
void dummy_spatializer_destroy(void);
uint32_t dummy_spatializer_source_apply(Source* source, const float* input, float* output, uint32_t framesIn, uint32_t framesOut);
uint32_t dummy_spatializer_tail(float *scratch, float* output, uint32_t frames);
void dummy_spatializer_setListenerPose(float position[4], float orientation[4]);
void dummy_spatializer_source_create(Source *);
void dummy_spatializer_source_destroy(Source *);
extern Spatializer dummySpatializer;

#ifdef LOVR_ENABLE_OCULUS_AUDIO
extern Spatializer oculusSpatializer;
#endif