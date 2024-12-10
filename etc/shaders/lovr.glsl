// Flags
#ifndef GL_COMPUTE_SHADER
layout(constant_id = 1000) const float flag_pointSize = 1.f;
layout(constant_id = 1002) const bool flag_passColor = true;
layout(constant_id = 1003) const bool flag_materialColor = true;
layout(constant_id = 1004) const bool flag_vertexColors = true;
layout(constant_id = 1005) const bool flag_uvTransform = true;
layout(constant_id = 1006) const bool flag_alphaCutoff = false;
layout(constant_id = 1007) const bool flag_glow = false;
layout(constant_id = 1008) const bool flag_normalMap = false;
layout(constant_id = 1009) const bool flag_vertexTangents = true;
layout(constant_id = 1010) const bool flag_colorTexture = true;
layout(constant_id = 1011) const bool flag_glowTexture = true;
layout(constant_id = 1012) const bool flag_metalnessTexture = true;
layout(constant_id = 1013) const bool flag_roughnessTexture = true;
layout(constant_id = 1014) const bool flag_ambientOcclusion = true;
layout(constant_id = 1015) const bool flag_clearcoatTexture = false;
layout(constant_id = 1016) const bool flag_tonemap = false;
#endif

// Resources
#ifndef GL_COMPUTE_SHADER
struct Camera {
  mat4 view;
  mat4 projection;
  mat4 viewProjection;
  mat4 inverseProjection;
};

struct Draw {
  mat4x3 transform;
  vec4 color;
};

layout(set = 0, binding = 0) uniform Globals { vec2 Resolution; float Time; };
layout(set = 0, binding = 1) uniform CameraBuffer { Camera Cameras[6]; };
layout(set = 0, binding = 2) uniform DrawBuffer { layout(row_major) Draw Draws[256]; };
layout(set = 0, binding = 3) uniform sampler Sampler;

struct MaterialData {
  vec4 color;
  vec4 glow;
  vec2 uvShift;
  vec2 uvScale;
  vec2 sdfRange;
  float metalness;
  float roughness;
  float clearcoat;
  float clearcoatRoughness;
  float occlusionStrength;
  float normalScale;
  float alphaCutoff;
};

layout(set = 1, binding = 0) uniform MaterialBuffer {
  MaterialData Material;
};

layout(set = 1, binding = 1) uniform texture2D ColorTexture;
layout(set = 1, binding = 2) uniform texture2D GlowTexture;
layout(set = 1, binding = 3) uniform texture2D MetalnessTexture;
layout(set = 1, binding = 4) uniform texture2D RoughnessTexture;
layout(set = 1, binding = 5) uniform texture2D ClearcoatTexture;
layout(set = 1, binding = 6) uniform texture2D OcclusionTexture;
layout(set = 1, binding = 7) uniform texture2D NormalTexture;

layout(push_constant) uniform PushConstants {
  uint DrawID;
};
#endif

// Attributes
#ifdef GL_VERTEX_SHADER
layout(location = 10) in vec4 VertexPosition;
layout(location = 11) in vec3 VertexNormal;
layout(location = 12) in vec2 VertexUV;
layout(location = 13) in vec4 VertexColor;
layout(location = 14) in vec4 VertexTangent;
#endif

// Framebuffer
#ifdef GL_FRAGMENT_SHADER
layout(location = 0) out vec4 PixelColor;
#endif

// Varyings
#ifdef GL_VERTEX_SHADER
layout(location = 10) out vec3 PositionWorld;
layout(location = 11) out vec3 Normal;
layout(location = 12) out vec2 UV;
layout(location = 13) out vec4 Color;
layout(location = 14) out vec4 Tangent;
#endif

#ifdef GL_FRAGMENT_SHADER
layout(location = 10) in vec3 PositionWorld;
layout(location = 11) in vec3 Normal;
layout(location = 12) in vec2 UV;
layout(location = 13) in vec4 Color;
layout(location = 14) in vec4 Tangent;
#endif

