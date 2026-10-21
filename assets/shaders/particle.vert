#version 410 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aColor;
layout(location = 3) in float aAlpha;

out vec2 TexCoord;
out vec3 Color;
out float Alpha;
out float FogDistance;

uniform mat4 viewProjection;

void main() {
    gl_Position = viewProjection * vec4(aPosition, 1.0);
    TexCoord = aTexCoord;
    Color = aColor;
    Alpha = aAlpha;
    FogDistance = length(aPosition);
}
