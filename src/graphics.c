#include "graphics.h"
#include "util.h"
#include <GLFW/glfw3.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

extern GLFWwindow* window;
typedef const struct aiScene* Model;

int lovrGraphicsClear(lua_State* L) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  return 0;
}

int lovrGraphicsPresent(lua_State* L) {
  glfwSwapBuffers(window);

  return 0;
}

int lovrGraphicsNewModel(lua_State* L) {
  const char* path = luaL_checkstring(L, -1);
  Model model = aiImportFile(path, aiProcessPreset_TargetRealtime_MaxQuality);

  if (model) {
    Model* userdata = (Model*) luaPushType(L, "model");
    *userdata = model;
  } else {
    // error
  }

  return 1;
}

const luaL_Reg lovrGraphics[] = {
  { "clear", lovrGraphicsClear },
  { "present", lovrGraphicsPresent },
  { "newModel", lovrGraphicsNewModel },
  { NULL, NULL }
};

int lovrModelGetVertexCount(lua_State* L) {
  const struct aiScene* scene = *(const struct aiScene**) luaL_checkudata(L, -1, "model");

  lua_pushnumber(L, scene->mMeshes[0]->mNumVertices);

  return 1;
}

const luaL_Reg lovrModel[] = {
  { "getVertexCount", lovrModelGetVertexCount },
  { NULL, NULL }
};
