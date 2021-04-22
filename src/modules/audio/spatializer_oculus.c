#include "spatializer.h"
#include "audio/audio.h"
#include "core/util.h"
#include "lib/miniaudio/miniaudio.h"
#include <stdlib.h>
#include <string.h>

//////// Just the definition of a pose from OVR_CAPI.h. Lets OVR_Audio work right.
#ifndef OVR_CAPI_h
#define OVR_CAPI_h
#if !defined(OVR_UNUSED_STRUCT_PAD)
    #define OVR_UNUSED_STRUCT_PAD(padName, size) char padName[size];
#endif

#if !defined(OVR_ALIGNAS)
    #if defined(__GNUC__) || defined(__clang__)
        #define OVR_ALIGNAS(n) __attribute__((aligned(n)))
    #elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
        #define OVR_ALIGNAS(n) __declspec(align(n))
    #elif defined(__CC_ARM)
        #define OVR_ALIGNAS(n) __align(n)
    #else
        #error Need to define OVR_ALIGNAS
    #endif
#endif

/// A quaternion rotation.
typedef struct OVR_ALIGNAS(4) ovrQuatf_
{
    float x, y, z, w;
} ovrQuatf;

/// A 2D vector with float components.
typedef struct OVR_ALIGNAS(4) ovrVector2f_
{
    float x, y;
} ovrVector2f;

/// A 3D vector with float components.
typedef struct OVR_ALIGNAS(4) ovrVector3f_
{
    float x, y, z;
} ovrVector3f;

/// A 4x4 matrix with float elements.
typedef struct OVR_ALIGNAS(4) ovrMatrix4f_
{
    float M[4][4];
} ovrMatrix4f;


/// Position and orientation together.
typedef struct OVR_ALIGNAS(4) ovrPosef_
{
    ovrQuatf     Orientation;
    ovrVector3f  Position;
} ovrPosef;

/// A full pose (rigid body) configuration with first and second derivatives.
///
/// Body refers to any object for which ovrPoseStatef is providing data.
/// It can be the HMD, Touch controller, sensor or something else. The context
/// depends on the usage of the struct.
typedef struct OVR_ALIGNAS(8) ovrPoseStatef_
{
    ovrPosef     ThePose;               ///< Position and orientation.
    ovrVector3f  AngularVelocity;       ///< Angular velocity in radians per second.
    ovrVector3f  LinearVelocity;        ///< Velocity in meters per second.
    ovrVector3f  AngularAcceleration;   ///< Angular acceleration in radians per second per second.
    ovrVector3f  LinearAcceleration;    ///< Acceleration in meters per second per second.
    OVR_UNUSED_STRUCT_PAD(pad0, 4)      ///< \internal struct pad.
    double       TimeInSeconds;         ///< Absolute time that this pose refers to. \see ovr_GetTimeInSeconds
} ovrPoseStatef;
#endif //////// end OVR_CAPI_h

#include <OVR_Audio.h>

