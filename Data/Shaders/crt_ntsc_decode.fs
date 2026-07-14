#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D burstPhaseTexture;
uniform sampler2D videoStateTexture;
uniform sampler2D verticalStateTexture;
uniform vec4 colDiffuse;
uniform float time;
uniform vec2 resolution;
uniform vec2 targetResolution;
uniform vec2 nativeResolution;
uniform vec4 targetDisplayRect;
uniform float ntscSubcarrierMHz;
uniform float ntscLineRateHz;
uniform float ntscFrameRateHz;
uniform float ntscActiveLines;
uniform float ntscActiveStartLine;
uniform float ntscContentLines;
uniform float ntscContentStartLine;
uniform float ntscActiveVideoUs;
uniform float ntscSetupLevel;
uniform float ntscLumaBandwidthMHz;
uniform float ntscChromaBandwidthIMHz;
uniform float ntscChromaBandwidthQMHz;
uniform float ntscChromaGain;
uniform float ntscChromaDelayNs;
uniform float ntscLumaPeaking;
uniform float ntscDifferentialGain;
uniform float ntscDifferentialPhaseDeg;
uniform float ntscColorKillerThreshold;
uniform float saturation;
uniform float horizontalJitter;

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

vec4 receiverVideoState() {
    vec4 stored = texture(videoStateTexture, vec2(0.5));
    float valid = smoothstep(0.001, 0.08, stored.b);
    // An unbound, freshly cleared or hot-reloaded state texture reads zero.
    // Zero is not a valid AGC gain: fall back to a neutral receiver until the
    // sync detector locks, otherwise one transient state clears the screen.
    float measuredBlank = stored.r * 2.50 - 0.75;
    float measuredGain = stored.g * 2.0;
    float measuredPhase = (stored.a - 0.5) * 4.0;
    float plausible = (abs(measuredBlank) <= 0.25 &&
        measuredGain >= 0.50 && measuredGain <= 2.0 &&
        abs(measuredPhase) <= 2.0) ? 1.0 : 0.0;
    valid *= plausible;
    // A valid loop has its full electrical authority. Implausible or missing
    // state falls back atomically to neutral instead of being partially
    // clamped into a different transfer function.
    float safeBlank = mix(0.0, measuredBlank, valid);
    float safeGain = mix(1.0, measuredGain, valid);
    float safePhase = mix(0.0, measuredPhase, valid);
    return vec4(
        (safeBlank + 0.75) / 2.50,
        safeGain * 0.50,
        stored.b,
        safePhase / 4.0 + 0.50
    );
}

float receiverVerticalPhaseLines() {
    vec4 stored = texture(verticalStateTexture, vec2(0.5));
    float phaseLines = (stored.r - 0.5) * 32.0;
    float plausible = abs(phaseLines) <= 16.0 ? 1.0 : 0.0;
    float valid = smoothstep(0.02, 0.20, stored.b) * plausible;
    return phaseLines * valid;
}

float decodeComposite(float screenX, float screenY) {
    vec2 uv = vec2(screenX, screenY) / max(resolution, vec2(1.0));
    float raw = texture(texture0, uv).r * 2.50 - 0.75;
    vec4 state = receiverVideoState();
    float clampLevel = state.r * 2.50 - 0.75;
    float agcGain = state.g * 2.0;
    return (raw - clampLevel) * agcGain;
}

float decodeChromaBand(float screenX, float screenY) {
    vec2 uv = vec2(screenX, screenY) / max(resolution, vec2(1.0));
    float band = texture(texture0, uv).g * 2.50 - 0.75;
    float agcGain = receiverVideoState().g * 2.0;
    return band * agcGain;
}

vec4 burstStateAt(float activeLine) {
    return texture(burstPhaseTexture, vec2(0.5));
}

float carrierPhase(float screenX, float activeLine) {
    float linePeriodUs = 1000000.0 / max(ntscLineRateHz, 1.0);
    float xTimeUs = screenX / max(resolution.x, 1.0) * linePeriodUs;
    vec4 burstState = burstStateAt(activeLine);
    float phaseError = (burstState.r - 0.5) * 2.0 * PI;
    return 2.0 * PI * ntscSubcarrierMHz * xTimeUs +
        PI * activeLine + phaseError;
}

