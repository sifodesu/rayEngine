#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float bloomRadius;

out vec4 finalColor;

void main() {
    vec2 stepUv = vec2(max(bloomRadius, 0.0) / max(resolution.x, 1.0), 0.0);
    vec3 color = texture(texture0, fragTexCoord).rgb * 0.2270270270;
    color += texture(texture0, fragTexCoord + stepUv * 1.3846153846).rgb * 0.3162162162;
    color += texture(texture0, fragTexCoord - stepUv * 1.3846153846).rgb * 0.3162162162;
    color += texture(texture0, fragTexCoord + stepUv * 3.2307692308).rgb * 0.0702702703;
    color += texture(texture0, fragTexCoord - stepUv * 3.2307692308).rgb * 0.0702702703;
    finalColor = vec4(color, 1.0) * colDiffuse * fragColor;
}
