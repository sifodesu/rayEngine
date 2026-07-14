#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D maskThermalTexture;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform float beamHorizontalSigma;
uniform float focusEdgeSoftness;
uniform float astigmatism;
uniform float maskStrength;
uniform float maskTriadsAcross;
uniform float maskType;
uniform float maskDoming;
uniform float maskCrosstalk;
uniform float phosphorSaturation;

out vec4 finalColor;

const float PI = 3.14159265358979323846;

float sincPi(float value) {
    float x = PI * value;
    return abs(x) < 0.0001 ? 1.0 : sin(x) / x;
}

float filteredPeriodicGaussian(float phase, float center, float sigma,
                               float footprint) {
    float value = 1.0;
    for (int harmonic = 1; harmonic <= 5; ++harmonic) {
        float k = float(harmonic);
        float coefficient = 2.0 * exp(-2.0 * PI * PI * sigma * sigma * k * k);
        coefficient *= sincPi(k * footprint);
        value += coefficient * cos(2.0 * PI * k * (phase - center));
    }
    return max(value, 0.0);
}

vec3 horizontalPhosphors(float triadPhase, float footprint) {
    float sigma = 0.105;
    return vec3(
        filteredPeriodicGaussian(triadPhase, 1.0 / 6.0, sigma, footprint),
        filteredPeriodicGaussian(triadPhase, 3.0 / 6.0, sigma, footprint),
        filteredPeriodicGaussian(triadPhase, 5.0 / 6.0, sigma, footprint)
    );
}

vec3 slotMask(vec2 tubeUv) {
    float triads = max(maskTriadsAcross, 32.0);
    float physicalRows = triads * 0.75 / 1.35;
    float slotRow = floor(tubeUv.y * physicalRows);
    float stagger = mod(slotRow, 2.0) * 0.5;
    float triadPhase = tubeUv.x * triads + stagger;
    float triadFootprint = max(fwidth(tubeUv.x * triads), 0.0001);
    vec3 mask = horizontalPhosphors(triadPhase, triadFootprint);

    float rowPhase = tubeUv.y * physicalRows;
    float rowFootprint = max(fwidth(rowPhase), 0.0001);
    float slotSigma = 0.31;
    float gaussianMean = sqrt(2.0 * PI) * slotSigma;
    float slotOpening = clamp(gaussianMean * filteredPeriodicGaussian(
        rowPhase, 0.5, slotSigma, rowFootprint), 0.0, 1.0);
    mask *= mix(0.48, 1.16, slotOpening);
    return mask;
}

vec3 physicalMask(vec2 tubeUv, float temperature) {
    float triads = max(maskTriadsAcross, 32.0);
    vec2 centered = tubeUv * 2.0 - 1.0;
    // Local steel-mask heating changes the aperture landing position. Doming
    // is zero at the mechanical centre and grows with radial displacement.
    float thermalPhase = max(maskDoming, 0.0) * temperature *
        centered.x * (0.35 + dot(centered, centered));
    float triadCoordinate = tubeUv.x * triads + thermalPhase;
    float triadFootprint = max(fwidth(tubeUv.x * triads), 0.0001);
    vec3 mask = horizontalPhosphors(triadCoordinate, triadFootprint);

    if (maskType >= 0.5 && maskType < 1.5) {
        vec2 domedUv = tubeUv;
        domedUv.x += thermalPhase / triads;
        mask = slotMask(domedUv);
    } else if (maskType >= 1.5) {
        float rows = triads * 0.75;
        float row = floor(tubeUv.y * rows);
        float triadPhase = triadCoordinate + mod(row, 2.0) * 0.5;
        float dotSigma = 0.22;
        float dotFootprint = max(fwidth(tubeUv.y * rows), 0.0001);
        float dotShape = clamp(sqrt(2.0 * PI) * dotSigma *
            filteredPeriodicGaussian(tubeUv.y * rows, 0.5, dotSigma,
                dotFootprint), 0.0, 1.0);
        mask = horizontalPhosphors(triadPhase, triadFootprint) *
            mix(0.16, 1.48, dotShape);
    }

    // Fourier coefficients are integrated over the actual pixel footprint, so
    // unresolved phosphor structure converges to neutral without moire.
    vec3 filtered = mix(vec3(1.0), mask, clamp(maskStrength, 0.0, 1.0));
    float leakage = clamp(maskCrosstalk, 0.0, 0.25);
    return mix(filtered, vec3(dot(filtered, vec3(1.0 / 3.0))), leakage);
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
    float maskTemperature = max(texture(maskThermalTexture, uv).r, 0.0);
    emission *= physicalMask(tubeUv, maskTemperature);
    float saturationScale = max(phosphorSaturation, 0.01);
    emission = (vec3(1.0) - exp(-max(emission, vec3(0.0)) /
        saturationScale)) * saturationScale;

    finalColor = vec4(max(emission, vec3(0.0)), 1.0) *
        vec4(colDiffuse.rgb * fragColor.rgb, colDiffuse.a * fragColor.a);
}
