#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#ifdef ANDROID
    #define MA_NO_RUNTIME_LINKING
#endif
//#define MA_DEBUG_OUTPUT
#include "miniaudio.h"
