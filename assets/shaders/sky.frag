#version 410 core

in vec3 direction;
out vec4 FragColor;

uniform vec3 horizonColor;
uniform vec3 zenithColor;

void main() {
    float height = clamp(normalize(direction).y * 0.5 + 0.5, 0.0, 1.0);
    FragColor = vec4(mix(horizonColor, zenithColor, smoothstep(0.0, 0.75, height)), 1.0);
}
