#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in uint aTile;
layout(location = 2) in uint aCorner;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const uint ATLAS_SIZE = 20u;
const float TILE = 1.0 / 20.0;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);

    uint tx = aTile % ATLAS_SIZE;
    uint ty = aTile / ATLAS_SIZE;

    vec2 base = vec2(tx, ty) * TILE;

    vec2 cornerUV = vec2(
    aCorner == 1u || aCorner == 2u,
    aCorner >= 2u
    );

    TexCoord = base + cornerUV * TILE;
}
