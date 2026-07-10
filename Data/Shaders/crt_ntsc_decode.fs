#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float time;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform float ntscSubcarrierMHz;
uniform float ntscFrameRateHz;
uniform float ntscActiveVideoUs;
uniform float ntscLumaBandwidthMHz;
uniform float ntscChromaBandwidthIMHz;
uniform float ntscChromaBandwidthQMHz;
uniform float ntscChromaGain;
uniform float ntscChromaDelayNs;
uniform float ntscLumaPeaking;
uniform float horizontalJitter;
uniform vec3 videoGain;
uniform vec3 videoCutoff;
uniform vec3 gunGamma;

out vec4 finalColor;

const float PI = 3.14159265358979323846;
const int FIR_RADIUS = 12;

vec3 yiqToRgb(vec3 yiq) {
    return vec3(
        yiq.x + 0.9563 * yiq.y + 0.6210 * yiq.z,
        yiq.x - 0.2721 * yiq.y - 0.6474 * yiq.z,
        yiq.x - 1.1070 * yiq.y + 1.7046 * yiq.z
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

float decodeComposite(float screenX, float screenY) {
    vec2 uv = vec2(screenX, screenY) / max(resolution, vec2(1.0));
    return texture(texture0, uv).r * 2.50 - 0.75;
}

float carrierPhase(float screenX, float activeLine, float field) {
    float tubeX = (screenX - displayRect.x) / max(displayRect.z, 1.0);
    float xTimeUs = tubeX * ntscActiveVideoUs;
    return 2.0 * PI * ntscSubcarrierMHz * xTimeUs +
        PI * activeLine + 0.5 * PI * mod(field, 4.0);
}

void main() {
    vec2 screenPx = fragTexCoord * resolution;
    vec2 tubeUv = (screenPx - displayRect.xy) /
        max(displayRect.zw, vec2(1.0));
    bool inside = tubeUv.x >= 0.0 && tubeUv.y >= 0.0 &&
                  tubeUv.x <= 1.0 && tubeUv.y <= 1.0;
    if (!inside) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float activeLine = floor(tubeUv.y * 240.0) + 21.0;
    float field = floor(time * ntscFrameRateHz);
    float sampleRateMHz = max(displayRect.z / max(ntscActiveVideoUs, 1.0), 1.0);
    float cutoffY = clamp(ntscLumaBandwidthMHz / sampleRateMHz, 0.005, 0.49);
    float cutoffI = clamp(ntscChromaBandwidthIMHz / sampleRateMHz, 0.005, 0.49);
    float cutoffQ = clamp(ntscChromaBandwidthQMHz / sampleRateMHz, 0.005, 0.49);
    float delaySamples = ntscChromaDelayNs * 0.001 * sampleRateMHz;

    // A healthy set still has small line-dependent AFC/yoke error. It moves
    // the decoded waveform, not the already formed RGB image.
    float jitter = horizontalJitter * (
        0.72 * sin(activeLine * 1.618 + time * 97.0) +
        0.28 * sin(activeLine * 0.071 + time * 17.0));
    float centerX = screenPx.x + jitter;
    float lumaSum = 0.0;
    float lumaWeight = 0.0;
    float iSum = 0.0;
    float iWeight = 0.0;
    float qSum = 0.0;
    float qWeight = 0.0;

    // The CT-1358 has no comb filter. Both branches therefore come from the
    // same one-dimensional composite waveform; dot crawl, cross-colour and
    // hanging chroma arise naturally instead of being overlaid as noise.
    for (int tap = -FIR_RADIUS; tap <= FIR_RADIUS; ++tap) {
        float offset = float(tap);
        float sampleX = centerX + offset;
        float composite = decodeComposite(sampleX, screenPx.y);
        float wy = lowpassTap(offset, cutoffY);
        lumaSum += composite * wy;
        lumaWeight += wy;

        float chromaX = centerX + offset - delaySamples;
        float chroma = decodeComposite(chromaX, screenPx.y);
        float phase = carrierPhase(chromaX, activeLine, field);
        float wi = lowpassTap(offset, cutoffI);
        float wq = lowpassTap(offset, cutoffQ);
        iSum += chroma * (2.0 * cos(phase)) * wi;
        qSum += chroma * (2.0 * sin(phase)) * wq;
        iWeight += wi;
        qWeight += wq;
    }

    float y = lumaSum / max(lumaWeight, 0.00001);
    float center = decodeComposite(centerX, screenPx.y);
    float neighbors = 0.5 * (
        decodeComposite(centerX - 1.0, screenPx.y) +
        decodeComposite(centerX + 1.0, screenPx.y));
    y += (center - neighbors) * ntscLumaPeaking;
    vec2 iq = vec2(
        iSum / max(iWeight, 0.00001),
        qSum / max(qWeight, 0.00001)
    ) * ntscChromaGain;

    vec3 drive = max(yiqToRgb(vec3(y, iq)) - videoCutoff, vec3(0.0));
    drive *= videoGain;
    vec3 linearLight = pow(max(drive, vec3(0.0)), max(gunGamma, vec3(0.1)));
    finalColor = vec4(linearLight, 1.0) *
        vec4(colDiffuse.rgb * fragColor.rgb, colDiffuse.a * fragColor.a);
}
