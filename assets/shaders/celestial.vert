#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

out vec2 TexCoord;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform vec2 uvOffset;
uniform vec2 uvScale;

void main() {
    TexCoord = uvOffset + aUV * uvScale;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
