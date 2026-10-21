#version 410 core
out vec4 FragColor;

in vec3 texCoord;
uniform samplerCube panorama;

void main() {
    FragColor = texture(panorama, texCoord);
}
