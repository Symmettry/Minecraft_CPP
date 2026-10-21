#version 410 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;

out vec2 TexCoord;

uniform mat4 projection;
uniform vec2 rotationOrigin;
uniform float rotationRadians;

void main()
{
    float cosine = cos(rotationRadians);
    float sine = sin(rotationRadians);
    vec2 relative = aPos - rotationOrigin;
    vec2 rotated = vec2(
        relative.x * cosine - relative.y * sine,
        relative.x * sine + relative.y * cosine
    ) + rotationOrigin;
    gl_Position = projection * vec4(rotated, 0.0, 1.0);
    TexCoord = aTex;
}
