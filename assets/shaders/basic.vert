#version 410 core

layout(location = 0) in uint aPos;
layout(location = 1) in uint aTile;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const uint ATLAS_SIZE = 20u;
const float TILE = 1.0 / 20.0;

void main() {
    float x = float((aPos >> 12u) & 0xFu);
    float y = float((aPos >> 4u) & 0xFFu);
    float z = float(aPos & 0xFu);

    uint flags = (aTile >> 12u) & 0x7u; // bits 12-14
    if ((flags & 0x1u) != 0u) x += 1.0; // OVERFLOW_X
    if ((flags & 0x2u) != 0u) y += 1.0; // OVERFLOW_Y
    if ((flags & 0x4u) != 0u) z += 1.0; // OVERFLOW_Z

    vec3 pos = vec3(x, y, z);
    gl_Position = projection * view * model * vec4(pos, 1.0);

    uint tileIndex = (aTile >> 3u) & 0x1FFu;
    uint corner = aTile & 0x7u;

    uint tx = tileIndex % ATLAS_SIZE;
    uint ty = tileIndex / ATLAS_SIZE;

    vec2 base = vec2(float(tx), float(ty)) * TILE;

    vec2 cornerUV = vec2(
    (corner == 1u || corner == 2u) ? 1.0 : 0.0,
    (corner >= 2u) ? 1.0 : 0.0
    );

    TexCoord = base + cornerUV * TILE;
}
