#include "graphics/gpu.h"
#include "graphics/opengl/opengl.h"
#include "resources/shaders.h"
#include "data/modelData.h"
#include "math/mat4.h"
#include <string.h>

#define MAX_TEXTURES 16

static struct {
  Texture* defaultTexture;
  BlendMode blendMode;
  BlendAlphaMode blendAlphaMode;
  bool culling;
  bool depthEnabled;
  CompareMode depthTest;
  bool depthWrite;
  float lineWidth;
  bool stencilEnabled;
  CompareMode stencilMode;
  int stencilValue;
  Winding winding;
  bool wireframe;
  uint32_t framebuffer;
  uint32_t indexBuffer;
  uint32_t program;
  Texture* textures[MAX_TEXTURES];
  uint32_t vertexArray;
  uint32_t vertexBuffer;
  uint32_t viewport[4];
  bool srgb;
  GraphicsLimits limits;
  GraphicsStats stats;
} state;

static void gammaCorrectColor(Color* color) {
  if (state.srgb) {
    color->r = lovrMathGammaToLinear(color->r);
    color->g = lovrMathGammaToLinear(color->g);
    color->b = lovrMathGammaToLinear(color->b);
  }
}

static GLenum convertCompareMode(CompareMode mode) {
  switch (mode) {
    case COMPARE_NONE: return GL_ALWAYS;
    case COMPARE_EQUAL: return GL_EQUAL;
    case COMPARE_NEQUAL: return GL_NOTEQUAL;
    case COMPARE_LESS: return GL_LESS;
    case COMPARE_LEQUAL: return GL_LEQUAL;
    case COMPARE_GREATER: return GL_GREATER;
    case COMPARE_GEQUAL: return GL_GEQUAL;
  }
}

void gpuInit(bool srgb, gpuProc (*getProcAddress)(const char*)) {
#ifndef EMSCRIPTEN
  gladLoadGLLoader((GLADloadproc) getProcAddress);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_PROGRAM_POINT_SIZE);
  if (srgb) {
    glEnable(GL_FRAMEBUFFER_SRGB);
  } else {
    glDisable(GL_FRAMEBUFFER_SRGB);
  }
#endif
  glEnable(GL_BLEND);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  state.srgb = srgb;
  state.blendMode = -1;
  state.blendAlphaMode = -1;
  state.culling = -1;
  state.depthEnabled = -1;
  state.depthTest = -1;
  state.depthWrite = -1;
  state.lineWidth = -1;
  state.stencilEnabled = -1;
  state.stencilMode = -1;
  state.stencilValue = -1;
  state.winding = -1;
  state.wireframe = -1;
}

void gpuDestroy() {
  lovrRelease(state.defaultTexture);
  for (int i = 0; i < MAX_TEXTURES; i++) {
    lovrRelease(state.textures[i]);
  }
}

void gpuClear(Canvas** canvas, int canvasCount, Color* color, float* depth, int* stencil) {
  gpuBindFramebuffer(canvasCount > 0 ? lovrCanvasGetId(canvas[0]) : 0);

  if (color) {
    gammaCorrectColor(color);
    float c[4] = { color->r, color->g, color->b, color->a };
    glClearBufferfv(GL_COLOR, 0, c);
    for (int i = 1; i < canvasCount; i++) {
      glClearBufferfv(GL_COLOR, i, c);
    }
  }

  if (depth) {
    glClearBufferfv(GL_DEPTH, 0, depth);
  }

  if (stencil) {
    glClearBufferiv(GL_STENCIL, 0, stencil);
  }
}

