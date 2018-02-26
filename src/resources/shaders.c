#include "resources/shaders.h"

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

const char* lovrShaderVertexPrefix = ""
#ifdef EMSCRIPTEN
"#version 300 es \n"
"precision mediump float; \n"
#else
"#version 150 \n"
#endif
"#define MAX_BONES 48 \n"
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
"uniform mat4 lovrView; \n"
"uniform mat4 lovrProjection; \n"
"uniform mat4 lovrTransform; \n"
"uniform mat3 lovrNormalMatrix; \n"
"uniform float lovrPointSize; \n"
"uniform mat4 lovrPose[MAX_BONES]; \n"
"#line 0 \n";

const char* lovrShaderFragmentPrefix = ""
#ifdef EMSCRIPTEN
"#version 300 es \n"
"precision mediump float; \n"
#else
"#version 150 \n"
"in vec4 gl_FragCoord; \n"
#endif
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
"#line 0 \n";

const char* lovrShaderVertexSuffix = ""
"void main() { \n"
"  texCoord = lovrTexCoord; \n"
"  vertexColor = lovrVertexColor; \n"
"  mat4 pose = \n"
"    lovrPose[lovrBones[0]] * lovrBoneWeights[0] + \n"
"    lovrPose[lovrBones[1]] * lovrBoneWeights[1] + \n"
"    lovrPose[lovrBones[2]] * lovrBoneWeights[2] + \n"
"    lovrPose[lovrBones[3]] * lovrBoneWeights[3]; \n"
"  gl_PointSize = lovrPointSize; \n"
"  gl_Position = position(lovrProjection, lovrTransform, pose * vec4(lovrPosition, 1.0)); \n"
"}";

const char* lovrShaderFragmentSuffix = ""
"void main() { \n"
"#ifdef MULTICANVAS \n"
"  colors(lovrColor, lovrDiffuseTexture, texCoord); \n"
"#else \n"
"  lovrCanvas[0] = color(lovrColor, lovrDiffuseTexture, texCoord); \n"
"#endif \n"
"}";

const char* lovrDefaultVertexShader = ""
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  return projection * transform * vertex; \n"
"}";

const char* lovrDefaultFragmentShader = ""
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  return graphicsColor * lovrDiffuseColor * vertexColor * texture(image, uv); \n"
"}";

const char* lovrSkyboxVertexShader = ""
"out vec3 texturePosition; \n"
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  texturePosition = vertex.xyz; \n"
"  texturePosition.y *= -1; \n"
"  return (projection * transform * vertex).xyww; \n"
"}";

const char* lovrSkyboxFragmentShader = ""
"in vec3 texturePosition; \n"
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  return graphicsColor * texture(lovrEnvironmentTexture, texturePosition); \n"
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

const char* lovrNoopVertexShader = ""
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  return vertex; \n"
"}";
