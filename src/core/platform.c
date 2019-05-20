#include "platform.h"

#ifdef _WIN32
#include "platform_windows.c.h"
#elif __APPLE__
#include "platform_macos.c.h"
#elif __ANDROID__
#include "platform_android.c.h"
#elif __linux__
#include "platform_linux.c.h"
#elif EMSCRIPTEN
#include "platform_web.c.h"
#endif
