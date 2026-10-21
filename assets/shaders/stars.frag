#version 410 core

out vec4 FragColor;
uniform float brightness;

void main() {
    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    if (dot(centered, centered) > 1.0) discard;
    FragColor = vec4(vec3(brightness), brightness);
}
