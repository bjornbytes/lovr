// "Null" implementation of all audio code for no-OpenAL mode
// Currently, some of these functions may lead to crashes (IE by returning null values)

// audio.c

#include "audio/audio.h"

ALenum lovrAudioConvertFormat(int bitDepth, int channelCount) { return 0; }
void lovrAudioInit() {}
void lovrAudioDestroy() {}
void lovrAudioUpdate() {}
void lovrAudioAdd(Source* source) {}
void lovrAudioGetDopplerEffect(float* factor, float* speedOfSound) {}
void lovrAudioGetMicrophoneNames(const char* names[MAX_MICROPHONES], uint8_t* count) {}
void lovrAudioGetOrientation(float* angle, float* ax, float* ay, float* az) {}
void lovrAudioGetPosition(float* x, float* y, float* z) {}
void lovrAudioGetVelocity(float* x, float* y, float* z) {}
float lovrAudioGetVolume() { return 0; }
bool lovrAudioHas(Source* source) { return false; }
bool lovrAudioIsSpatialized() { return false; }
void lovrAudioPause() {}
void lovrAudioResume() {}
void lovrAudioRewind() {}
void lovrAudioSetDopplerEffect(float factor, float speedOfSound) {}
void lovrAudioSetOrientation(float angle, float ax, float ay, float az) {}
void lovrAudioSetPosition(float x, float y, float z) {}
void lovrAudioSetVelocity(float x, float y, float z) {}
void lovrAudioSetVolume(float volume) {}
void lovrAudioStop() {}

// microphone.c

#include "audio/microphone.h"

Microphone* lovrMicrophoneCreate(const char* name, int samples, int sampleRate, int bitDepth, int channelCount) { return 0; }
void lovrMicrophoneDestroy(void* ref) {}
int lovrMicrophoneGetBitDepth(Microphone* microphone) { return 0; }
int lovrMicrophoneGetChannelCount(Microphone* microphone) { return 0; }
SoundData* lovrMicrophoneGetData(Microphone* microphone) { return NULL; }
const char* lovrMicrophoneGetName(Microphone* microphone) { return NULL; }
int lovrMicrophoneGetSampleCount(Microphone* microphone) { return 0; }
int lovrMicrophoneGetSampleRate(Microphone* microphone) { return 0; }
bool lovrMicrophoneIsRecording(Microphone* microphone) { return false; }
void lovrMicrophoneStartRecording(Microphone* microphone) {}
void lovrMicrophoneStopRecording(Microphone* microphone) {}

// source.c

#include "audio/source.h"

Source* lovrSourceCreateStatic(SoundData* soundData) { return NULL; }
Source* lovrSourceCreateStream(AudioStream* stream) { return NULL; }
void lovrSourceDestroy(void* ref) {}
SourceType lovrSourceGetType(Source* source) { return SOURCE_STATIC; }
int lovrSourceGetBitDepth(Source* source) { return 0; }
int lovrSourceGetChannelCount(Source* source)  { return 0; }
void lovrSourceGetCone(Source* source, float* innerAngle, float* outerAngle, float* outerGain)  {}
void lovrSourceGetDirection(Source* source, float* x, float* y, float* z)  {}
int lovrSourceGetDuration(Source* source) { return 0; }
void lovrSourceGetFalloff(Source* source, float* reference, float* max, float* rolloff) {}
float lovrSourceGetPitch(Source* source) { return 0; }
void lovrSourceGetPosition(Source* source, float* x, float* y, float* z) {}
void lovrSourceGetVelocity(Source* source, float* x, float* y, float* z) {}
int lovrSourceGetSampleRate(Source* source) { return 0; }
float lovrSourceGetVolume(Source* source) { return 0; }
void lovrSourceGetVolumeLimits(Source* source, float* min, float* max) {}
bool lovrSourceIsLooping(Source* source) { return false; }
bool lovrSourceIsPaused(Source* source) { return false; }
bool lovrSourceIsPlaying(Source* source) { return false; }
bool lovrSourceIsRelative(Source* source) { return false; }
bool lovrSourceIsStopped(Source* source) { return false; }
void lovrSourcePause(Source* source) {}
void lovrSourcePlay(Source* source) {}
void lovrSourceResume(Source* source) {}
void lovrSourceRewind(Source* source) {}
void lovrSourceSeek(Source* source, int sample) {}
void lovrSourceSetCone(Source* source, float inner, float outer, float outerGain) {}
void lovrSourceSetDirection(Source* source, float x, float y, float z) {}
void lovrSourceSetFalloff(Source* source, float reference, float max, float rolloff) {}
void lovrSourceSetLooping(Source* source, bool isLooping) {}
void lovrSourceSetPitch(Source* source, float pitch) {}
void lovrSourceSetPosition(Source* source, float x, float y, float z) {}
void lovrSourceSetRelative(Source* source, bool isRelative) {}
void lovrSourceSetVelocity(Source* source, float x, float y, float z) {}
void lovrSourceSetVolume(Source* source, float volume) {}
void lovrSourceSetVolumeLimits(Source* source, float min, float max) {}
void lovrSourceStop(Source* source) {}
void lovrSourceStream(Source* source, ALuint* buffers, int count) {}
int lovrSourceTell(Source* source) { return 0; }
