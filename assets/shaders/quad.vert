#version 410 core
layout(location = 0) in vec2 aPos;

uniform vec2 uPos;
uniform vec2 uSize;

void main() {
    vec2 pos = aPos * uSize + uPos;
    gl_Position = vec4(pos, 0.0, 1.0);
}
