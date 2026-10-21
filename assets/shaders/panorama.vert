#version 410 core
layout(location = 0) in vec3 aPos;

out vec3 texCoord;

uniform mat4 projection;
uniform mat4 view;

void main() {
    texCoord = aPos;
    vec4 position = projection * view * vec4(aPos, 1.0);
    gl_Position = position.xyww;
}
