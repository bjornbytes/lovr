#include "spatializer.h"
#include "core/maf.h"
#include <phonon.h>
#include <stdlib.h>
#include <string.h>

#define MONO (IPLAudioFormat) { IPL_CHANNELLAYOUTTYPE_SPEAKERS, IPL_CHANNELLAYOUT_MONO, .channelOrder = IPL_CHANNELORDER_INTERLEAVED }
#define STEREO (IPLAudioFormat) { IPL_CHANNELLAYOUTTYPE_SPEAKERS, IPL_CHANNELLAYOUT_STEREO, .channelOrder = IPL_CHANNELORDER_INTERLEAVED }
#define AMBISONIC (IPLAudioFormat) { .channelLayoutType = IPL_CHANNELLAYOUTTYPE_AMBISONICS, .ambisonicsOrder = 1, .ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN, .ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D, .channelOrder = IPL_CHANNELORDER_DEINTERLEAVED }

#ifdef _WIN32
#include <windows.h>
#define PHONON_LIBRARY "phonon.dll"
static void* phonon_dlopen(const char* library) { return LoadLibraryA(library); }
static FARPROC phonon_dlsym(void* library, const char* symbol) { return GetProcAddress(library, symbol); }
static void phonon_dlclose(void* library) { FreeLibrary(library); }
#else
#include <dlfcn.h>
#if __APPLE__
#define PHONON_LIBRARY "libphonon.dylib"
#else
#define PHONON_LIBRARY "libphonon.so"
#endif
static void* phonon_dlopen(const char* library) { return dlopen(library, RTLD_NOW | RTLD_LOCAL); }
static void* phonon_dlsym(void* library, const char* symbol) { return dlsym(library, symbol); }
static void phonon_dlclose(void* library) { dlclose(library); }
#endif

