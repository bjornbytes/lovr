#version 460
#extension GL_EXT_multiview : require

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec2 texcoord;
layout(location = 3) in vec4 color;

layout(location = 0) out vec4 vertexColor;

layout(set = 0, binding = 0) uniform Camera { mat4 projections[6], views[6]; };
layout(set = 0, binding = 1) uniform Transform { mat4 transform; };

void main() {
  vertexColor = color;
  gl_Position = projections[gl_ViewIndex] * views[gl_ViewIndex] * transform * position;
  gl_Position.y = -gl_Position.y;
  gl_Position.z = (gl_Position.z + gl_Position.w) / 2.;
}
