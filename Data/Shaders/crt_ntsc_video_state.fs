#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D prevTexture;
uniform vec4 colDiffuse;
uniform float frameTime;
uniform vec2 resolution;
uniform float ntscLineRateHz;
uniform float ntscAgcResponse;
uniform float ntscClampResponse;
uniform float ntscHorizontalPllBandwidthHz;

out vec4 finalColor;

float signalAt(float lineTimeUs, float lineIndex) {
    float linePeriodUs = 1000000.0 / max(ntscLineRateHz, 1.0);
    vec2 uv = vec2(lineTimeUs / linePeriodUs,
        (lineIndex + 0.5) / max(resolution.y, 1.0));
    return texture(texture0, uv).r * 2.50 - 0.75;
}

void main() {
    float blank = 0.0;
    float syncTip = 0.0;
    float edgeTimeSum = 0.0;
    float edgeWeightSum = 0.0;

    // Use several ordinary active lines so vertical-sync serrations never
    // disturb the receiver clamp or gain detector.
    for (int sampleIndex = 0; sampleIndex < 8; ++sampleIndex) {
        float line = 36.0 + float(sampleIndex) * 24.0;
        blank += signalAt(8.45 + float(sampleIndex) * 0.08, line);
        syncTip += signalAt(1.10 + float(sampleIndex) * 0.32, line);
    }
    blank /= 8.0;
    syncTip /= 8.0;

    // Locate the trailing horizontal-sync edge instead of assuming it always
    // occurs at 4.70 us. A differentiating phase detector feeds a persistent
    // first-order horizontal AFC loop.
    for (int edgeSample = 0; edgeSample < 12; ++edgeSample) {
        float edgeTime = 3.60 + float(edgeSample) * 0.20;
        float before = signalAt(edgeTime - 0.10, 72.0);
        float after = signalAt(edgeTime + 0.10, 72.0);
        float weight = max(after - before, 0.0);
        edgeTimeSum += edgeTime * weight;
        edgeWeightSum += weight;
    }
    float measuredEdgeError = edgeWeightSum > 0.0001
        ? edgeTimeSum / edgeWeightSum - 4.70 : 0.0;

    float syncExcursion = max(blank - syncTip, 0.001);
    float measuredGain = clamp(0.40 / syncExcursion, 0.50, 2.0);
    float measuredLock = smoothstep(0.16, 0.32, syncExcursion);
    vec4 previous = texture(prevTexture, vec2(0.5));
    float previousLock = clamp(previous.b, 0.0, 1.0);
    float previousBlank = previous.r * 2.50 - 0.75;
    float previousGain = previous.g * 2.0;
    float previousEdgeError = (previous.a - 0.5) * 4.0;
    float dt = max(frameTime, 0.0);

    if (previousLock < 0.001 || dt > 0.5) {
        previousBlank = blank;
        previousGain = measuredGain;
        previousEdgeError = measuredEdgeError;
    }

    float clampAlpha = 1.0 - exp(-dt / max(ntscClampResponse, 0.001));
    float agcAlpha = 1.0 - exp(-dt / max(ntscAgcResponse, 0.001));
    float filteredBlank = mix(previousBlank, blank, clampAlpha);
    float filteredGain = mix(previousGain, measuredGain, agcAlpha);
    float filteredLock = mix(previousLock, measuredLock,
        1.0 - exp(-dt / 0.050));
    float horizontalAlpha = 1.0 - exp(-2.0 * 3.14159265358979323846 *
        max(ntscHorizontalPllBandwidthHz, 0.1) * dt);
    float filteredEdgeError = mix(previousEdgeError, measuredEdgeError,
        clamp(horizontalAlpha, 0.0, 1.0) * measuredLock);

    vec4 stored = vec4(
        clamp((filteredBlank + 0.75) / 2.50, 0.0, 1.0),
        clamp(filteredGain * 0.5, 0.0, 1.0),
        filteredLock,
        clamp(filteredEdgeError / 4.0 + 0.5, 0.0, 1.0)
    );
    finalColor = stored * colDiffuse * fragColor;
}