///////// This function is a copypaste from OVR_Audio.h. They accidentally declared this function as "inline" instead of "static inline", so the body is ignored in C. I guess they only tested in C++.
static inline ovrResult pseudo_ovrAudio_GetReflectionBands(ovrAudioMaterialPreset Preset, AudioBands Bands)
{
    if (Preset >= ovrAudioMaterialPreset_COUNT || Bands == NULL)
    {
        return ovrError_AudioInvalidParam;
    }

    switch (Preset)
    {
    case ovrAudioMaterialPreset_AcousticTile:
        Bands[0] = 0.488168418f;
        Bands[1] = 0.361475229f;
        Bands[2] = 0.339595377f;
        Bands[3] = 0.498946249f;
        break;
    case ovrAudioMaterialPreset_Brick:
        Bands[0] = 0.975468814f;
        Bands[1] = 0.972064495f;
        Bands[2] = 0.949180186f;
        Bands[3] = 0.930105388f;
        break;
    case ovrAudioMaterialPreset_BrickPainted:
        Bands[0] = 0.975710571f;
        Bands[1] = 0.983324170f;
        Bands[2] = 0.978116691f;
        Bands[3] = 0.970052719f;
        break;
    case ovrAudioMaterialPreset_Carpet:
        Bands[0] = 0.987633705f;
        Bands[1] = 0.905486643f;
        Bands[2] = 0.583110571f;
        Bands[3] = 0.351053834f;
        break;
    case ovrAudioMaterialPreset_CarpetHeavy:
        Bands[0] = 0.977633715f;
        Bands[1] = 0.859082878f;
        Bands[2] = 0.526479602f;
        Bands[3] = 0.370790422f;
        break;
    case ovrAudioMaterialPreset_CarpetHeavyPadded:
        Bands[0] = 0.910534739f;
        Bands[1] = 0.530433178f;
        Bands[2] = 0.294055820f;
        Bands[3] = 0.270105422f;
        break;
    case ovrAudioMaterialPreset_CeramicTile:
        Bands[0] = 0.990000010f;
        Bands[1] = 0.990000010f;
        Bands[2] = 0.982753932f;
        Bands[3] = 0.980000019f;
        break;
    case ovrAudioMaterialPreset_Concrete:
        Bands[0] = 0.990000010f;
        Bands[1] = 0.983324170f;
        Bands[2] = 0.980000019f;
        Bands[3] = 0.980000019f;
        break;
    case ovrAudioMaterialPreset_ConcreteRough:
        Bands[0] = 0.989408433f;
        Bands[1] = 0.964494646f;
        Bands[2] = 0.922127008f;
        Bands[3] = 0.900105357f;
        break;
    case ovrAudioMaterialPreset_ConcreteBlock:
        Bands[0] = 0.635267377f;
        Bands[1] = 0.652230680f;
        Bands[2] = 0.671053469f;
        Bands[3] = 0.789051592f;
        break;
    case ovrAudioMaterialPreset_ConcreteBlockPainted:
        Bands[0] = 0.902957916f;
        Bands[1] = 0.940235913f;
        Bands[2] = 0.917584062f;
        Bands[3] = 0.919947326f;
        break;
    case ovrAudioMaterialPreset_Curtain:
        Bands[0] = 0.686494231f;
        Bands[1] = 0.545859993f;
        Bands[2] = 0.310078561f;
        Bands[3] = 0.399473131f;
        break;
    case ovrAudioMaterialPreset_Foliage:
        Bands[0] = 0.518259346f;
        Bands[1] = 0.503568292f;
        Bands[2] = 0.578688800f;
        Bands[3] = 0.690210819f;
        break;
    case ovrAudioMaterialPreset_Glass:
        Bands[0] = 0.655915797f;
        Bands[1] = 0.800631821f;
        Bands[2] = 0.918839693f;
        Bands[3] = 0.923488140f;
        break;
    case ovrAudioMaterialPreset_GlassHeavy:
        Bands[0] = 0.827098966f;
        Bands[1] = 0.950222731f;
        Bands[2] = 0.974604130f;
        Bands[3] = 0.980000019f;
        break;
    case ovrAudioMaterialPreset_Grass:
        Bands[0] = 0.881126285f;
        Bands[1] = 0.507170796f;
        Bands[2] = 0.131893098f;
        Bands[3] = 0.0103688836f;
        break;
    case ovrAudioMaterialPreset_Gravel:
        Bands[0] = 0.729294717f;
        Bands[1] = 0.373122454f;
        Bands[2] = 0.255317450f;
        Bands[3] = 0.200263441f;
        break;
    case ovrAudioMaterialPreset_GypsumBoard:
        Bands[0] = 0.721240044f;
        Bands[1] = 0.927690148f;
        Bands[2] = 0.934302270f;
        Bands[3] = 0.910105407f;
        break;
    case ovrAudioMaterialPreset_PlasterOnBrick:
        Bands[0] = 0.975696504f;
        Bands[1] = 0.979106009f;
        Bands[2] = 0.961063504f;
        Bands[3] = 0.950052679f;
        break;
    case ovrAudioMaterialPreset_PlasterOnConcreteBlock:
        Bands[0] = 0.881774724f;
        Bands[1] = 0.924773932f;
        Bands[2] = 0.951497555f;
        Bands[3] = 0.959947288f;
        break;
    case ovrAudioMaterialPreset_Soil:
        Bands[0] = 0.844084203f;
        Bands[1] = 0.634624243f;
        Bands[2] = 0.416662872f;
        Bands[3] = 0.400000036f;
        break;
    case ovrAudioMaterialPreset_SoundProof:
        Bands[0] = 0.000000000f;
        Bands[1] = 0.000000000f;
        Bands[2] = 0.000000000f;
        Bands[3] = 0.000000000f;
        break;
    case ovrAudioMaterialPreset_Snow:
        Bands[0] = 0.532252669f;
        Bands[1] = 0.154535770f;
        Bands[2] = 0.0509644151f;
        Bands[3] = 0.0500000119f;
        break;
    case ovrAudioMaterialPreset_Steel:
        Bands[0] = 0.793111682f;
        Bands[1] = 0.840140402f;
        Bands[2] = 0.925591767f;
        Bands[3] = 0.979736567f;
        break;
    case ovrAudioMaterialPreset_Water:
        Bands[0] = 0.970588267f;
        Bands[1] = 0.971753478f;
        Bands[2] = 0.978309572f;
        Bands[3] = 0.970052719f;
        break;
    case ovrAudioMaterialPreset_WoodThin:
        Bands[0] = 0.592423141f;
        Bands[1] = 0.858273327f;
        Bands[2] = 0.917242289f;
        Bands[3] = 0.939999998f;
        break;
    case ovrAudioMaterialPreset_WoodThick:
        Bands[0] = 0.812957883f;
        Bands[1] = 0.895329595f;
        Bands[2] = 0.941304684f;
        Bands[3] = 0.949947298f;
        break;
    case ovrAudioMaterialPreset_WoodFloor:
        Bands[0] = 0.852366328f;
        Bands[1] = 0.898992121f;
        Bands[2] = 0.934784114f;
        Bands[3] = 0.930052698f;
        break;
    case ovrAudioMaterialPreset_WoodOnConcrete:
        Bands[0] = 0.959999979f;
        Bands[1] = 0.941232264f;
        Bands[2] = 0.937923789f;
        Bands[3] = 0.930052698f;
        break;
    default:
        Bands[0] = 0.000000000f;
        Bands[1] = 0.000000000f;
        Bands[2] = 0.000000000f;
        Bands[3] = 0.000000000f;
        return ovrError_AudioInvalidParam;
    };

    return ovrSuccess;
}
//////// End fake OVR_Audio.h