// Builtins
#ifdef GL_COMPUTE_SHADER
#define SubgroupCount gl_NumSubgroups
#define WorkgroupCount gl_NumWorkGroups
#define WorkgroupSize gl_WorkGroupSize
#define WorkgroupID gl_WorkGroupID
#define GlobalThreadID gl_GlobalInvocationID
#define LocalThreadID gl_LocalInvocationID
#define LocalThreadIndex gl_LocalInvocationIndex
#else
#define ClipDistance gl_ClipDistance
#define CullDistance gl_CullDistance
#define PrimitiveID gl_PrimitiveID
#define ViewIndex gl_ViewIndex
#endif

#ifdef GL_VERTEX_SHADER
#define BaseInstance gl_BaseInstance
#define BaseVertex gl_BaseVertex
#define DrawIndex gl_DrawIndex
#define InstanceIndex gl_InstanceIndex
#define PointSize gl_PointSize
#define Position gl_Position
#define VertexIndex gl_VertexIndex
#endif

#ifdef GL_FRAGMENT_SHADER
#define FragCoord gl_FragCoord
#define FragDepth gl_FragDepth
#define FrontFacing gl_FrontFacing
#define PointCoord gl_PointCoord
#define SampleID gl_SampleID
#define SampleMaskIn gl_SampleMaskIn
#define SampleMask gl_SampleMask
#define SamplePosition gl_SamplePosition
#endif

// Constants
#define PI 3.141592653589793238462643383279502f
#define TAU (2.f * PI)
#define PI_2 (.5f * PI)

// Helpers

#define Constants uniform DefaultUniformBlock

#ifndef GL_COMPUTE_SHADER
#define Projection Cameras[ViewIndex].projection
#define View Cameras[ViewIndex].view
#define ViewProjection Cameras[ViewIndex].viewProjection
#define InverseProjection Cameras[ViewIndex].inverseProjection
#define CameraPositionWorld (-View[3].xyz * mat3(View))
#endif

#ifdef GL_VERTEX_SHADER
#define Transform mat4(Draws[DrawID].transform)
#define NormalMatrix (cofactor3(Draws[DrawID].transform))
#define PassColor Draws[DrawID].color
#define ClipFromLocal (ViewProjection * Transform)
#define ClipFromWorld (ViewProjection)
#define ClipFromView (Projection)
#define ViewFromLocal (View * Transform)
#define ViewFromWorld (View)
#define ViewFromClip (InverseProjection)
#define WorldFromLocal (Transform)
#define WorldFromView (inverse(View))
#define WorldFromClip (inverse(ViewProjection))
#define DefaultPosition (ClipFromLocal * VertexPosition)

mat3 cofactor3(mat4x3 m) {
  return mat3(vec3(
     (m[1][1] * m[2][2] - m[2][1] * m[1][2]),
    -(m[1][0] * m[2][2] - m[2][0] * m[1][2]),
     (m[1][0] * m[2][1] - m[2][0] * m[1][1])), vec3(
    -(m[0][1] * m[2][2] - m[2][1] * m[0][2]),
     (m[0][0] * m[2][2] - m[2][0] * m[0][2]),
    -(m[0][0] * m[2][1] - m[2][0] * m[0][1])), vec3(
     (m[0][1] * m[1][2] - m[1][1] * m[0][2]),
    -(m[0][0] * m[1][2] - m[1][0] * m[0][2]),
     (m[0][0] * m[1][1] - m[1][0] * m[0][1])
  ));
}
#endif

#ifdef GL_FRAGMENT_SHADER
#define DefaultColor (flag_colorTexture ? (Color * getPixel(ColorTexture, UV)) : Color)
#endif

// Helper for sampling textures using the default sampler set using Pass:setSampler
#ifndef GL_COMPUTE_SHADER
vec4 getPixel(texture2D t, vec2 uv) { return texture(sampler2D(t, Sampler), uv); }
vec4 getPixel(texture3D t, vec3 uvw) { return texture(sampler3D(t, Sampler), uvw); }
vec4 getPixel(textureCube t, vec3 dir) { return texture(samplerCube(t, Sampler), dir); }
vec4 getPixel(texture2DArray t, vec2 uv, float layer) { return texture(sampler2DArray(t, Sampler), vec3(uv, layer)); }
vec4 getPixel(textureCubeArray t, vec4 coord) { return texture(samplerCubeArray(t, Sampler), coord); }

