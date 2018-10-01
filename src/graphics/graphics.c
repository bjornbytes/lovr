#include "graphics/graphics.h"
#include "resources/shaders.h"
#include "data/rasterizer.h"
#include "event/event.h"
#include "filesystem/filesystem.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include "util.h"
#include "lib/glfw.h"
#include "lib/stb/stb_image.h"
#define _USE_MATH_DEFINES
#include <stdlib.h>
#include <string.h>
#include <math.h>

static GraphicsState state;

static void onCloseWindow(GLFWwindow* window) {
  if (window == state.window) {
    lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit = { false, 0 } });
  }
}

static void onResizeWindow(GLFWwindow* window, int width, int height) {
  if (window == state.window) {
    state.width = width;
    state.height = height;
  }
}

// Base

void lovrGraphicsInit(bool gammaCorrect) {
  state.gammaCorrect = gammaCorrect;
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  while (state.pipeline > 0) {
    lovrGraphicsPopPipeline();
  }
  lovrGraphicsSetShader(NULL);
  lovrGraphicsSetFont(NULL);
  lovrGraphicsSetCanvas(NULL);
  for (int i = 0; i < MAX_DEFAULT_SHADERS; i++) {
    lovrRelease(state.defaultShaders[i]);
  }
  lovrRelease(state.defaultMaterial);
  lovrRelease(state.defaultFont);
  lovrRelease(state.defaultMesh);
  lovrGpuDestroy();
  memset(&state, 0, sizeof(GraphicsState));
}

void lovrGraphicsPresent() {
  glfwSwapBuffers(state.window);
  lovrGpuPresent();
}

void lovrGraphicsCreateWindow(int w, int h, bool fullscreen, int msaa, const char* title, const char* icon) {
  lovrAssert(!state.window, "Window is already created");

  if ((state.window = glfwGetCurrentContext()) == NULL) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, msaa);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, state.gammaCorrect);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (fullscreen) {
      glfwWindowHint(GLFW_RED_BITS, mode->redBits);
      glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
      glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
      glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    }

    state.window = glfwCreateWindow(w ? w : mode->width, h ? h : mode->height, title, fullscreen ? monitor : NULL, NULL);
    if (!state.window) {
      glfwTerminate();
      lovrThrow("Could not create window");
    }

    if (icon) {
      GLFWimage image;
      size_t size;
      lovrAssert(lovrFilesystemIsFile(icon), "Could not read icon from %s", icon);
      void* data = lovrFilesystemRead(icon, &size);
      lovrAssert(data, "Could not read icon from %s", icon);
      image.pixels = stbi_load_from_memory(data, size, &image.width, &image.height, NULL, 4);
      lovrAssert(image.pixels, "Could not read icon from %s", icon);
      glfwSetWindowIcon(state.window, 1, &image);
      free(image.pixels);
      free(data);
    }

    glfwMakeContextCurrent(state.window);
    glfwSetWindowCloseCallback(state.window, onCloseWindow);
    glfwSetWindowSizeCallback(state.window, onResizeWindow);
  }

#ifndef EMSCRIPTEN
  glfwSwapInterval(0);
#endif
  glfwGetFramebufferSize(state.window, &state.width, &state.height);
  lovrGpuInit(state.gammaCorrect, glfwGetProcAddress);
  VertexFormat format;
  vertexFormatInit(&format);
  vertexFormatAppend(&format, "lovrPosition", ATTR_FLOAT, 3);
  vertexFormatAppend(&format, "lovrNormal", ATTR_FLOAT, 3);
  vertexFormatAppend(&format, "lovrTexCoord", ATTR_FLOAT, 2);
  state.defaultMesh = lovrMeshCreate(64, format, MESH_TRIANGLES, USAGE_STREAM);
  lovrGraphicsReset();
  state.initialized = true;
}

int lovrGraphicsGetWidth() {
  return state.width;
}

int lovrGraphicsGetHeight() {
  return state.height;
}

void lovrGraphicsSetCamera(Camera* camera, bool clear) {
  if (state.camera.canvas && (!camera || camera->canvas != state.camera.canvas)) {
    lovrCanvasResolve(state.camera.canvas);
  }

  if (!camera) {
    state.camera.canvas = NULL;
    state.camera.stereo = false;
    for (int i = 0; i < 2; i++) {
      mat4_identity(state.camera.viewMatrix[i]);
      mat4_perspective(state.camera.projection[i], .01f, 100.f, 67 * M_PI / 180., (float) state.width / state.height);
    }
  } else {
    state.camera = *camera;
  }

  if (clear) {
    Color backgroundColor = lovrGraphicsGetBackgroundColor();
    lovrGpuClear(state.camera.canvas, &backgroundColor, &(float) { 1. }, &(int) { 0 });
  }
}