#define E(f, a) { ovrResult rrresultz = (f a); if ( rrresultz != ovrSuccess ) { lovrLogRaw("Oculus Spatializer: %s failed (%d)\n", #f, rrresultz); } }

typedef struct {
  Source* source;
  bool usedSourceThisPlayback; // If true source was non-NULL at some point between midPlayback going high and tail()
  bool occupied; // If true either source->playing or Oculus Audio is doing an echo tailoff
} SourceRecord;

struct {
  ovrAudioContext context;
  SourceRecord sources[MAX_SOURCES];

  int sourceCount; // Number of active sources seen this playback
  int occupiedCount; // Number of sources+tailoffs seen this playback (ie strictly gte sourceCount)
  bool midPlayback; // An onPlayback callback is in progress

  bool poseUpdated; // setListenerPose has been called since the last playback
  ovrPoseStatef pose;
  ma_mutex poseLock; // Using ma_mutex in case holding a lovr lock inside a ma lock is weird
  bool poseLockInited;

  ovrAudioGeometry geometry;
  bool haveGeometry;
  bool haveBox;
} state;

static bool oculus_init(void) {
  if (!state.poseLockInited) {
    int mutexStatus = ma_mutex_init(&state.poseLock);
    lovrAssert(mutexStatus == MA_SUCCESS, "Failed to create audio mutex");
    state.poseLockInited = true;
  }

  // Initialize Oculus
  ovrAudioContextConfiguration config = { 0 };

  config.acc_Size = sizeof(config);
  config.acc_MaxNumSources = MAX_SOURCES;
  config.acc_SampleRate = SAMPLE_RATE;
  config.acc_BufferLength = BUFFER_SIZE; // Stereo

  if (ovrAudio_CreateContext(&state.context, &config) != ovrSuccess) {
    return false;
  }

  return true;
}

static void oculus_destroy(void) {
  ovrAudio_DestroyContext(state.context);
  ma_mutex_uninit(&state.poseLock);
  memset(&state, 0, sizeof(state));
}

