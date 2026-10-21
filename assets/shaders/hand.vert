#version 410 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;

out vec2 TexCoord;
out vec3 ModelPosition;

uniform mat4 projection;
uniform mat4 model;

void main() {
    TexCoord = uv;
    ModelPosition = vec3(model * vec4(position, 1.0));
    gl_Position = projection * model * vec4(position, 1.0);
}