// State

void lovrGraphicsReset() {
  while (state.pipeline > 0) {
    lovrGraphicsPopPipeline();
  }
  state.transform = 0;
  lovrGraphicsSetCamera(NULL, false);
  lovrGraphicsSetBackgroundColor((Color) { 0, 0, 0, 1 });
  lovrGraphicsSetBlendMode(BLEND_ALPHA, BLEND_ALPHA_MULTIPLY);
  lovrGraphicsSetCanvas(NULL);
  lovrGraphicsSetColor((Color) { 1, 1, 1, 1 });
  lovrGraphicsSetCullingEnabled(false);
  lovrGraphicsSetDefaultFilter((TextureFilter) { .mode = FILTER_TRILINEAR });
  lovrGraphicsSetDepthTest(COMPARE_LEQUAL, true);
  lovrGraphicsSetFont(NULL);
  lovrGraphicsSetLineWidth(1);
  lovrGraphicsSetPointSize(1);
  lovrGraphicsSetShader(NULL);
  lovrGraphicsSetStencilTest(COMPARE_NONE, 0);
  lovrGraphicsSetWinding(WINDING_COUNTERCLOCKWISE);
  lovrGraphicsSetWireframe(false);
  lovrGraphicsOrigin();
}

void lovrGraphicsPushPipeline() {
  lovrAssert(++state.pipeline < MAX_PIPELINES, "Unbalanced pipeline stack (more pushes than pops?)");
  memcpy(&state.pipelines[state.pipeline], &state.pipelines[state.pipeline - 1], sizeof(Pipeline));
  lovrRetain(state.pipelines[state.pipeline].canvas);
  lovrRetain(state.pipelines[state.pipeline].font);
  lovrRetain(state.pipelines[state.pipeline].shader);
}

void lovrGraphicsPopPipeline() {
  lovrRelease(state.pipelines[state.pipeline].canvas);
  lovrRelease(state.pipelines[state.pipeline].font);
  lovrRelease(state.pipelines[state.pipeline].shader);
  lovrAssert(--state.pipeline >= 0, "Unbalanced pipeline stack (more pops than pushes?)");
}

Color lovrGraphicsGetBackgroundColor() {
  return state.pipelines[state.pipeline].backgroundColor;
}

void lovrGraphicsSetBackgroundColor(Color color) {
  state.pipelines[state.pipeline].backgroundColor = color;
}

void lovrGraphicsGetBlendMode(BlendMode* mode, BlendAlphaMode* alphaMode) {
  *mode = state.pipelines[state.pipeline].blendMode;
  *alphaMode = state.pipelines[state.pipeline].blendAlphaMode;
}

void lovrGraphicsSetBlendMode(BlendMode mode, BlendAlphaMode alphaMode) {
  state.pipelines[state.pipeline].blendMode = mode;
  state.pipelines[state.pipeline].blendAlphaMode = alphaMode;
}

Canvas* lovrGraphicsGetCanvas() {
  return state.pipelines[state.pipeline].canvas;
}

void lovrGraphicsSetCanvas(Canvas* canvas) {
  if (state.pipelines[state.pipeline].canvas) {
    lovrCanvasResolve(state.pipelines[state.pipeline].canvas);
  }

  state.pipelines[state.pipeline].canvas = canvas;
}

Color lovrGraphicsGetColor() {
  return state.pipelines[state.pipeline].color;
}

void lovrGraphicsSetColor(Color color) {
  state.pipelines[state.pipeline].color = color;
}

bool lovrGraphicsIsCullingEnabled() {
  return state.pipelines[state.pipeline].culling;
}

void lovrGraphicsSetCullingEnabled(bool culling) {
  state.pipelines[state.pipeline].culling = culling;
}

TextureFilter lovrGraphicsGetDefaultFilter() {
  return state.defaultFilter;
}

void lovrGraphicsSetDefaultFilter(TextureFilter filter) {
  state.defaultFilter = filter;
}

void lovrGraphicsGetDepthTest(CompareMode* mode, bool* write) {
  *mode = state.pipelines[state.pipeline].depthTest;
  *write = state.pipelines[state.pipeline].depthWrite;
}

void lovrGraphicsSetDepthTest(CompareMode mode, bool write) {
  state.pipelines[state.pipeline].depthTest = mode;
  state.pipelines[state.pipeline].depthWrite = write;
}

