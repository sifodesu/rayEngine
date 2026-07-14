#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D phosphorTexture;
uniform sampler2D shadowMaskTexture;
uniform sampler2D ntscArtifactTexture;
uniform vec4 colDiffuse;

uniform float time;
uniform vec2 resolution;
uniform vec2 nativeResolution;
uniform vec4 displayRect;
uniform float curvature;
uniform float vignette;
uniform float edgeSoftness;
uniform float glow;
uniform float dotMask;
uniform float dotBlur;
uniform float bleed;
uniform float dotGridSize;
uniform float hexGrid;
uniform float alternateLineShift;
uniform float scanline;
uniform float chromaticAberration;
uniform float brightness;
uniform float sharpness;
uniform float persistence;
uniform float phosphorTrail;
uniform float ntscArtifacts;
uniform float overscan;
uniform float saturation;
uniform float maskBrightness;
uniform float maskOpacity;
uniform float maskScale;
uniform float bloomIntensity;
uniform float bloomSpread;
uniform float bloomPower;

out vec4 finalColor;

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

vec4 sampleSafe(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) {
        return vec4(0.0);
    }
    return texture(texture0, uv);
}

vec4 samplePhosphorSafe(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) {
        return vec4(0.0);
    }
    return texture(phosphorTexture, uv);
}

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 brightPart(vec3 color) {
    float luma = max(max(color.r, color.g), color.b);
    return color * smoothstep(0.22, 0.92, luma);
}

vec3 colorPow(vec3 color, float powerValue) {
    float actualLuma = max(luma(color), 0.0001);
    vec3 actualColor = color / actualLuma;
    return actualColor * pow(actualLuma, max(powerValue, 0.0001));
}

vec2 nativeToUv(vec2 nativePx) {
    vec2 sourcePx = displayRect.xy + (nativePx / max(nativeResolution, vec2(1.0))) * displayRect.zw;
    return sourcePx / max(resolution, vec2(1.0));
}

float gridSize() {
    return max(dotGridSize, 0.25);
}

float hexAmount() {
    return step(0.5, hexGrid);
}

float gridYPitch() {
    return mix(1.0, 0.8660254, hexAmount());
}

vec4 sampleCRTComposite(vec2 uv) {
    vec2 nativeStep = max((displayRect.zw / max(nativeResolution, vec2(1.0))) / max(resolution, vec2(1.0)),
                          1.0 / max(resolution, vec2(1.0)));
    vec2 sourcePx = uv * resolution;
    vec2 nativePx = ((sourcePx - displayRect.xy) / max(displayRect.zw, vec2(1.0))) * nativeResolution;
    vec2 artifactUv = nativePx / vec2(256.0, 224.0);
    vec3 artifact = texture(ntscArtifactTexture, artifactUv).rgb;

    vec4 left = sampleSafe(uv - vec2(nativeStep.x, 0.0));
    vec4 local = sampleSafe(uv);
    vec4 right = sampleSafe(uv + vec2(nativeStep.x, 0.0));
    vec3 tunedArtifact = artifact * clamp(ntscArtifacts, 0.0, 1.0);
    local.rgb = clamp(local.rgb + ((left.rgb - local.rgb) + (right.rgb - local.rgb)) * tunedArtifact, 0.0, 1.0);

    float localLuma = luma(local.rgb);
    float offset = 0.0;
    vec4 neighborLeft = sampleSafe(uv - vec2(nativeStep.x * 1.0, 0.0));
    vec4 neighborRight = sampleSafe(uv + vec2(nativeStep.x * 1.0, 0.0));
    offset += ((localLuma - luma(neighborLeft.rgb)) + (localLuma - luma(neighborRight.rgb))) * 1.0;
    neighborLeft = sampleSafe(uv - vec2(nativeStep.x * 2.0, 0.0));
    neighborRight = sampleSafe(uv + vec2(nativeStep.x * 2.0, 0.0));
    offset += ((localLuma - luma(neighborLeft.rgb)) + (localLuma - luma(neighborRight.rgb))) * -0.3162277;
    neighborLeft = sampleSafe(uv - vec2(nativeStep.x * 3.0, 0.0));
    neighborRight = sampleSafe(uv + vec2(nativeStep.x * 3.0, 0.0));
    offset += ((localLuma - luma(neighborLeft.rgb)) + (localLuma - luma(neighborRight.rgb))) * 0.1;
    local.rgb = clamp(local.rgb + offset * sharpness * mix(vec3(1.0), artifact, clamp(ntscArtifacts, 0.0, 1.0)), 0.0, 1.0);

    // The phosphor buffer is an excitation state: it has already decayed by
    // color channel and quenched low light, so this is not frame persistence.
    vec3 phosphorState = samplePhosphorSafe(uv).rgb;
    float temporalAmount = clamp(max(phosphorTrail, persistence), 0.0, 1.0);
    local.rgb = max(local.rgb, mix(local.rgb, phosphorState, temporalAmount));

    return local;
}

