#pragma once

#if EMSCRIPTEN
#define GLFW_INCLUDE_ES3
#define GLFW_INCLUDE_GLEXT
#elif __APPLE__
#define GLFW_INCLUDE_GLCOREARB
#include "glad/glad.h"
#elif _WIN32
#define APIENTRY __stdcall
#include "glad/glad.h"
#endif

#include <GLFW/glfw3.h>
