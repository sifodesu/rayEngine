#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float time;
uniform vec2 resolution;
uniform float ntscSubcarrierMHz;
uniform float ntscLineRateHz;
uniform float ntscFrameRateHz;
uniform float ntscActiveLines;
uniform float ntscActiveStartLine;

out vec4 finalColor;

const float PI = 3.14159265358979323846;

float chromaBandAt(float lineTimeUs, float lineIndex) {
    float linePeriodUs = 1000000.0 / max(ntscLineRateHz, 1.0);
    vec2 uv = vec2(lineTimeUs / linePeriodUs,
        (lineIndex + 0.5) / max(resolution.y, 1.0));
    return texture(texture0, uv).g * 2.50 - 0.75;
}

void main() {
    float sinCorrelation = 0.0;
    float cosCorrelation = 0.0;
    const float burstStartUs = 5.30;
    const float burstDurationUs = 2.52;

    // One colour oscillator is shared by the receiver. Correlate bursts from
    // several active lines into a single measurement instead of maintaining
    // an impossible independent PLL for every raster line.
    for (int lineSample = 0; lineSample < 16; ++lineSample) {
        float lineIndex = floor(ntscActiveStartLine + 4.0 +
            float(lineSample) * max(ntscActiveLines - 8.0, 1.0) /
            16.0 + 0.5);
        for (int sampleIndex = 0; sampleIndex < 12; ++sampleIndex) {
            float u = (float(sampleIndex) + 0.5) / 12.0;
            float lineTimeUs = burstStartUs + u * burstDurationUs;
            float referencePhase = 2.0 * PI * ntscSubcarrierMHz * lineTimeUs +
                PI * lineIndex;
            float burst = chromaBandAt(lineTimeUs, lineIndex);
            sinCorrelation += burst * sin(referencePhase);
            cosCorrelation += burst * cos(referencePhase);
        }
    }

    float phaseError = atan(cosCorrelation, sinCorrelation);
    float lockEnergy = length(vec2(sinCorrelation, cosCorrelation)) / 192.0;
    float locked = smoothstep(0.005, 0.03, lockEnergy);
    float storedPhase = phaseError / (2.0 * PI) + 0.5;
    finalColor = vec4(storedPhase, locked, lockEnergy, 1.0) *
        colDiffuse * fragColor;
}