Font* lovrGraphicsGetFont() {
  if (!state.pipelines[state.pipeline].font) {
    if (!state.defaultFont) {
      Rasterizer* rasterizer = lovrRasterizerCreate(NULL, 32);
      state.defaultFont = lovrFontCreate(rasterizer);
      lovrRelease(rasterizer);
    }

    lovrGraphicsSetFont(state.defaultFont);
  }

  return state.pipelines[state.pipeline].font;
}

void lovrGraphicsSetFont(Font* font) {
  lovrRetain(font);
  lovrRelease(state.pipelines[state.pipeline].font);
  state.pipelines[state.pipeline].font = font;
}

bool lovrGraphicsIsGammaCorrect() {
  return state.gammaCorrect;
}

float lovrGraphicsGetLineWidth() {
  return state.pipelines[state.pipeline].lineWidth;
}

void lovrGraphicsSetLineWidth(float width) {
  state.pipelines[state.pipeline].lineWidth = width;
}

float lovrGraphicsGetPointSize() {
  return state.pipelines[state.pipeline].pointSize;
}

void lovrGraphicsSetPointSize(float size) {
  state.pipelines[state.pipeline].pointSize = size;
}

Shader* lovrGraphicsGetShader() {
  return state.pipelines[state.pipeline].shader;
}

void lovrGraphicsSetShader(Shader* shader) {
  lovrAssert(!shader || lovrShaderGetType(shader) == SHADER_GRAPHICS, "Compute shaders can not be set as the active shader");
  lovrRetain(shader);
  lovrRelease(state.pipelines[state.pipeline].shader);
  state.pipelines[state.pipeline].shader = shader;
}

void lovrGraphicsGetStencilTest(CompareMode* mode, int* value) {
  *mode = state.pipelines[state.pipeline].stencilMode;
  *value = state.pipelines[state.pipeline].stencilValue;
}

void lovrGraphicsSetStencilTest(CompareMode mode, int value) {
  state.pipelines[state.pipeline].stencilMode = mode;
  state.pipelines[state.pipeline].stencilValue = value;
}

Winding lovrGraphicsGetWinding() {
  return state.pipelines[state.pipeline].winding;
}

void lovrGraphicsSetWinding(Winding winding) {
  state.pipelines[state.pipeline].winding = winding;
}

bool lovrGraphicsIsWireframe() {
  return state.pipelines[state.pipeline].wireframe;
}

void lovrGraphicsSetWireframe(bool wireframe) {
#ifndef EMSCRIPTEN
  state.pipelines[state.pipeline].wireframe = wireframe;
#endif
}

// Transforms

void lovrGraphicsPush() {
  lovrAssert(++state.transform < MAX_TRANSFORMS, "Unbalanced matrix stack (more pushes than pops?)");
  mat4_init(state.transforms[state.transform], state.transforms[state.transform - 1]);
}

void lovrGraphicsPop() {
  lovrAssert(--state.transform >= 0, "Unbalanced matrix stack (more pops than pushes?)");
}

void lovrGraphicsOrigin() {
  mat4_identity(state.transforms[state.transform]);
}

void lovrGraphicsTranslate(float x, float y, float z) {
  mat4_translate(state.transforms[state.transform], x, y, z);
}

void lovrGraphicsRotate(float angle, float ax, float ay, float az) {
  mat4_rotate(state.transforms[state.transform], angle, ax, ay, az);
}

void lovrGraphicsScale(float x, float y, float z) {
  mat4_scale(state.transforms[state.transform], x, y, z);
}

void lovrGraphicsMatrixTransform(mat4 transform) {
  mat4_multiply(state.transforms[state.transform], transform);
}

// Rendering

VertexPointer lovrGraphicsGetVertexPointer(uint32_t count) {
  lovrMeshResize(state.defaultMesh, count);
  return lovrMeshMapVertices(state.defaultMesh, 0, count, false, true);
}

void lovrGraphicsClear(Color* color, float* depth, int* stencil) {
  Pipeline* pipeline = &state.pipelines[state.pipeline];
  Canvas* canvas = pipeline->canvas ? pipeline->canvas : state.camera.canvas;
  lovrGpuClear(canvas, color, depth, stencil);
}

