#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
out vec2 TexCoord;
out vec3 ModelPosition;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 instanceModels[5];
uniform bool useInstances;
uniform vec2 uvMinimum;
uniform vec2 uvMaximum;
void main() {
    TexCoord = mix(uvMinimum, uvMaximum, aUV);
    mat4 activeModel = useInstances ? instanceModels[gl_InstanceID] : model;
    ModelPosition = vec3(activeModel * vec4(aPos, 1.0));
    gl_Position = projection * view * activeModel * vec4(aPos, 1.0);
}
