#version 410 core

layout(location = 0) in uint aVertex;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const uint ATLAS_SIZE = 20u;
const float TILE = 1.0 / 20.0;

void main() {
    // Extract components from packed vertex
    float x = float((aVertex >> 26u) & 0x1Fu); // 5 bits
    float y = float((aVertex >> 17u) & 0x1FFu); // 9 bits
    float z = float((aVertex >> 12u) & 0x1Fu); // 5 bits

    uint tileIndex = (aVertex >> 3u) & 0x1FFu; // 9 bits
    uint corner = aVertex & 0x7u; // 3 bits

    vec3 pos = vec3(x, y, z);
    gl_Position = projection * view * model * vec4(pos, 1.0);

    uint tx = tileIndex % ATLAS_SIZE;
    uint ty = tileIndex / ATLAS_SIZE;

    vec2 base = vec2(float(tx), float(ty)) * TILE;

    vec2 cornerUV = vec2(
    (corner == 1u || corner == 2u) ? 1.0 : 0.0,
    (corner >= 2u) ? 1.0 : 0.0
    );

    TexCoord = base + cornerUV * TILE;
}
