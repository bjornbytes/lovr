#include "resources/shaders.h"

const char* lovrShaderVertexPrefix = ""
"#define VERTEX VERTEX \n"
"#define MAX_BONES 48 \n"
"#define MAX_DRAWS 256 \n"
"#define lovrView lovrViews[lovrViewID] \n"
"#define lovrProjection lovrProjections[lovrViewID] \n"
"#define lovrModel lovrModels[lovrDrawID] \n"
"#define lovrTransform (lovrView * lovrModel) \n"
"#ifdef FLAG_uniformScale \n"
"#define lovrNormalMatrix mat3(lovrModel) \n"
"#else \n"
"#define lovrNormalMatrix mat3(transpose(inverse(lovrModel))) \n"
"#endif \n"
"#define lovrInstanceID (gl_InstanceID / lovrViewportCount) \n"
"#define lovrPoseMatrix ("
  "lovrPose[lovrBones[0]] * lovrBoneWeights[0] +"
  "lovrPose[lovrBones[1]] * lovrBoneWeights[1] +"
  "lovrPose[lovrBones[2]] * lovrBoneWeights[2] +"
  "lovrPose[lovrBones[3]] * lovrBoneWeights[3]"
  ") \n"
"#ifdef FLAG_animated \n"
"#define lovrVertex (lovrPoseMatrix * vec4(lovrPosition, 1.)) \n"
"#else \n"
"#define lovrVertex vec4(lovrPosition, 1.) \n"
"#endif \n"
"precision highp float; \n"
"precision highp int; \n"
"in vec3 lovrPosition; \n"
"in vec3 lovrNormal; \n"
"in vec2 lovrTexCoord; \n"
"in vec4 lovrVertexColor; \n"
"in vec4 lovrTangent; \n"
"in uvec4 lovrBones; \n"
"in vec4 lovrBoneWeights; \n"
"in uint lovrDrawID; \n"
"out vec2 texCoord; \n"
"out vec4 vertexColor; \n"
"out vec4 lovrGraphicsColor; \n"
"layout(std140) uniform lovrModelBlock { mat4 lovrModels[MAX_DRAWS]; }; \n"
"layout(std140) uniform lovrColorBlock { vec4 lovrColors[MAX_DRAWS]; }; \n"
"layout(std140) uniform lovrFrameBlock { mat4 lovrViews[2]; mat4 lovrProjections[2]; }; \n"
"uniform mat3 lovrMaterialTransform; \n"
"uniform float lovrPointSize; \n"
"uniform mat4 lovrPose[MAX_BONES]; \n"
"uniform lowp int lovrViewportCount; \n"
"#if defined MULTIVIEW \n"
"layout(num_views = 2) in; \n"
"#define lovrViewID (int(gl_ViewID_OVR)) \n"
"#elif defined INSTANCED_STEREO \n"
"#define lovrViewID gl_ViewportIndex \n"
"#else \n"
"uniform lowp int lovrViewID; \n"
"#endif \n"
"#line 0 \n";

const char* lovrShaderVertexSuffix = ""
"void main() { \n"
"  texCoord = (lovrMaterialTransform * vec3(lovrTexCoord, 1.)).xy; \n"
"  vertexColor = lovrVertexColor; \n"
"  lovrGraphicsColor = lovrColors[lovrDrawID]; \n"
"#if defined INSTANCED_STEREO \n"
"  gl_ViewportIndex = gl_InstanceID % lovrViewportCount; \n"
"#endif \n"
"  gl_PointSize = lovrPointSize; \n"
"  gl_Position = position(lovrProjection, lovrTransform, lovrVertex); \n"
"}";

