#include "graphics/gpu.h"
#include "graphics/opengl/opengl.h"
#include "resources/shaders.h"
#include "math/mat4.h"
#include "lib/glfw.h"
#include <string.h>

#define MAX_TEXTURES 16

static struct {
  Texture* defaultTexture;
  uint32_t framebuffer;
  uint32_t indexBuffer;
  uint32_t program;
  Texture* textures[MAX_TEXTURES];
  uint32_t vertexArray;
  uint32_t vertexBuffer;
  uint32_t viewport[4];
  bool srgb;
  GpuStats stats;
} state;

static void gammaCorrectColor(Color* color) {
  if (state.srgb) {
    color->r = lovrMathGammaToLinear(color->r);
    color->g = lovrMathGammaToLinear(color->g);
    color->b = lovrMathGammaToLinear(color->b);
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
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  state.srgb = srgb;
}

void gpuDestroy() {
  lovrRelease(state.defaultTexture);
  for (int i = 0; i < MAX_TEXTURES; i++) {
    lovrRelease(state.textures[i]);
  }
}

void gpuDraw(GpuDrawCommand* command) {
  Mesh* mesh = command->mesh;
  Material* material = command->material;
  Shader* shader = command->shader;
  int instances = command->instances;

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

  // Point size
  lovrShaderSetFloat(shader, "lovrPointSize", &command->pointSize, 1);

  // Color
  Color color = command->color;
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

GpuStats gpuGetStats() {
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
