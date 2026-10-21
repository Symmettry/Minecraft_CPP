#version 410 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in float alpha;
out vec2 TexCoord;
out float Alpha;
uniform mat4 projection;
uniform mat4 view;
void main() {
    TexCoord = uv;
    Alpha = alpha;
    gl_Position = projection * view * vec4(position, 1.0);
}