void lovrGraphicsDraw(DrawCommand* draw) {
  Mesh* mesh = draw->mesh;
  if (!mesh) {
    int drawCount = draw->range.count ? draw->range.count : (draw->index.count ? draw->index.count : draw->vertex.count);
    mesh = state.defaultMesh;
    lovrMeshSetDrawMode(mesh, draw->mode);
    lovrMeshSetDrawRange(mesh, draw->range.start, drawCount);
    if (draw->vertex.count) {
      VertexPointer vertexPointer = lovrGraphicsGetVertexPointer(draw->vertex.count);
      memcpy(vertexPointer.raw, draw->vertex.data, draw->vertex.count * 8 * sizeof(float));
      if (draw->index.count) {
        IndexPointer indexPointer = lovrMeshWriteIndices(mesh, draw->index.count, sizeof(uint16_t));
        memcpy(indexPointer.shorts, draw->index.data, draw->index.count * sizeof(uint16_t));
      } else {
        lovrMeshWriteIndices(mesh, 0, 0);
      }
    }
  }

  if (draw->transform) {
    lovrGraphicsPush();
    lovrGraphicsMatrixTransform(draw->transform);
  }

  Pipeline* pipeline = &state.pipelines[state.pipeline];

  Canvas* canvas = pipeline->canvas ? pipeline->canvas : state.camera.canvas;
  bool stereo = !draw->forceMono && (canvas ? lovrCanvasIsStereo(canvas) : state.camera.stereo);
  float w = (canvas ? lovrCanvasGetWidth(canvas) : state.width) >> stereo;
  float h = canvas ? lovrCanvasGetHeight(canvas) : state.height;
  float viewports[2][4] = { { 0, 0, w, h, }, { w, 0, w, h } };
  int viewportCount = 1 + stereo;

  Material* material = draw->material ? draw->material : state.defaultMaterial;
  if (!material) material = state.defaultMaterial = lovrMaterialCreate();
  if (!draw->material) {
    for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
      lovrMaterialSetTexture(material, i, draw->textures[i]);
    }
  }

  Shader* shader = pipeline->shader ? pipeline->shader : state.defaultShaders[draw->shader];
  if (!shader) shader = state.defaultShaders[draw->shader] = lovrShaderCreateDefault(draw->shader);

  mat4 transform = state.transforms[state.transform];
  lovrShaderSetMatrices(shader, "lovrModel", transform, 0, 16);
  lovrShaderSetMatrices(shader, "lovrViews", state.camera.viewMatrix[0], 0, 32);
  lovrShaderSetMatrices(shader, "lovrProjections", state.camera.projection[0], 0, 32);

  float modelView[32];
  mat4_multiply(mat4_set(modelView, state.camera.viewMatrix[0]), transform);
  mat4_multiply(mat4_set(modelView + 16, state.camera.viewMatrix[1]), transform);
  lovrShaderSetMatrices(shader, "lovrTransforms", modelView, 0, 32);

  if (lovrShaderHasUniform(shader, "lovrNormalMatrices")) {
    if (mat4_invert(modelView) && mat4_invert(modelView + 16)) {
      mat4_transpose(modelView);
      mat4_transpose(modelView + 16);
    } else {
      mat4_identity(modelView);
      mat4_identity(modelView + 16);
    }

    float normalMatrices[18] = {
      modelView[0], modelView[1], modelView[2],
      modelView[4], modelView[5], modelView[6],
      modelView[8], modelView[9], modelView[10],

      modelView[16], modelView[17], modelView[18],
      modelView[20], modelView[21], modelView[22],
      modelView[24], modelView[25], modelView[26]
    };

    lovrShaderSetMatrices(shader, "lovrNormalMatrices", normalMatrices, 0, 18);
  }

  float* pose = lovrMeshGetPose(mesh);
  if (pose) {
    lovrShaderSetMatrices(shader, "lovrPose", pose, 0, MAX_BONES * 16);
  } else {
    float identity[16];
    mat4_identity(identity);
    lovrShaderSetMatrices(shader, "lovrPose", identity, 0, 16);
  }

  lovrShaderSetInts(shader, "lovrViewportCount", &viewportCount, 0, 1);
  lovrShaderSetColor(shader, "lovrColor", pipeline->color);
  lovrShaderSetFloats(shader, "lovrPointSize", &pipeline->pointSize, 0, 1);

  for (int i = 0; i < MAX_MATERIAL_SCALARS; i++) {
    float value = lovrMaterialGetScalar(material, i);
    lovrShaderSetFloats(shader, lovrShaderScalarUniforms[i], &value, 0, 1);
  }

  for (int i = 0; i < MAX_MATERIAL_COLORS; i++) {
    lovrShaderSetColor(shader, lovrShaderColorUniforms[i], lovrMaterialGetColor(material, i));
  }

  for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
    Texture* texture = lovrMaterialGetTexture(material, i);
    lovrShaderSetTextures(shader, lovrShaderTextureUniforms[i], &texture, 0, 1);
  }

  lovrShaderSetMatrices(shader, "lovrMaterialTransform", material->transform, 0, 9);

  // Bind
  lovrCanvasBind(canvas, true);
  lovrGpuBindPipeline(pipeline);
  lovrMeshBind(mesh, shader, viewportCount);

  // Draw
  if (lovrGpuGetSupported()->singlepass) {
    int instances = MAX(draw->instances, 1) * viewportCount;
    lovrGpuSetViewports(&viewports[0][0], viewportCount);
    lovrShaderBind(shader);
    lovrMeshDraw(mesh, instances);
  } else {
    for (int i = 0; i < viewportCount; i++) {
      lovrGpuSetViewports(&viewports[i][0], 1);
      lovrShaderSetInts(shader, "lovrViewportIndex", &i, 0, 1);
      lovrShaderBind(shader);
      lovrMeshDraw(mesh, draw->instances);
    }
  }

  if (draw->transform) {
    lovrGraphicsPop();
  }
}

