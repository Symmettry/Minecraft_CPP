#version 410 core

out vec4 FragColor;

uniform vec3 uColor; // e.g., red = 1,0,0

void main() {
    FragColor = vec4(uColor, 1.0);
}