typedef IPLerror fn_iplCreateContext(IPLLogFunction logCallback, IPLAllocateFunction allocateCallback, IPLFreeFunction freeCallback, IPLhandle* context);
typedef IPLvoid fn_iplDestroyContext(IPLhandle* context);
typedef IPLvoid fn_iplCleanup(void);
typedef IPLerror fn_iplCreateScene(IPLhandle context, IPLhandle computeDevice, IPLSceneType sceneType, IPLint32 numMaterials, IPLMaterial* materials, IPLClosestHitCallback closestHitCallback, IPLAnyHitCallback anyHitCallback, IPLBatchedClosestHitCallback batchedClosestHitCallback, IPLBatchedAnyHitCallback batchedAnyHitCallback, IPLvoid* userData, IPLhandle* scene);
typedef IPLerror fn_iplDestroyScene(IPLhandle* scene);
typedef IPLvoid fn_iplSaveSceneAsObj(IPLhandle scene, IPLstring fileBaseName);
typedef IPLerror fn_iplCreateStaticMesh(IPLhandle scene, IPLint32 numVertices, IPLint32 numTriangles, IPLVector3* vertices, IPLTriangle* triangles, IPLint32* materialIndices, IPLhandle* staticMesh);
typedef IPLerror fn_iplDestroyStaticMesh(IPLhandle* staticMesh);
typedef IPLerror fn_iplCreateEnvironment(IPLhandle context, IPLhandle computeDevice, IPLSimulationSettings simulationSettings, IPLhandle scene, IPLhandle probeManager, IPLhandle* environment);
typedef IPLvoid fn_iplDestroyEnvironment(IPLhandle* environment);
typedef IPLerror fn_iplCreateEnvironmentalRenderer(IPLhandle context, IPLhandle environment, IPLRenderingSettings renderingSettings, IPLAudioFormat outputFormat, IPLSimulationThreadCreateCallback threadCreateCallback, IPLSimulationThreadDestroyCallback threadDestroyCallback, IPLhandle* renderer);
typedef IPLvoid fn_iplDestroyEnvironmentalRenderer(IPLhandle* renderer);
typedef IPLerror fn_iplCreateDirectSoundEffect(IPLAudioFormat inputFormat, IPLAudioFormat outputFormat, IPLRenderingSettings renderingSettings, IPLhandle* effect);
typedef IPLvoid fn_iplDestroyDirectSoundEffect(IPLhandle* effect);
typedef IPLvoid fn_iplApplyDirectSoundEffect(IPLhandle effect, IPLAudioBuffer inputAudio, IPLDirectSoundPath directSoundPath, IPLDirectSoundEffectOptions options, IPLAudioBuffer outputAudio);
typedef IPLvoid fn_iplFlushDirectSoundEffect(IPLhandle effect);
typedef IPLDirectSoundPath fn_iplGetDirectSoundPath(IPLhandle environment, IPLVector3 listenerPosition, IPLVector3 listenerAhead, IPLVector3 listenerUp, IPLSource source, IPLfloat32 sourceRadius, IPLint32 numSamples, IPLDirectOcclusionMode occlusionMode, IPLDirectOcclusionMethod occlusionMethod);
typedef IPLerror fn_iplCreateBinauralRenderer(IPLhandle context, IPLRenderingSettings renderingSettings, IPLHrtfParams params, IPLhandle* renderer);
typedef IPLvoid fn_iplDestroyBinauralRenderer(IPLhandle* renderer);
typedef IPLerror fn_iplCreateBinauralEffect(IPLhandle renderer, IPLAudioFormat inputFormat, IPLAudioFormat outputFormat, IPLhandle* effect);
typedef IPLvoid fn_iplDestroyBinauralEffect(IPLhandle* effect);
typedef IPLvoid fn_iplApplyBinauralEffect(IPLhandle effect, IPLhandle binauralRenderer, IPLAudioBuffer inputAudio, IPLVector3 direction, IPLHrtfInterpolation interpolation, IPLfloat32 spatialBlend, IPLAudioBuffer outputAudio);
typedef IPLvoid fn_iplFlushBinauralEffect(IPLhandle effect);
typedef IPLerror fn_iplCreateAmbisonicsBinauralEffect(IPLhandle renderer, IPLAudioFormat inputFormat, IPLAudioFormat outputFormat, IPLhandle* effect);
typedef IPLvoid fn_iplDestroyAmbisonicsBinauralEffect(IPLhandle* effect);
typedef IPLvoid fn_iplApplyAmbisonicsBinauralEffect(IPLhandle effect, IPLhandle binauralRenderer, IPLAudioBuffer inputAudio, IPLAudioBuffer outputAudio);
typedef IPLerror fn_iplCreateConvolutionEffect(IPLhandle renderer, IPLBakedDataIdentifier identifier, IPLSimulationType simulationType, IPLAudioFormat inputFormat, IPLAudioFormat outputFormat, IPLhandle* effect);
typedef IPLvoid fn_iplDestroyConvolutionEffect(IPLhandle* effect);
typedef IPLvoid fn_iplSetDryAudioForConvolutionEffect(IPLhandle effect, IPLSource source, IPLAudioBuffer dryAudio);
typedef IPLvoid fn_iplGetWetAudioForConvolutionEffect(IPLhandle effect, IPLVector3 listenerPosition, IPLVector3 listenerAhead, IPLVector3 listenerUp, IPLAudioBuffer wetAudio);
typedef IPLvoid fn_iplGetMixedEnvironmentalAudio(IPLhandle renderer, IPLVector3 listenerPosition, IPLVector3 listenerAhead, IPLVector3 listenerUp, IPLAudioBuffer mixedWetAudio);
typedef IPLvoid fn_iplFlushConvolutionEffect(IPLhandle effect);

#define PHONON_DECLARE(f) static fn_##f* phonon_##f;
#define PHONON_LOAD(f) phonon_##f = (fn_##f*) phonon_dlsym(state.library, #f);
#define PHONON_FOREACH(X)\
  X(iplCreateContext)\
  X(iplDestroyContext)\
  X(iplCleanup)\
  X(iplCreateScene)\
  X(iplDestroyScene)\
  X(iplSaveSceneAsObj)\
  X(iplCreateStaticMesh)\
  X(iplDestroyStaticMesh)\
  X(iplCreateEnvironment)\
  X(iplDestroyEnvironment)\
  X(iplCreateEnvironmentalRenderer)\
  X(iplDestroyEnvironmentalRenderer)\
  X(iplCreateDirectSoundEffect)\
  X(iplDestroyDirectSoundEffect)\
  X(iplApplyDirectSoundEffect)\
  X(iplFlushDirectSoundEffect)\
  X(iplGetDirectSoundPath)\
  X(iplCreateBinauralRenderer)\
  X(iplDestroyBinauralRenderer)\
  X(iplCreateBinauralEffect)\
  X(iplDestroyBinauralEffect)\
  X(iplApplyBinauralEffect)\
  X(iplFlushBinauralEffect)\
  X(iplCreateAmbisonicsBinauralEffect)\
  X(iplDestroyAmbisonicsBinauralEffect)\
  X(iplApplyAmbisonicsBinauralEffect)\
  X(iplCreateConvolutionEffect)\
  X(iplDestroyConvolutionEffect)\
  X(iplSetDryAudioForConvolutionEffect)\
  X(iplGetWetAudioForConvolutionEffect)\
  X(iplGetMixedEnvironmentalAudio)\
  X(iplFlushConvolutionEffect)