vec4 getPixel(sampler2D t, vec2 uv) { return texture(t, uv); }
vec4 getPixel(sampler3D t, vec3 uvw) { return texture(t, uvw); }
vec4 getPixel(samplerCube t, vec3 dir) { return texture(t, dir); }
vec4 getPixel(sampler2DArray t, vec2 uv, float layer) { return texture(t, vec3(uv, layer)); }
vec4 getPixel(samplerCubeArray t, vec4 coord) { return texture(t, coord); }
#endif

#ifdef GL_FRAGMENT_SHADER
// Surface contains all light-independent data needed for shading.  It can be calculated once per
// pixel and reused for multiple lights.  It stores information from the vertex shader and material
// inputs.  The Surface can be initialized using initSurface, and is passed into the other lighting
// functions.  Everything is in world space.
struct Surface {
  vec3 position; // Position of fragment
  vec3 normal; // Includes normal mapping
  vec3 geometricNormal; // Raw normal from vertex shader
  vec3 view; // The direction from the fragment to the camera
  vec3 reflection; // The view vector reflected about the normal

  vec3 f0;
  vec3 diffuse;
  vec3 emissive;
  vec4 baseColor;
  float metalness;
  float roughness;
  float roughness2;
  float occlusion;
  float clearcoat;
  float clearcoatRoughness;
};

#define TangentMatrix getTangentMatrix()

mat3 getTangentMatrix() {
  if (flag_vertexTangents) {
    vec3 N = normalize(Normal);
    vec3 T = normalize(Tangent.xyz);
    vec3 B = cross(N, T) * Tangent.w;
    return mat3(T, B, N);
  } else {
    // http://www.thetenthplanet.de/archives/1180
    vec3 N = normalize(Normal);
    vec3 dp1 = dFdx(PositionWorld);
    vec3 dp2 = dFdy(PositionWorld);
    vec2 duv1 = dFdx(UV);
    vec2 duv2 = dFdy(UV);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, N);
  }
}

Surface newSurface() {
  Surface surface;
  surface.position = PositionWorld;
  surface.geometricNormal = normalize(Normal);
  surface.normal = surface.geometricNormal;
  surface.view = normalize(CameraPositionWorld - PositionWorld);
  surface.emissive = vec3(0.);
  surface.baseColor = Color;
  surface.metalness = 1.;
  surface.roughness = 1.;
  surface.occlusion = 1.;
  surface.clearcoat = 0.;
  surface.clearcoatRoughness = 0.;
  return surface;
}

vec4 getMaterialBaseColor() {
  vec4 color = Color;
  if (flag_colorTexture) color *= getPixel(ColorTexture, UV);
  return color;
}

vec3 getMaterialEmissive() {
  vec3 emissive = Material.glow.rgb * Material.glow.a;
  if (flag_glow && flag_glowTexture) emissive *= getPixel(GlowTexture, UV).rgb;
  return emissive;
}

float getMaterialMetalness() {
  float metalness = Material.metalness;
  if (flag_metalnessTexture) metalness *= getPixel(MetalnessTexture, UV).b;
  return metalness;
}

float getMaterialRoughness() {
  float roughness = Material.roughness;
  if (flag_roughnessTexture) roughness *= getPixel(RoughnessTexture, UV).g;
  return roughness;
}

float getMaterialOcclusion() {
  float occlusion = 1.;
  if (flag_ambientOcclusion) occlusion *= getPixel(OcclusionTexture, UV).r * Material.occlusionStrength;
  return occlusion;
}

float getMaterialClearcoat() {
  float clearcoat = Material.clearcoat;
  if (flag_clearcoatTexture) clearcoat *= getPixel(ClearcoatTexture, UV).r;
  return clearcoat;
}

float getMaterialClearcoatRoughness() {
  return Material.clearcoatRoughness;
}

