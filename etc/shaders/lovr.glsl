// Flags
layout(constant_id = 1000) const bool applyUVTransform = true;
layout(constant_id = 1001) const bool applyAlphaCutoff = false;
layout(constant_id = 1002) const bool applyGlow = false;

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

layout(set = 0, binding = 0) uniform CameraBuffer { Camera Cameras[6]; };
layout(set = 0, binding = 1) uniform DrawBuffer { Draw Draws[256]; };
layout(set = 0, binding = 2) uniform sampler Sampler;

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
  float pointSize;
};

layout(set = 1, binding = 0) uniform MaterialBuffer {
  MaterialData Material;
};

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
layout(location = 0) out vec4 PixelColors[1];
#endif

// Varyings
#ifdef GL_VERTEX_SHADER
layout(location = 10) out vec3 Normal;
layout(location = 11) out vec4 Color;
layout(location = 12) out vec2 UV;
#endif

#ifdef GL_FRAGMENT_SHADER
layout(location = 10) in vec3 Normal;
layout(location = 11) in vec4 Color;
layout(location = 12) in vec2 UV;
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
vec4 getPixel(texture2D t, vec2 uv) { return texture(sampler2D(t, Sampler), uv); }
vec4 getPixel(texture3D t, vec3 uvw) { return texture(sampler3D(t, Sampler), uvw); }
vec4 getPixel(textureCube t, vec3 dir) { return texture(samplerCube(t, Sampler), dir); }
vec4 getPixel(texture2DArray t, vec3 uvw) { return texture(sampler2DArray(t, Sampler), uvw); }
#endif

// Entrypoints
#ifndef NO_DEFAULT_MAIN
#ifdef GL_VERTEX_SHADER
vec4 lovrmain();
void main() {
  Normal = NormalMatrix * VertexNormal;
  Color = VertexColor * Material.color * PassColor;
  UV = VertexUV;

  PointSize = Material.pointSize;
  Position = lovrmain();

  if (applyUVTransform) {
    UV *= Material.uvScale;
    UV += Material.uvShift;
  }
}
#endif

#ifdef GL_FRAGMENT_SHADER
vec4 lovrmain();
void main() {
  PixelColors[0] = lovrmain();

  if (applyGlow) {
    PixelColors[0].rgb += getPixel(GlowTexture, UV).rgb * Material.glow.rgb * Material.glow.a;
  }

  if (applyAlphaCutoff && PixelColors[0].a <= Material.alphaCutoff) {
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