void lovrGraphicsPoints(uint32_t count) {
  lovrGraphicsDraw(&(DrawCommand) {
    .mode = MESH_POINTS,
    .range = { 0, count }
  });
}

void lovrGraphicsLine(uint32_t count) {
  lovrGraphicsDraw(&(DrawCommand) {
    .mode = MESH_LINE_STRIP,
    .range = { 0, count }
  });
}

void lovrGraphicsTriangle(DrawMode mode, Material* material, float points[9]) {
  if (mode == DRAW_MODE_LINE) {
    lovrGraphicsDraw(&(DrawCommand) {
      .material = material,
      .mode = MESH_LINE_LOOP,
      .vertex.count = 3,
      .vertex.data = (float[]) {
        points[0], points[1], points[2], 0, 0, 0, 0, 0,
        points[3], points[4], points[5], 0, 0, 0, 0, 0,
        points[6], points[7], points[8], 0, 0, 0, 0, 0
      }
    });
  } else {
    float normal[3];
    vec3_cross(vec3_init(normal, &points[0]), &points[3]);
    lovrGraphicsDraw(&(DrawCommand) {
      .material = material,
      .mode = MESH_TRIANGLES,
      .vertex.count = 3,
      .vertex.data = (float[]) {
        points[0], points[1], points[2], normal[0], normal[1], normal[2], 0, 0,
        points[3], points[4], points[5], normal[0], normal[1], normal[2], 0, 0,
        points[6], points[7], points[8], normal[0], normal[1], normal[2], 0, 0
      }
    });
  }
}

void lovrGraphicsPlane(DrawMode mode, Material* material, mat4 transform) {
  if (mode == DRAW_MODE_LINE) {
    lovrGraphicsDraw(&(DrawCommand) {
      .transform = transform,
      .material = material,
      .mode = MESH_LINE_LOOP,
      .vertex.count = 4,
      .vertex.data = (float[]) {
        -.5, .5, 0,  0, 0, 0, 0, 0,
        .5, .5, 0,   0, 0, 0, 0, 0,
        .5, -.5, 0,  0, 0, 0, 0, 0,
        -.5, -.5, 0, 0, 0, 0, 0, 0
      }
    });
  } else if (mode == DRAW_MODE_FILL) {
    lovrGraphicsDraw(&(DrawCommand) {
      .transform = transform,
      .material = material,
      .mode = MESH_TRIANGLE_STRIP,
      .vertex.count = 4,
      .vertex.data = (float[]) {
        -.5, .5, 0,  0, 0, -1, 0, 1,
        -.5, -.5, 0, 0, 0, -1, 0, 0,
        .5, .5, 0,   0, 0, -1, 1, 1,
        .5, -.5, 0,  0, 0, -1, 1, 0
      }
    });
  }
}

