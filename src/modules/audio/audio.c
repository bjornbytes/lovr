#include "audio/audio.h"
#include "audio/source.h"
#include "modules/data/soundData.h"
#include "core/arr.h"
#include "core/maf.h"
#include "core/ref.h"
#include "util.h"
#include "lib/miniaudio/miniaudio.h"
#include <string.h>
#include <stdlib.h>

#ifdef LOVR_DEBUG_AUDIOTAP
// To get a record of what the audio callback is playing, define LOVR_DEBUG_AUDIOTAP,
// after running look in the lovr save directory for lovrDebugAudio.raw, and open as raw 32-bit floats
// Audacity (or Amadeus, on mac) can do this
#include "modules/filesystem/file.h"
#endif

// On some platforms if we request a callback length of N miniaudio will tend to actually give us callbacks of size N/3 because of "periods"
#define CALLBACK_PERIODS 3
#define BUFFER_LENGTH 128
#define CALLBACK_LENGTH (BUFFER_LENGTH*CALLBACK_PERIODS)

#ifdef LOVR_ENABLE_OCULUS_AUDIO
#define LOVR_REGULARIZE_CALLBACK_LENGTH
#endif

typedef arr_t(float) float_arr_t;

static struct {
  bool initialized;
  Source* sources; // When using Oculus Audio we keep two, the normal linked list, and a fixed size array of only the sources assigned to an Oculus Audio ID
  ma_device device;
  ma_mutex lock;

  float_arr_t decodeBuffer; // Temporary scratch space to perform Source decoding into.
#ifndef LOVR_ENABLE_OCULUS_AUDIO
  float_arr_t accumulateBuffer; // Temporary scratch space to perform mixing in.
  float residue; // Leftover residue from the previous call to handler().
#endif
#ifdef LOVR_DEBUG_AUDIOTAP
  File audiotapFile;
  bool audiotapWriting;
#endif
} state;

// Ensure a float_arr_t is of the correct size and return its data pointer.
// FIXME: For safety, this probably should force align to 16 bytes when Oculus Audio is in use
static float* getReservedBuffer(float_arr_t* a, int length, int channel) {
  length *= channel;
  arr_reserve(a, length);
  a->length = length;
  return a->data;
}

// Add N channels to N channels
static void lovrAudioMix(float* output, float* input, int frames, int channels) {
  frames *= channels;
  for (int f = 0; f < frames; f++) {
      output[f] += input[f];
  }
}

// Copy mono channel to N interleaved channels
static void lovrAudioUpmix(float* output, float* input, int frames, int channels) {
  for (int f = 0, o = 0; f < frames; f++) {
    for (int c = 0; c < channels; c++, o++) {
      output[o] = input[f];
    }
  }
}

// Straight copy audio data
static void lovrAudioCopy(float* output, float* input, int frames, int channels) {
  memcpy(output, input, frames*channels*sizeof(float));
}

// Zero an audio buffer
static void lovrAudioZero(float* output, int frames, int channels) {
  memset(output, 0, frames*channels*sizeof(float));
}

// Mix into a *mono* stream an inaudible repair for a sample residue (popping noise at the end of a sample).
// The "residue" parameter should be a single float sample. The value afterward will be what still remains.
// FIXME: This is a linear falloff. Something like a x*x*(3 - 2*x) falloff would reduce the chance of audible harmonics.
static const float linearFalloff = 1.f/2205.f; // At 44100hz this will produce a 10hz sawtooth wave shape
static void lovrAudioFalloffMix(float* output, float* residue, int frames) {
  int f = 0;
  if (isnan(*residue)) { *residue = 0.f; return; } // Ensure errors in decode do not break the pop correction permanently
  float sign = *residue < 0 ? -1.f : 1.f;
  *residue = fabs(*residue);
  if (*residue > 1) *residue = 1;       // Assume miniaudio clamps to -1..1
  while (*residue > 0 && f < frames) {
    *residue -= linearFalloff;
    if (*residue <= 0) {
      *residue = 0;
      break;
    }
    output[f] += *residue * sign;
    f++;
  }
  if (*residue > 0) *residue *= sign;
}

