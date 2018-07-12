#include "graphics/mesh.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include <stdint.h>
#include <stdbool.h>

#pragma once

typedef struct {
  int shaderSwitches;
  int drawCalls;
} GpuStats;

typedef struct {
  Mesh* mesh;
  Shader* shader;
  int instances;

/*
eventually:

  Canvas* canvas;
  uint32_t viewport[4];
  mat4 view;
  mat4 projection;
  mat4 model;

  Color color;
  float pointSize;

  Material* material;

  struct {
    CompareMode test;
    bool write;
  } depthMode;

  struct {
    BlendMode mode;
    BlendAlphaMode alphaMode;
  } blendMode;

  //etc.
*/
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
