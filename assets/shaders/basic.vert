#version 410 core

layout(location = 0) in uint aVertex;
layout(location = 1) in uint aTint;
layout(location = 2) in uint aShade;
layout(location = 3) in uint aModelOffset;
layout(location = 4) in uint aModelUv;

out vec2 TexCoord;
flat out vec2 TileOrigin;
flat out uint TintData;
flat out vec3 BiomeTint;
out float Shade;
out float FogDistance;

uniform mat4 viewProjection;
uniform vec3 chunkOffset;
uniform int skylightSubtracted;
uniform sampler2D grassColorMap;
uniform sampler2D foliageColorMap;

const uint ATLAS_SIZE = 20u;
const float TILE = 1.0 / 20.0;

void main() {
    // extract components from packed vertex
    uint x = (aVertex >> 26u) & 0x1Fu;  // 5 bits
    uint y = (aVertex >> 17u) & 0x1FFu; // 9 bits
    uint z = (aVertex >> 12u) & 0x1Fu;  // 5 bits

    uint tileIndex = (aVertex >> 3u) & 0x1FFu; // 9 bits
    uint height = aVertex & 0x7u; // eighths of a block; zero is full height
    bool lowerTop = (aVertex & 0x80000000u) != 0u;

    vec3 pos = vec3(x, y, z);
    pos += vec3(float(aModelOffset & 15u), float((aModelOffset >> 4u) & 15u),
        float((aModelOffset >> 8u) & 15u)) / 16.0;
    if ((aModelOffset & (1u << 31u)) != 0u) {
        pos.y = float((aModelOffset >> 12u) & 255u) / 255.0 + float(y);
    }
    bool plant = (aShade & (1u << 19u)) != 0u;
    if (plant) {
        vec2 corner = vec2(float(height & 1u), float((height >> 1u) & 1u));
        pos.xz += vec2(0.05) - corner * 0.1;
        pos.x += (float((aShade >> 20u) & 15u) / 15.0 - 0.5) * 0.5;
        pos.y += (float((aShade >> 24u) & 15u) / 15.0 - 1.0) * 0.2;
        pos.z += (float((aShade >> 28u) & 15u) / 15.0 - 0.5) * 0.5;
    } else {
        uint xOffset = (aShade >> 20u) & 3u;
        uint zOffset = (aShade >> 22u) & 3u;
        if (xOffset == 1u) pos.x -= 0.5;
        else if (xOffset == 2u) pos.x += 0.5;
        if (zOffset == 1u) pos.z -= 0.5;
        else if (zOffset == 2u) pos.z += 0.5;
        if ((aShade & (1u << 24u)) != 0u && !lowerTop) pos.y += 0.5;
    }
    if (!plant && lowerTop && height != 0u) {
        pos.y -= 1.0 - float(height) / 8.0;
    }
    vec3 cameraRelativePosition = pos + chunkOffset;
    gl_Position = viewProjection * vec4(cameraRelativePosition, 1.0);
    FogDistance = length(cameraRelativePosition);

    uint tx = tileIndex % ATLAS_SIZE;
    uint ty = tileIndex / ATLAS_SIZE;

    const uint corners[6] = uint[6](3u, 2u, 1u, 1u, 0u, 3u);
    uint corner = corners[gl_VertexID % 6];
    vec2 cornerUV = vec2(
    (corner == 1u || corner == 2u) ? 1.0 : 0.0,
    (corner > 1u) ? 1.0 : 0.0
    );
    if ((aModelUv & (1u << 10u)) != 0u) {
        cornerUV = vec2(float(aModelUv & 31u), float((aModelUv >> 5u) & 31u)) / 16.0;
    }
    uint face = (aShade >> 16u) & 7u;
    if (!plant && height != 0u && face < 4u && !lowerTop) {
        cornerUV.y = float(height) / 8.0;
    }

    TileOrigin = vec2(tx, ty) * TILE;
    TintData = aTint;
    uint tintKind = aTint & 7u;
    if (tintKind == 1u || tintKind == 2u || tintKind == 5u || tintKind == 6u) {
        ivec2 colorMapPosition = ivec2(int((aTint >> 3u) & 255u), int((aTint >> 11u) & 255u));
        switch (tintKind) {
            case 1u: case 5u: BiomeTint = texelFetch(grassColorMap, colorMapPosition, 0).rgb; break;
            case 2u: BiomeTint = texelFetch(foliageColorMap, colorMapPosition, 0).rgb; break;
            case 6u: // an explicit color packed into the tint (redstone dust power, stems)
                BiomeTint = vec3(float((aTint >> 3u) & 255u), float((aTint >> 11u) & 255u),
                    float((aTint >> 19u) & 255u)) / 255.0;
                break;
            default: BiomeTint = vec3(1.0); break;
        }
    } else {
        switch (tintKind) {
            case 3u: BiomeTint = vec3(0.3804, 0.6000, 0.3804); break;
            case 4u: BiomeTint = vec3(0.5020, 0.6549, 0.3333); break;
            default: BiomeTint = vec3(1.0); break;
        }
    }
    float directionShade = float(aShade & 255u) / 255.0;
    float blockLight = float((aShade >> 8u) & 15u) / 15.0;
    float skyLight = float(max(int((aShade >> 12u) & 15u) - skylightSubtracted, 0)) / 15.0;
    float light = max(blockLight, skyLight);
    float darkness = 1.0 - light;
    float vanillaBrightness = light / (darkness * 3.0 + 1.0);
    // entityRenderer.updateLightmap applies this floor after combining sky and block light; without it, light level zero becomes literal black instead of vanilla's dim 0.03
    vanillaBrightness = clamp(vanillaBrightness * 0.96 + 0.03, 0.0, 1.0);
    Shade = directionShade * vanillaBrightness;
    TexCoord = (vec2(tx, ty) + cornerUV) * TILE;
}
