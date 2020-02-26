#include <stdbool.h>
#include <stdint.h>

#pragma once

#define MAX_MICROPHONES 8

struct Source;

int lovrAudioConvertFormat(uint32_t bitDepth, uint32_t channelCount);

bool lovrAudioInit(void);
void lovrAudioDestroy(void);
void lovrAudioUpdate(void);
void lovrAudioAdd(struct Source* source);
void lovrAudioGetDopplerEffect(float* factor, float* speedOfSound);
void lovrAudioGetMicrophoneNames(const char* names[MAX_MICROPHONES], uint32_t* count);
void lovrAudioGetOrientation(float* orientation);
void lovrAudioGetPosition(float* position);
void lovrAudioGetVelocity(float* velocity);
float lovrAudioGetVolume(void);
bool lovrAudioHas(struct Source* source);
bool lovrAudioIsSpatialized(void);
void lovrAudioPause(void);
void lovrAudioSetDopplerEffect(float factor, float speedOfSound);
void lovrAudioSetOrientation(float* orientation);
void lovrAudioSetPosition(float* position);
void lovrAudioSetVelocity(float* velocity);
void lovrAudioSetVolume(float volume);
void lovrAudioStop(void);
