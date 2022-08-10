// Flags
layout(constant_id = 1000) const bool enableUVTransform = true;
layout(constant_id = 1001) const bool enableAlphaCutoff = false;
layout(constant_id = 1002) const bool enableGlow = false;
layout(constant_id = 1003) const bool useColorTexture = false;
layout(constant_id = 1004) const bool useGlowTexture = false;
layout(constant_id = 1005) const bool useMetalnessTexture = false;
layout(constant_id = 1006) const bool useRoughnessTexture = false;
layout(constant_id = 1007) const bool useOcclusionTexture = false;
layout(constant_id = 1008) const bool useClearcoatTexture = false;
layout(constant_id = 1009) const bool useNormalTexture = false;

// Resources
#ifndef GL_COMPUTE_SHADER
struct Camera {
  mat4 view;
  mat4 projection;
  mat4 viewProjection;
  mat4 inverseProjection;
};

struct Draw {
  mat4 transform;
  mat4 normalMatrix;
  vec4 color;
};

layout(set = 0, binding = 0) uniform Globals { vec4 Resolution; float Time; };
layout(set = 0, binding = 1) uniform CameraBuffer { Camera Cameras[6]; };
layout(set = 0, binding = 2) uniform DrawBuffer { Draw Draws[256]; };
layout(set = 0, binding = 3) uniform sampler Sampler;

layout(set = 1, binding = 0) uniform MaterialBuffer {
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
  float pointSize;
} Material;

layout(set = 1, binding = 1) uniform texture2D ColorTexture;
layout(set = 1, binding = 2) uniform texture2D GlowTexture;
layout(set = 1, binding = 3) uniform texture2D OcclusionTexture;
layout(set = 1, binding = 4) uniform texture2D MetalnessTexture;
layout(set = 1, binding = 5) uniform texture2D RoughnessTexture;
layout(set = 1, binding = 6) uniform texture2D ClearcoatTexture;
layout(set = 1, binding = 7) uniform texture2D NormalTexture;
#endif

// Attributes
#ifdef GL_VERTEX_SHADER
layout(location = 10) in vec4 VertexPosition;
layout(location = 11) in vec3 VertexNormal;
layout(location = 12) in vec2 VertexUV;
layout(location = 13) in vec4 VertexColor;
layout(location = 14) in vec3 VertexTangent;
#endif

// Framebuffer
#ifdef GL_FRAGMENT_SHADER
layout(location = 0) out vec4 PixelColor[1];
#endif

// Varyings
#ifdef GL_VERTEX_SHADER
layout(location = 10) out vec3 PositionWorld;
layout(location = 11) out vec3 Normal;
layout(location = 12) out vec4 Color;
layout(location = 13) out vec2 UV;
#endif

#ifdef GL_FRAGMENT_SHADER
layout(location = 10) in vec3 PositionWorld;
layout(location = 11) in vec3 Normal;
layout(location = 12) in vec4 Color;
layout(location = 13) in vec2 UV;
#endif

// Macros
#ifdef GL_COMPUTE_SHADER
#define SubgroupCount gl_NumSubgroups
#define WorkgroupCount gl_NumWorkGroups
#define WorkgroupSize gl_WorkGroupSize
#define WorkgroupID gl_WorkGroupID
#define GlobalThreadID gl_GlobalInvocationID
#define LocalThreadID gl_LocalInvocationID
#define LocalThreadIndex gl_LocalInvocationIndex
#else
#define BaseInstance gl_BaseInstance
#define BaseVertex gl_BaseVertex
#define ClipDistance gl_ClipDistance
#define CullDistance gl_CullDistance
#define DrawIndex gl_DrawIndex
#define InstanceIndex (gl_InstanceIndex - gl_BaseInstance)
#define FragCoord gl_FragCoord
#define FragDepth gl_FragDepth
#define FrontFacing gl_FrontFacing
#define PointCoord gl_PointCoord
#define PointSize gl_PointSize
#define Position gl_Position
#define PrimitiveID gl_PrimitiveID
#define SampleID gl_SampleID
#define SampleMaskIn gl_SampleMaskIn
#define SampleMask gl_SampleMask
#define SamplePosition gl_SamplePosition
#define VertexIndex gl_VertexIndex
#define ViewIndex gl_ViewIndex

