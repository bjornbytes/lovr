#include "spatializer.h"
#include "core/maf.h"
#include <phonon.h>
#include <stdlib.h>
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
typedef IPLerror fn_iplCreateBinauralRenderer(IPLhandle context, IPLRenderingSettings renderingSettings, IPLHrtfParams params, IPLhandle* renderer);
typedef IPLvoid fn_iplDestroyBinauralRenderer(IPLhandle* renderer);
typedef IPLerror fn_iplCreateBinauralEffect(IPLhandle renderer, IPLAudioFormat inputFormat, IPLAudioFormat outputFormat, IPLhandle* effect);
typedef IPLvoid fn_iplDestroyBinauralEffect(IPLhandle* effect);
typedef IPLvoid fn_iplApplyBinauralEffect(IPLhandle effect, IPLhandle binauralRenderer, IPLAudioBuffer inputAudio, IPLVector3 direction, IPLHrtfInterpolation interpolation, IPLfloat32 spatialBlend, IPLAudioBuffer outputAudio);

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
  X(iplGetDirectSoundPath)\
  X(iplCreateBinauralRenderer)\
  X(iplDestroyBinauralRenderer)\
  X(iplCreateBinauralEffect)\
  X(iplDestroyBinauralEffect)\
  X(iplApplyBinauralEffect)

PHONON_FOREACH(PHONON_DECLARE)

static struct {
  void* library;
  IPLhandle context;
  IPLhandle environment;
  IPLhandle directSoundEffect;
  IPLhandle binauralRenderer;
  IPLhandle binauralEffect;
  float listenerPosition[4];
  float listenerOrientation[4];
  float* scratchpad;
} state;

static void phonon_destroy(void);

bool phonon_init(SpatializerConfig config) {
  state.library = phonon_dlopen(PHONON_LIBRARY);
  if (!state.library) return false;

  PHONON_FOREACH(PHONON_LOAD)

  IPLerror status;

  status = phonon_iplCreateContext(NULL, NULL, NULL, &state.context);
  if (status != IPL_STATUS_SUCCESS) return phonon_destroy(), false;

  IPLSimulationSettings simulationSettings = { 0 };
  status = phonon_iplCreateEnvironment(state.context, NULL, simulationSettings, NULL, NULL, &state.environment);
  if (status != IPL_STATUS_SUCCESS) return phonon_destroy(), false;

  IPLAudioFormat mono = {
    .channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS,
    .channelLayout = IPL_CHANNELLAYOUT_MONO,
    .channelOrder = IPL_CHANNELORDER_INTERLEAVED
  };

  IPLAudioFormat stereo = {
    .channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS,
    .channelLayout = IPL_CHANNELLAYOUT_STEREO,
    .channelOrder = IPL_CHANNELORDER_INTERLEAVED
  };

  IPLRenderingSettings renderingSettings = {
    .samplingRate = config.sampleRate,
    .frameSize = config.fixedBufferSize
  };

  state.scratchpad = malloc(config.fixedBufferSize * sizeof(float));
  if (!state.scratchpad) return phonon_destroy(), false;

  status = phonon_iplCreateDirectSoundEffect(mono, mono, renderingSettings, &state.directSoundEffect);
  if (status != IPL_STATUS_SUCCESS) return phonon_destroy(), false;

  IPLHrtfParams hrtfParams = {
    .type = IPL_HRTFDATABASETYPE_DEFAULT
  };

  status = phonon_iplCreateBinauralRenderer(state.context, renderingSettings, hrtfParams, &state.binauralRenderer);
  if (status != IPL_STATUS_SUCCESS) return phonon_destroy(), false;

  status = phonon_iplCreateBinauralEffect(state.binauralRenderer, mono, stereo, &state.binauralEffect);
  if (status != IPL_STATUS_SUCCESS) return phonon_destroy(), false;

  return true;
}