PHONON_FOREACH(PHONON_DECLARE)

static struct {
  void* library;
  IPLhandle context;
  IPLhandle scene;
  IPLhandle mesh;
  IPLhandle environment;
  IPLhandle environmentalRenderer;
  IPLhandle binauralRenderer;
  IPLhandle ambisonicsBinauralEffect;
  IPLhandle binauralEffect[MAX_SOURCES];
  IPLhandle directSoundEffect[MAX_SOURCES];
  IPLhandle convolutionEffect[MAX_SOURCES];
  IPLRenderingSettings renderingSettings;
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

  state.renderingSettings.samplingRate = config.sampleRate;
  state.renderingSettings.frameSize = config.fixedBufferSize;
  state.renderingSettings.convolutionType = IPL_CONVOLUTIONTYPE_PHONON;

  state.scratchpad = malloc(config.fixedBufferSize * 4 * sizeof(float));
  if (!state.scratchpad) return phonon_destroy(), false;

  IPLHrtfParams hrtfParams = {
    .type = IPL_HRTFDATABASETYPE_DEFAULT
  };

  status = phonon_iplCreateBinauralRenderer(state.context, state.renderingSettings, hrtfParams, &state.binauralRenderer);
  if (status != IPL_STATUS_SUCCESS) return phonon_destroy(), false;

  status = phonon_iplCreateAmbisonicsBinauralEffect(state.binauralRenderer, AMBISONIC, STEREO, &state.ambisonicsBinauralEffect);
  if (status != IPL_STATUS_SUCCESS) return phonon_destroy(), false;

  return true;
}

void phonon_destroy() {
  if (state.scratchpad) free(state.scratchpad);
  for (size_t i = 0; i < MAX_SOURCES; i++) {
    if (state.binauralEffect[i]) phonon_iplDestroyBinauralEffect(&state.binauralEffect[i]);
    if (state.directSoundEffect[i]) phonon_iplDestroyDirectSoundEffect(&state.directSoundEffect[i]);
    if (state.convolutionEffect[i]) phonon_iplDestroyConvolutionEffect(&state.convolutionEffect[i]);
  }
  if (state.ambisonicsBinauralEffect) phonon_iplDestroyAmbisonicsBinauralEffect(&state.ambisonicsBinauralEffect);
  if (state.binauralRenderer) phonon_iplDestroyBinauralRenderer(&state.binauralRenderer);
  if (state.environmentalRenderer) phonon_iplDestroyEnvironmentalRenderer(&state.environmentalRenderer);
  if (state.environment) phonon_iplDestroyEnvironment(&state.environment);
  if (state.mesh) phonon_iplDestroyStaticMesh(&state.mesh);
  if (state.scene) phonon_iplDestroyStaticMesh(&state.scene);
  if (state.context) phonon_iplDestroyContext(&state.context);
  phonon_iplCleanup();
  phonon_dlclose(state.library);
  memset(&state, 0, sizeof(state));
}