#ifdef LOVR_ENABLE_OCULUS_AUDIO

#include "headset/oculus_math_shim.h"
#include "OVR_Audio.h"
#include "headset/oculus_mobile.h"

#define LOVR_SOURCE_MAX 16 // If this increases, change spatializerId size in Source

const static uint32_t lovrOvrFlags = 0; //ovrAudioSourceFlag_DirectTimeOfArrival;

static struct {
  uint32_t sampleRate;
  uint32_t bufferSize;
  ovrAudioContext context;
  struct {
    Source* source;
    bool occupied; // If true either source->playing or Oculus Audio is doing an echo tailoff
  } sources[LOVR_SOURCE_MAX];
  int sourceCount; // Number of active sources
  int occupiedCount; // Number of current sources+tailoffs
  float_arr_t unpackBuffer; // Temporary scratch space for storing Oculus Audio's output
} oastate;

static bool lovrSpatializerInit(float sampleRate) {
  oastate.sampleRate = sampleRate;
  return false;
}

static bool lovrSpatializerRealInit(uint32_t bufferSize) {
  ovrAudioContextConfiguration config = {};

  config.acc_Size = sizeof( config );
  config.acc_MaxNumSources = LOVR_SOURCE_MAX;
  config.acc_SampleRate = oastate.sampleRate;
  config.acc_BufferLength = bufferSize;

  if ( ovrAudio_CreateContext( &oastate.context, &config ) != ovrSuccess )
  {
    return true;
  }

  oastate.bufferSize = bufferSize;

  return false;
}

#else
// "Dummy" spatializer ignores position
static bool lovrSpatializerInit(float sampleRate) { return false; }
#endif

