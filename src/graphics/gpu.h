#include <stdint.h>
#include <stdbool.h>

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
