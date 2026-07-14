#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float ntscSubcarrierMHz;
uniform float ntscLineRateHz;
uniform float ntscChromaBandwidthIMHz;
uniform float ntscLineCombStrength;

out vec4 finalColor;

const float PI = 3.14159265358979323846;
const int BAND_RADIUS = 8;

float compositeAt(vec2 uv) {
    return texture(texture0, uv).r * 2.50 - 0.75;
}

float sincPi(float value) {
    float x = PI * value;
    return abs(x) < 0.0001 ? 1.0 : sin(x) / x;
}

float windowAt(float offset) {
    float radius = float(BAND_RADIUS) + 1.0;
    return 0.42 + 0.50 * cos(PI * offset / radius) +
        0.08 * cos(2.0 * PI * offset / radius);
}

float rawBandTap(float offset, float cutoff, float carrierOmega) {
    float lowpass = 2.0 * cutoff * sincPi(2.0 * cutoff * offset);
    return 2.0 * lowpass * cos(carrierOmega * offset) * windowAt(offset);
}

void main() {
    float sampleRateMHz = max(resolution.x * ntscLineRateHz / 1000000.0, 1.0);
    float carrierOmega = 2.0 * PI * ntscSubcarrierMHz / sampleRateMHz;
    float halfBandwidthMHz = max(ntscChromaBandwidthIMHz + 0.20, 0.40);
    float cutoff = clamp(halfBandwidthMHz / sampleRateMHz, 0.005, 0.24);

    // Remove the small DC leakage introduced by truncating the modulated sinc.
    float coefficientSum = 0.0;
    float windowSum = 0.0;
    for (int tap = -BAND_RADIUS; tap <= BAND_RADIUS; ++tap) {
        float offset = float(tap);
        coefficientSum += rawBandTap(offset, cutoff, carrierOmega);
        windowSum += windowAt(offset);
    }
    float dcCorrection = coefficientSum / max(windowSum, 0.0001);

    vec2 texel = vec2(1.0 / max(resolution.x, 1.0), 0.0);
    float bandSum = 0.0;
    float previousBandSum = 0.0;
    float nextBandSum = 0.0;
    float carrierResponse = 0.0;
    vec2 lineTexel = vec2(0.0, 1.0 / max(resolution.y, 1.0));
    for (int tap = -BAND_RADIUS; tap <= BAND_RADIUS; ++tap) {
        float offset = float(tap);
        float coefficient = rawBandTap(offset, cutoff, carrierOmega) -
            dcCorrection * windowAt(offset);
        vec2 sampleUv = fragTexCoord + texel * offset;
        bandSum += compositeAt(sampleUv) * coefficient;
        previousBandSum += compositeAt(sampleUv - lineTexel) * coefficient;
        nextBandSum += compositeAt(sampleUv + lineTexel) * coefficient;
        carrierResponse += coefficient * cos(carrierOmega * offset);
    }

    float response = max(carrierResponse, 0.0001);
    float spatialBand = bandSum / response;
    // NTSC chroma reverses carrier phase on adjacent lines, while a static
    // monochrome detail near Fsc does not. Align and average the neighbours:
    // true colour survives, line-coherent false colour cancels. A partial
    // blend retains some authentic no-comb dot crawl from the CT-1358.
    float lineCombBand = (0.50 * bandSum -
        0.25 * (previousBandSum + nextBandSum)) / response;
    float chromaBand = mix(spatialBand, lineCombBand,
        clamp(ntscLineCombStrength, 0.0, 1.0));
    float center = compositeAt(fragTexCoord);
    float trappedLuma = center - chromaBand * 0.92;

    // R: composite after the narrow 3.58 MHz trap. G: selected chroma band.
    vec2 stored = (vec2(trappedLuma, chromaBand) + 0.75) / 2.50;
    finalColor = vec4(stored.x, stored.y, 0.0, 1.0) *
        vec4(colDiffuse.rgb * fragColor.rgb, colDiffuse.a * fragColor.a);
}
