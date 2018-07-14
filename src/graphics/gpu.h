#include "graphics/canvas.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "math/math.h"
#include <stdint.h>
#include <stdbool.h>

#pragma once

#define MAX_CANVASES 4

typedef struct {
  int shaderSwitches;
  int drawCalls;
} GpuStats;

typedef struct {
  float projection[16];
  float view[16];
  uint32_t viewport[4];
  Canvas* canvas[MAX_CANVASES];
  int canvasCount;
  bool user;
} Layer;

typedef struct {
  Layer layer;
  mat4 transform;
  Shader* shader;
  Material* material;
  Mesh* mesh;
  Color color;
  float pointSize;
  int instances;
} GpuDrawCommand;

typedef void (*gpuProc)(void);

void gpuInit(bool srgb, gpuProc (*getProcAddress)(const char*));
void gpuDestroy();
void gpuDraw(GpuDrawCommand* command);
void gpuPresent();
GpuStats gpuGetStats();

// Ephemeral
void gpuBindFramebuffer(uint32_t framebuffer);
void gpuBindIndexBuffer(uint32_t indexBuffer);
void gpuBindTexture(Texture* texture, int slot);
void gpuBindVertexArray(uint32_t vertexArray);
void gpuBindVertexBuffer(uint32_t vertexBuffer);
Texture* gpuGetTexture(int slot);
void gpuSetViewport(uint32_t viewport[4]);
void gpuUseProgram(uint32_t program);
