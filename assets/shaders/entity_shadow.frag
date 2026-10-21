#version 410 core
in vec2 TexCoord;
in float Alpha;
out vec4 FragColor;
uniform sampler2D shadowTexture;
void main() {
    vec4 color = texture(shadowTexture, TexCoord);
    if (color.a < 0.01) discard;
    FragColor = vec4(color.rgb, color.a * Alpha);
}
