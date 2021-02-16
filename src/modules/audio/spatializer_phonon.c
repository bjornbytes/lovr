#include "spatializer.h"
#include "core/maf.h"
#include <phonon.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define PHONON_LIBRARY "phonon.dll"
static void* phonon_dlopen(const char* library) { return LoadLibraryA(library); }
static FARPROC phonon_dlsym(void* library, const char* symbol) { return GetProcAddress(library, symbol); }
static void phonon_dlclose(void* library) { FreeLibrary(library); }
#else
#include <dlfcn.h>
#define PHONON_LIBRARY "libphonon.so"
static void* phonon_dlopen(const char* library) { return dlopen(library, RTLD_NOW | RTLD_LOCAL); }
static void* phonon_dlsym(void* library, const char* symbol) { return dlsym(library, symbol); }
static void phonon_dlclose(void* library) { dlclose(library); }
#endif

typedef IPLerror fn_iplCreateContext(IPLLogFunction logCallback, IPLAllocateFunction allocateCallback, IPLFreeFunction freeCallback, IPLhandle* context);
typedef IPLvoid fn_iplDestroyContext(IPLhandle* context);
typedef IPLvoid fn_iplCleanup(void);
typedef IPLerror fn_iplCreateEnvironment(IPLhandle context, IPLhandle computeDevice, IPLSimulationSettings simulationSettings, IPLhandle scene, IPLhandle probeManager, IPLhandle* environment);
typedef IPLvoid fn_iplDestroyEnvironment(IPLhandle* environment);
typedef IPLerror fn_iplCreateDirectSoundEffect(IPLAudioFormat inputFormat, IPLAudioFormat outputFormat, IPLRenderingSettings renderingSettings, IPLhandle* effect);
typedef IPLvoid fn_iplDestroyDirectSoundEffect(IPLhandle* effect);
typedef IPLvoid fn_iplApplyDirectSoundEffect(IPLhandle effect, IPLAudioBuffer inputAudio, IPLDirectSoundPath directSoundPath, IPLDirectSoundEffectOptions options, IPLAudioBuffer outputAudio);
typedef IPLDirectSoundPath fn_iplGetDirectSoundPath(IPLhandle environment, IPLVector3 listenerPosition, IPLVector3 listenerAhead, IPLVector3 listenerUp, IPLSource source, IPLfloat32 sourceRadius, IPLint32 numSamples, IPLDirectOcclusionMode occlusionMode, IPLDirectOcclusionMethod occlusionMethod);

#define PHONON_DECLARE(f) static fn_##f* phonon_##f;
#define PHONON_LOAD(f) phonon_##f = (fn_##f*) phonon_dlsym(state.library, #f);
#define PHONON_FOREACH(X)\
  X(iplCreateContext)\
  X(iplDestroyContext)\
  X(iplCleanup)\
  X(iplCreateEnvironment)\
  X(iplDestroyEnvironment)\
  X(iplCreateDirectSoundEffect)\
  X(iplDestroyDirectSoundEffect)\
  X(iplApplyDirectSoundEffect)\
  X(iplGetDirectSoundPath)

PHONON_FOREACH(PHONON_DECLARE)

static struct {
  void* library;
  IPLhandle context;
  IPLhandle environment;
  IPLhandle directSoundEffect;
  float listenerPosition[4];
  float listenerOrientation[4];
} state;

static void phonon_destroy(void);

bool phonon_init(SpatializerConfig config) {
  state.library = phonon_dlopen(PHONON_LIBRARY);
  if (!state.library) return false;

  PHONON_FOREACH(PHONON_LOAD)

  IPLerror status;

  status = phonon_iplCreateContext(NULL, NULL, NULL, &state.context);
  if (status != IPL_STATUS_SUCCESS) {
    phonon_destroy();
    return false;
  }

  IPLSimulationSettings simulationSettings = { 0 };
  status = phonon_iplCreateEnvironment(state.context, NULL, simulationSettings, NULL, NULL, &state.environment);
  if (status != IPL_STATUS_SUCCESS) {
    phonon_destroy();
    return false;
  }

  IPLAudioFormat inputFormat = {
    .channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS,
    .channelLayout = IPL_CHANNELLAYOUT_MONO,
    .channelOrder = IPL_CHANNELORDER_INTERLEAVED
  };

  IPLAudioFormat outputFormat = {
    .channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS,
    .channelLayout = IPL_CHANNELLAYOUT_STEREO,
    .channelOrder = IPL_CHANNELORDER_INTERLEAVED
  };

  IPLRenderingSettings renderingSettings = {
    .samplingRate = config.sampleRate,
    .frameSize = config.fixedBufferSize
  };

  status = phonon_iplCreateDirectSoundEffect(inputFormat, outputFormat, renderingSettings, &state.directSoundEffect);
  if (status != IPL_STATUS_SUCCESS) {
    phonon_destroy();
    return false;
  }

  return true;
}

