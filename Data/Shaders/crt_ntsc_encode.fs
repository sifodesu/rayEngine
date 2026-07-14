#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float time;
uniform vec2 resolution;
uniform vec2 targetResolution;
uniform vec2 nativeResolution;
uniform vec4 displayRect;
uniform float ntscSubcarrierMHz;
uniform float ntscLineRateHz;
uniform float ntscFrameRateHz;
uniform float crtFramePhase;
uniform float ntscTotalLines;
uniform float ntscActiveLines;
uniform float ntscActiveStartLine;
uniform float ntscActiveVideoUs;
uniform float ntscSetupLevel;
uniform float ntscNoise;
uniform float ntscHum;

out vec4 finalColor;

const float PI = 3.14159265358979323846;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float verticalSyncWaveform(float lineIndex, float lineTimeUs,
                           float linePeriodUs,
                           out bool verticalPulseRegion) {
    // Native 240p repeats the same fixed vertical interval every
    // progressive frame: six pre-equalising, six serrated broad-sync and six
    // post-equalising half-line pulses, with no half-line frame displacement.
    // Keep the integer raster row as the source of truth. Reconstructing it
    // by dividing accumulated float time can round an exact line boundary to
    // the preceding row and produce a one-line vertical-sync glitch.
    float verticalLine = lineIndex;
    float halfLinePhase = mod(lineTimeUs, linePeriodUs * 0.5);
    verticalPulseRegion = verticalLine < 9.0;
    if (!verticalPulseRegion) return 0.0;

    if (verticalLine < 3.0 || verticalLine >= 6.0) {
        // 2.3 us equalising pulses at twice horizontal frequency.
        float pulse = 1.0 - smoothstep(2.23, 2.37, halfLinePhase);
        return -0.40 * pulse;
    }

    // Broad sync pulses separated by 4.7 us serrations so horizontal AFC can
    // remain locked throughout vertical retrace.
    float broadEnd = linePeriodUs * 0.5 - 4.70;
    float pulse = 1.0 - smoothstep(broadEnd - 0.07,
        broadEnd + 0.07, halfLinePhase);
    return -0.40 * pulse;
}

vec3 loadYiq() {
    vec3 stored = texture(texture0, fragTexCoord).rgb;
    return vec3(stored.r, (stored.g - 0.5) * 1.20,
        (stored.b - 0.5) * 1.05);
}

void main() {
    float linePeriodUs = 1000000.0 / max(ntscLineRateHz, 1.0);
    float lineIndex = floor(fragTexCoord.y * targetResolution.y);
    float lineTimeUs = fragTexCoord.x * linePeriodUs;
    // 262 is even, so 227.5 carrier cycles per line return to the same colour
    // phase at every progressive frame boundary. There is no colour-frame
    // sequence to inject here.
    float carrierPhase = 2.0 * PI * ntscSubcarrierMHz * lineTimeUs +
        PI * lineIndex;

    // System M line timing measured from the leading edge of horizontal sync.
    const float syncEndUs = 4.70;
    const float burstStartUs = 5.30;
    const float burstEndUs = 7.82;
    const float activeStartUs = 9.40;
    float activeEndUs = activeStartUs + ntscActiveVideoUs;
    bool verticalBlank = lineIndex < ntscActiveStartLine ||
        lineIndex >= ntscActiveStartLine + ntscActiveLines ||
        lineIndex >= ntscTotalLines;

    bool verticalPulseRegion = false;
    float verticalWaveform = verticalSyncWaveform(
        lineIndex, lineTimeUs, linePeriodUs, verticalPulseRegion);

    float composite = 0.0; // blanking level
    if (verticalPulseRegion) {
        composite = verticalWaveform;
    } else if (lineTimeUs < syncEndUs + 0.07) {
        // The source output stage has a finite ~140 ns transition; retaining
        // it at 4Fsc prevents an impossible one-sample brick-wall edge from
        // spraying energy over the entire receiver passband.
        float syncGate = 1.0 - smoothstep(syncEndUs - 0.07,
            syncEndUs + 0.07, lineTimeUs);
        composite = -0.40 * syncGate;
    } else if (lineTimeUs >= burstStartUs && lineTimeUs < burstEndUs) {
        // Burst is suppressed only during the equalising/serrated vertical
        // sync interval. It returns on the ordinary blanking lines before
        // and after active picture, as on a free-running System-M source.
        float burstEnvelope = smoothstep(burstStartUs,
            burstStartUs + 0.30, lineTimeUs) *
            (1.0 - smoothstep(burstEndUs - 0.30,
                burstEndUs, lineTimeUs));
        composite = 0.20 * burstEnvelope * sin(carrierPhase);
    } else if (!verticalBlank && lineTimeUs >= activeStartUs &&
               lineTimeUs < activeEndUs) {
        float activeX = (lineTimeUs - activeStartUs) /
            max(ntscActiveVideoUs, 0.001);
        vec3 yiq = loadYiq();
        float phase = carrierPhase + radians(33.0);
        float encoded = yiq.x + yiq.y * cos(phase) + yiq.z * sin(phase);
        float setup = clamp(ntscSetupLevel, 0.0, 0.20);
        float activeEnvelope = smoothstep(activeStartUs,
            activeStartUs + 0.14, lineTimeUs) *
            (1.0 - smoothstep(activeEndUs - 0.14,
                activeEndUs, lineTimeUs));
        composite = (setup + encoded * (1.0 - setup)) * activeEnvelope;
    }

    float framePeriod = 1.0 / max(ntscFrameRateHz, 1.0);
    float frameStart = time - crtFramePhase * framePeriod;
    float rasterTime = frameStart + lineIndex /
        max(ntscLineRateHz, 1.0) + lineTimeUs * 1.0e-6;
    float mainsHum = sin(2.0 * PI * 60.0 * rasterTime) * ntscHum;
    float randomNoise = (hash12(gl_FragCoord.xy +
        floor(time * ntscLineRateHz)) - 0.5) * 2.0 * ntscNoise;
    composite += mainsHum + randomNoise;

    // RGBA16F preserves overshoot and ringing outside the nominal range. An
    // RGBA8 fallback will clamp naturally, but the faithful path stays linear.
    float storedComposite = (composite + 0.75) / 2.50;
    finalColor = vec4(storedComposite, storedComposite, storedComposite, 1.0) *
        vec4(colDiffuse.rgb * fragColor.rgb, colDiffuse.a * fragColor.a);
}
