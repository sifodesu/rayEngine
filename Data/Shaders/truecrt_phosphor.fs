#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 resolution;
uniform float phosphorLayout;
uniform float phosphorPitchPx;
uniform float phosphorRoundness;
uniform float phosphorGap;
uniform float phosphorGain;
uniform float blackMatrix;
uniform float phosphorBleed;

out vec4 finalColor;

vec4 sampleSafe(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return vec4(0.0);
    return texture(texture0, uv);
}

vec3 channelFor(float index) {
    float c = mod(index, 3.0);
    if (c < 0.5) return vec3(1.0, 0.0, 0.0);
    if (c < 1.5) return vec3(0.0, 1.0, 0.0);
    return vec3(0.0, 0.0, 1.0);
}

float lumaOf(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec2 layoutCoord(vec2 px, float layoutType, float pitch) {
    if (layoutType < 1.5) {
        return vec2(px.x / pitch, px.y / (pitch * 0.8660254));
    }
    if (layoutType < 2.5) {
        return vec2(px.x / pitch, px.y / max(resolution.y, 1.0));
    }
    if (layoutType < 3.5) {
        return vec2(px.x / pitch, px.y / (pitch * 1.8));
    }
    return vec2(px.x / pitch, px.y / (pitch * 0.92));
}

vec2 cellCenterPx(vec2 cell, float layoutType, float pitch) {
    if (layoutType < 1.5) {
        float rowShift = mod(cell.y, 2.0) * 0.5;
        return vec2((cell.x + 0.5 + rowShift) * pitch, (cell.y + 0.5) * pitch * 0.8660254);
    }
    if (layoutType < 2.5) {
        return vec2((cell.x + 0.5) * pitch, (fragTexCoord.y * resolution.y));
    }
    if (layoutType < 3.5) {
        float slotShift = mod(cell.y, 2.0) * 1.5;
        return vec2((cell.x + 0.5 + slotShift) * pitch, (cell.y + 0.5) * pitch * 1.8);
    }
    float rowShift = mod(cell.y, 2.0) * 0.5;
    return vec2((cell.x + 0.5 + rowShift) * pitch, (cell.y + 0.5) * pitch * 0.92);
}

vec3 cellChannel(vec2 cell, float layoutType) {
    if (layoutType < 1.5) return channelFor(cell.x + cell.y * 2.0);
    if (layoutType < 2.5) return channelFor(cell.x);
    if (layoutType < 3.5) return channelFor(cell.x + mod(cell.y, 2.0));
    return channelFor(cell.x + cell.y * 2.0);
}

float phosphorShape(vec2 d, float layoutType, float pitch) {
    float gap = clamp(phosphorGap, 0.0, 0.95);
    float roundness = clamp(phosphorRoundness, 0.0, 1.0);
    if (layoutType > 1.5 && layoutType < 2.5) {
        float stripe = exp(-(d.x * d.x) / max(0.001, 0.13 + roundness * 0.28));
        return stripe * (1.0 - gap * 0.85);
    }

    vec2 q = d;
    if (layoutType > 2.5 && layoutType < 3.5) q.y *= 0.58;
    if (layoutType > 3.5) q *= vec2(1.08, 1.14);
    float r2 = dot(q, q);
    float shadowMask = step(3.5, layoutType);
    float core = exp(-r2 / max(0.001, mix(0.035, 0.022, shadowMask) + roundness * mix(0.16, 0.11, shadowMask)));
    float disc = 1.0 - smoothstep(mix(0.34, 0.25, shadowMask) - gap * 0.22,
                                  mix(0.52, 0.42, shadowMask) - gap * 0.18,
                                  length(q));
    return mix(disc, core, roundness);
}

vec3 phosphorEmission(vec2 px) {
    float layoutType = floor(phosphorLayout + 0.5);
    float pitch = max(phosphorPitchPx, 1.0);
    vec2 grid = layoutCoord(px, layoutType, pitch);
    vec2 baseCell = floor(grid);
    vec3 sum = vec3(0.0);

    for (int y = -3; y <= 3; ++y) {
        for (int x = -3; x <= 3; ++x) {
            vec2 cell = baseCell + vec2(float(x), float(y));
            vec2 centerPx = cellCenterPx(cell, layoutType, pitch);
            vec2 d = (px - centerPx) / pitch;
            vec3 channel = cellChannel(cell, layoutType);
            vec3 source = sampleSafe(centerPx / max(resolution, vec2(1.0))).rgb;
            float sourceEnergy = max(max(source.r, source.g), source.b);
            float phosphorGate = smoothstep(0.012, 0.075, sourceEnergy);
            float energy = dot(source, channel);
            float shape = phosphorShape(d, layoutType, pitch);
            float halo = exp(-dot(d, d) / max(0.001, 0.2 + phosphorBleed * 0.32)) * phosphorBleed * 0.18;
            sum += channel * energy * phosphorGate * (shape * phosphorGain + halo);
        }
    }

    float shadowMask = step(3.5, layoutType);
    float strength = clamp(phosphorGain, 0.0, 2.5);
    float apertureGrille = step(1.5, layoutType) * (1.0 - step(2.5, layoutType));
    float matrix = mix(1.0, mix(0.42, 0.28, shadowMask), clamp(blackMatrix * strength, 0.0, 1.0));
    vec2 local = (px - cellCenterPx(baseCell, layoutType, pitch)) / pitch;
    float aperture = smoothstep(0.58, 0.28, length(local));
    if (apertureGrille > 0.5) {
        float stripe = exp(-(local.x * local.x) / max(0.04, 0.16 + phosphorRoundness * 0.2));
        aperture = mix(1.0 - blackMatrix * 0.75, 1.0, stripe);
    }
    vec3 sourceAtPixel = sampleSafe(px / max(resolution, vec2(1.0))).rgb;
    float sourceLuma = lumaOf(sourceAtPixel);
    float visibleEnergy = smoothstep(0.01, 0.12, max(lumaOf(sum), sourceLuma));
    float chromaVisibility = clamp((0.035 + smoothstep(2.0, 7.0, pitch) * 0.12 - phosphorBleed * 0.035 + shadowMask * 0.025 + apertureGrille * 0.08) * strength, 0.0, 0.5);
    float luminanceAperture = mix(matrix, 1.0 + blackMatrix * 0.05, aperture);
    vec3 lumaMask = sourceAtPixel * luminanceAperture;
    vec3 normalizedPhosphor = sum * (sourceLuma / max(lumaOf(sum), 0.0001));
    vec3 reconstructed = mix(lumaMask, normalizedPhosphor, chromaVisibility);
    float maskAmount = clamp(visibleEnergy * strength, 0.0, 1.0);
    return mix(sourceAtPixel, reconstructed, maskAmount);
}

void main() {
    vec4 source = sampleSafe(fragTexCoord);
    float layoutType = floor(phosphorLayout + 0.5);
    if (layoutType < 0.5) {
        finalColor = source * colDiffuse * fragColor;
        return;
    }

    vec2 px = fragTexCoord * resolution;
    vec3 color = phosphorEmission(px);
    finalColor = vec4(max(color, vec3(0.0)), source.a) * colDiffuse * fragColor;
}
