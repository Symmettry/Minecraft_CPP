#version 410 core
in vec2 TexCoord;
in vec3 ModelPosition;
out vec4 FragColor;
uniform sampler2D blockAtlas;
uniform vec3 colorMultiplier;
void main() {
    vec4 color = texture(blockAtlas, TexCoord);
    if (color.a < 0.1) discard;
    vec3 normal = normalize(cross(dFdx(ModelPosition), dFdy(ModelPosition)));
    if (!gl_FrontFacing) normal = -normal;
    vec3 light0 = normalize(vec3(0.2, 1.0, -0.7));
    vec3 light1 = normalize(vec3(-0.2, 1.0, 0.7));
    float diffuse = min(1.0, 0.55 + max(dot(normal, light0), 0.0) * 0.3
        + max(dot(normal, light1), 0.0) * 0.2);
    FragColor = vec4(color.rgb * colorMultiplier * diffuse, color.a);
}
