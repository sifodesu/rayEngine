#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D prevTexture;
uniform vec4 colDiffuse;
uniform float frameTime;
uniform vec2 resolution;
uniform float ntscLineRateHz;
uniform float ntscVerticalPllBandwidthHz;

out vec4 finalColor;

const float PI = 3.14159265358979323846;

float signalAt(float lineTimeUs, float lineIndex) {
    float linePeriodUs = 1000000.0 / max(ntscLineRateHz, 1.0);
    vec2 uv = vec2(lineTimeUs / linePeriodUs,
        (lineIndex + 0.5) / max(resolution.y, 1.0));
    return texture(texture0, uv).r * 2.50 - 0.75;
}

float broadSyncScore(float lineIndex) {
    // Ordinary H sync and equalising pulses have returned to blank by these
    // samples. Only a serrated broad-sync half-line remains near sync tip.
    float middleA = signalAt(10.0, lineIndex);
    float middleB = signalAt(20.0, lineIndex);
    float depth = -0.5 * (middleA + middleB);
    return smoothstep(0.16, 0.32, depth);
}

void main() {
    float weightedLine = 0.0;
    float broadEnergy = 0.0;
    for (int line = 0; line < 18; ++line) {
        float score = broadSyncScore(float(line));
        weightedLine += float(line) * score;
        broadEnergy += score;
    }

    float measuredLock = smoothstep(1.2, 2.4, broadEnergy);
    // The three broad-sync lines in this progressive source are 3, 4 and 5;
    // their ideal centroid is line 4. A displacement is a vertical phase error.
    float measuredPhaseLines = broadEnergy > 0.001
        ? weightedLine / broadEnergy - 4.0 : 0.0;

    vec4 previous = texture(prevTexture, vec2(0.5));
    float previousPhaseLines = (previous.r - 0.5) * 32.0;
    float previousFrequency = (previous.g - 0.5) * 240.0;
    float previousLock = clamp(previous.b, 0.0, 1.0);
    float dt = max(frameTime, 0.0);

    if (previousLock < 0.001 || dt > 0.5) {
        previousPhaseLines = measuredPhaseLines;
        previousFrequency = 0.0;
    }

    float predictedPhase = previousPhaseLines + previousFrequency * dt;
    float error = measuredPhaseLines - predictedPhase;
    float bandwidth = max(ntscVerticalPllBandwidthHz, 0.1);
    float proportional = clamp(1.0 - exp(-2.0 * PI * bandwidth * dt),
        0.0, 0.98);
    float integral = 0.25 * proportional * proportional /
        max(dt, 0.0001);
    float phaseLines = predictedPhase + proportional * error * measuredLock;
    float frequency = clamp(previousFrequency +
        integral * error * measuredLock, -120.0, 120.0);

    float lockTau = measuredLock > previousLock ? 0.050 : 0.180;
    float lock = mix(previousLock, measuredLock,
        1.0 - exp(-dt / lockTau));
    vec4 stored = vec4(
        clamp(phaseLines / 32.0 + 0.5, 0.0, 1.0),
        clamp(frequency / 240.0 + 0.5, 0.0, 1.0),
        lock,
        clamp(broadEnergy / 3.0, 0.0, 1.0)
    );
    finalColor = stored * colDiffuse * fragColor;
}
