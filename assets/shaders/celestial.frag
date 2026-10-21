#version 410 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D celestialTexture;
uniform vec4 color;
uniform bool monochrome;

void main() {
    vec4 sampled = texture(celestialTexture, TexCoord);

    // the bundled 1.8.9 environment textures lack alpha but retain RGB premultiplied against black, so recover coverage to reveal the rendered sky
    float coverage = max(sampled.r, max(sampled.g, sampled.b));
    if (coverage < (1.0 / 255.0)) discard;

    sampled.rgb = monochrome ? vec3(1.0) : sampled.rgb / coverage;
    sampled.a = coverage;
    FragColor = sampled * color;
}