uint32_t phonon_apply(Source* source, const float* input, float* output, uint32_t frames, uint32_t why) {
  IPLAudioBuffer in = { .format = MONO, .numSamples = frames, .interleavedBuffer = (float*) input };
  IPLAudioBuffer tmp = { .format = MONO, .numSamples = frames, .interleavedBuffer = state.scratchpad };
  IPLAudioBuffer out = { .format = STEREO, .numSamples = frames, .interleavedBuffer = output };

  uint32_t index = lovrSourceGetIndex(source);

  float x[4], y[4], z[4];
  vec3_set(y, 0.f, 1.f, 0.f);
  vec3_set(z, 0.f, 0.f, -1.f);
  quat_rotate(state.listenerOrientation, y);
  quat_rotate(state.listenerOrientation, z);
  IPLVector3 listener = { state.listenerPosition[0], state.listenerPosition[1], state.listenerPosition[2] };
  IPLVector3 forward = { z[0], z[1], z[2] };
  IPLVector3 up = { y[0], y[1], y[2] };

  // TODO maybe this should use a matrix
  float position[4], orientation[4];
  lovrSourceGetPose(source, position, orientation);
  vec3_set(x, 1.f, 0.f, 0.f);
  vec3_set(y, 0.f, 1.f, 0.f);
  vec3_set(z, 0.f, 0.f, -1.f);
  quat_rotate(orientation, x);
  quat_rotate(orientation, y);
  quat_rotate(orientation, z);

  float weight, power;
  lovrSourceGetDirectivity(source, &weight, &power);

  IPLSource iplSource = {
    .position = (IPLVector3) { position[0], position[1], position[2] },
    .ahead = (IPLVector3) { z[0], z[1], z[2] },
    .up = (IPLVector3) { y[0], y[1], y[2] },
    .right = (IPLVector3) { x[0], x[1], x[2] },
    .directivity.dipoleWeight = weight,
    .directivity.dipolePower = power
  };

  IPLDirectOcclusionMode occlusion = IPL_DIRECTOCCLUSION_NONE;
  IPLDirectOcclusionMethod volumetric = IPL_DIRECTOCCLUSION_RAYCAST;
  float radius = 0.f;
  IPLint32 rays = 0;

  if (lovrSourceIsOcclusionEnabled(source)) {
    occlusion = lovrSourceIsTransmissionEnabled(source) ? IPL_DIRECTOCCLUSION_TRANSMISSIONBYFREQUENCY : IPL_DIRECTOCCLUSION_NOTRANSMISSION;
    radius = lovrSourceGetRadius(source);

    if (radius > 0.f) {
      volumetric = IPL_DIRECTOCCLUSION_VOLUMETRIC;
      rays = 32;
    }
  }

  IPLDirectSoundPath path = phonon_iplGetDirectSoundPath(state.environment, listener, forward, up, iplSource, radius, rays, occlusion, volumetric);

  IPLDirectSoundEffectOptions options = {
    .applyDistanceAttenuation = lovrSourceIsFalloffEnabled(source) ? IPL_TRUE : IPL_FALSE,
    .applyAirAbsorption = lovrSourceIsAbsorptionEnabled(source) ? IPL_TRUE : IPL_FALSE,
    .applyDirectivity = weight > 0.f && power > 0.f ? IPL_TRUE : IPL_FALSE,
    .directOcclusionMode = occlusion
  };

  phonon_iplApplyDirectSoundEffect(state.directSoundEffect[index], in, path, options, tmp);

  if (lovrSourceIsShared(source)) {
    memset(output, 0, frames * 2 * sizeof(float));
    // ambisonic panning effect
  } else {
    float blend = lovrSourceGetSpatialBlend(source);
    IPLHrtfInterpolation interpolation = lovrSourceGetInterpolation(source) == SOURCE_BILINEAR ? IPL_HRTFINTERPOLATION_BILINEAR : IPL_HRTFINTERPOLATION_NEAREST;
    phonon_iplApplyBinauralEffect(state.binauralEffect[index], state.binauralRenderer, tmp, path.direction, interpolation, blend, out);
  }

  if (lovrSourceIsReverbEnabled(source)) {
    phonon_iplSetDryAudioForConvolutionEffect(state.convolutionEffect[index], iplSource, in);
  }

  return frames;
}

uint32_t phonon_tail(float* scratch, float* output, uint32_t frames) {
  IPLAudioBuffer out = { .format = STEREO, .numSamples = frames, .interleavedBuffer = output };

  IPLAudioBuffer tmp = {
    .format = AMBISONIC,
    .numSamples = frames,
    .deinterleavedBuffer = (float*[4]) {
      state.scratchpad + frames * 0,
      state.scratchpad + frames * 1,
      state.scratchpad + frames * 2,
      state.scratchpad + frames * 3
    }
  };

  float y[4], z[4];
  vec3_set(y, 0.f, 1.f, 0.f);
  vec3_set(z, 0.f, 0.f, -1.f);
  quat_rotate(state.listenerOrientation, y);
  quat_rotate(state.listenerOrientation, z);
  IPLVector3 listener = { state.listenerPosition[0], state.listenerPosition[1], state.listenerPosition[2] };
  IPLVector3 forward = { z[0], z[1], z[2] };
  IPLVector3 up = { y[0], y[1], y[2] };

  memset(state.scratchpad, 0, 4 * frames * sizeof(float));
  phonon_iplGetMixedEnvironmentalAudio(state.environmentalRenderer, listener, forward, up, tmp);
  phonon_iplApplyAmbisonicsBinauralEffect(state.ambisonicsBinauralEffect, state.binauralRenderer, tmp, out);
  return frames;
}

void phonon_setListenerPose(float position[4], float orientation[4]) {
  memcpy(state.listenerPosition, position, sizeof(state.listenerPosition));
  memcpy(state.listenerOrientation, orientation, sizeof(state.listenerOrientation));
}

