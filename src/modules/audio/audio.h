#include <stdbool.h>
#include <stdint.h>

#pragma once

struct SoundData;
struct Decoder;

typedef enum {
  TIME_FRAMES,
  TIME_SECONDS
} TimeUnit;

typedef struct Source {
  struct Source* next;
  struct Decoder* decoder;
  float volume;
  float position[4];
#ifdef LOVR_ENABLE_OCULUS_AUDIO
  unsigned spatializerId: 4; // Only meaningful while playing
#else
  float residue; // Used to prevent pops if the sound ends suddenly
#endif
  bool playing : 1;
  bool looping : 1;
  bool tracked : 1;
} Source;

bool lovrAudioInit(void);
void lovrAudioDestroy(void);

void lovrAudioLock(void);
void lovrAudioUnlock(void);

Source* lovrSourceInit(Source* source, struct Decoder* decoder);
#define lovrSourceCreate(...) lovrSourceInit(lovrAlloc(Source), __VA_ARGS__)
void lovrSourceDestroy(void* ref);
void lovrSourcePlay(Source* source);
void lovrSourcePause(Source* source);
bool lovrSourceIsPlaying(Source* source);
bool lovrSourceIsLooping(Source* source);
void lovrSourceSetLooping(Source* source, bool loop);
float lovrSourceGetVolume(Source* source);
void lovrSourceSetVolume(Source* source, float volume);
struct Decoder* lovrSourceGetDecoder(Source* source);
