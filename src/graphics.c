#include "glfw.h"
#include "graphics.h"
#include "model.h"
#include "util.h"
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

int lovrGraphicsNewModel(lua_State* L) {
  const char* path = luaL_checkstring(L, -1);
  const struct aiScene* scene = aiImportFile(path, aiProcessPreset_TargetRealtime_MaxQuality);

  if (scene) {
    Model* model = scene->mMeshes[0];
    luax_pushmodel(L, model);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

const luaL_Reg lovrGraphics[] = {
  { "clear", lovrGraphicsClear },
  { "present", lovrGraphicsPresent },
  { "newModel", lovrGraphicsNewModel },
  { NULL, NULL }
};