void gpuDraw(GpuDrawCommand* command) {
  Mesh* mesh = command->mesh;
  Material* material = command->material;
  Shader* shader = command->shader;
  Pipeline* pipeline = &command->pipeline;
  int instances = command->instances;

  // Blend mode
  if (state.blendMode != pipeline->blendMode || state.blendAlphaMode != pipeline->blendAlphaMode) {
    state.blendMode = pipeline->blendMode;
    state.blendAlphaMode = pipeline->blendAlphaMode;

    GLenum srcRGB = state.blendMode == BLEND_MULTIPLY ? GL_DST_COLOR : GL_ONE;
    if (srcRGB == GL_ONE && state.blendAlphaMode == BLEND_ALPHA_MULTIPLY) {
      srcRGB = GL_SRC_ALPHA;
    }

    switch (state.blendMode) {
      case BLEND_ALPHA:
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(srcRGB, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;

      case BLEND_ADD:
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(srcRGB, GL_ONE, GL_ZERO, GL_ONE);
        break;

      case BLEND_SUBTRACT:
        glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        glBlendFuncSeparate(srcRGB, GL_ONE, GL_ZERO, GL_ONE);
        break;

      case BLEND_MULTIPLY:
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(srcRGB, GL_ZERO, GL_DST_COLOR, GL_ZERO);
        break;

      case BLEND_LIGHTEN:
        glBlendEquation(GL_MAX);
        glBlendFuncSeparate(srcRGB, GL_ZERO, GL_ONE, GL_ZERO);
        break;

      case BLEND_DARKEN:
        glBlendEquation(GL_MIN);
        glBlendFuncSeparate(srcRGB, GL_ZERO, GL_ONE, GL_ZERO);
        break;

      case BLEND_SCREEN:
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(srcRGB, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ONE_MINUS_SRC_COLOR);
        break;

      case BLEND_REPLACE:
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(srcRGB, GL_ZERO, GL_ONE, GL_ZERO);
        break;
    }
  }

  // Culling
  if (state.culling != pipeline->culling) {
    state.culling = pipeline->culling;
    if (state.culling) {
      glEnable(GL_CULL_FACE);
    } else {
      glDisable(GL_CULL_FACE);
    }
  }

  // Depth test
  if (state.depthTest != pipeline->depthTest) {
    state.depthTest = pipeline->depthTest;
    if (state.depthTest != COMPARE_NONE) {
      if (!state.depthEnabled) {
        state.depthEnabled = true;
        glEnable(GL_DEPTH_TEST);
      }
      glDepthFunc(convertCompareMode(state.depthTest));
    } else if (state.depthEnabled) {
      state.depthEnabled = false;
      glDisable(GL_DEPTH_TEST);
    }
  }

  // Depth write
  if (state.depthWrite != pipeline->depthWrite) {
    state.depthWrite = pipeline->depthWrite;
    glDepthMask(state.depthWrite);
  }

  // Line width
  if (state.lineWidth != pipeline->lineWidth) {
    state.lineWidth = state.lineWidth;
    glLineWidth(state.lineWidth);
  }

  // Stencil mode
  if (state.stencilMode != pipeline->stencilMode || state.stencilValue != pipeline->stencilValue) {
    state.stencilMode = pipeline->stencilMode;
    state.stencilValue = pipeline->stencilValue;
    if (state.stencilMode != COMPARE_NONE) {
      if (!state.stencilEnabled) {
        state.stencilEnabled = true;
        glEnable(GL_STENCIL_TEST);
      }

      GLenum glMode = GL_ALWAYS;
      switch (state.stencilMode) {
        case COMPARE_EQUAL: glMode = GL_EQUAL; break;
        case COMPARE_NEQUAL: glMode = GL_NOTEQUAL; break;
        case COMPARE_LESS: glMode = GL_GREATER; break;
        case COMPARE_LEQUAL: glMode = GL_GEQUAL; break;
        case COMPARE_GREATER: glMode = GL_LESS; break;
        case COMPARE_GEQUAL: glMode = GL_LEQUAL; break;
        default: break;
      }

      glStencilFunc(glMode, state.stencilValue, 0xff);
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    } else if (state.stencilEnabled) {
      state.stencilEnabled = false;
      glDisable(GL_STENCIL_TEST);
    }
  }

  // Winding
  if (state.winding != pipeline->winding) {
    state.winding = pipeline->winding;
    glFrontFace(state.winding == WINDING_CLOCKWISE ? GL_CW : GL_CCW);
  }

  // Wireframe
  if (state.wireframe != pipeline->wireframe) {
    state.wireframe = pipeline->wireframe;
#ifndef EMSCRIPTEN
    glPolygonMode(GL_FRONT_AND_BACK, state.wireframe ? GL_LINE : GL_FILL);
#endif
  }

  // Transform
  lovrShaderSetMatrix(shader, "lovrProjection", command->layer.projection, 16);
  lovrShaderSetMatrix(shader, "lovrView", command->layer.view, 16);
  lovrShaderSetMatrix(shader, "lovrModel", command->transform, 16);

  float modelView[16];
  mat4_multiply(mat4_set(modelView, command->layer.view), command->transform);
  lovrShaderSetMatrix(shader, "lovrTransform", modelView, 16);

  if (lovrShaderHasUniform(shader, "lovrNormalMatrix")) {
    if (mat4_invert(modelView)) {
      mat4_transpose(modelView);
    } else {
      mat4_identity(modelView);
    }

    float normalMatrix[9] = {
      modelView[0], modelView[1], modelView[2],
      modelView[4], modelView[5], modelView[6],
      modelView[8], modelView[9], modelView[10],
    };

    lovrShaderSetMatrix(shader, "lovrNormalMatrix", normalMatrix, 9);
  }

  // Pose
  float* pose = lovrMeshGetPose(mesh);
  if (pose) {
    lovrShaderSetMatrix(shader, "lovrPose", pose, MAX_BONES * 16);
  } else {
    float identity[16];
    mat4_identity(identity);
    lovrShaderSetMatrix(shader, "lovrPose", identity, 16);
  }

  // Point size
  lovrShaderSetFloat(shader, "lovrPointSize", &pipeline->pointSize, 1);

  // Color
  Color color = pipeline->color;
  gammaCorrectColor(&color);
  float data[4] = { color.r, color.g, color.b, color.a };
  lovrShaderSetFloat(shader, "lovrColor", data, 4);

  // Material
  for (int i = 0; i < MAX_MATERIAL_SCALARS; i++) {
    float value = lovrMaterialGetScalar(material, i);
    lovrShaderSetFloat(shader, lovrShaderScalarUniforms[i], &value, 1);
  }

  for (int i = 0; i < MAX_MATERIAL_COLORS; i++) {
    Color color = lovrMaterialGetColor(material, i);
    gammaCorrectColor(&color);
    float data[4] = { color.r, color.g, color.b, color.a };
    lovrShaderSetFloat(shader, lovrShaderColorUniforms[i], data, 4);
  }

  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    Texture* texture = lovrMaterialGetTexture(material, i);
    lovrShaderSetTexture(shader, lovrShaderTextureUniforms[i], &texture, 1);
  }

  // Layer
  gpuBindFramebuffer(command->layer.canvasCount > 0 ? lovrCanvasGetId(command->layer.canvas[0]) : 0);
  gpuSetViewport(command->layer.viewport);

  // Shader
  gpuUseProgram(lovrShaderGetProgram(shader));
  lovrShaderBind(shader);

  // Attributes
  lovrMeshBind(mesh, shader);

  // Draw (TODEW)
  uint32_t rangeStart, rangeCount;
  lovrMeshGetDrawRange(mesh, &rangeStart, &rangeCount);
  uint32_t indexCount;
  size_t indexSize;
  lovrMeshReadIndices(mesh, &indexCount, &indexSize);
  GLenum glDrawMode = lovrConvertMeshDrawMode(lovrMeshGetDrawMode(mesh));
  if (indexCount > 0) {
    size_t count = rangeCount ? rangeCount : indexCount;
    GLenum indexType = indexSize == sizeof(uint16_t) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    size_t offset = rangeStart * indexSize;
    if (instances > 1) {
      glDrawElementsInstanced(glDrawMode, count, indexType, (GLvoid*) offset, instances);
    } else {
      glDrawElements(glDrawMode, count, indexType, (GLvoid*) offset);
    }
  } else {
    size_t count = rangeCount ? rangeCount : lovrMeshGetVertexCount(mesh);
    if (instances > 1) {
      glDrawArraysInstanced(glDrawMode, rangeStart, count, instances);
    } else {
      glDrawArrays(glDrawMode, rangeStart, count);
    }
  }

  state.stats.drawCalls++;
}

