#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 resolution;
uniform float time;
uniform float signalMode;
uniform float inputGamma;
uniform float lumaBandwidth;
uniform float chromaBandwidth;
uniform float chromaDelay;
uniform float ntscPhase;
uniform float dotCrawl;

out vec4 finalColor;

float lumaOf(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 srgbToWorking(vec3 color) {
    float g = max(inputGamma, 1.0);
    return pow(max(color, vec3(0.0)), vec3(g));
}

vec4 sampleSafe(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return vec4(0.0);
    return texture(texture0, uv);
}

vec3 blurHorizontal(vec2 uv, float radiusPx) {
    vec2 px = vec2(1.0 / max(resolution.x, 1.0), 0.0);
    vec3 sum = sampleSafe(uv).rgb * 0.38;
    sum += sampleSafe(uv - px * radiusPx).rgb * 0.24;
    sum += sampleSafe(uv + px * radiusPx).rgb * 0.24;
    sum += sampleSafe(uv - px * radiusPx * 2.0).rgb * 0.07;
    sum += sampleSafe(uv + px * radiusPx * 2.0).rgb * 0.07;
    return sum;
}

void main() {
    vec2 uv = fragTexCoord;
    vec4 raw = sampleSafe(uv);
    vec3 source = srgbToWorking(raw.rgb);
    float mode = floor(signalMode + 0.5);
    if (mode < 0.5) {
        finalColor = vec4(source, raw.a) * colDiffuse * fragColor;
        return;
    }

    float lumaRadius = (1.0 - clamp(lumaBandwidth, 0.0, 1.0)) * 3.0;
    float chromaRadius = (1.0 - clamp(chromaBandwidth, 0.0, 1.0)) * 7.0;

    vec3 lumaFiltered = mix(source, srgbToWorking(blurHorizontal(uv, max(lumaRadius, 0.001))), step(0.001, lumaRadius));
    float y = lumaOf(lumaFiltered);

    vec2 delayUv = uv - vec2(chromaDelay / max(resolution.x, 1.0), 0.0);
    vec3 chromaFiltered = mix(source, srgbToWorking(blurHorizontal(delayUv, max(chromaRadius, 0.001))), step(0.001, chromaRadius));
    vec3 chroma = chromaFiltered - vec3(lumaOf(chromaFiltered));

    float crawlPhase = (fragTexCoord.x * resolution.x * 0.5) + (fragTexCoord.y * resolution.y) + time * 59.94 + ntscPhase * 4.0;
    float crawl = sin(crawlPhase * 3.14159265) * dotCrawl * 0.08 * step(1.5, mode);
    chroma.rg += vec2(crawl, -crawl);

    vec3 color = max(vec3(y) + chroma, vec3(0.0));
    finalColor = vec4(color, raw.a) * colDiffuse * fragColor;
}
