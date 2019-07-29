#include "resources/shaders.h"

const char* lovrShaderVertexPrefix = ""
"#define VERTEX VERTEX \n"
"#define MAX_BONES 48 \n"
"#define MAX_DRAWS 256 \n"
"#define lovrView lovrViews[lovrViewID] \n"
"#define lovrProjection lovrProjections[lovrViewID] \n"
"#define lovrModel lovrModels[lovrDrawID] \n"
"#define lovrTransform (lovrView * lovrModel) \n"
"#define lovrNormalMatrix mat3(transpose(inverse(lovrTransform))) \n"
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
"in vec4 lovrTangent; \n"
"in uvec4 lovrBones; \n"
"in vec4 lovrBoneWeights; \n"
"in uint lovrDrawID; \n"
"out vec2 texCoord; \n"
"out vec4 vertexColor; \n"
"out vec4 lovrColor; \n"
"layout(std140) uniform lovrModelBlock { mat4 lovrModels[MAX_DRAWS]; }; \n"
"layout(std140) uniform lovrColorBlock { vec4 lovrColors[MAX_DRAWS]; }; \n"
"layout(std140) uniform lovrFrameBlock { mat4 lovrViews[2]; mat4 lovrProjections[2]; }; \n"
"uniform mat3 lovrMaterialTransform; \n"
"uniform float lovrPointSize; \n"
"uniform mat4 lovrPose[MAX_BONES]; \n"
"uniform lowp int lovrViewportCount; \n"
"#if defined MULTIVIEW \n"
"layout(num_views = 2) in; \n"
"#define lovrViewID gl_ViewID_OVR \n"
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
"  lovrColor = lovrColors[lovrDrawID]; \n"
"#if defined INSTANCED_STEREO \n"
"  gl_ViewportIndex = gl_InstanceID % lovrViewportCount; \n"
"#endif \n"
"  gl_PointSize = lovrPointSize; \n"
"  vec4 vertexPosition = vec4(lovrPosition, 1.); \n"
"#ifdef FLAG_skinned \n"
"  vertexPosition = lovrPoseMatrix * vertexPosition; \n"
"#endif \n"
"  gl_Position = position(lovrProjection, lovrTransform, vertexPosition); \n"
"}";

const char* lovrShaderFragmentPrefix = ""
"#define PIXEL PIXEL \n"
"#define FRAGMENT FRAGMENT \n"
"in vec2 texCoord; \n"
"in vec4 vertexColor; \n"
"in vec4 lovrColor; \n"
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
"uniform samplerCube lovrEnvironmentTexture; \n"
"uniform lowp int lovrViewportCount; \n"
"#if defined MULTIVIEW \n"
"#define lovrViewID gl_ViewID_OVR \n"
"#elif defined INSTANCED_STEREO \n"
"#define lovrViewID gl_ViewportIndex \n"
"#else \n"
"uniform lowp int lovrViewID; \n"
"#endif \n"
"#line 0 \n";

const char* lovrShaderFragmentSuffix = ""
"void main() { \n"
"#if defined(MULTICANVAS) || defined(FLAG_multicanvas) \n"
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

const char* lovrUnlitVertexShader = ""
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  return projection * transform * vertex; \n"
"}";

const char* lovrUnlitFragmentShader = ""
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  return graphicsColor * lovrDiffuseColor * vertexColor * texture(image, uv); \n"
"}";

const char* lovrStandardVertexShader = ""
"out vec3 vViewPosition; \n"
"out vec3 vWorldPosition; \n"
"out mat3 vTangentMatrix; \n"
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  vViewPosition = vec3(transform * vertex); \n"
"  vWorldPosition = vec3(lovrModel * vertex); \n"
"  vec3 normal = normalize(mat3(lovrModel) * lovrNormal); // TODO non-uniform scale \n"
"  vec3 tangent = normalize(mat3(lovrModel) * lovrTangent.xyz); \n"
"  vec3 bitangent = cross(normal, tangent) * lovrTangent.w; \n"
"  vTangentMatrix = mat3(tangent, bitangent, normal); \n"
"  return projection * transform * vertex; \n"
"}";

