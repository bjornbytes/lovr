#include "spatializer.h"
#include "core/maf.h"
#include <phonon.h>
#include <stdlib.h>
#include <string.h>

#define PHONON_THREADS 1
#define PHONON_RAYS 4096
#define PHONON_BOUNCES 4
#define PHONON_DIFFUSE_SAMPLES 1024
#define PHONON_OCCLUSION_SAMPLES 32
#define PHONON_MAX_REVERB 1.f
// If this is changed, the scratchpad needs to grow to account for the increase in channels
#define PHONON_AMBISONIC_ORDER 1

#define MONO (IPLAudioFormat) { IPL_CHANNELLAYOUTTYPE_SPEAKERS, IPL_CHANNELLAYOUT_MONO, .channelOrder = IPL_CHANNELORDER_INTERLEAVED }
#define STEREO (IPLAudioFormat) { IPL_CHANNELLAYOUTTYPE_SPEAKERS, IPL_CHANNELLAYOUT_STEREO, .channelOrder = IPL_CHANNELORDER_INTERLEAVED }
#define AMBISONIC (IPLAudioFormat) {\
  .channelLayoutType = IPL_CHANNELLAYOUTTYPE_AMBISONICS,\
  .ambisonicsOrder = PHONON_AMBISONIC_ORDER,\
  .ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN,\
  .ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D,\
  .channelOrder = IPL_CHANNELORDER_DEINTERLEAVED }

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

bool phonon_init() {
  state.library = phonon_dlopen(PHONON_LIBRARY);
  if (!state.library) return false;

  PHONON_FOREACH(PHONON_LOAD)

  IPLerror status;

  status = phonon_iplCreateContext(NULL, NULL, NULL, &state.context);
  if (status != IPL_STATUS_SUCCESS) return phonon_destroy(), false;

  IPLSimulationSettings simulationSettings = { 0 };
  status = phonon_iplCreateEnvironment(state.context, NULL, simulationSettings, NULL, NULL, &state.environment);
  if (status != IPL_STATUS_SUCCESS) return phonon_destroy(), false;

  state.renderingSettings.samplingRate = SAMPLE_RATE;
  state.renderingSettings.frameSize = BUFFER_SIZE;
  state.renderingSettings.convolutionType = IPL_CONVOLUTIONTYPE_PHONON;

  state.scratchpad = malloc(BUFFER_SIZE * 4 * sizeof(float));
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
    .airAbsorptionModel.type = IPL_AIRABSORPTION_EXPONENTIAL,
    .directivity.dipoleWeight = weight,
    .directivity.dipolePower = power
  };

  lovrAudioGetAbsorption(iplSource.airAbsorptionModel.coefficients);

  IPLDirectOcclusionMode occlusion = IPL_DIRECTOCCLUSION_NONE;
  IPLDirectOcclusionMethod volumetric = IPL_DIRECTOCCLUSION_RAYCAST;
  float radius = 0.f;
  IPLint32 rays = 0;

  if (state.mesh && lovrSourceIsEffectEnabled(source, EFFECT_OCCLUSION)) {
    bool transmission = lovrSourceIsEffectEnabled(source, EFFECT_TRANSMISSION);
    occlusion = transmission ? IPL_DIRECTOCCLUSION_TRANSMISSIONBYFREQUENCY : IPL_DIRECTOCCLUSION_NOTRANSMISSION;
    radius = lovrSourceGetRadius(source);

    if (radius > 0.f) {
      volumetric = IPL_DIRECTOCCLUSION_VOLUMETRIC;
      rays = PHONON_OCCLUSION_SAMPLES;
    }
  }

  IPLDirectSoundPath path = phonon_iplGetDirectSoundPath(state.environment, listener, forward, up, iplSource, radius, rays, occlusion, volumetric);

  IPLDirectSoundEffectOptions options = {
    .applyDistanceAttenuation = lovrSourceIsEffectEnabled(source, EFFECT_ATTENUATION) ? IPL_TRUE : IPL_FALSE,
    .applyAirAbsorption = lovrSourceIsEffectEnabled(source, EFFECT_ABSORPTION) ? IPL_TRUE : IPL_FALSE,
    .applyDirectivity = weight > 0.f && power > 0.f ? IPL_TRUE : IPL_FALSE,
    .directOcclusionMode = occlusion
  };

  phonon_iplApplyDirectSoundEffect(state.directSoundEffect[index], in, path, options, tmp);

  float blend = 1.f;
  IPLHrtfInterpolation interpolation = IPL_HRTFINTERPOLATION_NEAREST;
  phonon_iplApplyBinauralEffect(state.binauralEffect[index], state.binauralRenderer, tmp, path.direction, interpolation, blend, out);

  if (state.mesh && lovrSourceIsEffectEnabled(source, EFFECT_REVERB)) {
    phonon_iplSetDryAudioForConvolutionEffect(state.convolutionEffect[index], iplSource, in);
  }

  return frames;
}

