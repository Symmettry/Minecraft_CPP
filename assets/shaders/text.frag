#version 410 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D fontTexture;
uniform vec4 textColor;

void main()
{
    float alpha = texture(fontTexture, TexCoord).r;
    FragColor = vec4(textColor.rgb, alpha * textColor.a);
}
