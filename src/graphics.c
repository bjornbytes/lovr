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

int lovrGraphicsGetClearColor(lua_State* L) {
  GLfloat clearColor[4];
  glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);

  lua_pushnumber(L, clearColor[0]);
  lua_pushnumber(L, clearColor[1]);
  lua_pushnumber(L, clearColor[2]);
  lua_pushnumber(L, clearColor[3]);

  return 4;
}

int lovrGraphicsSetClearColor(lua_State* L) {
  float r = luaL_checknumber(L, 1);
  float g = luaL_checknumber(L, 2);
  float b = luaL_checknumber(L, 3);
  float a = 1.0;

  if (lua_gettop(L) == 4) {
    a = luaL_checknumber(L, 4);
  }

  glClearColor(r, g, b, a);

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
  int size = luaL_checkint(L, 1);

  Buffer* buffer = (Buffer*) malloc(sizeof(Buffer));

  buffer->data = (GLfloat*) malloc(size * 3 * sizeof(GLfloat));

  glGenBuffers(1, &buffer->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(buffer->data) * sizeof(GL_FLOAT), buffer->data, GL_STATIC_DRAW);

  glGenVertexArrays(1, &buffer->vao);

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
  { "getClearColor", lovrGraphicsGetClearColor },
  { "setClearColor", lovrGraphicsSetClearColor },
  { "setShader", lovrGraphicsSetShader },
  { "newModel", lovrGraphicsNewModel },
  { "newBuffer", lovrGraphicsNewBuffer },
  { "newShader", lovrGraphicsNewShader },
  { NULL, NULL }
};
