#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in float fragBlend;
in vec3 fragNormal;
in vec3 fragCamDir;
in vec3 fragLightDir;

uniform sampler2D compFrameTexture;
uniform sampler2D shadowMaskTexture;
uniform vec4 colDiffuse;

uniform float time;
uniform vec2 uvScalar;
uniform vec2 uvOffset;
uniform vec2 crtMaskScale;
uniform float sampleOverscan;
uniform float curvature;
uniform float glow;
uniform float dotMask;
uniform float dotBlur;
uniform float bleed;
uniform float dotGridSize;
uniform float hexGrid;
uniform float alternateLineShift;
uniform float scanline;
uniform float chromaticAberration;
uniform float dimming;
uniform float saturation;
uniform float maskBrightness;
uniform float maskOpacity;
uniform float reflectionScalar;
uniform float diffuseBrightness;
uniform float specBrightness;
uniform float specPower;
uniform float fresnelBrightness;
uniform float brightness;
uniform vec4 frameColor;

out vec4 finalColor;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec4 sampleComposite(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return vec4(0.0);
    uv.y = 1.0 - uv.y;
    return texture(compFrameTexture, uv);
}

vec3 brightPart(vec3 color) {
    float bright = max(max(color.r, color.g), color.b);
    return color * smoothstep(0.22, 0.92, bright);
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

vec4 sampleSourceCell(vec2 cell, vec2 texSize) {
    float rowShift = mod(cell.y, 2.0) * alternateLineShift;
    vec2 centerPx = vec2(
        (cell.x + 0.5 + rowShift) * gridSize(),
        (cell.y + 0.5) * gridSize() * gridYPitch()
    );
    return sampleComposite(centerPx / max(texSize, vec2(1.0)));
}

vec4 sampleCell(vec2 cell, vec2 texSize) {
    vec4 sampleValue = sampleSourceCell(cell, texSize);
    sampleValue.rgb = applyPhosphorGrid(sampleValue.rgb, cell);
    return sampleValue;
}

vec3 sampleRgbPhosphors(vec2 cell, vec2 local, vec2 texSize) {
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
            vec3 source = sampleSourceCell(sourceCell, texSize).rgb;
            float energy = dot(source, channel);
            vec2 d = local - offset;
            float core = exp(-dot(d, d) / coreRadius2);
            float halo = exp(-dot(d, d) / haloRadius2) * bleed * 0.55;
            sum += channel * energy * (core * 2.85 + halo);
        }
    }

    return clamp(sum, 0.0, 1.35);
}

vec3 samplePhosphorBleed(vec2 cell, vec2 local, vec2 texSize) {
    float radius = mix(0.34, 1.45, clamp(dotBlur, 0.0, 1.0)) + bleed * 0.34;
    float radius2 = max(radius * radius, 0.0001);
    vec3 sum = vec3(0.0);
    float weightSum = 0.0;

    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            vec2 offset = vec2(float(x), float(y));
            vec2 d = local - offset;
            float w = exp(-dot(d, d) / radius2);
            sum += sampleCell(cell + offset, texSize).rgb * w;
            weightSum += w;
        }
    }

    return sum / max(weightSum, 0.0001);
}