static void handler(ma_device* device, void* output, const void* input, uint32_t frames) {
  ma_mutex_lock(&state.lock);

  float* decode = getReservedBuffer(&state.decodeBuffer, frames, 1);

#ifdef LOVR_ENABLE_OCULUS_AUDIO
  float* unpack = getReservedBuffer(&oastate.unpackBuffer, frames, 2);
  lovrAudioZero(output, frames, 2); // This path assumes output is stereo since that's what we request below.

  { // Tell Oculus Audio where the headset is
    ovrPoseStatef pose;
    lovrOculusMobileRecreatePose(&pose);
    ovrAudio_SetListenerPoseStatef(oastate.context, &pose);
  }

  for(int idx = 0; idx < LOVR_SOURCE_MAX; idx++) { // Do one pass over the sources already registered with Oculus Audio before playing
    Source* source = oastate.sources[idx].source;
    if (source) { // A source is present; tell Oculus Audio its position
      ovrAudio_SetAudioSourcePos(oastate.context, idx, source->position[0], source->position[1], source->position[2]);
    } else if (oastate.sources[idx].occupied) { // No source is present but an echo tailoff is playing
      uint32_t outStatus = lovrOvrFlags;
      ovrAudio_SpatializeMonoSourceInterleaved(oastate.context, idx, &outStatus, unpack, NULL);
      if (outStatus & ovrAudioSpatializationStatus_Finished) {
        oastate.sources[idx].occupied = false;
        oastate.occupiedCount--;
      }
      lovrAudioMix(output, unpack, frames, 2);
    }
  }
#else
  float* accumulate = getReservedBuffer(&state.accumulateBuffer, frames, 1); // All sources will be mixed into here
  lovrAudioZero(accumulate, frames, 1);
  lovrAudioFalloffMix(accumulate, &state.residue, frames); // Before we start playing get rid of the residue from the previous frame
#endif

  uint32_t n = 0; // How much data has been written into mono buffer?
  for (Source** s = &state.sources; *s;) {
    Source* source = *s;
    int decodeOffset = 0; // How much of the decode buffer is currently filled out
    int decodeGoal, decodeLen; // How much did we try to decode, how much did we actually decode
#ifdef LOVR_ENABLE_OCULUS_AUDIO
    int spatializerId = source->spatializerId;
    float residue = 0;
#endif

    if (source->playing) {
      decode: // Attempt to decode from the empty part of the decode buffer to its end
        decodeGoal = frames - decodeOffset; // How many bytes would be needed to fill the buffer
        decodeLen = lovrDecoderDecode(source->decoder, decodeGoal, 1, (uint8_t*)(decode + decodeOffset));
        decodeOffset += decodeLen;
#ifndef LOVR_ENABLE_OCULUS_AUDIO
        if (decodeOffset == frames) // Let the source remember its last played sample, in case it gets stopped before handler() is called again
          source->residue = decode[frames-1];
#endif

        if (decodeLen < decodeGoal) { // The buffer is not full
          if (source->looping) {      // Looping sound; repeat decode until full
            lovrDecoderSeek(source->decoder, 0);
            goto decode;
          } else {                    // Not a looping sound; the sound is finished.
            source->playing = false;
            goto remove;
          }
        }

        s = &source->next; // If we did not goto remove, we need to move to the next node
    } else {
      remove:
#ifdef LOVR_ENABLE_OCULUS_AUDIO
        oastate.sources[spatializerId].source = NULL;
#endif
        *s = source->next; // Delete node and move to next
        source->tracked = false;
        lovrRelease(Source, source);
    }

#ifdef LOVR_ENABLE_OCULUS_AUDIO
    if (decodeOffset < frames) // Oculus Audio buffer is of fixed size, so any unused space must be zeroed
      lovrAudioZero(decode + decodeOffset, frames - decodeOffset, 1);

    uint32_t outStatus = 0;
    ovrAudio_SpatializeMonoSourceInterleaved(oastate.context, spatializerId, &outStatus, unpack, decode);
    if (!oastate.sources[spatializerId].source && (outStatus & ovrAudioSpatializationStatus_Finished)) { // Source done playing, echo tailoff is done
      oastate.sources[spatializerId].occupied = false;
      oastate.occupiedCount--;
    }
    lovrAudioMix(output, unpack, frames, 2); // Mix Oculus Audio's output into the callback output
#else
    lovrAudioMix(accumulate, decode, decodeOffset, 1);
    if (decodeOffset < frames) { // If the sound ended with a DC offset, trail that off like it were a stopped sound
      float residue = decodeOffset > 0 ? decode[decodeOffset-1] : source->residue;
      lovrAudioFalloffMix(accumulate + decodeOffset, &residue, frames - decodeOffset);
      state.residue += residue; // Still some residue for next frame
    }
#endif
  }

#ifndef LOVR_ENABLE_OCULUS_AUDIO
  lovrAudioUpmix(output, accumulate, frames, 2); // Dummy mixer mixed mono, but output wants stereo
#endif

#ifdef LOVR_DEBUG_AUDIOTAP
  if (state.audiotapWriting)
    lovrFileWrite(&state.audiotapFile, output, 2*FRAME_SIZE(frames));
#endif

  ma_mutex_unlock(&state.lock);
}

