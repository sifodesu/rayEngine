#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;

out vec4 finalColor;

void main() {
    vec2 texel = 1.0 / max(resolution, vec2(1.0));
    vec3 color = vec3(0.0);
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            float tent = (3.0 - abs(float(x))) * (3.0 - abs(float(y)));
            color += texture(texture0,
                fragTexCoord + vec2(float(x), float(y)) * texel).rgb * tent;
        }
    }
    color /= 81.0;
    finalColor = vec4(color, 1.0) * colDiffuse * fragColor;
}