void gpuPresent() {
  memset(&state.stats, 0, sizeof(state.stats));
}

GraphicsLimits lovrGraphicsGetLimits() {
  if (!state.limits.initialized) {
#ifdef EMSCRIPTEN
    glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, state.limits.pointSizes);
#else
    glGetFloatv(GL_POINT_SIZE_RANGE, state.limits.pointSizes);
#endif
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &state.limits.textureSize);
    glGetIntegerv(GL_MAX_SAMPLES, &state.limits.textureMSAA);
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &state.limits.textureAnisotropy);
    state.limits.initialized = 1;
  }

  return state.limits;
}

GraphicsStats lovrGraphicsGetStats() {
  return state.stats;
}

// Ephemeral

void gpuBindFramebuffer(uint32_t framebuffer) {
  if (state.framebuffer != framebuffer) {
    state.framebuffer = framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  }
}

void gpuBindIndexBuffer(uint32_t indexBuffer) {
  if (state.indexBuffer != indexBuffer) {
    state.indexBuffer = indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
  }
}

void gpuBindTexture(Texture* texture, int slot) {
  lovrAssert(slot >= 0 && slot < MAX_TEXTURES, "Invalid texture slot %d", slot);

  if (!texture) {
    if (!state.defaultTexture) {
      TextureData* textureData = lovrTextureDataGetBlank(1, 1, 0xff, FORMAT_RGBA);
      state.defaultTexture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true, false);
      lovrRelease(textureData);
    }

    texture = state.defaultTexture;
  }

  if (texture != state.textures[slot]) {
    lovrRetain(texture);
    lovrRelease(state.textures[slot]);
    state.textures[slot] = texture;
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(texture->glType, lovrTextureGetId(texture));
  }
}

void gpuBindVertexArray(uint32_t vertexArray) {
  if (state.vertexArray != vertexArray) {
    state.vertexArray = vertexArray;
    glBindVertexArray(vertexArray);
  }
}

void gpuBindVertexBuffer(uint32_t vertexBuffer) {
  if (state.vertexBuffer != vertexBuffer) {
    state.vertexBuffer = vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  }
}

Texture* gpuGetTexture(int slot) {
  lovrAssert(slot >= 0 && slot < MAX_TEXTURES, "Invalid texture slot %d", slot);
  return state.textures[slot];
}

void gpuSetViewport(uint32_t viewport[4]) {
  if (memcmp(state.viewport, viewport, 4 * sizeof(uint32_t))) {
    memcpy(state.viewport, viewport, 4 * sizeof(uint32_t));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  }
}

void gpuUseProgram(uint32_t program) {
  if (state.program != program) {
    state.program = program;
    glUseProgram(program);
    state.stats.shaderSwitches++;
  }
}
