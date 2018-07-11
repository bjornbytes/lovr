#include "graphics/texture.h"
#include <stdint.h>
#include <stdbool.h>

#pragma once

typedef struct {
  uint32_t shaderSwitches;
} GpuStats;

typedef void (*gpuProc)(void);

void gpuInit(bool srgb, gpuProc (*getProcAddress)(const char*));
void gpuDestroy();
void gpuPresent();
void gpuBindFramebuffer(uint32_t framebuffer);
void gpuBindIndexBuffer(uint32_t indexBuffer);
void gpuBindTexture(Texture* texture, int slot);
void gpuBindVertexArray(uint32_t vertexArray);
void gpuBindVertexBuffer(uint32_t vertexBuffer);
Texture* gpuGetTexture(int slot);
void gpuSetViewport(uint32_t viewport[4]);
void gpuUseProgram(uint32_t program);
