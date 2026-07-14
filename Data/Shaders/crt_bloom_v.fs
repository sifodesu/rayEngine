#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float bloomRadius;
uniform vec3 bloomRadiusRGB;

out vec4 finalColor;

vec3 sampleChromatic(float offset) {
    vec3 radius = max(bloomRadiusRGB, vec3(0.01));
    vec2 baseStep = vec2(0.0, max(bloomRadius, 0.0) /
        max(resolution.y, 1.0));
    return vec3(
        texture(texture0, fragTexCoord + baseStep * offset * radius.r).r,
        texture(texture0, fragTexCoord + baseStep * offset * radius.g).g,
        texture(texture0, fragTexCoord + baseStep * offset * radius.b).b
    );
}

void main() {
    vec3 color = texture(texture0, fragTexCoord).rgb * 0.2270270270;
    color += sampleChromatic( 1.3846153846) * 0.3162162162;
    color += sampleChromatic(-1.3846153846) * 0.3162162162;
    color += sampleChromatic( 3.2307692308) * 0.0702702703;
    color += sampleChromatic(-3.2307692308) * 0.0702702703;
    finalColor = vec4(color, 1.0) * colDiffuse * fragColor;
}
