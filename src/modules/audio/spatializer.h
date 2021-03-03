#include "audio.h"

// Private Source functions for spatializer use
intptr_t* lovrSourceGetSpatializerMemoField(Source* source);
uint32_t lovrSourceGetIndex(Source* source);

typedef struct {
  bool (*init)(void);
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
  bool (*setGeometry)(float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial material);
  void (*sourceCreate)(Source* source);
  void (*sourceDestroy)(Source* source);
  const char* name;
} Spatializer;

#ifdef LOVR_ENABLE_PHONON
extern Spatializer phononSpatializer;
#endif
#ifdef LOVR_ENABLE_OCULUS_AUDIO
extern Spatializer oculusSpatializer;
#endif
extern Spatializer simpleSpatializer;