void lovrGraphicsBox(DrawMode mode, Material* material, mat4 transform) {
  if (mode == DRAW_MODE_LINE) {
    lovrGraphicsDraw(&(DrawCommand) {
      .transform = transform,
      .material = material,
      .mode = MESH_LINES,
      .vertex.count = 8,
      .vertex.data = (float[]) {
        // Front
        -.5, .5, -.5,  0, 0, 0, 0, 0,
        .5, .5, -.5,   0, 0, 0, 0, 0,
        .5, -.5, -.5,  0, 0, 0, 0, 0,
        -.5, -.5, -.5, 0, 0, 0, 0, 0,

        // Back
        -.5, .5, .5,   0, 0, 0, 0, 0,
        .5, .5, .5,    0, 0, 0, 0, 0,
        .5, -.5, .5,   0, 0, 0, 0, 0,
        -.5, -.5, .5,  0, 0, 0, 0, 0
      },
      .index.count = 24,
      .index.data = (uint16_t[]) {
        0, 1, 1, 2, 2, 3, 3, 0, // Front
        4, 5, 5, 6, 6, 7, 7, 4, // Back
        0, 4, 1, 5, 2, 6, 3, 7  // Connections
      }
    });
  } else {
    lovrGraphicsDraw(&(DrawCommand) {
      .transform = transform,
      .material = material,
      .mode = MESH_TRIANGLE_STRIP,
      .vertex.count = 26,
      .vertex.data = (float[]) {
        // Front
        -.5, -.5, -.5,  0, 0, -1, 0, 0,
        -.5, .5, -.5,   0, 0, -1, 0, 1,
        .5, -.5, -.5,   0, 0, -1, 1, 0,
        .5, .5, -.5,    0, 0, -1, 1, 1,

        // Right
        .5, .5, -.5,    1, 0, 0,  0, 1,
        .5, .5, .5,     1, 0, 0,  1, 1,
        .5, -.5, -.5,   1, 0, 0,  0, 0,
        .5, -.5, .5,    1, 0, 0,  1, 0,

        // Back
        .5, -.5, .5,    0, 0, 1,  0, 0,
        .5, .5, .5,     0, 0, 1,  0, 1,
        -.5, -.5, .5,   0, 0, 1,  1, 0,
        -.5, .5, .5,    0, 0, 1,  1, 1,

        // Left
        -.5, .5, .5,   -1, 0, 0,  0, 1,
        -.5, .5, -.5,  -1, 0, 0,  1, 1,
        -.5, -.5, .5,  -1, 0, 0,  0, 0,
        -.5, -.5, -.5, -1, 0, 0,  1, 0,

        // Bottom
        -.5, -.5, -.5,  0, -1, 0, 0, 0,
        .5, -.5, -.5,   0, -1, 0, 1, 0,
        -.5, -.5, .5,   0, -1, 0, 0, 1,
        .5, -.5, .5,    0, -1, 0, 1, 1,

        // Adjust
        .5, -.5, .5,    0, 1, 0,  0, 1,
        -.5, .5, -.5,   0, 1, 0,  0, 1,

        // Top
        -.5, .5, -.5,   0, 1, 0,  0, 1,
        -.5, .5, .5,    0, 1, 0,  0, 0,
        .5, .5, -.5,    0, 1, 0,  1, 1,
        .5, .5, .5,     0, 1, 0,  1, 0
      }
    });
  }
}

void lovrGraphicsArc(DrawMode mode, ArcMode arcMode, Material* material, mat4 transform, float theta1, float theta2, int segments) {
  if (fabsf(theta1 - theta2) >= 2 * M_PI) {
    theta1 = 0;
    theta2 = 2 * M_PI;
  }

  bool hasCenterPoint = arcMode == ARC_MODE_PIE && fabsf(theta1 - theta2) < 2 * M_PI;
  uint32_t count = segments + 1 + hasCenterPoint;
  VertexPointer vertices = lovrGraphicsGetVertexPointer(count);
  lovrMeshWriteIndices(state.defaultMesh, 0, 0);

  if (hasCenterPoint) {
    memcpy(vertices.floats, ((float[]) { 0, 0, 0, 0, 0, 1, .5, .5 }), 8 * sizeof(float));
    vertices.floats += 8;
  }

  float theta = theta1;
  float angleShift = (theta2 - theta1) / (float) segments;

  for (int i = 0; i <= segments; i++) {
    float x = cos(theta) * .5;
    float y = sin(theta) * .5;
    memcpy(vertices.floats, ((float[]) { x, y, 0, 0, 0, 1, x + .5, 1 - (y + .5) }), 8 * sizeof(float));
    vertices.floats += 8;
    theta += angleShift;
  }

  lovrGraphicsDraw(&(DrawCommand) {
    .transform = transform,
    .material = material,
    .mode = mode == DRAW_MODE_LINE ? (arcMode == ARC_MODE_OPEN ? MESH_LINE_STRIP : MESH_LINE_LOOP) : MESH_TRIANGLE_FAN,
    .range = { 0, count }
  });
}

void lovrGraphicsCircle(DrawMode mode, Material* material, mat4 transform, int segments) {
  lovrGraphicsArc(mode, ARC_MODE_OPEN, material, transform, 0, 2 * M_PI, segments);
}

