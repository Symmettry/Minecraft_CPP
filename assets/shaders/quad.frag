#version 410 core
out vec4 FragColor;
in vec2 texCoord;

uniform vec4 uColor;
uniform sampler2D uTexture;
uniform vec4 uUvRect;
uniform bool uUseTexture;
uniform bool uPixelSnap;

void main() {
    if (uUseTexture) {
        if (uPixelSnap) {
            vec2 atlasSize = vec2(textureSize(uTexture, 0));
            vec2 regionOrigin = round(uUvRect.xy * atlasSize);
            vec2 regionSize = round(uUvRect.zw * atlasSize);
            vec2 localTexel = clamp(floor(texCoord * regionSize), vec2(0.0), regionSize - vec2(1.0));
            FragColor = texelFetch(uTexture, ivec2(regionOrigin + localTexel), 0) * uColor;
        } else {
            FragColor = texture(uTexture, uUvRect.xy + texCoord * uUvRect.zw) * uColor;
        }
    } else {
        FragColor = uColor;
    }
}