void main() {
    vec2 outputPx = fragTexCoord * targetResolution;
    vec2 tubeUv = (outputPx - targetDisplayRect.xy) /
        max(targetDisplayRect.zw, vec2(1.0));
    bool inside = tubeUv.x >= 0.0 && tubeUv.y >= 0.0 &&
                  tubeUv.x <= 1.0 && tubeUv.y <= 1.0;
    if (!inside) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Only the 192 source-bearing progressive lines are expanded over the
    // visible tube. Sync and setup-black lines remain in the waveform but do
    // not become duplicate picture rows.
    float activeLine = clamp(ntscContentStartLine + floor(
        tubeUv.y * ntscContentLines + receiverVerticalPhaseLines()),
        ntscContentStartLine,
        ntscContentStartLine + ntscContentLines - 1.0);
    float sampleRateMHz = max(resolution.x * ntscLineRateHz / 1000000.0, 1.0);
    float cutoffY = clamp(ntscLumaBandwidthMHz / sampleRateMHz, 0.005, 0.49);
    float cutoffI = clamp(ntscChromaBandwidthIMHz / sampleRateMHz, 0.005, 0.49);
    float cutoffQ = clamp(ntscChromaBandwidthQMHz / sampleRateMHz, 0.005, 0.49);
    float cutoffYSoft = clamp(cutoffY * 0.55, 0.005, 0.49);
    float delaySamples = ntscChromaDelayNs * 0.001 * sampleRateMHz;

    // A healthy set still has small line-dependent AFC/yoke error. It moves
    // the decoded waveform, not the already formed RGB image.
    float jitter = horizontalJitter * (
        0.72 * sin(activeLine * 1.618 + 2.0 * PI * time * 15.5) +
        0.28 * sin(activeLine * 0.071 + 2.0 * PI * time * 2.75));
    float linePeriodUs = 1000000.0 / max(ntscLineRateHz, 1.0);
    float activeStartUs = 9.40;
    float horizontalPhaseUs =
        (receiverVideoState().a - 0.5) * 4.0;
    float centerX = (activeStartUs + horizontalPhaseUs +
        tubeUv.x * ntscActiveVideoUs) /
        linePeriodUs * resolution.x + jitter;
    float signalY = activeLine + 0.5;
    float lumaSum = 0.0;
    float lumaWeight = 0.0;
    float softLumaSum = 0.0;
    float softLumaWeight = 0.0;
    float iSum = 0.0;
    float iWeight = 0.0;
    float qSum = 0.0;
    float qWeight = 0.0;

    // The CT-1358 has no comb filter: separation is only the horizontal trap
    // and band-pass from the preceding stage. Dot crawl, cross-colour and
    // hanging chroma therefore still arise from the waveform itself.
    for (int tap = -FIR_RADIUS; tap <= FIR_RADIUS; ++tap) {
        float offset = float(tap);
        float sampleX = centerX + offset;
        float composite = decodeComposite(sampleX, signalY);
        float wy = lowpassTap(offset, cutoffY);
        lumaSum += composite * wy;
        lumaWeight += wy;
        float wySoft = lowpassTap(offset, cutoffYSoft);
        softLumaSum += composite * wySoft;
        softLumaWeight += wySoft;

        float chromaX = centerX + offset - delaySamples;
        float chroma = decodeChromaBand(chromaX, signalY);
        // NTSC I/Q axes are rotated 33 degrees from the burst reference.
        // Encoding uses the same axes; demodulating against the raw burst
        // reference would rotate every decoded colour by 33 degrees.
        float phase = carrierPhase(chromaX, activeLine) + radians(33.0);
        float wi = lowpassTap(offset, cutoffI);
        float wq = lowpassTap(offset, cutoffQ);
        iSum += chroma * (2.0 * cos(phase)) * wi;
        qSum += chroma * (2.0 * sin(phase)) * wq;
        iWeight += wi;
        qWeight += wq;
    }

    float y = lumaSum / max(lumaWeight, 0.00001);
    float softY = softLumaSum / max(softLumaWeight, 0.00001);
    y += (y - softY) * ntscLumaPeaking;
    vec2 iq = vec2(
        iSum / max(iWeight, 0.00001),
        qSum / max(qWeight, 0.00001)
    ) * ntscChromaGain;

    vec4 burstState = burstStateAt(activeLine);
    float colorLock = smoothstep(
        max(ntscColorKillerThreshold, 0.0),
        min(max(ntscColorKillerThreshold, 0.0) + 0.16, 1.0),
        burstState.b);
    float accGain = burstState.a * 2.0;
    iq *= colorLock * accGain;

    // The transmitter maps active black to 7.5 IRE and white to 100 IRE.
    // Restore black to zero drive after the receiver clamp and undo the same
    // 0.925 excursion scaling for both luminance and chrominance.
    float setup = clamp(ntscSetupLevel, 0.0, 0.20);
    float activeExcursion = 1.0 - setup;
    y = (y - setup) / activeExcursion;
    iq /= activeExcursion;

    float normalizedLuma = clamp(y, 0.0, 1.0) * 2.0 - 1.0;
    iq *= max(1.0 + ntscDifferentialGain * normalizedLuma, 0.0);
    float differentialPhase = radians(ntscDifferentialPhaseDeg) *
        normalizedLuma;
    iq = mat2(
         cos(differentialPhase), sin(differentialPhase),
        -sin(differentialPhase), cos(differentialPhase)
    ) * iq;
    iq *= max(saturation, 0.0);

    // The decoder outputs cathode-drive voltage. Gun cutoff, output-stage
    // bandwidth, B+ loading and current law belong to crt_drive.fs.
    vec3 cathodeVoltage = max(yiqToRgb(vec3(y, iq)), vec3(0.0));
    finalColor = vec4(cathodeVoltage, 1.0) *
        vec4(colDiffuse.rgb * fragColor.rgb, colDiffuse.a * fragColor.a);
}