void phonon_destroy() {
  if (state.directSoundEffect) phonon_iplDestroyDirectSoundEffect(&state.directSoundEffect);
  if (state.environment) phonon_iplDestroyEnvironment(&state.environment);
  if (state.context) phonon_iplDestroyContext(&state.context);
  phonon_iplCleanup();
  phonon_dlclose(state.library);
}

uint32_t phonon_apply(Source* source, const float* input, float* output, uint32_t frames, uint32_t why) {
  float forward[4] = { 0.f, 0.f, -1.f };
  float up[4] = { 0.f, 1.f, 0.f };

  quat_rotate(state.listenerOrientation, forward);
  quat_rotate(state.listenerOrientation, up);

  IPLVector3 listenerPosition = { state.listenerPosition[0], state.listenerPosition[1], state.listenerPosition[2] };
  IPLVector3 listenerForward = { forward[0], forward[1], forward[2] };
  IPLVector3 listenerUp = { up[0], up[1], up[2] };

  // Using a matrix may be more effective here?
  float position[4], orientation[4], right[4];
  lovrSourceGetPose(source, position, orientation);
  vec3_set(forward, 0.f, 0.f, -1.f);
  vec3_set(up, 0.f, 1.f, 0.f);
  vec3_set(right, 1.f, 0.f, 0.f);
  quat_rotate(orientation, forward);
  quat_rotate(orientation, up);
  quat_rotate(orientation, right);

  IPLSource iplSource = {
    .position = (IPLVector3) { position[0], position[1], position[2] },
    .ahead = (IPLVector3) { forward[0], forward[1], forward[2] },
    .up = (IPLVector3) { up[0], up[1], up[2] },
    .right = (IPLVector3) { right[0], right[1], right[2] },
    .directivity = {
      .dipoleWeight = 0.f,
      .dipolePower = 0.f
    },
    .distanceAttenuationModel.type = IPL_DISTANCEATTENUATION_DEFAULT,
    .airAbsorptionModel.type = IPL_AIRABSORPTION_DEFAULT
  };

  IPLDirectOcclusionMode occlusionMode = IPL_DIRECTOCCLUSION_NONE;
  IPLDirectOcclusionMethod occlusionMethod = IPL_DIRECTOCCLUSION_RAYCAST;
  IPLDirectSoundPath path = phonon_iplGetDirectSoundPath(state.environment, listenerPosition, listenerForward, listenerUp, iplSource, 0.f, 1, occlusionMode, occlusionMethod);

  IPLDirectSoundEffectOptions options = {
    .applyDistanceAttenuation = IPL_TRUE,
    .applyAirAbsorption = IPL_TRUE,
    .applyDirectivity = IPL_TRUE,
    .directOcclusionMode = occlusionMode
  };

  IPLAudioFormat format = {
    .channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS,
    .channelLayout = IPL_CHANNELLAYOUT_MONO,
    .channelOrder = IPL_CHANNELORDER_INTERLEAVED
  };

  IPLAudioBuffer inputBuffer = {
    .format = format,
    .numSamples = frames,
    .interleavedBuffer = (float*) input
  };

  IPLAudioBuffer outputBuffer = {
    .format = format,
    .numSamples = frames,
    .interleavedBuffer = output
  };

  phonon_iplApplyDirectSoundEffect(state.directSoundEffect, inputBuffer, path, options, outputBuffer);
  return frames;
}

uint32_t phonon_tail(float* scratch, float* output, uint32_t frames) {
  return 0;
}

// TODO lock/transact
void phonon_setListenerPose(float position[4], float orientation[4]) {
  memcpy(state.listenerPosition, position, sizeof(state.listenerPosition));
  memcpy(state.listenerOrientation, orientation, sizeof(state.listenerOrientation));
}

void phonon_sourceCreate(Source* source) {
  //
}

void phonon_sourceDestroy(Source* source) {
  //
}

Spatializer phononSpatializer = {
  .init = phonon_init,
  .destroy = phonon_destroy,
  .apply = phonon_apply,
  .tail = phonon_tail,
  .setListenerPose = phonon_setListenerPose,
  .sourceCreate = phonon_sourceCreate,
  .sourceDestroy = phonon_sourceDestroy,
  .name = "phonon"
};
