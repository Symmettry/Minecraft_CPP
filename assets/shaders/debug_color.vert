#version 410 core

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;

out vec4 Color;

void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    Color = aColor;
}
