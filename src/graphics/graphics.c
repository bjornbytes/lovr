#include "graphics/graphics.h"
#include "data/rasterizer.h"
#include "resources/shaders.h"
#include "event/event.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include "util.h"
#include "lib/stb/stb_image.h"
#define _USE_MATH_DEFINES
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "graphics/opengl/opengl.h"

static GraphicsState state;

static void onCloseWindow(GLFWwindow* window) {
  if (window == state.window) {
    EventType type = EVENT_QUIT;
    EventData data = { .quit = { false, 0 } };
    Event event = { .type = type, .data = data };
    lovrEventPush(event);
  }
}

// Base

void lovrGraphicsInit() {
  //
}

void lovrGraphicsDestroy() {
  if (!state.initialized) return;
  lovrGraphicsSetShader(NULL);
  lovrGraphicsSetFont(NULL);
  lovrGraphicsSetCanvas(NULL, 0);
  for (int i = 0; i < DEFAULT_SHADER_COUNT; i++) {
    lovrRelease(state.defaultShaders[i]);
  }
  lovrRelease(state.defaultMaterial);
  lovrRelease(state.defaultFont);
  lovrRelease(state.mesh);
  gpuDestroy();
  memset(&state, 0, sizeof(GraphicsState));
}

void lovrGraphicsReset() {
  int width, height;
  lovrGraphicsGetDimensions(&width, &height);
  state.transform = 0;
  state.layer = 0;
  memcpy(state.layers[state.layer].viewport, (int[]) { 0, 0, width, height }, 4 * sizeof(uint32_t));
  mat4_perspective(state.layers[state.layer].projection, .01f, 100.f, 67 * M_PI / 180., (float) width / height);
  mat4_identity(state.layers[state.layer].view);
  lovrGraphicsSetBackgroundColor((Color) { 0, 0, 0, 1. });
  lovrGraphicsSetBlendMode(BLEND_ALPHA, BLEND_ALPHA_MULTIPLY);
  lovrGraphicsSetCanvas(NULL, 0);
  lovrGraphicsSetColor((Color) { 1., 1., 1., 1. });
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

void lovrGraphicsClear(Color* color, float* depth, int* stencil) {
  Layer layer = state.layers[state.layer];
  gpuClear(layer.canvas, layer.canvasCount, color, depth, stencil);
}

void lovrGraphicsPresent() {
  glfwSwapBuffers(state.window);
  gpuPresent();
}

void lovrGraphicsCreateWindow(int w, int h, bool fullscreen, int msaa, const char* title, const char* icon) {
  lovrAssert(!state.window, "Window is already created");

#ifndef EMSCRIPTEN
  if ((state.window = glfwGetCurrentContext()) == NULL) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, msaa);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, state.gammaCorrect);
#else
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SAMPLES, msaa);
    glfwWindowHint(GLFW_SRGB_CAPABLE, state.gammaCorrect);
#endif

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
      image.pixels = stbi_load(icon, &image.width, &image.height, NULL, 3);
      glfwSetWindowIcon(state.window, 1, &image);
      free(image.pixels);
    }

    glfwMakeContextCurrent(state.window);
    glfwSetWindowCloseCallback(state.window, onCloseWindow);
#ifndef EMSCRIPTEN
  }

  glfwSwapInterval(0);
#endif
  gpuInit(state.gammaCorrect, glfwGetProcAddress);
  VertexFormat format;
  vertexFormatInit(&format);
  vertexFormatAppend(&format, "lovrPosition", ATTR_FLOAT, 3);
  vertexFormatAppend(&format, "lovrNormal", ATTR_FLOAT, 3);
  vertexFormatAppend(&format, "lovrTexCoord", ATTR_FLOAT, 2);
  state.mesh = lovrMeshCreate(64, format, MESH_TRIANGLES, MESH_STREAM);
  lovrGraphicsReset();
  state.initialized = true;
}

void lovrGraphicsGetDimensions(int* width, int* height) {
  glfwGetFramebufferSize(state.window, width, height);
}

// State

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

void lovrGraphicsGetCanvas(Canvas** canvas, int* count) {
  Layer layer = state.layers[state.layer];
  if (layer.user) {
    *count = layer.canvasCount;
    memcpy(canvas, layer.canvas, layer.canvasCount * sizeof(Canvas*));
  } else {
    *count = 0;
  }
}

