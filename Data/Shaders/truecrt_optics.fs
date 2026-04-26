#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 resolution;
uniform float curvature;
uniform float vignette;
uniform float edgeSoftness;
uniform float brightness;
uniform float saturation;
uniform float glassDiffusion;
uniform float halation;
uniform float tubeGlow;
uniform float whitePoint;

out vec4 finalColor;

float lumaOf(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

vec4 sampleSafe(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return vec4(0.0);
    return texture(texture0, uv);
}

vec3 blur9(vec2 uv, float radiusPx) {
    vec2 px = radiusPx / max(resolution, vec2(1.0));
    vec3 sum = sampleSafe(uv).rgb * 0.24;
    sum += sampleSafe(uv + vec2( px.x, 0.0)).rgb * 0.12;
    sum += sampleSafe(uv + vec2(-px.x, 0.0)).rgb * 0.12;
    sum += sampleSafe(uv + vec2(0.0,  px.y)).rgb * 0.12;
    sum += sampleSafe(uv + vec2(0.0, -px.y)).rgb * 0.12;
    sum += sampleSafe(uv + vec2( px.x,  px.y)).rgb * 0.07;
    sum += sampleSafe(uv + vec2(-px.x,  px.y)).rgb * 0.07;
    sum += sampleSafe(uv + vec2( px.x, -px.y)).rgb * 0.07;
    sum += sampleSafe(uv + vec2(-px.x, -px.y)).rgb * 0.07;
    return sum;
}

void main() {
    vec2 centered = fragTexCoord * 2.0 - 1.0;
    float r2 = dot(centered, centered);
    vec2 warped = centered * (1.0 + curvature * r2);
    vec2 sourceUv = warped * 0.5 + 0.5;
    float insideSource = step(0.0, sourceUv.x) * step(0.0, sourceUv.y) * step(sourceUv.x, 1.0) * step(sourceUv.y, 1.0);

    vec4 source = sampleSafe(sourceUv);
    vec3 soft = blur9(sourceUv, mix(1.0, 8.0, clamp(glassDiffusion, 0.0, 1.5)));
    float bright = smoothstep(0.25, 1.8, lumaOf(source.rgb));
    vec3 color = source.rgb;
    color = mix(color, soft, clamp(glassDiffusion, 0.0, 1.0) * 0.22);
    color += soft * halation * bright * 0.55;
    color += vec3(0.13, 0.08, 0.16) * tubeGlow * smoothstep(1.15, 0.0, r2) * 0.22;

    float gray = lumaOf(color);
    color = mix(vec3(gray), color, saturation);
    color *= brightness * whitePoint;
    color *= 1.0 - vignette * smoothstep(0.12, 1.38, r2);

    float tubeMask = 1.0;
    if (edgeSoftness > 0.0001) {
        float edge = sdRoundBox(centered, vec2(0.925, 0.875), 0.19);
        tubeMask = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, edge);
    }
    float mask = insideSource * tubeMask;
    color *= mask;

    finalColor = vec4(max(color, vec3(0.0)), source.a * mask) * colDiffuse * fragColor;
}
