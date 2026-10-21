#version 410 core

in vec2 TexCoord;
flat in vec2 TileOrigin;
flat in uint TintData;
flat in vec3 BiomeTint;
in float Shade;
in float FogDistance;
out vec4 FragColor;

uniform sampler2D texture1;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform float fogDensity;
uniform int fogMode;

void main() {
    const float atlasTexel = 1.0 / 320.0;
    const float tileSize = 1.0 / 20.0;
    vec2 sampleCoord = clamp(TexCoord,
        TileOrigin + vec2(atlasTexel * 0.5),
        TileOrigin + vec2(tileSize - atlasTexel * 0.5));
    vec4 color = (TintData & (1u << 31u)) != 0u
        ? textureLod(texture1, sampleCoord, 0.0)
        : texture(texture1, sampleCoord);
    if (color.a < 0.1) discard;
    uint tintKind = TintData & 7u;
    if (tintKind == 5u) {
            uint overlayTile = (TintData >> 19u) & 0x1FFu;
            vec2 overlayOrigin = vec2(overlayTile % 20u, overlayTile / 20u) * tileSize;
            vec2 localCoord = (TexCoord - TileOrigin) / tileSize;
            vec2 overlayCoord = clamp(overlayOrigin + localCoord * tileSize,
                overlayOrigin + vec2(atlasTexel * 0.5),
                overlayOrigin + vec2(tileSize - atlasTexel * 0.5));
            vec4 overlay = texture(texture1, overlayCoord);
            vec3 overlayColor = overlay.rgb;
            if (overlay.a > 0.0 && overlay.a < 1.0) {
                overlayColor = min(overlayColor / overlay.a, vec3(1.0));
            }
            if (overlay.a >= 0.1) color.rgb = overlayColor * BiomeTint;
    } else color.rgb *= BiomeTint;
    color.rgb *= Shade;
    float fogVisibility;
    switch (fogMode) {
        case 1:
            fogVisibility = exp(-fogDensity * FogDistance);
            break;
        default:
            fogVisibility = (fogEnd - FogDistance) / (fogEnd - fogStart);
            break;
    }
    fogVisibility = clamp(fogVisibility, 0.0, 1.0);
    FragColor = vec4(mix(fogColor, color.rgb, fogVisibility), color.a);
}