#define DrawID gl_BaseInstance
#define Projection Cameras[ViewIndex].projection
#define View Cameras[ViewIndex].view
#define ViewProjection Cameras[ViewIndex].viewProjection
#define InverseProjection Cameras[ViewIndex].inverseProjection
#define Transform Draws[DrawID].transform
#define NormalMatrix mat3(Draws[DrawID].normalMatrix)
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

#define CameraPositionWorld (-View[3].xyz * mat3(View))

#define DefaultPosition (ClipFromLocal * VertexPosition)
#define DefaultColor (Color * getPixel(ColorTexture, UV))
#endif

// Constants
#define PI 3.141592653589793238462643383279502f
#define TAU (2.f * PI)
#define PI_2 (.5f * PI)

// Helpers
#ifndef GL_COMPUTE_SHADER

// Helper for sampling textures using the default sampler set using Pass:setSampler
vec4 getPixel(texture2D t, vec2 uv) { return texture(sampler2D(t, Sampler), uv); }
vec4 getPixel(texture3D t, vec3 uvw) { return texture(sampler3D(t, Sampler), uvw); }
vec4 getPixel(textureCube t, vec3 dir) { return texture(samplerCube(t, Sampler), dir); }
vec4 getPixel(texture2DArray t, vec3 uvw) { return texture(sampler2DArray(t, Sampler), uvw); }

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
  float roughness2;
  float occlusion;
  float clearcoat;
  float clearcoatRoughness;
  float alpha;
};

void initSurface(out Surface surface) {
  surface.position = PositionWorld;

  if (useNormalTexture) {
    //surface.normal = TangentMatrix * vec3(getPixel(NormalTexture, UV)) * Material.normalScale;
  } else {
    surface.normal = normalize(Normal);
  }

  surface.geometricNormal = normalize(Normal);
  surface.view = CameraPositionWorld - PositionWorld;
  surface.reflection = reflect(-surface.view, surface.normal);

  vec4 color = Color;
  if (useColorTexture) color *= getPixel(ColorTexture, UV);

  float metallic = Material.metalness;
  if (useMetalnessTexture) metallic *= getPixel(MetalnessTexture, UV).b;

  surface.f0 = mix(vec3(.04), color.rgb, metallic);
  surface.diffuse = mix(color.rgb, vec3(0.), metallic);

  surface.emissive = Material.glow.rgb * Material.glow.a;
  if (useGlowTexture) surface.emissive *= getPixel(GlowTexture, UV).rgb;

  float roughness = Material.roughness;
  if (useRoughnessTexture) roughness *= getPixel(RoughnessTexture, UV).g;
  surface.roughness2 = roughness * roughness;

  surface.occlusion = 1.;
  if (useOcclusionTexture) surface.occlusion *= getPixel(OcclusionTexture, UV).r * Material.occlusionStrength;

  surface.clearcoat = Material.clearcoat;
  if (useClearcoatTexture) surface.clearcoat *= getPixel(ClearcoatTexture, UV).r;

  surface.clearcoatRoughness = Material.clearcoatRoughness;

  surface.alpha = color.a;
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
  vec3 specular = (D * G) * F;

  return (diffuse + specular) * color.rgb * (NoL * color.a * visibility);
}
#endif

// Entrypoints
#ifndef NO_DEFAULT_MAIN
#ifdef GL_VERTEX_SHADER
vec4 lovrmain();
void main() {
  PositionWorld = vec3(WorldFromLocal * VertexPosition);
  Normal = NormalMatrix * VertexNormal;
  Color = VertexColor * Material.color * PassColor;
  UV = VertexUV;

  PointSize = Material.pointSize;
  Position = lovrmain();

  if (enableUVTransform) {
    UV *= Material.uvScale;
    UV += Material.uvShift;
  }
}
#endif

#ifdef GL_FRAGMENT_SHADER
vec4 lovrmain();
void main() {
  PixelColor[0] = lovrmain();

  if (enableGlow) {
    PixelColor[0].rgb += getPixel(GlowTexture, UV).rgb * Material.glow.rgb * Material.glow.a;
  }

  if (enableAlphaCutoff && PixelColor[0].a <= Material.alphaCutoff) {
    discard;
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
