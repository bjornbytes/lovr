#include "graphics/canvas.h"
#include "graphics/font.h"
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
} GraphicsStats;

typedef enum {
  BLEND_ALPHA,
  BLEND_ADD,
  BLEND_SUBTRACT,
  BLEND_MULTIPLY,
  BLEND_LIGHTEN,
  BLEND_DARKEN,
  BLEND_SCREEN,
  BLEND_REPLACE
} BlendMode;

typedef enum {
  BLEND_ALPHA_MULTIPLY,
  BLEND_PREMULTIPLIED
} BlendAlphaMode;

typedef enum {
  DRAW_MODE_FILL,
  DRAW_MODE_LINE
} DrawMode;

typedef enum {
  ARC_MODE_PIE,
  ARC_MODE_OPEN,
  ARC_MODE_CLOSED
} ArcMode;

typedef enum {
  WINDING_CLOCKWISE,
  WINDING_COUNTERCLOCKWISE
} Winding;

typedef enum {
  COMPARE_NONE,
  COMPARE_EQUAL,
  COMPARE_NEQUAL,
  COMPARE_LESS,
  COMPARE_LEQUAL,
  COMPARE_GREATER,
  COMPARE_GEQUAL
} CompareMode;

typedef enum {
  STENCIL_REPLACE,
  STENCIL_INCREMENT,
  STENCIL_DECREMENT,
  STENCIL_INCREMENT_WRAP,
  STENCIL_DECREMENT_WRAP,
  STENCIL_INVERT
} StencilAction;

typedef struct {
  bool initialized;
  float pointSizes[2];
  int textureSize;
  int textureMSAA;
  float textureAnisotropy;
} GraphicsLimits;

typedef struct {
  Color backgroundColor;
  BlendMode blendMode;
  BlendAlphaMode blendAlphaMode;
  Color color;
  bool culling;
  CompareMode depthTest;
  bool depthWrite;
  Font* font;
  float lineWidth;
  float pointSize;
  Shader* shader;
  CompareMode stencilMode;
  int stencilValue;
  Winding winding;
  bool wireframe;
} Pipeline;

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
  Pipeline pipeline;
  int instances;
} GpuDrawCommand;

typedef void (*gpuProc)(void);

void gpuInit(bool srgb, gpuProc (*getProcAddress)(const char*));
void gpuDestroy();
void gpuClear(Canvas** canvas, int canvasCount, Color* color, float* depth, int* stencil);
void gpuDraw(GpuDrawCommand* command);
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
