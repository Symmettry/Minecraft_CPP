#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aFace;

out vec2 TexCoord;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform vec2 tileOrigin;
uniform vec3 blockMinimum;
uniform vec3 blockMaximum;

void main() {
    vec3 blockPosition = mix(blockMinimum, blockMaximum, aPos + vec3(0.5));
    vec2 modelUv;
    switch (int(aFace + 0.5)) {
        case 0: modelUv = vec2(blockPosition.x, 1.0 - blockPosition.y); break;
        case 1: modelUv = vec2(1.0 - blockPosition.x, 1.0 - blockPosition.y); break;
        case 2: modelUv = vec2(blockPosition.z, 1.0 - blockPosition.y); break;
        case 3: modelUv = vec2(1.0 - blockPosition.z, 1.0 - blockPosition.y); break;
        case 4: modelUv = vec2(blockPosition.z, blockPosition.x); break;
        case 5: modelUv = vec2(blockPosition.x, blockPosition.z); break;
        default: modelUv = vec2(aUV.x, 1.0 - aUV.y); break;
    }
    if (int(aFace + 0.5) >= 4) {
        modelUv = vec2(1.0 - modelUv.y, modelUv.x);
        modelUv.x = 1.0 - modelUv.x;
    }
    TexCoord = tileOrigin + modelUv / 20.0;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
