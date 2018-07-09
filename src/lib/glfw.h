#pragma once

#ifdef _WIN32
#define APIENTRY __stdcall
#endif

#if EMSCRIPTEN
#define GLFW_INCLUDE_ES3
#define GLFW_INCLUDE_GLEXT
#else
#include "glad/glad.h"
#endif

#include <GLFW/glfw3.h>
