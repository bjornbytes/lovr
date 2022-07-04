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

layout(set = 0, binding = 0) uniform Cameras { Camera cameras[6]; };
layout(set = 0, binding = 1) uniform Draws { Draw draws[256]; };
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
  float glowStrength;
  float normalScale;
  float alphaCutoff;
  float pointSize;
};

layout(set = 1, binding = 0) uniform MaterialBlock { MaterialData Material; };
layout(set = 1, binding = 1) uniform texture2D Texture;
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
layout(location = 10) out vec3 FragNormal;
layout(location = 11) out vec4 FragColor;
layout(location = 12) out vec2 FragUV;
#else
layout(location = 10) in vec3 FragNormal;
layout(location = 11) in vec4 FragColor;
layout(location = 12) in vec2 FragUV;
#endif

// Macros
#ifdef GL_COMPUTE_SHADER
//
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
#define PrimitiveId gl_PrimitiveID
#define SampleId gl_SampleID
#define SampleMaskIn gl_SampleMaskIn
#define SampleMask gl_SampleMask
#define SamplePosition gl_SamplePosition
#define VertexIndex gl_VertexIndex
#define ViewIndex gl_ViewIndex

#define DrawId gl_BaseInstance
#define Projection cameras[ViewIndex].projection
#define View cameras[ViewIndex].view
#define ViewProjection cameras[ViewIndex].viewProjection
#define InverseProjection cameras[ViewIndex].inverseProjection
#define Transform draws[DrawId].transform
#define NormalMatrix mat3(draws[DrawId].normalMatrix)
#define Color draws[DrawId].color

#define ClipFromLocal (ViewProjection * Transform)
#define ClipFromWorld (ViewProjection)
#define ClipFromView (Projection)
#define ViewFromLocal (View * Transform)
#define ViewFromWorld (View)
#define WorldFromLocal (Transform)

#define DefaultPosition (ClipFromLocal * VertexPosition)
#define DefaultColor (FragColor * texture(sampler2D(Texture, Sampler), FragUV) * Material.color)
#endif
