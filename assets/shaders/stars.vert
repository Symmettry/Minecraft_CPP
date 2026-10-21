#version 410 core
layout(location = 0) in vec3 aPos;

uniform mat4 projection;
uniform mat4 view;

void main() {
    vec4 position = projection * view * vec4(aPos, 1.0);
    gl_Position = position.xyww;
    gl_PointSize = 1.5;
}
