#version 410 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D blockAtlas;

void main() {
    vec4 color = texture(blockAtlas, TexCoord);
    if (color.a < 0.1) discard;
    FragColor = vec4(color.rgb, color.a * 0.5);
}
