#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform vec2 targetResolution;
uniform vec2 nativeResolution;
uniform vec4 displayRect;
uniform float ntscLineRateHz;
uniform float ntscActiveLines;
uniform float ntscActiveStartLine;
uniform float ntscContentLines;
uniform float ntscContentStartLine;
uniform float ntscActiveVideoUs;
uniform float ntscSourceLumaBandwidthMHz;
uniform float ntscSourceIBandwidthMHz;
uniform float ntscSourceQBandwidthMHz;

out vec4 finalColor;

const float PI = 3.14159265358979323846;
const int FIR_RADIUS = 12;

vec3 rgbToYiq(vec3 rgb) {
    return vec3(
        dot(rgb, vec3(0.299000,  0.587000,  0.114000)),
        dot(rgb, vec3(0.595716, -0.274453, -0.321263)),
        dot(rgb, vec3(0.211456, -0.522591,  0.311135))
    );
}

float sincPi(float value) {
    float x = PI * value;
    return abs(x) < 0.0001 ? 1.0 : sin(x) / x;
}

float lowpassTap(float offset, float cutoffCyclesPerSample) {
    float radius = float(FIR_RADIUS) + 1.0;
    float window = 0.42 + 0.50 * cos(PI * offset / radius) +
        0.08 * cos(2.0 * PI * offset / radius);
    return 2.0 * cutoffCyclesPerSample *
        sincPi(2.0 * cutoffCyclesPerSample * offset) * window;
}

vec3 sampleSourceTube(vec2 tubeUv) {
    if (tubeUv.x < 0.0 || tubeUv.x > 1.0 ||
        tubeUv.y < 0.0 || tubeUv.y > 1.0) {
        return vec3(0.0);
    }
    vec2 sourcePx = displayRect.xy + tubeUv * displayRect.zw;
    return clamp(texture(texture0,
        sourcePx / max(resolution, vec2(1.0))).rgb, 0.0, 1.0);
}

void main() {
    float linePeriodUs = 1000000.0 / max(ntscLineRateHz, 1.0);
    float lineTimeUs = fragTexCoord.x * linePeriodUs;
    const float activeStartUs = 9.40;
    float activeX = (lineTimeUs - activeStartUs) /
        max(ntscActiveVideoUs, 0.001);
    float lineIndex = floor(fragTexCoord.y * targetResolution.y);
    float contentY = lineIndex - ntscContentStartLine;

    bool isActiveVideo = activeX >= 0.0 && activeX <= 1.0 &&
        contentY >= 0.0 && contentY < ntscContentLines;
    if (!isActiveVideo) {
        finalColor = vec4(0.0, 0.5, 0.5, 1.0) * colDiffuse * fragColor;
        return;
    }

    // Filtering is performed at the fixed 4Fsc waveform rate, so changing the
    // host window size no longer changes the electrical source bandwidth.
    float sampleRateMHz = max(targetResolution.x * ntscLineRateHz / 1000000.0, 1.0);
    float activeSamples = max(ntscActiveVideoUs * sampleRateMHz, 1.0);
    // The engine contributes only 320 independent samples over the 52.655 us
    // active interval (about 6.08 MHz pixel clock). Passing the nominal
    // broadcast 4.2 MHz bandwidth here would preserve resampling images above
    // the source Nyquist limit and manufacture severe cross-colour near Fsc.
    // Model the console/source reconstruction filter before NTSC modulation.
    float sourceNyquistMHz = 0.5 * nativeResolution.x /
        max(ntscActiveVideoUs, 0.001);
    float reconstructedLumaMHz = min(ntscSourceLumaBandwidthMHz,
        sourceNyquistMHz * 0.985);
    vec3 cutoff = clamp(vec3(
        reconstructedLumaMHz,
        ntscSourceIBandwidthMHz,
        ntscSourceQBandwidthMHz
    ) / sampleRateMHz, vec3(0.005), vec3(0.49));
    vec3 sum = vec3(0.0);
    vec3 weightSum = vec3(0.0);
    // The source owns exactly 192 progressive lines. The remaining active
    // electrical lines carry setup black; they are not invented image rows.
    float tubeY = (contentY + 0.5) / ntscContentLines;

    for (int tap = -FIR_RADIUS; tap <= FIR_RADIUS; ++tap) {
        float offset = float(tap);
        vec3 yiq = rgbToYiq(sampleSourceTube(
            vec2(activeX + offset / activeSamples, tubeY)));
        vec3 weight = vec3(
            lowpassTap(offset, cutoff.x),
            lowpassTap(offset, cutoff.y),
            lowpassTap(offset, cutoff.z)
        );
        sum += yiq * weight;
        weightSum += weight;
    }
    vec3 yiq = sum / max(weightSum, vec3(0.00001));
    vec3 stored = vec3(
        yiq.x,
        yiq.y / 1.20 + 0.5,
        yiq.z / 1.05 + 0.5
    );
    finalColor = vec4(stored, 1.0) * colDiffuse * fragColor;
}
