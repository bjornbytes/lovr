#ifdef LOVR_WEBGL
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include "lib/glad/glad.h"
#endif

#include <stdint.h>

#pragma once

#define GPU_MESH_FIELDS \
  uint32_t vao; \
  uint32_t ibo;

#define GPU_SHADER_FIELDS \
  uint32_t program;
