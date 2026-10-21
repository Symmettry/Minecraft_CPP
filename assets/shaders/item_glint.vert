#version 410 core
layout(location = 0) in vec3 aPos;
out vec2 TexCoord;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 instanceModels[5];
uniform float glintOffset;
void main() {
    vec4 world = instanceModels[gl_InstanceID] * vec4(aPos, 1.0);
    TexCoord = vec2((aPos.x + aPos.y) * 0.5 + glintOffset,
        (aPos.y - aPos.x) * 0.5 + glintOffset * 0.5);
    gl_Position = projection * view * world;
}
