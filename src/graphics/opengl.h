#if LOVR_WEBGL
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include "lib/glad/glad.h"
#endif

#pragma once

#define GPU_BUFFER_FIELDS \
  GLsync lock; \
  uint32_t id; \
  uint8_t incoherent;

#define GPU_CANVAS_FIELDS \
  uint32_t framebuffer; \
  uint32_t resolveBuffer; \
  uint32_t depthBuffer; \
  bool immortal;

#define GPU_MESH_FIELDS \
  uint32_t vao;

#define GPU_SHADER_FIELDS \
  uint32_t program;

#define GPU_TEXTURE_FIELDS \
  GLuint id; \
  GLuint msaaId; \
  GLenum target; \
  uint8_t incoherent;