void phonon_destroy() {
  if (state.scratchpad) free(state.scratchpad);
  if (state.binauralEffect) phonon_iplDestroyBinauralEffect(&state.binauralEffect);
  if (state.binauralRenderer) phonon_iplDestroyBinauralRenderer(&state.binauralRenderer);
  if (state.directSoundEffect) phonon_iplDestroyDirectSoundEffect(&state.directSoundEffect);
  if (state.environment) phonon_iplDestroyEnvironment(&state.environment);
  if (state.context) phonon_iplDestroyContext(&state.context);
  phonon_iplCleanup();
  phonon_dlclose(state.library);
  memset(&state, 0, sizeof(state));
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

  float absorption[3];
  lovrSourceGetAbsorption(source, absorption);
  float dipoleWeight;
  float dipolePower;
  lovrSourceGetDirectivity(source, &dipoleWeight, &dipolePower);
  float falloff = lovrSourceGetFalloff(source);

  IPLSource iplSource = {
    .position = (IPLVector3) { position[0], position[1], position[2] },
    .ahead = (IPLVector3) { forward[0], forward[1], forward[2] },
    .up = (IPLVector3) { up[0], up[1], up[2] },
    .right = (IPLVector3) { right[0], right[1], right[2] },
    .directivity.dipoleWeight = dipoleWeight,
    .directivity.dipolePower = dipolePower,
    .distanceAttenuationModel.type = IPL_DISTANCEATTENUATION_INVERSEDISTANCE,
    .distanceAttenuationModel.minDistance = falloff,
    .airAbsorptionModel.type = IPL_AIRABSORPTION_EXPONENTIAL,
    .airAbsorptionModel.coefficients = { absorption[0], absorption[1], absorption[2] }
  };

  IPLDirectOcclusionMode occlusionMode = IPL_DIRECTOCCLUSION_NONE;
  IPLDirectOcclusionMethod occlusionMethod = IPL_DIRECTOCCLUSION_RAYCAST;
  IPLDirectSoundPath path = phonon_iplGetDirectSoundPath(state.environment, listenerPosition, listenerForward, listenerUp, iplSource, 0.f, 1, occlusionMode, occlusionMethod);

  IPLDirectSoundEffectOptions options = {
    .applyDistanceAttenuation = falloff > 0.f ? IPL_TRUE : IPL_FALSE,
    .applyAirAbsorption = (absorption[0] > 0.f || absorption[1] > 0.f || absorption[2] > 0.f) ? IPL_TRUE : IPL_FALSE,
    .applyDirectivity = dipoleWeight > 0.f ? IPL_TRUE : IPL_FALSE,
    .directOcclusionMode = occlusionMode
  };

  IPLAudioFormat mono = {
    .channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS,
    .channelLayout = IPL_CHANNELLAYOUT_MONO,
    .channelOrder = IPL_CHANNELORDER_INTERLEAVED
  };

  IPLAudioFormat stereo = {
    .channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS,
    .channelLayout = IPL_CHANNELLAYOUT_STEREO,
    .channelOrder = IPL_CHANNELORDER_INTERLEAVED
  };

  IPLAudioBuffer inputBuffer = {
    .format = mono,
    .numSamples = frames,
    .interleavedBuffer = (float*) input
  };

  IPLAudioBuffer scratchBuffer = {
    .format = mono,
    .numSamples = frames,
    .interleavedBuffer = state.scratchpad
  };

  IPLAudioBuffer outputBuffer = {
    .format = stereo,
    .numSamples = frames,
    .interleavedBuffer = output
  };

  IPLHrtfInterpolation interpolation = IPL_HRTFINTERPOLATION_NEAREST;
  float spatialBlend = 1.f;

  phonon_iplApplyDirectSoundEffect(state.directSoundEffect, inputBuffer, path, options, scratchBuffer);
  phonon_iplApplyBinauralEffect(state.binauralEffect, state.binauralRenderer, scratchBuffer, path.direction, interpolation, spatialBlend, outputBuffer);
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