uint32_t phonon_tail(float* scratch, float* output, uint32_t frames) {
  if (!state.mesh) return 0;

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

bool phonon_setGeometry(float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial material) {
  if (state.mesh) phonon_iplDestroyStaticMesh(&state.mesh);
  if (state.scene) phonon_iplDestroyScene(&state.scene);
  if (state.environment) phonon_iplDestroyEnvironment(&state.environment);
  if (state.environmentalRenderer) phonon_iplDestroyEnvironment(&state.environmentalRenderer);

  IPLMaterial materials[] = {
    [MATERIAL_GENERIC] = { .10f, .20f, .30f, .05f, .100f, .050f, .030f },
    [MATERIAL_BRICK] = { .03f, .04f, .07f, .05f, .015f, .015f, .015f },
    [MATERIAL_CARPET] = { .24f, .69f, .73f, .05f, .020f, .005f, .003f },
    [MATERIAL_CERAMIC] = { .01f, .02f, .02f, .05f, .060f, .044f, .011f },
    [MATERIAL_CONCRETE] = { .05f, .07f, .08f, .05f, .015f, .002f, .001f },
    [MATERIAL_GLASS] = { .06f, .03f, .02f, .05f, .060f, .044f, .011f },
    [MATERIAL_GRAVEL] = { .60f, .70f, .80f, .05f, .031f, .012f, .008f },
    [MATERIAL_METAL] = { .20f, .07f, .06f, .05f, .200f, .025f, .010f },
    [MATERIAL_PLASTER] = { .12f, .06f, .04f, .05f, .056f, .056f, .004f },
    [MATERIAL_ROCK] = { .13f, .20f, .24f, .05f, .015f, .002f, .001f },
    [MATERIAL_WOOD] = { .11f, .07f, .06f, .05f, .070f, .014f, .005f }
  };

  IPLSimulationSettings settings = (IPLSimulationSettings) {
    .sceneType = IPL_SCENETYPE_PHONON,
    .maxNumOcclusionSamples = PHONON_OCCLUSION_SAMPLES,
    .numRays = PHONON_RAYS,
    .numDiffuseSamples = PHONON_DIFFUSE_SAMPLES,
    .numBounces = PHONON_BOUNCES,
    .numThreads = PHONON_THREADS,
    .irDuration = PHONON_MAX_REVERB,
    .ambisonicsOrder = PHONON_AMBISONIC_ORDER,
    .maxConvolutionSources = MAX_SOURCES,
    .bakingBatchSize = 1,
    .irradianceMinDistance = .1f
  };

  IPLint32* triangleMaterials = malloc(indexCount / 3 * sizeof(IPLint32));
  if (!triangleMaterials) goto fail;

  for (uint32_t i = 0; i < indexCount / 3; i++) {
    triangleMaterials[i] = material;
  }

  IPLerror status;
  status = phonon_iplCreateScene(state.context, NULL, IPL_SCENETYPE_PHONON, 1, materials, NULL, NULL, NULL, NULL, NULL, &state.scene);
  if (status != IPL_STATUS_SUCCESS) goto fail;

  if (vertexCount > 0 && indexCount > 0) {
    status = phonon_iplCreateStaticMesh(state.scene, vertexCount, indexCount / 3, (IPLVector3*) vertices, (IPLTriangle*) indices, triangleMaterials, &state.mesh);
    if (status != IPL_STATUS_SUCCESS) goto fail;
  }

  status = phonon_iplCreateEnvironment(state.context, NULL, settings, state.scene, NULL, &state.environment);
  if (status != IPL_STATUS_SUCCESS) goto fail;

  status = phonon_iplCreateEnvironmentalRenderer(state.context, state.environment, state.renderingSettings, AMBISONIC, NULL, NULL, &state.environmentalRenderer);
  if (status != IPL_STATUS_SUCCESS) goto fail;

  free(triangleMaterials);
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

  if (!state.binauralEffect[index]) {
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
