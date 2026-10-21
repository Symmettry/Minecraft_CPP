#version 410 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D glintTexture;
void main() {
    vec4 sampleColor = texture(glintTexture, TexCoord);
    if (sampleColor.a < 0.01) discard;
    FragColor = vec4(sampleColor.rgb * vec3(0.5, 0.25, 0.8), sampleColor.a * 0.7);
}
