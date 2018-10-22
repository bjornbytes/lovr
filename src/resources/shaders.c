#include "resources/shaders.h"

const char* lovrShaderVertexPrefix = ""
"#define VERTEX VERTEX \n"
"#define MAX_BONES 48 \n"
"#define lovrView lovrViews[lovrViewportIndex] \n"
"#define lovrProjection lovrProjections[lovrViewportIndex] \n"
"#define lovrTransform lovrTransforms[lovrViewportIndex] \n"
"#define lovrNormalMatrix lovrNormalMatrices[lovrViewportIndex] \n"
"#define lovrInstanceID (gl_InstanceID / lovrViewportCount) \n"
"#define lovrPoseMatrix ("
  "lovrPose[lovrBones[0]] * lovrBoneWeights[0] +"
  "lovrPose[lovrBones[1]] * lovrBoneWeights[1] +"
  "lovrPose[lovrBones[2]] * lovrBoneWeights[2] +"
  "lovrPose[lovrBones[3]] * lovrBoneWeights[3]"
  ") \n"
"in vec3 lovrPosition; \n"
"in vec3 lovrNormal; \n"
"in vec2 lovrTexCoord; \n"
"in vec4 lovrVertexColor; \n"
"in vec3 lovrTangent; \n"
"in ivec4 lovrBones; \n"
"in vec4 lovrBoneWeights; \n"
"out vec2 texCoord; \n"
"out vec4 vertexColor; \n"
"uniform mat4 lovrModel; \n"
"uniform mat4 lovrViews[2]; \n"
"uniform mat4 lovrProjections[2]; \n"
"uniform mat4 lovrTransforms[2]; \n"
"uniform mat3 lovrNormalMatrices[2]; \n"
"uniform mat3 lovrMaterialTransform; \n"
"uniform float lovrPointSize; \n"
"uniform mat4 lovrPose[MAX_BONES]; \n"
"uniform int lovrViewportCount; \n"
"#if SINGLEPASS \n"
"#define lovrViewportIndex gl_ViewportIndex \n"
"#else \n"
"uniform int lovrViewportIndex; \n"
"#endif \n"
"#line 0 \n";

const char* lovrShaderVertexSuffix = ""
"void main() { \n"
"  texCoord = (lovrMaterialTransform * vec3(lovrTexCoord, 1.)).xy; \n"
"  vertexColor = lovrVertexColor; \n"
"#if SINGLEPASS \n"
"  gl_ViewportIndex = gl_InstanceID % lovrViewportCount; \n"
"#endif \n"
"  gl_PointSize = lovrPointSize; \n"
"  gl_Position = position(lovrProjection, lovrTransform, vec4(lovrPosition, 1.0)); \n"
"}";

const char* lovrShaderFragmentPrefix = ""
"#define PIXEL PIXEL \n"
"#define FRAGMENT FRAGMENT \n"
"in vec2 texCoord; \n"
"in vec4 vertexColor; \n"
"out vec4 lovrCanvas[gl_MaxDrawBuffers]; \n"
"uniform float lovrMetalness; \n"
"uniform float lovrRoughness; \n"
"uniform vec4 lovrColor; \n"
"uniform vec4 lovrDiffuseColor; \n"
"uniform vec4 lovrEmissiveColor; \n"
"uniform sampler2D lovrDiffuseTexture; \n"
"uniform sampler2D lovrEmissiveTexture; \n"
"uniform sampler2D lovrMetalnessTexture; \n"
"uniform sampler2D lovrRoughnessTexture; \n"
"uniform sampler2D lovrOcclusionTexture; \n"
"uniform sampler2D lovrNormalTexture; \n"
"uniform samplerCube lovrEnvironmentTexture; \n"
"uniform int lovrViewportCount; \n"
"#if SINGLEPASS \n"
"#define lovrViewportIndex gl_ViewportIndex \n"
"#else \n"
"uniform int lovrViewportIndex; \n"
"#endif \n"
"#line 0 \n";

const char* lovrShaderFragmentSuffix = ""
"void main() { \n"
"#ifdef MULTICANVAS \n"
"  colors(lovrColor, lovrDiffuseTexture, texCoord); \n"
"#else \n"
"  lovrCanvas[0] = color(lovrColor, lovrDiffuseTexture, texCoord); \n"
"#endif \n"
"}";

const char* lovrShaderComputePrefix = ""
"#version 430 \n"
"#line 0 \n";

const char* lovrShaderComputeSuffix = ""
"void main() { \n"
"  compute(); \n"
"}";

const char* lovrDefaultVertexShader = ""
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  return projection * transform * vertex; \n"
"}";

const char* lovrDefaultFragmentShader = ""
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  return graphicsColor * lovrDiffuseColor * vertexColor * texture(image, uv); \n"
"}";

const char* lovrCubeVertexShader = ""
"out vec3 texturePosition[2]; \n"
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  texturePosition[lovrViewportIndex] = inverse(mat3(transform)) * (inverse(projection) * vertex).xyz; \n"
"  return vertex; \n"
"}";

const char* lovrCubeFragmentShader = ""
"in vec3 texturePosition[2]; \n"
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  return graphicsColor * texture(lovrEnvironmentTexture, texturePosition[lovrViewportIndex] * vec3(-1, 1, 1)); \n"
"}";

const char* lovrPanoFragmentShader = ""
"in vec3 texturePosition[2]; \n"
"#define PI 3.141592653589 \n"
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  vec3 direction = texturePosition[lovrViewportIndex]; \n"
"  float theta = acos(-direction.y / length(direction)); \n"
"  float phi = atan(direction.x, -direction.z); \n"
"  uv = vec2(.5 + phi / (2. * PI), theta / PI); \n"
"  return graphicsColor * texture(lovrDiffuseTexture, uv); \n"
"}";

const char* lovrFontFragmentShader = ""
"float median(float r, float g, float b) { \n"
"  return max(min(r, g), min(max(r, g), b)); \n"
"} \n"
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  vec3 col = texture(image, uv).rgb; \n"
"  float sdf = median(col.r, col.g, col.b); \n"
"  float w = fwidth(sdf); \n"
"  float alpha = smoothstep(.5 - w, .5 + w, sdf); \n"
"  return vec4(graphicsColor.rgb, graphicsColor.a * alpha); \n"
"}";

const char* lovrFillVertexShader = ""
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  return vertex; \n"
"}";

const char* lovrShaderScalarUniforms[] = {
  "lovrMetalness",
  "lovrRoughness"
};

const char* lovrShaderColorUniforms[] = {
  "lovrDiffuseColor",
  "lovrEmissiveColor"
};

const char* lovrShaderTextureUniforms[] = {
  "lovrDiffuseTexture",
  "lovrEmissiveTexture",
  "lovrMetalnessTexture",
  "lovrRoughnessTexture",
  "lovrOcclusionTexture",
  "lovrNormalTexture",
  "lovrEnvironmentTexture"
};
