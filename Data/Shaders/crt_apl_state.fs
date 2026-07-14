#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D prevTexture;
uniform sampler2D previousDriveTexture;
uniform vec4 colDiffuse;
uniform float frameTime;
uniform float highVoltageResponse;
uniform float highVoltageSag;
uniform float bPlusResponse;
uniform float bPlusSag;
uniform float beamCurrentLimit;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform float crtFramePhase;
uniform float crtFramesAdvanced;
uniform float ntscLineRateHz;
uniform float ntscFrameRateHz;
uniform float ntscActiveLines;
uniform float ntscActiveStartLine;
uniform float ntscContentLines;
uniform float ntscContentStartLine;
uniform float ntscActiveVideoUs;

out vec4 finalColor;

float luma(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    float currentApl = 0.0;
    float currentPeak = 0.0;
    float framePeriod = 1.0 / max(ntscFrameRateHz, 1.0);
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 16; ++x) {
            vec2 tubeUv = (vec2(float(x), float(y)) + 0.5) /
                vec2(16.0, 12.0);
            vec2 uv = (displayRect.xy + tubeUv * displayRect.zw) /
                max(resolution, vec2(1.0));
            float rasterLine = ntscContentStartLine +
                tubeUv.y * ntscContentLines;
            float activeTime = 9.40e-6 + tubeUv.x *
                ntscActiveVideoUs * 1.0e-6;
            float scanPhase = (rasterLine / max(ntscLineRateHz, 1.0) +
                activeTime) / framePeriod;
            float currentLuma = luma(max(texture(texture0, uv).rgb,
                vec3(0.0)));
            float previousLuma = luma(max(texture(previousDriveTexture, uv).rgb,
                vec3(0.0)));
            float scanMix = crtFramesAdvanced > 1.5
                ? 1.0 : step(scanPhase, crtFramePhase);
            float sampleLuma = mix(previousLuma, currentLuma, scanMix);
            currentApl += sampleLuma;
            currentPeak = max(currentPeak, sampleLuma);
        }
    }
    currentApl /= 192.0;

    vec4 previous = texture(prevTexture, vec2(0.5));
    float previousApl = max(previous.r, 0.0);
    float dt = max(frameTime, 0.0);
    float loadResponse = 1.0 - exp(-dt / 0.012);
    float filteredApl = mix(previousApl, currentApl, loadResponse);
    float normalizedLoad = smoothstep(max(beamCurrentLimit, 0.05) * 0.18,
        max(beamCurrentLimit, 0.05), 0.72 * filteredApl + 0.28 * currentPeak);

    // Lumped flyback/EHT and regulated B+ supplies. Each rail follows the
    // exact first-order RC solution, with a faster discharge under load than
    // recovery through the source impedance.
    float previousHv = previous.g > 0.25 ? previous.g : 1.0;
    float targetHv = clamp(1.0 - max(highVoltageSag, 0.0) * normalizedLoad,
        0.50, 1.05);
    float hvTau = max(highVoltageResponse, 0.001) *
        (targetHv < previousHv ? 0.30 : 1.0);
    float hv = mix(previousHv, targetHv, 1.0 - exp(-dt / hvTau));

    float previousBPlus = previous.b > 0.25 ? previous.b : 1.0;
    float targetBPlus = clamp(1.0 - max(bPlusSag, 0.0) * normalizedLoad,
        0.65, 1.05);
    float bPlusTau = max(bPlusResponse, 0.001) *
        (targetBPlus < previousBPlus ? 0.45 : 1.0);
    float bPlus = mix(previousBPlus, targetBPlus,
        1.0 - exp(-dt / bPlusTau));

    float heater = previous.a > 0.25 ? previous.a : 1.0;
    finalColor = vec4(filteredApl, hv, bPlus, heater) *
        colDiffuse * fragColor;
}
