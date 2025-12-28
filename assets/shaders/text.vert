#version 410 core
layout(location = 0) in vec2 aPos;  // pixel-space position
layout(location = 1) in vec2 aTex;

out vec2 TexCoord;

uniform mat4 projection;

void main()
{
    // aPos is already in pixel units
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    TexCoord = aTex;
}