Surface applyMaterial(inout Surface surface) {
  surface.baseColor = getMaterialBaseColor();
  surface.emissive = getMaterialEmissive();
  surface.metalness = getMaterialMetalness();
  surface.roughness = getMaterialRoughness();
  surface.occlusion = getMaterialOcclusion();
  surface.clearcoat = getMaterialClearcoat();
  surface.clearcoatRoughness = getMaterialClearcoatRoughness();
  if (flag_normalMap) {
    vec3 normalScale = vec3(Material.normalScale, Material.normalScale, 1.);
    surface.normal = TangentMatrix * normalize((getPixel(NormalTexture, UV).rgb * 2. - 1.) * normalScale);
  }
  return surface;
}

void finalizeSurface(inout Surface surface) {
  if (!FrontFacing) {
    surface.normal = -surface.normal;
    surface.geometricNormal = -surface.geometricNormal;
  }
  surface.reflection = reflect(-surface.view, surface.normal);
  surface.f0 = mix(vec3(.04), surface.baseColor.rgb, surface.metalness);
  surface.diffuse = mix(surface.baseColor.rgb, vec3(0.), surface.metalness);
  surface.roughness = max(surface.roughness, .05);
  surface.roughness2 = surface.roughness * surface.roughness;
}

Surface getDefaultSurface() {
  Surface surface = newSurface();
  applyMaterial(surface);
  finalizeSurface(surface);
  return surface;
}

// Deprecated
void initSurface(out Surface surface) {
  surface = getDefaultSurface();
}

float D_GGX(const Surface surface, float NoH) {
  float alpha2 = surface.roughness2 * surface.roughness2;
  float denom = (NoH * NoH) * (alpha2 - 1.) + 1.;
  return alpha2 / (PI * denom * denom);
}

float G_SmithGGXCorrelated(const Surface surface, float NoV, float NoL) {
  float alpha2 = surface.roughness2 * surface.roughness2;
  float GGXV = NoL * sqrt(alpha2 + (1. - alpha2) * (NoV * NoV));
  float GGXL = NoV * sqrt(alpha2 + (1. - alpha2) * (NoL * NoL));
  return .5 / (GGXV + GGXL);
}

vec3 F_Schlick(const Surface surface, float VoH) {
  return surface.f0 + (vec3(1.) - surface.f0) * pow(1. - VoH, 5.);
}

// Evaluates a direct light for a given surface
vec3 getLighting(const Surface surface, vec3 direction, vec4 color, float visibility) {
  if (visibility <= 0.) {
    return vec3(0.);
  }

  // Parameters
  vec3 N = surface.normal;
  vec3 V = surface.view;
  vec3 L = normalize(-direction);
  vec3 H = normalize(V + L);
  vec3 R = surface.reflection;
  float NoV = abs(dot(N, V)) + 1e-8;
  float NoL = clamp(dot(N, L), 0., 1.);
  float NoH = clamp(dot(N, H), 0., 1.);
  float VoH = clamp(dot(V, H), 0., 1.);

  // Diffuse
  float Fd_Lambert = 1. / PI;
  vec3 diffuse = surface.diffuse * Fd_Lambert;

  // Specular
  float D = D_GGX(surface, NoH);
  float G = G_SmithGGXCorrelated(surface, NoV, NoL);
  vec3 F = F_Schlick(surface, VoH);
  vec3 specular = vec3(D * G) * F;

  return (diffuse + specular) * color.rgb * (NoL * color.a * visibility);
}

// https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
vec2 prefilteredBRDF(float NoV, float roughness) {
  vec4 c0 = vec4(-1., -.0275, -.572, .022);
  vec4 c1 = vec4(1., .0425, 1.04, -.04);
  vec4 r = roughness * c0 + c1;
  float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
  return vec2(-1.04, 1.04) * a004 + r.zw;
}

vec3 evaluateSphericalHarmonics(vec3 sh[9], vec3 n) {
  return max(
    sh[0] +
    sh[1] * n.y +
    sh[2] * n.z +
    sh[3] * n.x +
    sh[4] * n.y * n.x +
    sh[5] * n.y * n.z +
    sh[6] * (3. * n.z * n.z - 1.) +
    sh[7] * n.z * n.x +
    sh[8] * (n.x * n.x - n.y * n.y)
  , 0.);
}

