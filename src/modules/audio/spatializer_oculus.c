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

        ovrAudio_SetListenerPoseStatef(state.context, &pose); // Upload pose
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
      ovrAudio_ResetAudioSource(state.context, idx);
    }
  }

  // This source has (or was just assigned) a record.
  if (idx >= 0) {
    uint32_t outStatus = 0;
    state.sources[idx].usedSourceThisPlayback = true;

    float position[4], orientation[4];
    lovrSourceGetPose(source, position, orientation);

    ovrAudio_SetAudioSourcePos(state.context, idx, position[0], position[1], position[2]);

    ovrAudio_SpatializeMonoSourceInterleaved(state.context, idx, &outStatus, output, input);

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
  bool didAnything = false;
  for (int idx = 0; idx < MAX_SOURCES; idx++) {
    // If a sound is finished, feed in NULL input on its index until reverb tail completes.
    if (state.sources[idx].occupied && !state.sources[idx].usedSourceThisPlayback) {
      uint32_t outStatus = 0;
      if (!didAnything) {
        didAnything = true;
        memset(output, 0, frames*sizeof(float)*2);
      }
      ovrAudio_SpatializeMonoSourceInterleaved(state.context, idx, &outStatus, scratch, NULL);
      if (outStatus & ovrAudioSpatializationStatus_Finished) {
        state.sources[idx].occupied = false;
      }
      for (unsigned int i = 0; i < frames * 2; i++) {
        output[i] += scratch[i];
      }
    }
  }
  return didAnything ? frames : 0;
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

bool oculus_setGeometry(float* vertices, uint32_t* indices, uint32_t vertexCount, uint32_t indexCount, AudioMaterial material) {
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
