#version 410 core
out vec4 FragColor;

in vec2 texCoord;
uniform sampler2D sourceTexture;

void main() {
    const float pixel = 1.0 / 256.0;
    vec4 color = texture(sourceTexture, texCoord) * 0.5454545;
    color += texture(sourceTexture, texCoord + vec2(-pixel, 0.0)) * 0.2727273;
    color += texture(sourceTexture, texCoord + vec2(pixel, 0.0)) * 0.1818182;
    FragColor = color;
}