static uint32_t oculus_apply(Source* source, const float* input, float* output, uint32_t framesIn, uint32_t framesOut) {
  if (!state.midPlayback) { // Run this code only on the first Source of a playback
    state.midPlayback = true;

    for (int idx = 0; idx < MAX_SOURCES; idx++) { // Clear presence tracking and get starting positions
      SourceRecord* record = &state.sources[idx];
      record->usedSourceThisPlayback = false;

      if (record->source) {
        state.sourceCount++;
      }

      if (record->occupied) {
        state.occupiedCount++;
      }
    }

    if (state.poseUpdated) {
      { // Tell Oculus Audio where the headset is
        ovrPoseStatef pose;

        ma_mutex_lock(&state.poseLock); // Do nothing inside lock but make a copy of the pose
        memcpy(&pose, &state.pose, sizeof(pose));
        state.poseUpdated = false;
        ma_mutex_unlock(&state.poseLock);

        E(ovrAudio_SetListenerPoseStatef, (state.context, &pose)); // Upload pose
      }
      state.poseUpdated = false;
    }
  }

  intptr_t* spatializerMemo = lovrSourceGetSpatializerMemoField(source);

  // Lovr allows for an unlimited number of simultaneous sources but OculusAudio makes us predeclare a limit.
  // We maintain a list of sources and keep the index each source is associated with in its memo field.
  // So that spatializers don't need to be notified of pauses and unpauses, we assign fields anew each onPlayback call.
  int idx = *spatializerMemo;

  // This source had a record, but we gave it away.
  if (idx >= 0 && state.sources[idx].source != source) {
    idx = *spatializerMemo = -1;
  }

  // This source doesn't have a record. If it's playing, try to assign it one.
  // If there are no free source records, we will simply not play the sound,
  // but if there's a record which is only playing a tail, in *that* case we will override the tail.
  if (idx < 0 && lovrSourceIsPlaying(source)) {
    if (state.occupiedCount < MAX_SOURCES) { // There's an empty slot
      for (idx = 0; idx < MAX_SOURCES; idx++) {
        if (!state.sources[idx].occupied) { // Claim the first unoccupied slot
          break;
        }
      }
    } else if (state.sourceCount < MAX_SOURCES) { // There's a slot doing a tail
      for (idx = 0; idx < MAX_SOURCES; idx++) {
        if (!state.sources[idx].occupied && !state.sources[idx].usedSourceThisPlayback) { // Does OculusAudio allow reusing indexes within a playback? Let's guess no for now.
          break;
        }
      }
    }

    if (idx >= 0) { // Successfully assigned
      *spatializerMemo = idx;
      state.sourceCount++;
      state.occupiedCount++;
      state.sources[idx].source = source;
      state.sources[idx].occupied = true;
      E(ovrAudio_ResetAudioSource, (state.context, idx));
    }
  }

  // This source has (or was just assigned) a record.
  if (idx >= 0) {
    uint32_t outStatus = 0;
    state.sources[idx].usedSourceThisPlayback = true;

    float position[4], orientation[4];
    lovrSourceGetPose(source, position, orientation);

    E(ovrAudio_SetAudioSourcePos, (state.context, idx, position[0], position[1], position[2]));

    E(ovrAudio_SpatializeMonoSourceInterleaved, (state.context, idx, &outStatus, output, input));

    if (!lovrSourceIsPlaying(source)) { // Source is finished
      state.sources[idx].source = NULL;
      *spatializerMemo = -1;
      if (outStatus & ovrAudioSpatializationStatus_Finished) { // Source done playing, echo tailoff is done
        state.sources[idx].occupied = false;
      }
    }
    return framesOut;
  }
  return 0;
}

static uint32_t oculus_tail(float* scratch, float* output, uint32_t frames) {
  memset(output, 0, frames*sizeof(float)*2); // FIXME: Any way to tell if SharedReverbLR will do anything ahead of time?
  uint32_t outStatus = 0;
  for (int idx = 0; idx < MAX_SOURCES; idx++) {
    // If a sound is finished, feed in NULL input on its index until reverb tail completes.
    if (state.sources[idx].occupied && !state.sources[idx].usedSourceThisPlayback) {
      E(ovrAudio_SpatializeMonoSourceInterleaved, (state.context, idx, &outStatus, scratch, NULL));
      if (outStatus & ovrAudioSpatializationStatus_Finished) {
        state.sources[idx].occupied = false;
      }
      for (unsigned int i = 0; i < frames * 2; i++) {
        output[i] += scratch[i];
      }
    }
  }
  if (state.haveGeometry || state.haveBox) { // MixInSharedReverb only works after reverb enabled
    E(ovrAudio_MixInSharedReverbInterleaved, (state.context, &outStatus, output));
  }
  //lovrLogRaw("Spatializer test OUTSTATUS %d\n", outStatus);
  return frames;
}

