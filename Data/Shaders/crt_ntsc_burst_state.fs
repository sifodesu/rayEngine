#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D prevTexture;
uniform vec4 colDiffuse;
uniform float frameTime;
uniform float ntscBurstPllBandwidthHz;
uniform float ntscAccResponse;

out vec4 finalColor;

const float PI = 3.14159265358979323846;

float wrapPhase(float phase) {
    return mod(phase + PI, 2.0 * PI) - PI;
}

void main() {
    vec4 measurement = texture(texture0, fragTexCoord);
    vec4 previous = texture(prevTexture, fragTexCoord);
    float measuredPhase = (measurement.r - 0.5) * 2.0 * PI;
    float measuredLock = clamp(measurement.g, 0.0, 1.0);
    float measuredEnergy = max(measurement.b, 0.0);
    float previousLock = clamp(previous.b, 0.0, 1.0);
    float dt = max(frameTime, 0.0);

    // A long suspend destroys useful loop phase memory. Reacquire directly
    // from burst instead of displaying the stale oscillator while the capped
    // loop correction crawls back into lock.
    if (dt > 0.5 && measuredLock > 0.0) {
        previous = vec4(measurement.r, 0.5, measuredLock, previous.a);
        previousLock = measuredLock;
    }

    const float maxFrequency = 2.0 * PI * 120.0;
    float previousPhase = (previous.r - 0.5) * 2.0 * PI;
    float previousFrequency = (previous.g - 0.5) * 2.0 * maxFrequency;
    float predictedPhase = wrapPhase(previousPhase + previousFrequency * dt);
    float error = wrapPhase(measuredPhase - predictedPhase);
    float bandwidth = max(ntscBurstPllBandwidthHz, 0.1);
    float proportional = clamp(1.0 - exp(-2.0 * PI * bandwidth * dt),
        0.0, 0.95);
    float integral = 0.25 * proportional * proportional / max(dt, 0.0001);

    float phase = wrapPhase(predictedPhase +
        proportional * error * measuredLock);
    float frequency = clamp(previousFrequency +
        integral * error * measuredLock, -maxFrequency, maxFrequency);

    if (previousLock < 0.001 && measuredLock > 0.0) {
        phase = measuredPhase;
        frequency = 0.0;
    }

    float lockAttack = 1.0 - exp(-dt / 0.035);
    float lockRelease = 1.0 - exp(-dt / 0.120);
    float lockResponse = measuredLock > previousLock ? lockAttack : lockRelease;
    float lock = mix(previousLock, measuredLock, lockResponse);

    float previousAcc = previousLock > 0.001 ? previous.a * 2.0 : 1.0;
    float targetAcc = clamp(0.10 / max(measuredEnergy, 0.005), 0.35, 2.0);
    float accResponse = 1.0 - exp(-dt / max(ntscAccResponse, 0.001));
    float acc = mix(previousAcc, targetAcc, accResponse * measuredLock);

    vec4 stored = vec4(
        phase / (2.0 * PI) + 0.5,
        frequency / (2.0 * maxFrequency) + 0.5,
        lock,
        acc * 0.5
    );
    finalColor = stored * colDiffuse * fragColor;
}
