#version 460
#extension GL_EXT_multiview : require

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec2 texcoord;

layout(set = 0, binding = 0) uniform Camera { mat4 view[6], projection[6]; };
layout(set = 0, binding = 1) uniform Transforms { mat4 transforms[]; };

void main() {
  gl_Position = projection[gl_ViewIndex] * view[gl_ViewIndex] * transforms[0] * position;
}
