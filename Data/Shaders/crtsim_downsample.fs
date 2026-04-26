#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float bloomDownsampleSpread;

out vec4 finalColor;

vec4 sampleSafe(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return vec4(0.0);
    return texture(texture0, uv);
}

void main() {
    vec2 aspect = vec2(resolution.y / max(resolution.x, 1.0), 1.0);
    vec2 bloomScale = aspect * bloomDownsampleSpread;
    vec2 p0 = vec2(0.000000, 0.000000);
    vec2 p1 = vec2(0.000000, 1.000000);
    vec2 p2 = vec2(0.000000, -1.000000);
    vec2 p3 = vec2(-0.866025, 0.500000);
    vec2 p4 = vec2(-0.866025, -0.500000);
    vec2 p5 = vec2(0.866025, 0.500000);
    vec2 p6 = vec2(0.866025, -0.500000);

    vec4 bloom = vec4(0.0);
    bloom += sampleSafe(fragTexCoord + p0 * bloomScale);
    bloom += sampleSafe(fragTexCoord + p1 * bloomScale);
    bloom += sampleSafe(fragTexCoord + p2 * bloomScale);
    bloom += sampleSafe(fragTexCoord + p3 * bloomScale);
    bloom += sampleSafe(fragTexCoord + p4 * bloomScale);
    bloom += sampleSafe(fragTexCoord + p5 * bloomScale);
    bloom += sampleSafe(fragTexCoord + p6 * bloomScale);
    finalColor = bloom * (1.0 / 7.0) * colDiffuse * fragColor;
}
