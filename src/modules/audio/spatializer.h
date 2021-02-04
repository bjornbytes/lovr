#include "audio.h"
#include "core/maf.h"

typedef struct {
  int maxSourcesHint;
  int fixedBuffer;
  int sampleRate;
} SpatializerConfig;

typedef struct {
  // return true on success
  bool (*init)(SpatializerConfig config);
  void (*destroy)(void);

  // input is mono, output is interleaved stereo, framesIn is mono frames, framesOut is stereo frames.
  // Safe to assume framesIn == framesOut unless spatializer requests needFixedBuffer.
  // Return value is number of samples written into output.
  uint32_t (*apply)(Source* source, const float* input, float* output, uint32_t framesIn, uint32_t framesOut);
  // called at end of frame for any "additional noise", like echo.
  // output is stereo, frames is stereo frames, scratch is a buffer the length of output (in case that helps)
  // return value is number of stereo frames written.
  uint32_t (*tail)(float* scratch, float* output, uint32_t frames);

  void (*setListenerPose)(float position[4], float orientation[4]);

  void (*sourceCreate)(Source* source);
  void (*sourceDestroy)(Source* source);

  bool buffered;
  const char* name;
} Spatializer;

extern Spatializer simpleSpatializer;
#ifdef LOVR_ENABLE_OCULUS_AUDIO
extern Spatializer oculusSpatializer;
#endif