bool phonon_setGeometry(float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount) {
  if (state.mesh) phonon_iplDestroyStaticMesh(&state.mesh);
  if (state.scene) phonon_iplDestroyScene(&state.scene);
  if (state.environment) phonon_iplDestroyEnvironment(&state.environment);
  if (state.environmentalRenderer) phonon_iplDestroyEnvironment(&state.environmentalRenderer);

  IPLMaterial material = (IPLMaterial) {
    .lowFreqAbsorption = .2f,
    .midFreqAbsorption = .07f,
    .highFreqAbsorption = .06f,
    .scattering = .05f,
    .lowFreqTransmission = .2f,
    .midFreqTransmission = .025f,
    .highFreqTransmission = .01f
  };

  IPLSimulationSettings settings = (IPLSimulationSettings) {
    .sceneType = IPL_SCENETYPE_PHONON,
    .maxNumOcclusionSamples = 32,
    .numRays = 4096,
    .numDiffuseSamples = 1024,
    .numBounces = 8,
    .numThreads = 1,
    .irDuration = 1.f,
    .ambisonicsOrder = 1,
    .maxConvolutionSources = MAX_SOURCES,
    .bakingBatchSize = 1,
    .irradianceMinDistance = .1f
  };

  IPLint32* materials = malloc(indexCount / 3 * sizeof(IPLint32));
  if (!materials) goto fail;
  memset(materials, 0, indexCount / 3 * sizeof(IPLint32));

  IPLerror status;
  status = phonon_iplCreateScene(state.context, NULL, IPL_SCENETYPE_PHONON, 1, &material, NULL, NULL, NULL, NULL, NULL, &state.scene);
  if (status != IPL_STATUS_SUCCESS) goto fail;

  status = phonon_iplCreateStaticMesh(state.scene, vertexCount, indexCount / 3, (IPLVector3*) vertices, (IPLTriangle*) indices, materials, &state.mesh);
  if (status != IPL_STATUS_SUCCESS) goto fail;

  status = phonon_iplCreateEnvironment(state.context, NULL, settings, state.scene, NULL, &state.environment);
  if (status != IPL_STATUS_SUCCESS) goto fail;

  status = phonon_iplCreateEnvironmentalRenderer(state.context, state.environment, state.renderingSettings, AMBISONIC, NULL, NULL, &state.environmentalRenderer);
  if (status != IPL_STATUS_SUCCESS) goto fail;

  free(materials);
  return true;

fail:
  free(materials);
  if (state.mesh) phonon_iplDestroyStaticMesh(&state.mesh);
  if (state.scene) phonon_iplDestroyScene(&state.scene);
  if (state.environment) phonon_iplDestroyEnvironment(&state.environment);
  if (state.environmentalRenderer) phonon_iplDestroyEnvironmentalRenderer(&state.environmentalRenderer);
  phonon_iplCreateEnvironment(state.context, NULL, settings, NULL, NULL, &state.environment);
  phonon_iplCreateEnvironmentalRenderer(state.context, state.environment, state.renderingSettings, AMBISONIC, NULL, NULL, &state.environmentalRenderer);
  return false;
}

void phonon_sourceCreate(Source* source) {
  uint32_t index = lovrSourceGetIndex(source);

  if (!state.binauralEffect[index] && !lovrSourceIsShared(source)) {
    phonon_iplCreateBinauralEffect(state.binauralRenderer, MONO, STEREO, &state.binauralEffect[index]);
  }

  if (!state.directSoundEffect[index]) {
    phonon_iplCreateDirectSoundEffect(MONO, MONO, state.renderingSettings, &state.directSoundEffect[index]);
  }

  if (!state.convolutionEffect[index]) {
    IPLBakedDataIdentifier id = { 0 };
    phonon_iplCreateConvolutionEffect(state.environmentalRenderer, id, IPL_SIMTYPE_REALTIME, MONO, AMBISONIC, &state.convolutionEffect[index]);
  }
}

void phonon_sourceDestroy(Source* source) {
  uint32_t index = lovrSourceGetIndex(source);
  if (state.binauralEffect[index]) phonon_iplFlushBinauralEffect(state.binauralEffect[index]);
  if (state.directSoundEffect[index]) phonon_iplFlushDirectSoundEffect(state.directSoundEffect[index]);
  if (state.convolutionEffect[index]) phonon_iplFlushConvolutionEffect(state.convolutionEffect[index]);
}

Spatializer phononSpatializer = {
  .init = phonon_init,
  .destroy = phonon_destroy,
  .apply = phonon_apply,
  .tail = phonon_tail,
  .setListenerPose = phonon_setListenerPose,
  .setGeometry = phonon_setGeometry,
  .sourceCreate = phonon_sourceCreate,
  .sourceDestroy = phonon_sourceDestroy,
  .name = "phonon"
};
