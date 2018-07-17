#pragma once

#if EMSCRIPTEN
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#else
#include "lib/glad/glad.h"
#endif