#ifdef LOVR_REGULARIZE_CALLBACK_LENGTH
// Miniaudio cannot guarantee us fixed-length callbacks. However, Oculus Audio requires them.
// Chop up the output buffer into regularly sized chunks, and save any overshoot until next callback.
// TODO: This entirely does not work with input
static struct {
  float_arr_t regularizeBuffer; // Run handler() into this buffer
  int leftover; // Unused frames from last run
} regularizerState;
static void miniaudioHandler(ma_device* device, void* output, const void* input, uint32_t frames) {
  float* regularize = getReservedBuffer(&regularizerState.regularizeBuffer, frames, 2); // This path assumes output is stereo since that's what we request below.
  float* outputFloat = output; // As we fill out the buffer, we will move this pointer forward and decrement frames
  if (regularizerState.leftover > 0) { // Last handler call produced more audio than we could play
    int leftoverCopy = MIN(frames, regularizerState.leftover);
    float* leftoverCopyFrom = regularize + ((BUFFER_LENGTH - leftoverCopy) * 2);
    lovrAudioCopy(outputFloat, leftoverCopyFrom, leftoverCopy, 2);

    outputFloat += (leftoverCopy*2);
    regularizerState.leftover -= leftoverCopy;
    frames -= leftoverCopy;
  }
  while (frames > 0) {
    if (frames < BUFFER_LENGTH) { // Not enough room to fit in output. Save some for later.
      handler(device, regularize, input, BUFFER_LENGTH);

      lovrAudioCopy(outputFloat, regularize, frames, 2);
      regularizerState.leftover = BUFFER_LENGTH - frames;
      break; // Done
    }
   
    handler(device, outputFloat, input, BUFFER_LENGTH);

    outputFloat += (BUFFER_LENGTH*2);
    frames -= BUFFER_LENGTH;
  }
}
#else
#define miniaudioHandler handler
#endif

bool lovrAudioInit() {
  if (state.initialized) return false;

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 2;
  config.sampleRate = SAMPLE_RATE;
  config.dataCallback = miniaudioHandler;
  config.bufferSizeInFrames = CALLBACK_LENGTH;
  config.periods = CALLBACK_PERIODS;

  if (lovrSpatializerInit(config.sampleRate)) {
    return false;
  }

#ifdef LOVR_ENABLE_OCULUS_AUDIO
  // Initialize spatializer and presize buffers to some good guesses for their real lengths
  lovrSpatializerRealInit(BUFFER_LENGTH); // Initialize Oculus Audio's buffers to a reasonable guess of the callback length Miniaudio will actually pick on Android
  getReservedBuffer(&regularizerState.regularizeBuffer, BUFFER_LENGTH, 2);
  getReservedBuffer(&state.decodeBuffer, BUFFER_LENGTH, 1);
  getReservedBuffer(&oastate.unpackBuffer, BUFFER_LENGTH, 2);
#else
  getReservedBuffer(&state.decodeBuffer, CALLBACK_LENGTH, 1); // Without the regularizer, miniaudio will *sometimes* give us callbacks as long as CALLBACK_LENGTH, so prepare for that
  getReservedBuffer(&state.accumulateBuffer, CALLBACK_LENGTH, 1);
#endif

  if (ma_device_init(NULL, &config, &state.device)) {
    return false;
  }

  if (ma_mutex_init(state.device.pContext, &state.lock)) {
    ma_device_uninit(&state.device);
    return false;
  }

  if (ma_device_start(&state.device)) {
    ma_device_uninit(&state.device);
    return false;
  }

#ifdef LOVR_DEBUG_AUDIOTAP
  lovrFileInit(&state.audiotapFile, "lovrDebugAudio.raw");
  state.audiotapWriting = lovrFileOpen(&state.audiotapFile, OPEN_WRITE);
#endif

  return state.initialized = true;
}

void lovrAudioDestroy() {
  if (!state.initialized) return;
  ma_device_uninit(&state.device);
  ma_mutex_uninit(&state.lock);
#ifdef LOVR_DEBUG_AUDIOTAP
  if (state.audiotapWriting) {
    lovrFileClose(&state.audiotapFile);
    state.audiotapWriting = false;
  }
#endif
  while (state.sources) {
    Source* source = state.sources;
    state.sources = source->next;
    source->next = NULL;
    source->tracked = false;
    lovrRelease(Source, source);
  }
  memset(&state, 0, sizeof(state));

#if LOVR_ENABLE_OCULUS_AUDIO
  if (oastate.bufferSize) {
    ovrAudio_DestroyContext( oastate.context );
    oastate.bufferSize = 0;
  }
#endif
}

Source* lovrSourceInit(Source* source, Decoder* decoder) {
  source->volume = 1.f;
  source->decoder = decoder;
  lovrRetain(decoder);
  return source;
}