const char* lovrShaderFragmentPrefix = ""
"#define PIXEL PIXEL \n"
"#define FRAGMENT FRAGMENT \n"
"#define lovrTexCoord texCoord \n"
"#define lovrVertexColor vertexColor \n"
"#ifdef FLAG_highp \n"
"precision highp float; \n"
"precision highp int; \n"
"#else \n"
"precision mediump float; \n"
"precision mediump int; \n"
"#endif \n"
"in vec2 texCoord; \n"
"in vec4 vertexColor; \n"
"in vec4 lovrGraphicsColor; \n"
"out vec4 lovrCanvas[gl_MaxDrawBuffers]; \n"
"uniform float lovrMetalness; \n"
"uniform float lovrRoughness; \n"
"uniform vec4 lovrDiffuseColor; \n"
"uniform vec4 lovrEmissiveColor; \n"
"uniform sampler2D lovrDiffuseTexture; \n"
"uniform sampler2D lovrEmissiveTexture; \n"
"uniform sampler2D lovrMetalnessTexture; \n"
"uniform sampler2D lovrRoughnessTexture; \n"
"uniform sampler2D lovrOcclusionTexture; \n"
"uniform sampler2D lovrNormalTexture; \n"
"uniform lowp int lovrViewportCount; \n"
"#if defined MULTIVIEW \n"
"#define lovrViewID gl_ViewID_OVR \n"
"#elif defined INSTANCED_STEREO \n"
"#define lovrViewID gl_ViewportIndex \n"
"#else \n"
"uniform lowp int lovrViewID; \n"
"#endif \n"
"#ifdef MULTIVIEW \n"
"#define sampler2DMultiview sampler2DArray \n"
"vec4 textureMultiview(sampler2DMultiview t, vec2 uv) { \n"
"  return texture(t, vec3(uv, lovrViewID)); \n"
"} \n"
"#else \n"
"#define sampler2DMultiview sampler2D \n"
"vec4 textureMultiview(sampler2DMultiview t, vec2 uv) { \n"
"  uv = clamp(uv, 0., 1.) * vec2(.5, 1.) + vec2(lovrViewID) * vec2(.5, 0.); \n"
"  return texture(t, uv); \n"
"} \n"
"#endif \n"
"#line 0 \n";

const char* lovrShaderFragmentSuffix = ""
"void main() { \n"
"#if defined(MULTICANVAS) || defined(FLAG_multicanvas) \n"
"  colors(lovrGraphicsColor, lovrDiffuseTexture, texCoord); \n"
"#else \n"
"  lovrCanvas[0] = color(lovrGraphicsColor, lovrDiffuseTexture, lovrTexCoord); \n"
"#ifdef FLAG_alphaCutoff \n"
"  if (lovrCanvas[0].a < FLAG_alphaCutoff) { \n"
"    discard; \n"
"  } \n"
"#endif \n"
#if defined(LOVR_WEBGL) || defined(LOVR_USE_PICO)
"  lovrCanvas[0].rgb = pow(lovrCanvas[0].rgb, vec3(.4545)); \n"
#endif
"#endif \n"
"}";

#ifdef LOVR_GLES
const char* lovrShaderComputePrefix = ""
"#version 310 es \n"
"#line 0 \n";
#else
const char* lovrShaderComputePrefix = ""
"#version 430 \n"
"#line 0 \n";
#endif

const char* lovrShaderComputeSuffix = ""
"void main() { \n"
"  compute(); \n"
"}";

const char* lovrUnlitVertexShader = ""
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  return lovrProjection * lovrTransform * lovrVertex; \n"
"}";

const char* lovrUnlitFragmentShader = ""
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  return lovrGraphicsColor * lovrVertexColor * lovrDiffuseColor * texture(lovrDiffuseTexture, lovrTexCoord); \n"
"}";

const char* lovrStandardVertexShader = ""
"out vec3 vVertexPositionWorld; \n"
"out vec3 vCameraPositionWorld; \n"
"#ifdef FLAG_normalMap \n"
"out mat3 vTangentMatrix; \n"
"#else \n"
"out vec3 vNormal; \n"
"#endif \n"

"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  vVertexPositionWorld = vec3(lovrModel * lovrVertex); \n"
"  vCameraPositionWorld = -lovrView[3].xyz * mat3(lovrView); \n"
"#ifdef FLAG_normalMap \n"
"  vec3 normal = normalize(lovrNormalMatrix * lovrNormal); \n"
"  vec3 tangent = normalize(lovrNormalMatrix * lovrTangent.xyz); \n"
"  vec3 bitangent = cross(normal, tangent) * lovrTangent.w; \n"
"  vTangentMatrix = mat3(tangent, bitangent, normal); \n"
"#else \n"
"  vNormal = normalize(lovrNormalMatrix * lovrNormal); \n"
"#endif \n"
"  return lovrProjection * lovrTransform * lovrVertex; \n"
"}";

const char* lovrStandardFragmentShader = ""
"#define PI 3.14159265358979 \n"
"#ifdef GL_ES \n"
"#define EPS 1e-2 \n"
"#else \n"
"#define EPS 1e-5 \n"
"#endif \n"