void lovrGraphicsCylinder(Material* material, float x1, float y1, float z1, float x2, float y2, float z2, float r1, float r2, bool capped, int segments) {
  float axis[3] = { x1 - x2, y1 - y2, z1 - z2 };
  float n[3] = { x1 - x2, y1 - y2, z1 - z2 };
  float p[3];
  float q[3];

  uint32_t vertexCount = ((capped && r1) * (segments + 2) + (capped && r2) * (segments + 2) + 2 * (segments + 1));
  uint32_t indexCount = 3 * segments * ((capped && r1) + (capped && r2) + 2);

  VertexPointer vertices = lovrGraphicsGetVertexPointer(vertexCount);
  IndexPointer indices = lovrMeshWriteIndices(state.defaultMesh, indexCount, sizeof(uint32_t));
  float* baseVertex = vertices.floats;

  vec3_init(p, n);

  if (n[0] == 0 && n[2] == 0) {
    p[0] += 1;
  } else {
    p[1] += 1;
  }

  vec3_init(q, p);
  vec3_cross(q, n);
  vec3_cross(n, q);
  vec3_init(p, n);
  vec3_normalize(p);
  vec3_normalize(q);
  vec3_normalize(axis);

#define PUSH_CYLINDER_VERTEX(x, y, z, nx, ny, nz) \
  *vertices.floats++ = x; \
  *vertices.floats++ = y; \
  *vertices.floats++ = z; \
  *vertices.floats++ = nx; \
  *vertices.floats++ = ny; \
  *vertices.floats++ = nz; \
  *vertices.floats++ = 0; \
  *vertices.floats++ = 0;
#define PUSH_CYLINDER_TRIANGLE(i1, i2, i3) \
  *indices.ints++ = i1; \
  *indices.ints++ = i2; \
  *indices.ints++ = i3; \

  // Ring
  for (int i = 0; i <= segments; i++) {
    float theta = i * (2 * M_PI) / segments;
    n[0] = cos(theta) * p[0] + sin(theta) * q[0];
    n[1] = cos(theta) * p[1] + sin(theta) * q[1];
    n[2] = cos(theta) * p[2] + sin(theta) * q[2];
    PUSH_CYLINDER_VERTEX(x1 + r1 * n[0], y1 + r1 * n[1], z1 + r1 * n[2], n[0], n[1], n[2]);
    PUSH_CYLINDER_VERTEX(x2 + r2 * n[0], y2 + r2 * n[1], z2 + r2 * n[2], n[0], n[1], n[2]);
  }

  // Top
  int topOffset = (segments + 1) * 2;
  if (capped && r1 != 0) {
    PUSH_CYLINDER_VERTEX(x1, y1, z1, axis[0], axis[1], axis[2]);
    for (int i = 0; i <= segments; i++) {
      int j = i * 2 * 8;
      PUSH_CYLINDER_VERTEX(baseVertex[j + 0], baseVertex[j + 1], baseVertex[j + 2], axis[0], axis[1], axis[2]);
    }
  }

  // Bottom
  int bottomOffset = (segments + 1) * 2 + (1 + segments + 1) * (capped && r1 != 0);
  if (capped && r2 != 0) {
    PUSH_CYLINDER_VERTEX(x2, y2, z1, -axis[0], -axis[1], -axis[2]);
    for (int i = 0; i <= segments; i++) {
      int j = i * 2 * 8 + 8;
      PUSH_CYLINDER_VERTEX(baseVertex[j + 0], baseVertex[j + 1], baseVertex[j + 2], -axis[0], -axis[1], -axis[2]);
    }
  }

  // Indices
  for (int i = 0; i < segments; i++) {
    int j = 2 * i;
    PUSH_CYLINDER_TRIANGLE(j, j + 1, j + 2);
    PUSH_CYLINDER_TRIANGLE(j + 1, j + 3, j + 2);

    if (capped && r1 != 0) {
      PUSH_CYLINDER_TRIANGLE(topOffset, topOffset + i + 1, topOffset + i + 2);
    }

    if (capped && r2 != 0) {
      PUSH_CYLINDER_TRIANGLE(bottomOffset, bottomOffset + i + 1, bottomOffset + i + 2);
    }
  }
#undef PUSH_CYLINDER_VERTEX
#undef PUSH_CYLINDER_TRIANGLE

  lovrGraphicsDraw(&(DrawCommand) {
    .material = material,
    .mode = MESH_TRIANGLES,
    .range = { 0, indexCount }
  });
}