void lovrSourceDestroy(void* ref) {
  Source* source = ref;
  lovrAssert(!source->tracked, "Unreachable");
  lovrRelease(Decoder, source->decoder);
}

void lovrSourcePlay(Source* source) {
  ma_mutex_lock(&state.lock);

  if (!source->playing) {

    source->playing = true;

    if (!source->tracked) {
      lovrRetain(source);
      source->tracked = true;
      source->next = state.sources;
      state.sources = source;
    }

#ifdef LOVR_ENABLE_OCULUS_AUDIO
    // Find a slot in oastate.sources to place the new source
    // Note 1: This is far from ideal. When either tails or sources get overriden it
    // isn't necessarily in a "fair" order. It would be nice to have two little heaps
    // Note 2: This code goes out of its way to not cutoff tails unless it has to.
    // The OVR_Audio docs *imply*, but do not *say*, that playing a sound over a tail
    // has negative effects. It is possible that removing the Reset() below
    // would make it perfectly safe for a sound to overlap with an echo tail.
    int spatializerId;
    if (oastate.occupiedCount < LOVR_SOURCE_MAX) { // There's a free space to claim
      for (spatializerId = 0; spatializerId < LOVR_SOURCE_MAX; spatializerId++)
        if (!oastate.sources[spatializerId].occupied)
          break;
    } else if (oastate.sourceCount < LOVR_SOURCE_MAX) { // Must kill tail
      for (spatializerId = LOVR_SOURCE_MAX-1; spatializerId > 0; spatializerId--)
        if (!oastate.sources[spatializerId].source)
          break;
      oastate.occupiedCount--;
    } else { // Must kill sound
      Source* target;
      for (Source* s = state.sources; s->next; s++) // Pick final playing Source in list
        if (s->playing)
          target = s;
      target->playing = false;
      spatializerId = target->spatializerId;
      oastate.occupiedCount--;
      oastate.sourceCount--;
    }
    source->spatializerId = spatializerId;
    oastate.sources[spatializerId].source = source;
    oastate.sources[spatializerId].occupied = true;
    oastate.sourceCount++;
    oastate.occupiedCount++;

    if (oastate.bufferSize) // If context initialized
      ovrAudio_ResetAudioSource(oastate.context, spatializerId);
#else
    source->residue = 0;
#endif
  }

  ma_mutex_unlock(&state.lock);
}

void lovrSourcePause(Source* source) {
  ma_mutex_lock(&state.lock);

#ifdef LOVR_ENABLE_OCULUS_AUDIO
  if (source->playing) { // Notice: still occupied
    oastate.sources[source->spatializerId].source = NULL;
    oastate.sourceCount--;
    source->playing = false;
  }
#else
  source->playing = false;
#endif

  ma_mutex_unlock(&state.lock);
}

bool lovrSourceIsPlaying(Source* source) {
  return source->playing;
}

bool lovrSourceIsLooping(Source* source) {
  return source->looping;
}

void lovrSourceSetLooping(Source* source, bool loop) {
  source->looping = loop;
}

void lovrSourceGetPosition(Source* source, vec3 position) {
  memcpy(position, source->position, sizeof(float)*3); // No lock, assume positions are only set on main thread
}

void lovrSourceSetPosition(Source* source, vec3 position) {
  ma_mutex_lock(&state.lock);
  memcpy(source->position, position, sizeof(float)*3);
  ma_mutex_unlock(&state.lock);
}

float lovrSourceGetVolume(Source* source) {
  return source->volume;
}

void lovrSourceSetVolume(Source* source, float volume) {
  ma_mutex_lock(&state.lock);
  source->volume = volume;
  ma_mutex_unlock(&state.lock);
}

Decoder* lovrSourceGetDecoder(Source* source) {
  return source->decoder;
}

void lovrAudioLock() {
  ma_mutex_lock(&state.lock);
}

void lovrAudioUnlock() {
  ma_mutex_unlock(&state.lock);
}
