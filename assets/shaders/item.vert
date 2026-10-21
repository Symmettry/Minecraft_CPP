#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aFace;
out vec2 TexCoord;
out vec3 ModelPosition;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 instanceModels[5];
uniform bool useInstances;
uniform vec2 tileOrigins[6];
void main() {
    vec2 uv = aUV;
    int face = int(aFace + 0.5);
    switch (face) {
        case 0: case 1: case 2: case 3: uv.y = 1.0 - uv.y; break;
        default: break;
    }
    TexCoord = tileOrigins[clamp(face, 0, 5)] + uv / 20.0;
    mat4 activeModel = useInstances ? instanceModels[gl_InstanceID] : model;
    ModelPosition = vec3(activeModel * vec4(aPos, 1.0));
    gl_Position = projection * view * activeModel * vec4(aPos, 1.0);
}
