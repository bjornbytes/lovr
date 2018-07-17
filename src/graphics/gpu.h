#include "graphics/canvas.h"
#include "graphics/font.h"
#include "graphics/graphics.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "math/math.h"
#include <stdint.h>
#include <stdbool.h>

#pragma once

typedef void (*gpuProc)(void);

void gpuInit(bool srgb, gpuProc (*getProcAddress)(const char*));
void gpuDestroy();
void gpuClear(Canvas** canvas, int canvasCount, Color* color, float* depth, int* stencil);
void gpuDraw(DrawCommand* command);
void gpuPresent();

// Ephemeral
void gpuBindFramebuffer(uint32_t framebuffer);
void gpuBindIndexBuffer(uint32_t indexBuffer);
void gpuBindTexture(Texture* texture, int slot);
void gpuBindVertexArray(uint32_t vertexArray);
void gpuBindVertexBuffer(uint32_t vertexBuffer);
Texture* gpuGetTexture(int slot);
void gpuSetViewport(uint32_t viewport[4]);
void gpuUseProgram(uint32_t program);
