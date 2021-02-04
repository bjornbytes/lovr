#define MINIAUDIO_IMPLEMENTATION
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
