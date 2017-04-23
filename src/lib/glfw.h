#pragma once

#ifdef _WIN32
#define APIENTRY __stdcall
#elif EMSCRIPTEN
#define GLFW_INCLUDE_ES2
#endif

#include "lib/glad/glad.h"
#include <GLFW/glfw3.h>
