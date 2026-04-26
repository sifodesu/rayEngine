#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D shadowMaskTexture;
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
uniform float overscan;
uniform float pixelRatio;
uniform float dimming;
uniform float saturation;
uniform float maskBrightness;
uniform float maskOpacity;
uniform float maskScale;
uniform float frameEnabled;
uniform float reflectionScalar;
uniform float diffuseBrightness;
uniform float specBrightness;
uniform float specPower;
uniform float fresnelBrightness;
uniform vec3 lightPos;
uniform vec4 frameColor;

out vec4 finalColor;

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec4 sampleSafe(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return vec4(0.0);
    return texture(texture0, uv);
}

vec3 brightPart(vec3 color) {
    float bright = max(max(color.r, color.g), color.b);
    return color * smoothstep(0.22, 0.92, bright);
}

vec2 nativeToUv(vec2 nativePx) {
    vec2 sourcePx = displayRect.xy + (nativePx / max(nativeResolution, vec2(1.0))) * displayRect.zw;
    return sourcePx / max(resolution, vec2(1.0));
}

float gridSize() {
    return max(dotGridSize, 0.25);
}

float phosphorAmount() {
    return step(0.5, hexGrid);
}

float gridYPitch() {
    return mix(1.0, 0.8660254, phosphorAmount());
}

vec3 phosphorChannel(vec2 cell) {
    float index = mod(cell.x + cell.y * 2.0, 3.0);
    if (index < 0.5) return vec3(1.0, 0.0, 0.0);
    if (index < 1.5) return vec3(0.0, 1.0, 0.0);
    return vec3(0.0, 0.0, 1.0);
}

vec3 applyPhosphorGrid(vec3 color, vec2 cell) {
    vec3 phosphor = clamp(color * phosphorChannel(cell) * 2.65, 0.0, 1.0);
    return mix(color, phosphor, phosphorAmount());
}

vec4 sampleNativeSourceDot(vec2 cell) {
    float rowShift = mod(cell.y, 2.0) * alternateLineShift;
    vec2 nativeCenter = vec2(
        (cell.x + 0.5 + rowShift) * gridSize(),
        (cell.y + 0.5) * gridSize() * gridYPitch()
    );
    return sampleSafe(nativeToUv(nativeCenter));
}

vec4 sampleNativeRawDot(vec2 cell) {
    vec4 sampleValue = sampleNativeSourceDot(cell);
    sampleValue.rgb = applyPhosphorGrid(sampleValue.rgb, cell);
    return sampleValue;
}

vec3 sampleRgbPhosphors(vec2 cell, vec2 local) {
    float blur = clamp(dotBlur, 0.0, 1.0);
    float coreRadius = mix(0.075, 0.22, blur);
    float haloRadius = mix(0.22, 0.72, blur) + bleed * 0.20;
    float coreRadius2 = max(coreRadius * coreRadius, 0.0001);
    float haloRadius2 = max(haloRadius * haloRadius, 0.0001);
    vec3 sum = vec3(0.0);

    for (int y = -3; y <= 3; ++y) {
        for (int x = -3; x <= 3; ++x) {
            vec2 offset = vec2(float(x), float(y));
            vec2 sourceCell = cell + offset;
            vec3 channel = phosphorChannel(sourceCell);
            vec3 source = sampleNativeSourceDot(sourceCell).rgb;
            float energy = dot(source, channel);
            vec2 d = local - offset;
            float core = exp(-dot(d, d) / coreRadius2);
            float halo = exp(-dot(d, d) / haloRadius2) * bleed * 0.55;
            sum += channel * energy * (core * 2.85 + halo);
        }
    }

    return clamp(sum, 0.0, 1.35);
}

