#include "glfw.h"
#include "graphics.h"
#include "model.h"
#include "buffer.h"
#include "shader.h"
#include "util.h"
#include <stdlib.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

int lovrGraphicsClear(lua_State* L) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  return 0;
}

int lovrGraphicsPresent(lua_State* L) {
  glfwSwapBuffers(window);

  return 0;
}

// TODO default shader
int lovrGraphicsSetShader(lua_State* L) {
  GLuint shader = (GLuint) luaL_checknumber(L, 1);
  glUseProgram(shader);

  return 0;
}

int lovrGraphicsNewModel(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  const struct aiScene* scene = aiImportFile(path, aiProcessPreset_TargetRealtime_MaxQuality);

  if (scene) {
    Model* model = scene->mMeshes[0];
    luax_pushmodel(L, model);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

int lovrGraphicsNewBuffer(lua_State* L) {
  Buffer* buffer = (Buffer*) malloc(sizeof(Buffer));

  buffer->data = (GLfloat*) malloc(100);

  buffer->data[0] = -0.5;
  buffer->data[1] = -0.5;
  buffer->data[2] = 0.0;

  buffer->data[3] = 0.5;
  buffer->data[4] = -0.5;
  buffer->data[5] = 0.0;

  buffer->data[6] = 0.0;
  buffer->data[7] = 0.5;
  buffer->data[8] = 0.0;

  GLuint id;

  id = buffer->vbo;
  glGenBuffers(1, &id);
  glBindBuffer(GL_ARRAY_BUFFER, id);
  glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(GLfloat), buffer->data, GL_STATIC_DRAW);

  id = buffer->vao;
  glGenVertexArrays(1, &id);
  glBindVertexArray(id);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, id);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

  luax_pushbuffer(L, buffer);

  return 1;
}

int lovrGraphicsNewShader(lua_State* L) {
  const char* vertexShaderSource = luaL_checkstring(L, 1);
  const char* fragmentShaderSource = luaL_checkstring(L, 2);

  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
  GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
  GLuint shader = linkShaders(vertexShader, fragmentShader);

  if (shader) {
    lua_pushnumber(L, shader);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

const luaL_Reg lovrGraphics[] = {
  { "clear", lovrGraphicsClear },
  { "present", lovrGraphicsPresent },
  { "setShader", lovrGraphicsSetShader },
  { "newModel", lovrGraphicsNewModel },
  { "newBuffer", lovrGraphicsNewBuffer },
  { "newShader", lovrGraphicsNewShader },
  { NULL, NULL }
};