"in vec3 vVertexPositionWorld; \n"
"in vec3 vCameraPositionWorld; \n"
"#ifdef FLAG_normalMap \n"
"in mat3 vTangentMatrix; \n"
"#else \n"
"in vec3 vNormal; \n"
"#endif \n"

"uniform vec3 lovrLightDirection; \n"
"uniform vec4 lovrLightColor; \n"
"uniform samplerCube lovrEnvironmentMap; \n"
"uniform vec3 lovrSphericalHarmonics[9]; \n"
"uniform float lovrExposure; \n"

"float D_GGX(float NoH, float roughness); \n"
"float G_SmithGGXCorrelated(float NoV, float NoL, float roughness); \n"
"vec3 F_Schlick(vec3 F0, float VoH); \n"
"vec3 E_SphericalHarmonics(vec3 sh[9], vec3 n); \n"
"vec2 prefilteredBRDF(float NoV, float roughness); \n"
"vec3 tonemap_ACES(vec3 color); \n"

"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  vec3 result = vec3(0.); \n"

// Parameters
"  vec3 baseColor = texture(lovrDiffuseTexture, lovrTexCoord).rgb * lovrDiffuseColor.rgb; \n"
"  float metalness = texture(lovrMetalnessTexture, lovrTexCoord).b * lovrMetalness; \n"
"  float roughness = max(texture(lovrRoughnessTexture, lovrTexCoord).g * lovrRoughness, .05); \n"
"#ifdef FLAG_normalMap \n"
"  vec3 N = normalize(vTangentMatrix * (texture(lovrNormalTexture, lovrTexCoord).rgb * 2. - 1.)); \n"
"#else \n"
"  vec3 N = normalize(vNormal); \n"
"#endif \n"
"  vec3 V = normalize(vCameraPositionWorld - vVertexPositionWorld); \n"
"  vec3 L = normalize(-lovrLightDirection); \n"
"  vec3 H = normalize(V + L); \n"
"  vec3 R = normalize(reflect(-V, N)); \n"
"  float NoV = abs(dot(N, V)) + EPS; \n"
"  float NoL = clamp(dot(N, L), 0., 1.); \n"
"  float NoH = clamp(dot(N, H), 0., 1.); \n"
"  float VoH = clamp(dot(V, H), 0., 1.); \n"

// Direct lighting
"  vec3 F0 = mix(vec3(.04), baseColor, metalness); \n"
"  float D = D_GGX(NoH, roughness); \n"
"  float G = G_SmithGGXCorrelated(NoV, NoL, roughness); \n"
"  vec3 F = F_Schlick(F0, VoH); \n"
"  vec3 specularDirect = vec3(D * G * F); \n"
"  vec3 diffuseDirect = (vec3(1.) - F) * (1. - metalness) * baseColor; \n"
"  result += (diffuseDirect / PI + specularDirect) * NoL * lovrLightColor.rgb * lovrLightColor.a; \n"

// Indirect lighting
"#ifdef FLAG_indirectLighting \n"
"  vec2 lookup = prefilteredBRDF(NoV, roughness); \n"
"  float mipmapCount = log2(float(textureSize(lovrEnvironmentMap, 0).x)); \n"
"  vec3 specularIndirect = (F0 * lookup.r + lookup.g) * textureLod(lovrEnvironmentMap, R, roughness * mipmapCount).rgb; \n"
"  vec3 diffuseIndirect = diffuseDirect * E_SphericalHarmonics(lovrSphericalHarmonics, N); \n"
"#ifdef FLAG_occlusion \n" // Occlusion only affects indirect diffuse light
"  diffuseIndirect *= texture(lovrOcclusionTexture, lovrTexCoord).r; \n"
"#endif \n"
"  result += diffuseIndirect + specularIndirect; \n"
"#endif \n"

// Emissive
"#ifdef FLAG_emissive \n" // Currently emissive texture and color have to be used together
"  result += texture(lovrEmissiveTexture, lovrTexCoord).rgb * lovrEmissiveColor.rgb; \n"
"#endif \n"

// Tonemap
"#ifndef FLAG_skipTonemap \n"
"  result = tonemap_ACES(result * lovrExposure); \n"
"#endif \n"

"  return lovrGraphicsColor * vec4(result, 1.); \n"
"}"