vec3 samplePhosphorBleed(vec2 cell, vec2 local) {
    float radius = mix(0.34, 1.45, clamp(dotBlur, 0.0, 1.0)) + bleed * 0.34;
    float radius2 = max(radius * radius, 0.0001);
    vec3 sum = vec3(0.0);
    float weightSum = 0.0;

    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            vec2 offset = vec2(float(x), float(y));
            vec2 d = local - offset;
            float w = exp(-dot(d, d) / radius2);
            sum += sampleNativeRawDot(cell + offset).rgb * w;
            weightSum += w;
        }
    }

    return sum / max(weightSum, 0.0001);
}

vec3 frameLighting(vec2 centered, float frameBand) {
    vec3 light = normalize(lightPos);
    vec3 normal = normalize(vec3(centered * vec2(0.7, 0.9), 1.0));
    vec3 view = vec3(0.0, 0.0, 1.0);
    vec3 halfVec = normalize(light + view);

    float diffuse = max(dot(normal, light), 0.0) * diffuseBrightness;
    float spec = pow(max(dot(normal, halfVec), 0.0), max(specPower, 0.001)) * specBrightness;
    float fresnel = pow(max(1.0 - dot(view, normal), 0.0), 2.0) * fresnelBrightness;
    float reflected = smoothstep(0.92, 0.1, length(centered - vec2(0.2, -0.35))) * reflectionScalar;

    vec3 base = frameColor.rgb;
    vec3 tint = vec3(0.175, 0.15, 0.2) * diffuse;
    vec3 highlight = vec3(0.25) * spec + vec3(0.45, 0.4, 0.5) * fresnel + vec3(0.9, 0.8, 0.65) * reflected;
    return (base + tint + highlight) * frameBand;
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 centered = uv * 2.0 - 1.0;
    float r2 = dot(centered, centered);

    vec2 warpedCentered = centered * (1.0 + curvature * r2);
    vec2 sourceUv = warpedCentered * 0.5 + 0.5;
    sourceUv = (sourceUv - vec2(0.5)) / max(overscan, 0.001) + vec2(0.5);
    sourceUv.y = (sourceUv.y - 0.5) * max(pixelRatio, 0.001) + 0.5;

    float insideSource = step(0.0, sourceUv.x) * step(0.0, sourceUv.y) * step(sourceUv.x, 1.0) * step(sourceUv.y, 1.0);
    vec4 source = sampleSafe(sourceUv);

    vec2 sourcePx = sourceUv * resolution;
    vec2 nativePx = ((sourcePx - displayRect.xy) / max(displayRect.zw, vec2(1.0))) * nativeResolution;
    vec2 dotPx = vec2(nativePx.x / gridSize(), nativePx.y / (gridSize() * gridYPitch()));
    float rowShift = mod(floor(dotPx.y), 2.0) * alternateLineShift;
    vec2 shiftedDotPx = dotPx - vec2(rowShift, 0.0);
    vec2 cell = floor(shiftedDotPx);
    vec2 pixelLocal = fract(shiftedDotPx) - vec2(0.5);

    float dist = length(pixelLocal);
    float dotEffect = clamp(max(max(dotMask, min(glow, 1.0)), min(bleed, 1.0)), 0.0, 1.0);
    vec4 base = sampleNativeRawDot(cell);
    vec3 phosphor = samplePhosphorBleed(cell, pixelLocal);
    vec3 color = mix(source.rgb, mix(base.rgb, phosphor, clamp(bleed, 0.0, 1.0)), dotEffect);
    float rgbPhosphorAmount = phosphorAmount() * clamp(dotMask, 0.0, 1.0);
    color = mix(color, sampleRgbPhosphors(cell, pixelLocal), rgbPhosphorAmount);

    float core = 1.0 - smoothstep(0.12, mix(0.62, 0.96, dotBlur), dist);
    float halo = exp(-dist * dist * mix(6.2, 1.45, dotBlur));
    float wideHalo = exp(-dist * dist * mix(2.8, 0.62, dotBlur));
    float roundPixel = clamp(core * 0.50 + halo * 0.72 + wideHalo * bleed * 0.34, 0.0, 1.65);
    float aperture = mix(1.0, roundPixel * 1.03, clamp(dotMask, 0.0, 1.0));
    aperture = mix(aperture, 1.0, rgbPhosphorAmount);
    float scan = 1.0 - scanline * (0.5 + 0.5 * cos(nativePx.y * 6.2831853));

    vec3 bloom = vec3(0.0);
    bloom += brightPart(sampleNativeRawDot(cell + vec2( 1.0,  0.0)).rgb) * 1.15;
    bloom += brightPart(sampleNativeRawDot(cell + vec2(-1.0,  0.0)).rgb) * 1.15;
    bloom += brightPart(sampleNativeRawDot(cell + vec2( 0.0,  1.0)).rgb) * 1.00;
    bloom += brightPart(sampleNativeRawDot(cell + vec2( 0.0, -1.0)).rgb) * 1.00;
    bloom += brightPart(sampleNativeRawDot(cell + vec2( 1.0,  1.0)).rgb) * 0.78;
    bloom += brightPart(sampleNativeRawDot(cell + vec2(-1.0, -1.0)).rgb) * 0.78;
    bloom += brightPart(sampleNativeRawDot(cell + vec2( 2.0,  0.0)).rgb) * 0.58;
    bloom += brightPart(sampleNativeRawDot(cell + vec2(-2.0,  0.0)).rgb) * 0.58;
    bloom += brightPart(sampleNativeRawDot(cell + vec2( 0.0,  2.0)).rgb) * 0.42;
    bloom += brightPart(sampleNativeRawDot(cell + vec2( 0.0, -2.0)).rgb) * 0.42;
    bloom += brightPart(sampleNativeRawDot(cell + vec2( 4.0,  0.0)).rgb) * 0.24;
    bloom += brightPart(sampleNativeRawDot(cell + vec2(-4.0,  0.0)).rgb) * 0.24;

    vec2 radialCell = normalize(centered + vec2(0.0001)) * chromaticAberration;
    vec3 chromaGlow;
    chromaGlow.r = brightPart(sampleNativeRawDot(cell + round(radialCell * vec2(1.25, 0.75))).rgb).r;
    chromaGlow.g = brightPart(phosphor).g;
    chromaGlow.b = brightPart(sampleNativeRawDot(cell - round(radialCell * vec2(1.25, 0.75))).rgb).b;
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
    color *= mix(1.0, 0.72, clamp(dimming, 0.0, 1.0));

    float grayscale = luma(color);
    color = mix(vec3(grayscale), color, saturation);
    float edge = sdRoundBox(centered, vec2(0.925, 0.875), 0.19);
    float safeEdgeSoftness = max(edgeSoftness, 0.0001);
    float rawTubeMask = 1.0 - smoothstep(-safeEdgeSoftness, safeEdgeSoftness, edge);
    float tubeMask = mix(1.0, rawTubeMask, step(0.0001, edgeSoftness));

    color *= brightness;
    color = clamp(color, vec3(0.0), vec3(1.0));

    float screenMask = insideSource * tubeMask;
    vec4 crtColor = vec4(color * colDiffuse.rgb * fragColor.rgb, source.a * colDiffuse.a * fragColor.a);
    vec4 outputColor = mix(vec4(0.0, 0.0, 0.0, colDiffuse.a * fragColor.a), crtColor, screenMask);

    float outerFrame = 1.0 - smoothstep(0.0, 0.035, sdRoundBox(centered, vec2(0.985, 0.935), 0.24));
    float innerFrameCut = 1.0 - smoothstep(-0.01, 0.035, sdRoundBox(centered, vec2(0.925, 0.875), 0.19));
    float frameBand = clamp(outerFrame - innerFrameCut, 0.0, 1.0) * clamp(frameEnabled, 0.0, 1.0);
    vec3 frameRgb = frameLighting(centered, frameBand);
    outputColor.rgb = mix(outputColor.rgb, clamp(frameRgb, vec3(0.0), vec3(1.0)), frameBand);
    outputColor.a = max(outputColor.a, frameBand * frameColor.a * colDiffuse.a * fragColor.a);

    finalColor = outputColor;
}