// Oculus math primitives

static void oculusUnpackQuat(ovrQuatf* oq, float* lq) {
  oq->x = lq[0]; oq->y = lq[1]; oq->z = lq[2]; oq->w = lq[3];
}

static void oculusUnpackVec(ovrVector3f* ov, float* p) {
  ov->x = p[0]; ov->y = p[1]; ov->z = p[2];
}

static void oculusRecreatePose(ovrPoseStatef* out, float position[4], float orientation[4]) {
  ovrPosef pose;
  oculusUnpackVec(&pose.Position, position);
  oculusUnpackQuat(&pose.Orientation, orientation);
  out->ThePose = pose;
  float zero[4] = { 0 }; // TODO
  oculusUnpackVec(&out->AngularVelocity, zero);
  oculusUnpackVec(&out->LinearVelocity, zero);
  oculusUnpackVec(&out->AngularAcceleration, zero);
  oculusUnpackVec(&out->LinearAcceleration, zero);
  out->TimeInSeconds = 0; //TODO-OS
}

static void oculus_setListenerPose(float position[4], float orientation[4]) {
  ovrPoseStatef pose;

  oculusRecreatePose(&pose, position, orientation);

  ma_mutex_lock(&state.poseLock); // Do nothing inside lock but make a copy of the pose
  memcpy(&state.pose, &pose, sizeof(state.pose));
  state.poseUpdated = true;
  ma_mutex_unlock(&state.poseLock);
}

static ovrAudioMaterialPreset lovrToOvrMaterial(AudioMaterial material) {
  switch (material) {
    case MATERIAL_GENERIC: default: return ovrAudioMaterialPreset_AcousticTile;
    case MATERIAL_BRICK: return ovrAudioMaterialPreset_Brick;
    case MATERIAL_CARPET: return ovrAudioMaterialPreset_Carpet;
    case MATERIAL_CERAMIC: return ovrAudioMaterialPreset_CeramicTile;
    case MATERIAL_CONCRETE: return ovrAudioMaterialPreset_Concrete;
    case MATERIAL_GLASS: return ovrAudioMaterialPreset_Glass;
    case MATERIAL_GRAVEL: return ovrAudioMaterialPreset_Gravel;
    case MATERIAL_METAL: return ovrAudioMaterialPreset_Steel;
    case MATERIAL_PLASTER: return ovrAudioMaterialPreset_PlasterOnConcreteBlock;
    case MATERIAL_ROCK: return ovrAudioMaterialPreset_WoodOnConcrete;
    case MATERIAL_WOOD: return ovrAudioMaterialPreset_WoodThick;
  }
}

// FIXME: This could be much simplified by removing the value printouts
static bool oculus_reverbEnable(bool simple, bool late, bool random) {
  ovrResult result;
  #define REVERB_ENABLES 3
  ovrAudioEnable enables[REVERB_ENABLES] = {ovrAudioEnable_SimpleRoomModeling, ovrAudioEnable_LateReverberation, ovrAudioEnable_RandomizeReverb};
  const char *names[REVERB_ENABLES] = {"ovrAudioEnable_SimpleRoomModeling", "ovrAudioEnable_LateReverberation", "ovrAudioEnable_RandomizeReverb"};
  bool values[REVERB_ENABLES] = {simple, late, random};
  for(int c = 0; c < REVERB_ENABLES; c++) {  
    result = ovrAudio_Enable(state.context, enables[c], values[c]);
    if (result != ovrSuccess) {
      lovrLogRaw("Oculus Spatializer: SET ENABLED %s FAILED (%d)\n", names[c], result);
      return 1;
    }
  }
  return false;
}

