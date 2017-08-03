#pragma once

#if EMSCRIPTEN
#define GLFW_INCLUDE_ES3
#define GLFW_INCLUDE_GLEXT
#elif __APPLE__
#define GLFW_INCLUDE_GLCOREARB
#elif _WIN32
#define APIENTRY __stdcall
#endif

#ifndef EMSCRIPTEN
#include "glad/glad.h"
#endif

#include <GLFW/glfw3.h>