void lovrGraphicsSetCanvas(Canvas** canvas, int count) {
  if (state.layers[state.layer].user) {
    lovrGraphicsPopLayer();
  }

  lovrGraphicsPushLayer(canvas, count, true);
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

void lovrGraphicsSetGammaCorrect(bool gammaCorrect) {
  state.gammaCorrect = gammaCorrect;
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
  if (shader != state.pipelines[state.pipeline].shader) {
    lovrRetain(shader);
    lovrRelease(state.pipelines[state.pipeline].shader);
    state.pipelines[state.pipeline].shader = shader;
  }
}

void lovrGraphicsGetStencilTest(CompareMode* mode, int* value) {
  *mode = state.pipelines[state.pipeline].stencilMode;
  *value = state.pipelines[state.pipeline].stencilValue;
}

void lovrGraphicsSetStencilTest(CompareMode mode, int value) {
  if (state.stencilWriting) {
    return;
  }

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
  if (++state.transform >= MAX_TRANSFORMS) {
    lovrThrow("Unbalanced matrix stack (more pushes than pops?)");
  }

  memcpy(state.transforms[state.transform], state.transforms[state.transform - 1], 2 * 16 * sizeof(float));
}

void lovrGraphicsPop() {
  if (--state.transform < 0) {
    lovrThrow("Unbalanced matrix stack (more pops than pushes?)");
  }
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

// Primitives

VertexPointer lovrGraphicsGetVertexPointer(uint32_t count) {
  lovrMeshResize(state.mesh, count);
  return lovrMeshMapVertices(state.mesh, 0, count, false, true);
}

void lovrGraphicsPoints(uint32_t count) {
  lovrGraphicsDraw(&(GraphicsDraw) {
    .mode = MESH_POINTS,
    .range = { 0, count }
  });
}

void lovrGraphicsLine(uint32_t count) {
  lovrGraphicsDraw(&(GraphicsDraw) {
    .mode = MESH_LINE_STRIP,
    .range = { 0, count }
  });
}

void lovrGraphicsTriangle(DrawMode mode, Material* material, float points[9]) {
  if (mode == DRAW_MODE_LINE) {
    lovrGraphicsDraw(&(GraphicsDraw) {
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
    lovrGraphicsDraw(&(GraphicsDraw) {
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
    lovrGraphicsDraw(&(GraphicsDraw) {
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
    lovrGraphicsDraw(&(GraphicsDraw) {
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
    lovrGraphicsDraw(&(GraphicsDraw) {
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
    lovrGraphicsDraw(&(GraphicsDraw) {
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
  lovrMeshWriteIndices(state.mesh, 0, 0);

  if (hasCenterPoint) {
    memcpy(vertices.floats, (float[]) { 0, 0, 0, 0, 0, 1, .5, .5 }, 8 * sizeof(float));
    vertices.floats += 8;
  }

  float theta = theta1;
  float angleShift = (theta2 - theta1) / (float) segments;

  for (int i = 0; i <= segments; i++) {
    float x = cos(theta) * .5;
    float y = sin(theta) * .5;
    memcpy(vertices.floats, (float[]) { x, y, 0, 0, 0, 1, x + .5, 1 - (y + .5) }, 8 * sizeof(float));
    vertices.floats += 8;
    theta += angleShift;
  }

  lovrGraphicsDraw(&(GraphicsDraw) {
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
  IndexPointer indices = lovrMeshWriteIndices(state.mesh, indexCount, sizeof(uint32_t));
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

  lovrGraphicsDraw(&(GraphicsDraw) {
    .material = material,
    .mode = MESH_TRIANGLES,
    .range = { 0, indexCount }
  });
}

void lovrGraphicsSphere(Material* material, mat4 transform, int segments) {
  VertexPointer vertices = lovrGraphicsGetVertexPointer((segments + 1) * (segments + 1));
  IndexPointer indices = lovrMeshWriteIndices(state.mesh, segments * segments * 6, sizeof(uint32_t));

  for (int i = 0; i <= segments; i++) {
    float v = i / (float) segments;
    for (int j = 0; j <= segments; j++) {
      float u = j / (float) segments;

      float x = sin(u * 2 * M_PI) * sin(v * M_PI);
      float y = cos(v * M_PI);
      float z = -cos(u * 2 * M_PI) * sin(v * M_PI);
      memcpy(vertices.floats, (float[]) { x, y, z, x, y, z, u, 1 - v }, 8 * sizeof(float));
      vertices.floats += 8 * sizeof(float);
    }
  }

  for (int i = 0; i < segments; i++) {
    unsigned int offset0 = i * (segments + 1);
    unsigned int offset1 = (i + 1) * (segments + 1);
    for (int j = 0; j < segments; j++) {
      unsigned int i0 = offset0 + j;
      unsigned int i1 = offset1 + j;
      memcpy(indices.ints, (uint32_t[]) { i0, i1, i0 + 1, i1, i1 + 1, i0 + 1}, 6 * sizeof(uint32_t));
      indices.ints += 6 * sizeof(uint32_t);
    }
  }

  lovrGraphicsDraw(&(GraphicsDraw) {
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
  lovrGraphicsDraw(&(GraphicsDraw) {
    .shader = type == TEXTURE_CUBE ? SHADER_CUBE : SHADER_PANO,
    .textures[TEXTURE_DIFFUSE] = texture,
    .textures[TEXTURE_ENVIRONMENT_MAP] = texture,
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
  lovrMeshWriteIndices(state.mesh, 0, 0);

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(transform);
  lovrGraphicsScale(scale, scale, scale);
  lovrGraphicsTranslate(0, offsety, 0);
  lovrGraphicsPushPipeline();
  state.pipelines[state.pipeline].depthWrite = false;
  lovrGraphicsDraw(&(GraphicsDraw) {
    .shader = SHADER_FONT,
    .textures[TEXTURE_DIFFUSE] = font->texture,
    .mode = MESH_TRIANGLES,
    .range = { 0, vertexCount }
  });
  lovrGraphicsPopPipeline();
  lovrGraphicsPop();
}

void lovrGraphicsStencil(StencilAction action, int replaceValue, StencilCallback callback, void* userdata) {
  CompareMode mode;
  bool write;
  lovrGraphicsGetDepthTest(&mode, &write);
  lovrGraphicsSetDepthTest(mode, false);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  if (!state.stencilEnabled) {
    glEnable(GL_STENCIL_TEST);
    state.stencilEnabled = true;
  }

  GLenum glAction;
  switch (action) {
    case STENCIL_REPLACE: glAction = GL_REPLACE; break;
    case STENCIL_INCREMENT: glAction = GL_INCR; break;
    case STENCIL_DECREMENT: glAction = GL_DECR; break;
    case STENCIL_INCREMENT_WRAP: glAction = GL_INCR_WRAP; break;
    case STENCIL_DECREMENT_WRAP: glAction = GL_DECR_WRAP; break;
    case STENCIL_INVERT: glAction = GL_INVERT; break;
  }

  glStencilFunc(GL_ALWAYS, replaceValue, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, glAction);

  state.stencilWriting = true;
  callback(userdata);
  state.stencilWriting = false;

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  lovrGraphicsSetDepthTest(mode, write);
  lovrGraphicsSetStencilTest(state.pipelines[state.pipeline].stencilMode, state.pipelines[state.pipeline].stencilValue);
}

void lovrGraphicsFill(Texture* texture) {
  lovrGraphicsPushPipeline();
  lovrGraphicsSetDepthTest(COMPARE_NONE, false);
  lovrGraphicsDraw(&(GraphicsDraw) {
    .shader = SHADER_FILL,
    .textures[TEXTURE_DIFFUSE] = texture,
    .mode = MESH_TRIANGLE_STRIP,
    .vertex.count = 4,
    .vertex.data = (float[]) {
      -1, 1, 0,  0, 0, 0, 0, 1,
      -1, -1, 0, 0, 0, 0, 0, 0,
      1, 1, 0,   0, 0, 0, 1, 1,
      1, -1, 0,  0, 0, 0, 1, 0
    }
  });
  lovrGraphicsPopPipeline();
}

// Internal
void lovrGraphicsDraw(GraphicsDraw* draw) {
  if (draw->transform) {
    lovrGraphicsPush();
    lovrGraphicsMatrixTransform(draw->transform);
  }

  Shader* shader = state.pipelines[state.pipeline].shader ? state.pipelines[state.pipeline].shader : state.defaultShaders[draw->shader];
  if (!shader) shader = state.defaultShaders[draw->shader] = lovrShaderCreateDefault(draw->shader);

  Mesh* mesh = draw->mesh;
  if (!mesh) {
    int drawCount = draw->range.count ? draw->range.count : (draw->index.count ? draw->index.count : draw->vertex.count);
    mesh = state.mesh;
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

  Material* material = draw->material;
  if (!material) {
    material = state.defaultMaterial;

    if (!material) {
      material = state.defaultMaterial = lovrMaterialCreate(true);
    }

    for (int i = 0; i < MAX_MATERIAL_TEXTURES; i++) {
      lovrMaterialSetTexture(material, i, draw->textures[i]);
    }
  }

  gpuDraw(&(GpuDrawCommand) {
    .layer = state.layers[state.layer],
    .shader = shader,
    .material = material,
    .transform = state.transforms[state.transform],
    .mesh = mesh,
    .pipeline = state.pipelines[state.pipeline],
    .instances = draw->instances
  });

  if (draw->transform) {
    lovrGraphicsPop();
  }
}

void lovrGraphicsPushLayer(Canvas** canvas, int count, bool user) {
  lovrAssert(count <= MAX_CANVASES, "Attempt to set %d canvases (the maximum is %d)", count, MAX_CANVASES);
  lovrAssert(++state.layer < MAX_LAYERS, "Layer overflow");

  for (int i = 0; i < count; i++) {
    lovrRetain(canvas[i]);
  }

  Layer* prevLayer = &state.layers[state.layer - 1];
  for (int i = 0; i < prevLayer->canvasCount; i++) {
    lovrRelease(prevLayer->canvas[i]);
  }

  Layer* layer = &state.layers[state.layer];
  memcpy(layer, prevLayer, sizeof(Layer));
  layer->canvasCount = count;
  layer->user = user;

  if (count > 0) {
    memcpy(layer->canvas, canvas, count * sizeof(Canvas*));
    gpuBindFramebuffer(lovrCanvasGetId(canvas[0]));

    GLenum buffers[MAX_CANVASES];
    for (int i = 0; i < count; i++) {
      buffers[i] = GL_COLOR_ATTACHMENT0 + i;
      glFramebufferTexture2D(GL_FRAMEBUFFER, buffers[i], GL_TEXTURE_2D, lovrTextureGetId((Texture*) canvas[i]), 0);
    }
    glDrawBuffers(count, buffers);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    lovrAssert(status != GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS, "All multicanvas canvases must have the same dimensions");
    lovrAssert(status == GL_FRAMEBUFFER_COMPLETE, "Unable to bind framebuffer");
  }
}

void lovrGraphicsPopLayer() {
  Layer* layer = &state.layers[state.layer];
  if (layer->canvasCount > 0) {
    lovrCanvasResolve(layer->canvas[0]);
  }

  lovrAssert(--state.layer >= 0, "Layer underflow");
}

void lovrGraphicsPushPipeline() {
  if (++state.pipeline >= MAX_PIPELINES) {
    lovrThrow("Unbalanced pipeline stack (more pushes than pops?)");
  }

  memcpy(&state.pipelines[state.pipeline], &state.pipelines[state.pipeline - 1], sizeof(Pipeline));
}

void lovrGraphicsPopPipeline() {
  if (--state.pipeline < 0) {
    lovrThrow("Unbalanced pipeline stack (more pops than pushes?)");
  }
}

void lovrGraphicsSetCamera(mat4 projection, mat4 view) {
  mat4_set(state.layers[state.layer].projection, projection);
  mat4_set(state.layers[state.layer].view, view);
}

void lovrGraphicsSetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
  state.layers[state.layer].viewport[0] = x;
  state.layers[state.layer].viewport[1] = y;
  state.layers[state.layer].viewport[2] = width;
  state.layers[state.layer].viewport[3] = height;
}
