#define MINIAUDIO_IMPLEMENTATION
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MA_ENABLE_ALSA
#define MA_ENABLE_COREAUDIO
#define MA_ENABLE_OPENSL
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#ifdef ANDROID
#define MA_NO_RUNTIME_LINKING
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "miniaudio.h"
#pragma clang diagnostic pop
