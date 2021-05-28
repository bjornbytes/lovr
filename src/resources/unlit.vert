#version 460
#extension GL_EXT_multiview : require

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 color;

layout(set = 0, binding = 0) uniform Camera { mat4 projections[6], views[6]; };
layout(set = 0, binding = 1) uniform PerDraw { mat4 transforms[1]; };

void main() {
  gl_Position = projections[gl_ViewIndex] * views[gl_ViewIndex] * position;
  gl_Position.y = -gl_Position.y;
}