static bool oculus_setGeometry(float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial material, GeometryMode mode) {
  // No lock because audio.h holds a lock when this is called
  if (state.haveGeometry) {
    E(ovrAudio_DestroyAudioGeometry, (state.geometry));
    state.haveGeometry = false;
  }
  state.haveBox = false;

  ovrResult result;

  if (mode == GEOMETRY_DISABLE) {
    if (!oculus_reverbEnable(false, false, true))
      return false;
    lovrLogRaw("Oculus Spatializer: Disabled geometry");
    return true;
  } else if (mode == GEOMETRY_BOX) {
    AudioBands materialBands;
    pseudo_ovrAudio_GetReflectionBands(lovrToOvrMaterial(material), materialBands);

    float minVertex[3], maxVertex[3]; // Compute AABB
    bool anyVertices = false;
    for(int indexIndex = 0; indexIndex < indexCount; indexIndex++) {
      int index = indices[indexIndex]*3;
      if (!anyVertices) {
        for(int offset = 0; offset < 3; offset++) {
          minVertex[offset] = maxVertex[offset] = vertices[index+offset];
        }
        anyVertices = true;
      } else {
        for(int offset = 0; offset < 3; offset++) {
          float value = vertices[index+offset];
          if (value < minVertex[offset]) minVertex[offset] = value;
          if (value > maxVertex[offset]) maxVertex[offset] = value;
        }
      }
    }

    ovrAudioVector3f center = {(minVertex[0]+maxVertex[0])/2,
                          (minVertex[1]+maxVertex[1])/2,
                          (minVertex[2]+maxVertex[2])/2};

    ovrAudioAdvancedBoxRoomParameters parameters;
    parameters.abrp_Size = sizeof(ovrAudioAdvancedBoxRoomParameters);
    memcpy(parameters.abrp_ReflectLeft, materialBands, sizeof(AudioBands));
    memcpy(parameters.abrp_ReflectRight, materialBands, sizeof(AudioBands));
    memcpy(parameters.abrp_ReflectUp, materialBands, sizeof(AudioBands));
    memcpy(parameters.abrp_ReflectDown, materialBands, sizeof(AudioBands));
    memcpy(parameters.abrp_ReflectBehind, materialBands, sizeof(AudioBands));
    memcpy(parameters.abrp_ReflectFront, materialBands, sizeof(AudioBands));
    parameters.abrp_Width = maxVertex[0]-minVertex[0];
    parameters.abrp_Height = maxVertex[1]-minVertex[1];
    parameters.abrp_Depth = maxVertex[2]-minVertex[2];
    parameters.abrp_LockToListenerPosition = 0;
    parameters.abrp_RoomPosition = center;

    result = ovrAudio_SetAdvancedBoxRoomParameters(state.context, &parameters);

    if (result != ovrSuccess) {
      lovrLogRaw("Oculus Spatializer: CREATE GEOMETRY FAILED (%d)\n", result);
      return false;
    }
    
    if (!oculus_reverbEnable(true, true, true))
      return false;

    state.haveBox = true;

    lovrLogRaw("Oculus Spatializer: Set box-model geometry success");

    return true;
  } else if (mode == GEOMETRY_MESH) {
    result = ovrAudio_CreateAudioGeometry(state.context, &state.geometry);
    if (result != ovrSuccess) {
      lovrLogRaw("Oculus Spatializer: CREATE GEOMETRY FAILED (%d)\n", result);
      return false;
    }

    state.haveGeometry = true;

    // see OVR_Audio_Propagation.h
    ovrAudioMeshGroup meshGroup = {0, indexCount/3, ovrAudioFaceType_Triangles, NULL}; // All fields except material

    result = ovrAudio_AudioGeometryUploadMeshArrays(state.geometry,
      vertices, 0, vertexCount, sizeof(float)*3, ovrAudioScalarType_Float32,
      indices, 0, indexCount, ovrAudioScalarType_UInt32,
      &meshGroup, 1);

    if (result != ovrSuccess) {
      lovrLogRaw("Oculus Spatializer: UPLOAD GEOMETRY FAILED (%d)\n", result);
      return false;
    }

    if (!oculus_reverbEnable(true, true, true))
      return false;

    lovrLogRaw("Oculus Spatializer: Set geometry success");

    return true;
  }

  return false;
}

static void oculus_sourceCreate(Source* source) {
  intptr_t* spatializerMemo = lovrSourceGetSpatializerMemoField(source);
  *spatializerMemo = -1;
}

static void oculus_sourceDestroy(Source *source) {
  intptr_t* spatializerMemo = lovrSourceGetSpatializerMemoField(source);
  if (*spatializerMemo >= 0) {
    state.sources[*spatializerMemo].source = NULL;
  }
}

Spatializer oculusSpatializer = {
  .init = oculus_init,
  .destroy = oculus_destroy,
  .apply = oculus_apply,
  .tail = oculus_tail,
  .setListenerPose = oculus_setListenerPose,
  .setGeometry = oculus_setGeometry,
  .sourceCreate = oculus_sourceCreate,
  .sourceDestroy = oculus_sourceDestroy, // Need noop
  .name = "oculus"
};
