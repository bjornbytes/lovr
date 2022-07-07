#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

layout(location = 0) out vec3 Direction;

void main() {
  Color = PassColor * VertexColor;

  const vec2 uvs[6] = vec2[6](
    vec2(-1, -1),
    vec2(-1, +1),
    vec2(+1, -1),
    vec2(+1, -1),
    vec2(-1, +1),
    vec2(+1, +1)
  );

  vec2 uv = uvs[VertexIndex % 6];
  vec3 ray = vec3(uv, -1.);
  mat3 inverseViewOrientation = transpose(mat3(View));
  Direction = normalize(inverseViewOrientation * (InverseProjection * vec4(ray, 1.)).xyz);
  Position = vec4(uv, 1, 1);
}