// Helpers
"float D_GGX(float NoH, float roughness) { \n"
"  float alpha = roughness * roughness; \n"
"  float alpha2 = alpha * alpha; \n"
"  float denom = (NoH * NoH) * (alpha2 - 1.) + 1.; \n"
"  return alpha2 / (PI * denom * denom); \n"
"} \n"

"float G_SmithGGXCorrelated(float NoV, float NoL, float roughness) { \n"
"  float alpha = roughness * roughness; \n"
"  float alpha2 = alpha * alpha; \n"
"  float GGXV = NoL * sqrt(alpha2 + (1. - alpha2) * (NoV * NoV)); \n"
"  float GGXL = NoV * sqrt(alpha2 + (1. - alpha2) * (NoL * NoL)); \n"
"  return .5 / max(GGXV + GGXL, EPS); \n"
"} \n"

"vec3 F_Schlick(vec3 F0, float VoH) { \n"
"  return F0 + (vec3(1.) - F0) * pow(1. - VoH, 5.); \n"
"} \n"

"vec3 E_SphericalHarmonics(vec3 sh[9], vec3 n) { \n"
"  n = -n; // WHY \n"
"  return max("
    "sh[0] + "
    "sh[1] * n.y + "
    "sh[2] * n.z + "
    "sh[3] * n.x + "
    "sh[4] * n.y * n.x + "
    "sh[5] * n.y * n.z + "
    "sh[6] * (3. * n.z * n.z - 1.) + "
    "sh[7] * n.z * n.x + "
    "sh[8] * (n.x * n.x - n.y * n.y)"
  ", 0.); \n"
"} \n"

// https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
"vec2 prefilteredBRDF(float NoV, float roughness) { \n"
"  vec4 c0 = vec4(-1., -.0275, -.572, .022); \n"
"  vec4 c1 = vec4(1., .0425, 1.04, -.04); \n"
"  vec4 r = roughness * c0 + c1; \n"
"  float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y; \n"
"  return vec2(-1.04, 1.04) * a004 + r.zw; \n"
"} \n"

"vec3 tonemap_ACES(vec3 color) { \n"
"  float a = 2.51; \n"
"  float b = 0.03; \n"
"  float c = 2.43; \n"
"  float d = 0.59; \n"
"  float e = 0.14; \n"
"  return (color * (a * color + b)) / (color * (c * color + d) + e); \n"
"}";

const char* lovrCubeVertexShader = ""
"out vec3 texturePosition[2]; \n"
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  texturePosition[lovrViewID] = inverse(mat3(lovrTransform)) * (inverse(lovrProjection) * lovrVertex).xyz; \n"
"  return lovrVertex; \n"
"}";

const char* lovrCubeFragmentShader = ""
"in vec3 texturePosition[2]; \n"
"uniform samplerCube lovrSkyboxTexture; \n"
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  return lovrGraphicsColor * texture(lovrSkyboxTexture, texturePosition[lovrViewID] * vec3(-1, 1, 1)); \n"
"}";

const char* lovrPanoFragmentShader = ""
"in vec3 texturePosition[2]; \n"
"#define PI 3.141592653589 \n"
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  vec3 direction = texturePosition[lovrViewID]; \n"
"  float theta = acos(-direction.y / length(direction)); \n"
"  float phi = atan(direction.x, -direction.z); \n"
"  vec2 cubeUv = vec2(.5 + phi / (2. * PI), theta / PI); \n"
"  return lovrGraphicsColor * texture(lovrDiffuseTexture, cubeUv); \n"
"}";

const char* lovrFontFragmentShader = ""
"float median(float r, float g, float b) { \n"
"  return max(min(r, g), min(max(r, g), b)); \n"
"} \n"
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  vec3 col = texture(lovrDiffuseTexture, lovrTexCoord).rgb; \n"
"  float sdf = median(col.r, col.g, col.b); \n"
"  float w = fwidth(sdf); \n"
"  float alpha = smoothstep(.5 - w, .5 + w, sdf); \n"
"  if (alpha <= 0.0) discard; \n"
"  return vec4(lovrGraphicsColor.rgb, lovrGraphicsColor.a * alpha); \n"
"}";

const char* lovrFillVertexShader = ""
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  return lovrVertex; \n"
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
  "lovrNormalTexture"
};

const char* lovrShaderAttributeNames[] = {
  "lovrPosition",
  "lovrNormal",
  "lovrTexCoord",
  "lovrVertexColor",
  "lovrTangent",
  "lovrBones",
  "lovrBoneWeights"
};
