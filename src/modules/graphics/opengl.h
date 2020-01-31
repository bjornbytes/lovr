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

#define GPU_BUFFER_FIELDS \
  uint8_t incoherent; \
  uint32_t id;

#define GPU_CANVAS_FIELDS \
  bool immortal; \
  uint32_t framebuffer; \
  uint32_t resolveBuffer; \
  uint32_t depthBuffer;

#define GPU_MESH_FIELDS \
  uint32_t vao; \
  uint32_t ibo;

#define GPU_SHADER_FIELDS \
  uint32_t program;

#define GPU_TEXTURE_FIELDS \
  bool native; \
  uint8_t incoherent; \
  GLuint id; \
  GLuint msaaId; \
  GLenum target;
