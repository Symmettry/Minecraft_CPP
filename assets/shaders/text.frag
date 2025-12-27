#version 410 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D fontTexture;
uniform vec4 textColor;

void main()
{
    vec4 sampled = texture(fontTexture, TexCoord);
    FragColor = vec4(textColor.rgb, sampled.r * textColor.a); // multiply alpha
}
