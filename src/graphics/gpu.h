#include <stdint.h>
#include <stdbool.h>

#ifdef EMSCRIPTEN
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#else
#include "lib/glad/glad.h"
#endif

#pragma once

typedef struct {
  uint32_t shaderSwitches;
} GpuStats;

typedef void (*gpuProc)(void);

void gpuInit(bool srgb, gpuProc (*getProcAddress)(const char*));
void gpuPresent();
void gpuBindFramebuffer(uint32_t framebuffer);
void gpuBindIndexBuffer(uint32_t indexBuffer);
void gpuBindVertexArray(uint32_t vertexArray);
void gpuBindVertexBuffer(uint32_t vertexBuffer);
void gpuSetViewport(uint32_t viewport[4]);
void gpuUseProgram(uint32_t program);
