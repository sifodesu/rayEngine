#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform float beamHorizontalSigma;
uniform float focusEdgeSoftness;
uniform float astigmatism;
uniform float maskStrength;
uniform float maskTriadsAcross;
uniform float maskType;

out vec4 finalColor;

float periodicDistance(float value, float center) {
    return abs(fract(value - center + 0.5) - 0.5);
}

vec3 horizontalPhosphors(float triadPhase) {
    float sigma = 0.105;
    vec3 phosphor = exp(-0.5 * pow(vec3(
        periodicDistance(triadPhase, 1.0 / 6.0),
        periodicDistance(triadPhase, 3.0 / 6.0),
        periodicDistance(triadPhase, 5.0 / 6.0)
    ) / sigma, vec3(2.0)));
    // Unit average energy: the mask redistributes light, it does not create it.
    return phosphor * 3.80;
}

vec3 slotMask(vec2 tubeUv) {
    float triads = max(maskTriadsAcross, 32.0);
    float physicalRows = triads * 0.75 / 1.35;
    float slotRow = floor(tubeUv.y * physicalRows);
    float stagger = mod(slotRow, 2.0) * 0.5;
    float triadPhase = tubeUv.x * triads + stagger;
    vec3 mask = horizontalPhosphors(triadPhase);

    float verticalPhase = fract(tubeUv.y * physicalRows) - 0.5;
    float slotOpening = exp(-0.5 * pow(verticalPhase / 0.31, 2.0));
    mask *= mix(0.22, 1.30, slotOpening);
    return mask;
}

vec3 physicalMask(vec2 tubeUv) {
    float triads = max(maskTriadsAcross, 32.0);
    vec3 mask = horizontalPhosphors(tubeUv.x * triads);

    if (maskType >= 0.5 && maskType < 1.5) {
        mask = slotMask(tubeUv);
    } else if (maskType >= 1.5) {
        float rows = triads * 0.75;
        float row = floor(tubeUv.y * rows);
        float triadPhase = tubeUv.x * triads + mod(row, 2.0) * 0.5;
        float dotY = fract(tubeUv.y * rows) - 0.5;
        mask = horizontalPhosphors(triadPhase) *
            mix(0.16, 1.48, exp(-0.5 * pow(dotY / 0.22, 2.0)));
    }

    // Analytic prefilter: once a display pixel spans a full triad the physical
    // mask converges to neutral instead of aliasing into the rainbow grid that
    // a naive RGB overlay creates.
    float triadsPerPixel = triads / max(displayRect.z, 1.0);
    float resolvable = 1.0 - smoothstep(0.45, 1.20, triadsPerPixel);
    return mix(vec3(1.0), mask,
        clamp(maskStrength, 0.0, 1.0) * resolvable);
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 screenPx = uv * resolution;
    vec2 tubeUv = (screenPx - displayRect.xy) /
        max(displayRect.zw, vec2(1.0));
    bool inside = tubeUv.x >= 0.0 && tubeUv.y >= 0.0 &&
                  tubeUv.x <= 1.0 && tubeUv.y <= 1.0;
    if (!inside) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec2 centered = tubeUv * 2.0 - 1.0;
    float radius2 = dot(centered, centered);
    float edgeFocus = 1.0 + max(focusEdgeSoftness, 0.0) * radius2;
    float horizontalAstigmatism = 1.0 + max(astigmatism, 0.0) *
        abs(centered.x) * (0.35 + abs(centered.y));
    float sigma = max(beamHorizontalSigma, 0.15) *
        edgeFocus * horizontalAstigmatism;
    vec2 texel = vec2(1.0 / max(resolution.x, 1.0), 0.0);
    vec3 emission = vec3(0.0);
    float weightSum = 0.0;

    for (int tap = -5; tap <= 5; ++tap) {
        float distanceValue = float(tap);
        float weight = exp(-0.5 * pow(distanceValue / sigma, 2.0));
        emission += texture(texture0, uv + texel * distanceValue).rgb * weight;
        weightSum += weight;
    }
    emission /= max(weightSum, 0.0001);
    emission *= physicalMask(tubeUv);

    finalColor = vec4(max(emission, vec3(0.0)), 1.0) *
        vec4(colDiffuse.rgb * fragColor.rgb, colDiffuse.a * fragColor.a);
}