vec4 sampleCRT(vec2 uv) {
    vec2 scaledUv = uv * uvScalar + uvOffset;
    vec2 maskUv = scaledUv * crtMaskScale;
    vec3 mask = texture(shadowMaskTexture, maskUv).rgb + vec3(maskBrightness);
    mask = mix(vec3(1.0), mask, clamp(maskOpacity, 0.0, 1.0));

    vec2 overUv = scaledUv * sampleOverscan - ((sampleOverscan - 1.0) * 0.5);
    vec2 centered = overUv - vec2(0.5);
    float rsq = dot(centered, centered);
    overUv = centered + centered * (-curvature * rsq) + vec2(0.5);

    vec2 texSize = vec2(textureSize(compFrameTexture, 0));
    vec3 source = sampleComposite(overUv).rgb;
    vec2 pixel = overUv * texSize;
    vec2 dotPx = vec2(pixel.x / gridSize(), pixel.y / (gridSize() * gridYPitch()));
    float rowShift = mod(floor(dotPx.y), 2.0) * alternateLineShift;
    vec2 shiftedDotPx = dotPx - vec2(rowShift, 0.0);
    vec2 cell = floor(shiftedDotPx);
    vec2 pixelLocal = fract(shiftedDotPx) - vec2(0.5);

    float dist = length(pixelLocal);
    float dotEffect = clamp(max(dotMask, min(glow, 1.0)), 0.0, 1.0);
    vec3 base = sampleCell(cell, texSize).rgb;
    vec3 phosphor = samplePhosphorBleed(cell, pixelLocal, texSize);
    vec3 color = mix(source, mix(base, phosphor, clamp(bleed, 0.0, 1.0)), dotEffect);
    float rgbPhosphorAmount = phosphorAmount() * clamp(dotMask, 0.0, 1.0);
    color = mix(color, sampleRgbPhosphors(cell, pixelLocal, texSize), rgbPhosphorAmount);

    float core = 1.0 - smoothstep(0.12, mix(0.62, 0.96, dotBlur), dist);
    float halo = exp(-dist * dist * mix(6.2, 1.45, dotBlur));
    float wideHalo = exp(-dist * dist * mix(2.8, 0.62, dotBlur));
    float roundPixel = clamp(core * 0.50 + halo * 0.72 + wideHalo * bleed * 0.34, 0.0, 1.65);
    float aperture = mix(1.0, roundPixel * 1.03, clamp(dotMask, 0.0, 1.0));
    aperture = mix(aperture, 1.0, rgbPhosphorAmount);
    float scan = 1.0 - scanline * (0.5 + 0.5 * cos(pixel.y * 6.2831853));

    vec2 radialUv = normalize(uv - vec2(0.5) + vec2(0.0001)) * chromaticAberration / max(texSize, vec2(1.0));
    vec3 chromaGlow;
    chromaGlow.r = brightPart(sampleComposite(overUv + radialUv).rgb).r;
    chromaGlow.g = brightPart(phosphor).g;
    chromaGlow.b = brightPart(sampleComposite(overUv - radialUv).rgb).b;
    vec3 glowBleed = chromaGlow * glow * 0.24;
    color += glowBleed;

    color *= mask;
    color *= aperture * scan;
    color += glowBleed * (halo + wideHalo * bleed) * 0.28;

    float gray = luma(color);
    color = mix(vec3(gray), color, saturation);
    return vec4(color, 1.0);
}

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 camDir = normalize(fragCamDir);
    vec3 lightDir = normalize(fragLightDir);

    float diffuse = max(dot(normal, lightDir), 0.0);
    vec3 worldUp = vec3(0.0, 0.0, 1.0);
    float hemi = dot(normal, worldUp) * 0.5 + 0.5;
    hemi = hemi * 0.4 + 0.3;
    vec3 colorDiff = frameColor.rgb * (diffuse + hemi) * diffuseBrightness;

    vec3 halfVec = normalize(lightDir + camDir);
    float spec = pow(max(dot(normal, halfVec), 0.0), max(specPower, 0.001));
    vec3 colorSpec = vec3(0.25) * spec * specBrightness;

    vec3 emissive = sampleCRT(fragTexCoord).rgb;
    colorSpec += emissive * fragBlend * reflectionScalar;

    float fresnel = 1.0 - dot(camDir, normal);
    fresnel = fresnel * fresnel * fresnelBrightness;
    vec3 colorFres = vec3(0.15) * fresnel;

    vec3 color = (colorFres + colorDiff + colorSpec) * mix(vec3(1.0), fragColor.rgb, clamp(dimming, 0.0, 1.0));
    color *= brightness;

    finalColor = vec4(clamp(color, 0.0, 1.0), frameColor.a) * colDiffuse;
}
