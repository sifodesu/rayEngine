#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D upsampleTexture;
uniform vec4 colDiffuse;

uniform float curvature;
uniform float edgeSoftness;
uniform float bloomIntensity;
uniform float bloomPower;
uniform float outputGamma;

out vec4 finalColor;

float lumaOf(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

vec3 colorPow(vec3 color, float powerValue) {
    float actualLuma = max(lumaOf(color), 0.0001);
    vec3 actualColor = color / actualLuma;
    return actualColor * pow(actualLuma, max(powerValue, 0.0001));
}

float crtMask(vec2 uv) {
    vec2 centered = uv * 2.0 - 1.0;
    float r2 = dot(centered, centered);
    vec2 warped = centered * (1.0 + curvature * r2);
    vec2 sourceUv = warped * 0.5 + 0.5;
    float insideSource = step(0.0, sourceUv.x) * step(0.0, sourceUv.y) * step(sourceUv.x, 1.0) * step(sourceUv.y, 1.0);

    float tubeMask = 1.0;
    if (edgeSoftness > 0.0001) {
        float edge = sdRoundBox(centered, vec2(0.925, 0.875), 0.19);
        tubeMask = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, edge);
    }
    return insideSource * tubeMask;
}

void main() {
    vec4 preBloom = texture(texture0, fragTexCoord);
    vec4 blurred = texture(upsampleTexture, fragTexCoord);
    vec3 color = preBloom.rgb + colorPow(max(blurred.rgb, vec3(0.0)), bloomPower) * bloomIntensity;
    float mask = crtMask(fragTexCoord);
    color *= mask;

    float g = max(outputGamma, 1.0);
    color = pow(max(color, vec3(0.0)), vec3(1.0 / g));
    finalColor = vec4(clamp(color, vec3(0.0), vec3(1.0)), preBloom.a * mask) * colDiffuse * fragColor;
}
