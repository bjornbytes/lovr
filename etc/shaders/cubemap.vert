#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : require

#include "lovr.glsl"

layout(location = 2) out vec3 FragDirection;

void main() {
  FragColor = VertexColor * Color;

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
  FragDirection = normalize(inverseViewOrientation * (InverseProjection * vec4(ray, 1.)).xyz);
  Position = vec4(uv, 1, 1);
}
