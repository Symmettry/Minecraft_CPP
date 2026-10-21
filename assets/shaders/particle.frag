#version 410 core

in vec2 TexCoord;
in vec3 Color;
in float Alpha;
in float FogDistance;
out vec4 FragColor;

uniform sampler2D blockAtlas;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform float fogDensity;
uniform int fogMode;

void main() {
    vec4 color = textureLod(blockAtlas, TexCoord, 0.0);
    if (color.a < 0.1) discard;
    color.rgb *= Color;
    color.a *= Alpha;
    float fogVisibility;
    switch (fogMode) {
        case 1: fogVisibility = exp(-fogDensity * FogDistance); break;
        default: fogVisibility = (fogEnd - FogDistance) / (fogEnd - fogStart); break;
    }
    color.rgb = mix(fogColor, color.rgb, clamp(fogVisibility, 0.0, 1.0));
    FragColor = color;
}