void lovrGraphicsSphere(Material* material, mat4 transform, int segments) {
  VertexPointer vertices = lovrGraphicsGetVertexPointer((segments + 1) * (segments + 1));
  IndexPointer indices = lovrMeshWriteIndices(state.defaultMesh, segments * segments * 6, sizeof(uint32_t));

  for (int i = 0; i <= segments; i++) {
    float v = i / (float) segments;
    for (int j = 0; j <= segments; j++) {
      float u = j / (float) segments;

      float x = sin(u * 2 * M_PI) * sin(v * M_PI);
      float y = cos(v * M_PI);
      float z = -cos(u * 2 * M_PI) * sin(v * M_PI);
      memcpy(vertices.floats, ((float[]) { x, y, z, x, y, z, u, 1 - v }), 8 * sizeof(float));
      vertices.floats += 8;
    }
  }

  for (int i = 0; i < segments; i++) {
    unsigned int offset0 = i * (segments + 1);
    unsigned int offset1 = (i + 1) * (segments + 1);
    for (int j = 0; j < segments; j++) {
      unsigned int i0 = offset0 + j;
      unsigned int i1 = offset1 + j;
      memcpy(indices.ints, ((uint32_t[]) { i0, i1, i0 + 1, i1, i1 + 1, i0 + 1 }), 6 * sizeof(uint32_t));
      indices.ints += 6;
    }
  }

  lovrGraphicsDraw(&(DrawCommand) {
    .transform = transform,
    .material = material,
    .mode = MESH_TRIANGLES,
    .range = { 0, segments * segments * 6 }
  });
}

void lovrGraphicsSkybox(Texture* texture, float angle, float ax, float ay, float az) {
  TextureType type = lovrTextureGetType(texture);
  lovrAssert(type == TEXTURE_CUBE || type == TEXTURE_2D, "Only 2D and cube textures can be used as skyboxes");
  lovrGraphicsPushPipeline();
  lovrGraphicsSetWinding(WINDING_COUNTERCLOCKWISE);
  lovrGraphicsDraw(&(DrawCommand) {
    .shader = type == TEXTURE_CUBE ? SHADER_CUBE : SHADER_PANO,
    .textures[TEXTURE_DIFFUSE] = type == TEXTURE_2D ? texture : NULL,
    .textures[TEXTURE_ENVIRONMENT_MAP] = type == TEXTURE_CUBE ? texture : NULL,
    .mode = MESH_TRIANGLE_STRIP,
    .vertex.count = 4,
    .vertex.data = (float[]) {
      -1, 1, 1,  0, 0, 0, 0, 0,
      -1, -1, 1, 0, 0, 0, 0, 0,
      1, 1, 1,   0, 0, 0, 0, 0,
      1, -1, 1,  0, 0, 0, 0, 0
    }
  });
  lovrGraphicsPopPipeline();
}

void lovrGraphicsPrint(const char* str, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign) {
  Font* font = lovrGraphicsGetFont();
  float scale = 1 / font->pixelDensity;
  float offsety;
  uint32_t vertexCount;
  uint32_t maxVertices = strlen(str) * 6;
  VertexPointer vertexPointer = lovrGraphicsGetVertexPointer(maxVertices);
  lovrFontRender(font, str, wrap, halign, valign, vertexPointer, &offsety, &vertexCount);
  lovrMeshWriteIndices(state.defaultMesh, 0, 0);

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(transform);
  lovrGraphicsScale(scale, scale, scale);
  lovrGraphicsTranslate(0, offsety, 0);
  lovrGraphicsPushPipeline();
  state.pipelines[state.pipeline].depthWrite = false;
  lovrGraphicsDraw(&(DrawCommand) {
    .shader = SHADER_FONT,
    .textures[TEXTURE_DIFFUSE] = font->texture,
    .mode = MESH_TRIANGLES,
    .range = { 0, vertexCount }
  });
  lovrGraphicsPopPipeline();
  lovrGraphicsPop();
}

void lovrGraphicsFill(Texture* texture, float u, float v, float w, float h) {
  lovrGraphicsPushPipeline();
  lovrGraphicsSetDepthTest(COMPARE_NONE, false);
  lovrGraphicsDraw(&(DrawCommand) {
    .forceMono = true,
    .shader = SHADER_FILL,
    .textures[TEXTURE_DIFFUSE] = texture,
    .mode = MESH_TRIANGLE_STRIP,
    .vertex.count = 4,
    .vertex.data = (float[]) {
      -1, 1, 0,  0, 0, 0, u, v + h,
      -1, -1, 0, 0, 0, 0, u, v,
      1, 1, 0,   0, 0, 0, u + w, v + h,
      1, -1, 0,  0, 0, 0, u + w, v
    }
  });
  lovrGraphicsPopPipeline();
}