vec4 sampleNativeRawDot(vec2 cell) {
    float rowShift = mod(cell.y, 2.0) * alternateLineShift;
    vec2 nativeCenter = vec2(
        (cell.x + 0.5 + rowShift) * gridSize(),
        (cell.y + 0.5) * gridSize() * gridYPitch()
    );
    return sampleSafe(nativeToUv(nativeCenter));
}

vec4 sampleNativeDot(vec2 cell) {
    float rowShift = mod(cell.y, 2.0) * alternateLineShift;
    vec2 nativeCenter = vec2(
        (cell.x + 0.5 + rowShift) * gridSize(),
        (cell.y + 0.5) * gridSize() * gridYPitch()
    );
    return sampleCRTComposite(nativeToUv(nativeCenter));
}

vec4 sampleNativePhosphorDot(vec2 cell) {
    float rowShift = mod(cell.y, 2.0) * alternateLineShift;
    vec2 nativeCenter = vec2(
        (cell.x + 0.5 + rowShift) * gridSize(),
        (cell.y + 0.5) * gridSize() * gridYPitch()
    );
    return samplePhosphorSafe(nativeToUv(nativeCenter));
}

vec3 samplePhosphorBleed(vec2 cell, vec2 local) {
    float radius = mix(0.34, 1.35, clamp(dotBlur, 0.0, 1.0)) + bleed * 0.30;
    float radius2 = max(radius * radius, 0.0001);
    vec3 sum = vec3(0.0);
    float weightSum = 0.0;

    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            vec2 offset = vec2(float(x), float(y));
            vec2 d = local - offset;
            float w = exp(-dot(d, d) / radius2);
            sum += sampleNativePhosphorDot(cell + offset).rgb * w;
            weightSum += w;
        }
    }

    return sum / max(weightSum, 0.0001);
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 centered = uv * 2.0 - 1.0;
    float r2 = dot(centered, centered);

    vec2 warpedCentered = centered * (1.0 + curvature * r2);
    vec2 sourceUv = warpedCentered * 0.5 + 0.5;
    sourceUv = (sourceUv - vec2(0.5)) / max(overscan, 0.001) + vec2(0.5);
    float insideSource = step(0.0, sourceUv.x) * step(0.0, sourceUv.y) * step(sourceUv.x, 1.0) * step(sourceUv.y, 1.0);
    vec4 source = sampleCRTComposite(sourceUv);

    vec2 sourcePx = sourceUv * resolution;
    vec2 nativePx = ((sourcePx - displayRect.xy) / max(displayRect.zw, vec2(1.0))) * nativeResolution;
    vec2 dotPx = vec2(nativePx.x / gridSize(), nativePx.y / (gridSize() * gridYPitch()));
    float rowShift = mod(floor(dotPx.y), 2.0) * alternateLineShift;
    vec2 shiftedDotPx = dotPx - vec2(rowShift, 0.0);
    vec2 cell = floor(shiftedDotPx);
    vec4 base = sampleNativeDot(cell);

    vec2 pixelLocal = fract(shiftedDotPx) - vec2(0.5);
    float dist = length(pixelLocal);
    float dotEffect = clamp(max(max(dotMask, min(glow, 1.0)), min(bleed, 1.0)), 0.0, 1.0);
    vec3 phosphor = samplePhosphorBleed(cell, pixelLocal);
    vec3 color = mix(source.rgb, mix(base.rgb, phosphor, clamp(bleed, 0.0, 1.0)), dotEffect);

    float core = 1.0 - smoothstep(0.12, mix(0.62, 0.92, dotBlur), dist);
    float halo = exp(-dist * dist * mix(6.2, 1.55, dotBlur));
    float wideHalo = exp(-dist * dist * mix(2.8, 0.72, dotBlur));
    float roundPixel = clamp(core * 0.58 + halo * 0.70 + wideHalo * bleed * 0.26, 0.0, 1.55);
    float aperture = mix(1.0, roundPixel * 1.03, clamp(dotMask, 0.0, 1.0));
    // Center the beam profile inside every displayed dot. Basing this on the
    // native raster made a 0.5-native-pixel dot inherit only one half of the
    // scanline curve, and the animated phase shifted energy between its top
    // and bottom halves.
    float scanProfile = 0.5 - 0.5 * cos(pixelLocal.y * 6.2831853);
    float scan = 1.0 - scanline * scanProfile;

    vec3 bloom = vec3(0.0);
    bloom += brightPart(sampleNativePhosphorDot(cell + vec2( 1.0,  0.0)).rgb) * 1.15;
    bloom += brightPart(sampleNativePhosphorDot(cell + vec2(-1.0,  0.0)).rgb) * 1.15;
    bloom += brightPart(sampleNativePhosphorDot(cell + vec2( 0.0,  1.0)).rgb) * 1.00;
    bloom += brightPart(sampleNativePhosphorDot(cell + vec2( 0.0, -1.0)).rgb) * 1.00;
    bloom += brightPart(sampleNativePhosphorDot(cell + vec2( 1.0,  1.0)).rgb) * 0.78;
    bloom += brightPart(sampleNativePhosphorDot(cell + vec2(-1.0, -1.0)).rgb) * 0.78;
    bloom += brightPart(sampleNativePhosphorDot(cell + vec2( 2.0,  0.0)).rgb) * 0.58;
    bloom += brightPart(sampleNativePhosphorDot(cell + vec2(-2.0,  0.0)).rgb) * 0.58;
    bloom += brightPart(sampleNativePhosphorDot(cell + vec2( 0.0,  2.0)).rgb) * 0.42;
    bloom += brightPart(sampleNativePhosphorDot(cell + vec2( 0.0, -2.0)).rgb) * 0.42;
    bloom += brightPart(sampleNativePhosphorDot(cell + vec2( 4.0,  0.0)).rgb) * 0.24;
    bloom += brightPart(sampleNativePhosphorDot(cell + vec2(-4.0,  0.0)).rgb) * 0.24;
    vec2 radialCell = normalize(centered + vec2(0.0001)) * chromaticAberration;
    vec3 chromaGlow;
    chromaGlow.r = brightPart(sampleNativePhosphorDot(cell + round(radialCell * vec2(1.25, 0.75))).rgb).r;
    chromaGlow.g = brightPart(phosphor).g;
    chromaGlow.b = brightPart(sampleNativePhosphorDot(cell - round(radialCell * vec2(1.25, 0.75))).rgb).b;
    vec3 glowBleed = (bloom * mix(0.12, 0.24, clamp(bleed, 0.0, 1.0)) + chromaGlow * 0.24) * glow;
    color += glowBleed;

    vec2 maskUv = sourceUv * vec2(nativeResolution.x * 0.5, nativeResolution.y) * max(maskScale, 0.001);
    vec3 shadowMask = texture(shadowMaskTexture, maskUv).rgb + vec3(maskBrightness);
    shadowMask = mix(vec3(1.0), shadowMask, clamp(maskOpacity, 0.0, 1.0));
    color *= shadowMask;

    color *= vec3(aperture);
    color += glowBleed * (halo + wideHalo * bleed) * 0.28;
    color *= scan;

    float glass = 1.0 - vignette * smoothstep(0.12, 1.35, r2);
    color *= glass;
    color += vec3(0.10, 0.075, 0.06) * smoothstep(1.05, 0.05, length(centered - vec2(0.18, -0.24))) * glow * 0.10;

    vec2 bloomScale = vec2(bloomSpread * (resolution.y / max(resolution.x, 1.0)), bloomSpread);
    vec3 screenBloom = vec3(0.0);
    screenBloom += brightPart(samplePhosphorSafe(sourceUv).rgb);
    screenBloom += brightPart(samplePhosphorSafe(sourceUv + vec2( 0.000000,  1.000000) * bloomScale).rgb);
    screenBloom += brightPart(samplePhosphorSafe(sourceUv + vec2( 0.000000, -1.000000) * bloomScale).rgb);
    screenBloom += brightPart(samplePhosphorSafe(sourceUv + vec2(-0.866025,  0.500000) * bloomScale).rgb);
    screenBloom += brightPart(samplePhosphorSafe(sourceUv + vec2(-0.866025, -0.500000) * bloomScale).rgb);
    screenBloom += brightPart(samplePhosphorSafe(sourceUv + vec2( 0.866025,  0.500000) * bloomScale).rgb);
    screenBloom += brightPart(samplePhosphorSafe(sourceUv + vec2( 0.866025, -0.500000) * bloomScale).rgb);
    screenBloom *= 1.0 / 7.0;
    color += colorPow(screenBloom, bloomPower) * bloomIntensity;

    float grayscale = luma(color);
    color = mix(vec3(grayscale), color, saturation);
    float edge = sdRoundBox(centered, vec2(0.925, 0.875), 0.19);
    float safeEdgeSoftness = max(edgeSoftness, 0.0001);
    float rawTubeMask = 1.0 - smoothstep(-safeEdgeSoftness, safeEdgeSoftness, edge);
    float tubeMask = mix(1.0, rawTubeMask, step(0.0001, edgeSoftness));

    color *= brightness;

    color = clamp(color, vec3(0.0), vec3(1.0));
    float alpha = mix(source.a, base.a, dotEffect) * tubeMask * insideSource;
    vec4 crtColor = vec4(color * colDiffuse.rgb * fragColor.rgb, alpha * colDiffuse.a * fragColor.a);
    finalColor = mix(vec4(0.0, 0.0, 0.0, colDiffuse.a * fragColor.a), crtColor, insideSource * tubeMask);
}
