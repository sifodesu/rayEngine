#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform float time;
uniform float intensity;
uniform float speed;
uniform float pixelShift;
uniform float colorShift;
uniform float orientationJitter;
uniform float blockFlip;
uniform float bandFrequency;
uniform float seed;
uniform vec2 frameUvMin;
uniform vec2 frameUvMax;
uniform vec2 frameSizePx;

out vec4 finalColor;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 frameToTextureUv(vec2 frameUv) {
    return mix(frameUvMin, frameUvMax, clamp(frameUv, vec2(0.0), vec2(1.0)));
}

void main() {
    vec2 frameSpan = max(frameUvMax - frameUvMin, vec2(0.0001));
    vec2 localUv = (fragTexCoord - frameUvMin) / frameSpan;
    localUv = clamp(localUv, vec2(0.0), vec2(1.0));

    float amount = clamp(intensity, 0.0, 4.0);
    float t = time * max(speed, 0.0);
    float bands = max(bandFrequency, 1.0);
    float band = floor(localUv.y * bands);
    float tick = floor(t * 12.0);

    float bandNoise = hash12(vec2(band + seed, tick));
    float burst = smoothstep(0.48, 0.92, bandNoise) * clamp(amount, 0.0, 1.5);
    float shiftPixels = (bandNoise * 2.0 - 1.0) * pixelShift * burst;

    vec2 uv = localUv;
    uv.x += shiftPixels / max(frameSizePx.x, 1.0);

    float orientNoise = hash12(vec2(band * 3.17 + seed, floor(t * 5.0)));
    float orientGate = step(0.86, orientNoise) * clamp(orientationJitter * amount, 0.0, 1.0);
    float bandLocalY = fract(localUv.y * bands);
    float flippedBandY = (band + (1.0 - bandLocalY)) / bands;
    uv.y = mix(uv.y, flippedBandY, orientGate);

    vec2 blockCount = max(ceil(frameSizePx / 8.0), vec2(1.0));
    vec2 blockId = floor(localUv * blockCount);
    vec2 blockMin = blockId / blockCount;
    vec2 blockMax = min((blockId + vec2(1.0)) / blockCount, vec2(1.0));
    vec2 blockSpan = max(blockMax - blockMin, vec2(0.0001));
    vec2 blockUv = clamp((uv - blockMin) / blockSpan, vec2(0.0), vec2(1.0));

    float blockNoise = hash12(blockId + vec2(seed, floor(t * 7.0)));
    float blockGate = step(1.0 - clamp(blockFlip * amount, 0.0, 1.0), blockNoise);
    float blockMode = floor(hash12(blockId + vec2(seed + 41.0, floor(t * 5.0))) * 4.0);

    vec2 flippedBlockUv = blockUv;
    if (blockMode < 1.0) {
        flippedBlockUv.x = 1.0 - flippedBlockUv.x;
    } else if (blockMode < 2.0) {
        flippedBlockUv.y = 1.0 - flippedBlockUv.y;
    } else if (blockMode < 3.0) {
        flippedBlockUv = vec2(flippedBlockUv.y, 1.0 - flippedBlockUv.x);
    } else {
        flippedBlockUv = vec2(1.0 - flippedBlockUv.y, flippedBlockUv.x);
    }
    uv = mix(uv, blockMin + flippedBlockUv * blockSpan, blockGate);

    vec2 texUv = frameToTextureUv(uv);
    vec2 split = vec2(colorShift * amount / max(frameSizePx.x, 1.0), 0.0);

    vec4 base = texture(texture0, texUv);
    vec4 redSample = texture(texture0, frameToTextureUv(uv + split));
    vec4 blueSample = texture(texture0, frameToTextureUv(uv - split));

    vec3 rgb = vec3(redSample.r, base.g, blueSample.b);
    float chromaNoise = hash12(vec2(floor(localUv.y * bands * 0.5) + seed, tick + 19.0));
    rgb += (chromaNoise - 0.5) * 0.18 * amount * burst;
    rgb = mix(base.rgb, rgb, clamp(amount, 0.0, 1.0));

    float alpha = base.a * colDiffuse.a * fragColor.a;
    finalColor = vec4(rgb * colDiffuse.rgb * fragColor.rgb, alpha);
}