vec3 getIndirectLighting(const Surface surface, textureCube environment, vec3 sphericalHarmonics[9]) {
  float NoV = dot(surface.normal, surface.view);
  vec2 lookup = prefilteredBRDF(NoV, surface.roughness);

  int mipmapCount = textureQueryLevels(samplerCube(environment, Sampler));
  vec3 ibl = textureLod(samplerCube(environment, Sampler), surface.reflection, surface.roughness * mipmapCount).rgb;
  vec3 specular = (surface.f0 * lookup.r + lookup.g) * ibl;

  vec3 sh = evaluateSphericalHarmonics(sphericalHarmonics, surface.normal);
  vec3 diffuse = surface.diffuse * surface.occlusion * sh;

  return diffuse + specular;
}

vec3 tonemap(vec3 x) {
  float a = 2.51;
  float b = 0.03;
  float c = 2.43;
  float d = 0.59;
  float e = 0.14;
  return (x * (a * x + b)) / (x * (c * x + d) + e);
}
#endif

vec3 gammaToLinear(vec3 color) {
  return mix(pow((color + .055) / 1.055, vec3(2.4)), color / 12.92, lessThanEqual(color, vec3(.04045)));
}

vec3 linearToGamma(vec3 color) {
  return mix(1.055 * pow(color, vec3(1. / 2.4)) - .055, color * 12.92, lessThanEqual(color, vec3(.0031308)));
}

uint packSnorm10x3(vec4 v) {
  return
    ((int(v.x * 511.) & 0x3ff) <<  0) |
    ((int(v.y * 511.) & 0x3ff) << 10) |
    ((int(v.z * 511.) & 0x3ff) << 20) |
    ((int(v.w * 2.) & 0x3) << 30);
}

// The weird 22 bit shift basically does sign-extension of a 10-bit value stored in a 32-bit int
vec4 unpackSnorm10x3(uint n) {
  return vec4(
    max((int((n >> 0)  & 0x3ff) << 22 >> 22) / 511., -1.),
    max((int((n >> 10) & 0x3ff) << 22 >> 22) / 511., -1.),
    max((int((n >> 20) & 0x3ff) << 22 >> 22) / 511., -1.),
    max(float((n >> 30) & 0x3), -1.)
  );
}

// Entrypoints
#ifndef NO_DEFAULT_MAIN
#ifdef GL_VERTEX_SHADER
vec4 lovrmain();
void main() {
  PositionWorld = vec3(WorldFromLocal * VertexPosition);
  Normal = NormalMatrix * VertexNormal;
  UV = VertexUV;

  Color = vec4(1.0);
  if (flag_passColor) Color *= PassColor;
  if (flag_materialColor) Color *= Material.color;
  if (flag_vertexColors) Color *= VertexColor;

  if (flag_vertexTangents) {
    Tangent = vec4(NormalMatrix * VertexTangent.xyz, VertexTangent.w);
  }

  PointSize = flag_pointSize;
  Position = lovrmain();

  if (flag_uvTransform) {
    UV *= Material.uvScale;
    UV += Material.uvShift;
  }
}
#endif

#ifdef GL_FRAGMENT_SHADER
vec4 lovrmain();
void main() {
  PixelColor = lovrmain();

  if (flag_glow) {
    if (flag_glowTexture) {
      PixelColor.rgb += getPixel(GlowTexture, UV).rgb * Material.glow.rgb * Material.glow.a;
    } else {
      PixelColor.rgb += Material.glow.rgb * Material.glow.a;
    }
  }

  if (flag_tonemap) {
    PixelColor.rgb = tonemap(PixelColor.rgb);
  }

  if (flag_alphaCutoff) { // Nesting the ifs instead of using && to work around a glslang bug
    if (PixelColor.a <= Material.alphaCutoff) {
      discard;
    }
  }
}
#endif

#ifdef GL_COMPUTE_SHADER
void lovrmain();
void main() {
  lovrmain();
}
#endif
#endif
