#include "audio/audio.h"
#include "audio/spatializer.h"
#include "lib/miniaudio/miniaudio.h"
#include "oculus_spatializer_math_shim.h"
#include "OVR_Audio.h"
#include <stdlib.h>

typedef struct {
  Source* source;
  bool usedSourceThisPlayback; // If true source was non-NULL at some point between midPlayback going high and tail()
  bool occupied; // If true either source->playing or Oculus Audio is doing an echo tailoff
} SourceRecord;

struct {
  uint32_t sampleRate;

  ovrAudioContext context;
  SourceRecord *sources;
  int sourceMax;   // Maximum allowed simultaneous sources

  int sourceCount; // Number of active sources seen this playback
  int occupiedCount; // Number of sources+tailoffs seen this playback (ie strictly gte sourceCount)
  bool midPlayback; // An onPlayback callback is in progress

  bool poseUpdated; // setListenerPose has been called since the last playback
  ovrPoseStatef pose;
  ma_mutex poseLock; // Using ma_mutex in case holding a lovr lock inside a ma lock is weird
  bool poseLockInited;
} state;

static bool oculus_spatializer_init(SpatializerConfigIn configIn, SpatializerConfigOut *configOut)
{
  // Initialize own state
  state.sampleRate = configIn.sampleRate;
  configOut->needFixedBuffer = true;
  state.sourceMax = configIn.maxSourcesHint;
  state.sources = calloc(state.sourceMax, sizeof(SourceRecord));

  if (!state.poseLockInited) {
    int mutexStatus = ma_mutex_init(&state.poseLock);
    lovrAssert(mutexStatus == MA_SUCCESS, "Failed to create audio mutex");
    state.poseLockInited = true;
  }

  // Initialize Oculus
  ovrAudioContextConfiguration config = {0};

  config.acc_Size = sizeof( config );
  config.acc_MaxNumSources = state.sourceMax;
  config.acc_SampleRate = state.sampleRate;
  config.acc_BufferLength = configIn.fixedBuffer; // Stereo

  if ( ovrAudio_CreateContext( &state.context, &config ) != ovrSuccess )
  {
    return false;
  }

  return true;
}
static void oculus_spatializer_destroy(void)
{
  free(state.sources);
  state.sources = NULL;
}
static uint32_t oculus_spatializer_source_apply(Source* source, const float* input, float* output, uint32_t framesIn, uint32_t framesOut) {
  if (!state.midPlayback) { // Run this code only on the first Source of a playback
    state.midPlayback = true;

    for(int idx = 0; idx < state.sourceMax; idx++) { // Clear presence tracking and get starting positions
      SourceRecord *record = &state.sources[idx];
      record->usedSourceThisPlayback = false;
      if (record->source)
        state.sourceCount++;
      if (record->occupied)
        state.occupiedCount++;
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

  intptr_t *spatializerMemo = lovrSourceGetSpatializerMemoField(source);

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
    if (state.occupiedCount < state.sourceMax) { // There's an empty slot
      for (idx = 0; idx < state.sourceMax; idx++)
        if (!state.sources[idx].occupied) // Claim the first unoccupied slot
          break;
    } else if (state.sourceCount < state.sourceMax) { // There's a slot doing a tail
      for (idx = 0; idx < state.sourceMax; idx++)
        if (!state.sources[idx].occupied // Claim the first unsourced slot
         && !state.sources[idx].usedSourceThisPlayback) // Does OculusAudio allow reusing indexes within a playback? Let's guess no for now.
          break;
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

static uint32_t oculus_spatializer_tail(float *scratch, float* output, uint32_t frames) {
  bool didAnything = false;
  for (int idx = 0; idx < state.sourceMax; idx++) {
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
      for(unsigned int i = 0; i < frames*2; i++) {
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
static void oculusUnpackVec(ovrVector3f* ov, float *p) {
  ov->x = p[0]; ov->y = p[1]; ov->z = p[2];
}

static void oculusRecreatePose(ovrPoseStatef *out, float position[4], float orientation[4]) {
  ovrPosef pose;
  oculusUnpackVec(&pose.Position, position);
  oculusUnpackQuat(&pose.Orientation, orientation);
  out->ThePose = pose;
  float zero[4] = {0}; // TODO
  oculusUnpackVec(&out->AngularVelocity, zero);
  oculusUnpackVec(&out->LinearVelocity, zero);
  oculusUnpackVec(&out->AngularAcceleration, zero);
  oculusUnpackVec(&out->LinearAcceleration, zero);
  out->TimeInSeconds = 0; //TODO-OS
}

static void oculus_spatializer_setListenerPose(float position[4], float orientation[4]) {
  ovrPoseStatef pose;

  oculusRecreatePose(&pose, position, orientation);

  ma_mutex_lock(&state.poseLock); // Do nothing inside lock but make a copy of the pose
  memcpy(&state.pose, &pose, sizeof(state.pose));
  state.poseUpdated = true;
  ma_mutex_unlock(&state.poseLock);
}
static void oculus_spatializer_source_create(Source *source) {
  intptr_t *spatializerMemo = lovrSourceGetSpatializerMemoField(source);
  *spatializerMemo = -1;
}
static void oculus_spatializer_source_destroy(Source *source) {
  intptr_t *spatializerMemo = lovrSourceGetSpatializerMemoField(source);
  if (*spatializerMemo >= 0)
    state.sources[*spatializerMemo].source = NULL;
}
Spatializer oculusSpatializer = {
  oculus_spatializer_init,
  oculus_spatializer_destroy,
  oculus_spatializer_source_apply,
  oculus_spatializer_tail,
  oculus_spatializer_setListenerPose,
  oculus_spatializer_source_create,
  oculus_spatializer_source_destroy, // Need noop
  "oculus"
};