const char* lovrStandardFragmentShader = ""
"#define PI 3.14159265358979 \n"
"in vec3 vViewPosition; \n"
"in vec3 vWorldPosition; \n"
"in mat3 vTangentMatrix; \n"
"uniform vec3 lovrIrradiance[9]; \n"
"uniform vec3 lovrLightDirection = vec3(-1., -1., -1.); \n"
""
"float D_GGX(float NoH, float roughness) { \n"
"  float alpha = roughness * roughness; \n"
"  float alpha2 = alpha * alpha; \n"
"  float denom = (NoH * NoH) * (alpha2 - 1.) + 1.; \n"
"  return alpha2 / (PI * denom * denom); \n"
"} \n"
""
"float G_SmithGGXCorrelated(float NoV, float NoL, float roughness) { \n"
"  float alpha = roughness * roughness; \n"
"  float alpha2 = alpha * alpha; \n"
"  float GGXV = NoL * sqrt(alpha2 + (1. - alpha2) * (NoV * NoV)); \n"
"  float GGXL = NoV * sqrt(alpha2 + (1. - alpha2) * (NoL * NoL)); \n"
"  return .5 / max(GGXV + GGXL, 1e-5); \n"
"} \n"
""
"vec3 F_Schlick(vec3 F0, float VoH) { \n"
"  return F0 + (vec3(1.) - F0) * pow(1. - VoH, 5.); \n"
"} \n"
""
"vec3 E_SphericalHarmonics(vec3 sh[9], vec3 n) { \n"
"  n = -n; // WHY (maybe cmgen is returning upside down coefficients?) \n"
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
""
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  vec3 normal = vTangentMatrix * (texture(lovrNormalTexture, uv).rgb * 2. - 1.); \n"
"  vec3 baseColor = texture(lovrDiffuseTexture, uv).rgb * lovrDiffuseColor.rgb; \n"
"  vec3 emissive = texture(lovrEmissiveTexture, uv).rgb * lovrEmissiveColor.rgb; \n"
"  float occlusion = texture(lovrOcclusionTexture, uv).r; \n"
"  float metalness = texture(lovrMetalnessTexture, uv).b * lovrMetalness; \n"
"  float roughness = max(texture(lovrRoughnessTexture, uv).g * lovrRoughness, .05); \n"
"  vec3 F0 = mix(vec3(.04), baseColor, metalness); \n"
""
"  vec3 N = normalize(normal); \n"
"  vec3 V = normalize(-vViewPosition); \n"
"  vec3 L = normalize(-lovrLightDirection); \n"
"  vec3 H = normalize(V + L); \n"
""
"  float NoV = abs(dot(N, V)) + 1e-5; \n"
"  float NoL = clamp(dot(N, L), 0., 1.); \n"
"  float NoH = clamp(dot(N, H), 0., 1.); \n"
"  float VoH = clamp(dot(V, H), 0., 1.); \n"
""
"  float D = D_GGX(NoH, roughness); \n"
"  float G = G_SmithGGXCorrelated(NoV, NoL, roughness); \n"
"  vec3 F = F_Schlick(F0, VoH); \n"
"  vec3 E = E_SphericalHarmonics(lovrIrradiance, N); \n"
""
"  vec3 specular = vec3(D * G * F); \n"
"  vec3 diffuse = (vec3(1.) - F) * (1. - metalness) * baseColor; \n"
""
"  vec3 direct = (diffuse / PI + specular) * NoL * occlusion; \n"
"  vec3 indirect = diffuse * E; \n"
""
"  return vec4(direct + indirect + emissive, 1.); \n"
"}";

const char* lovrCubeVertexShader = ""
"out vec3 texturePosition[2]; \n"
"vec4 position(mat4 projection, mat4 transform, vec4 vertex) { \n"
"  texturePosition[lovrViewID] = inverse(mat3(transform)) * (inverse(projection) * vertex).xyz; \n"
"  return vertex; \n"
"}";

const char* lovrCubeFragmentShader = ""
"in vec3 texturePosition[2]; \n"
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  return graphicsColor * texture(lovrEnvironmentTexture, texturePosition[lovrViewID] * vec3(-1, 1, 1)); \n"
"}";

const char* lovrPanoFragmentShader = ""
"in vec3 texturePosition[2]; \n"
"#define PI 3.141592653589 \n"
"vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
"  vec3 direction = texturePosition[lovrViewID]; \n"
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
"  if (alpha <= 0.0) discard; \n"
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

const char* lovrShaderAttributeNames[] = {
  "lovrPosition",
  "lovrNormal",
  "lovrTexCoord",
  "lovrVertexColor",
  "lovrTangent",
  "lovrBones",
  "lovrBoneWeights"
};
