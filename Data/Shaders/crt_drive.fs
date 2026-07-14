#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D aplTexture;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform float time;
uniform float crtFramePhase;
uniform float ntscLineRateHz;
uniform float ntscFrameRateHz;
uniform float ntscActiveLines;
uniform float ntscActiveStartLine;
uniform float ntscContentLines;
uniform float ntscContentStartLine;
uniform float ntscActiveVideoUs;
uniform float videoOutputBandwidthMHz;
uniform float cathodeDriveHeadroom;
uniform float spaceChargeCompression;
uniform float beamCurrentLimit;
uniform float beamCurrentCompression;
uniform float bPlusRipple;
uniform vec3 videoGain;
uniform vec3 videoCutoff;
uniform vec3 gunGamma;

out vec4 finalColor;

const float PI = 3.14159265358979323846;

float luma(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec2 screenPx = fragTexCoord * resolution;
    vec2 tubeUv = (screenPx - displayRect.xy) /
        max(displayRect.zw, vec2(1.0));
    float framePeriod = 1.0 / max(ntscFrameRateHz, 1.0);
    float frameStart = time - crtFramePhase * framePeriod;
    float rasterLine = ntscContentStartLine +
        clamp(tubeUv.y, 0.0, 1.0) * ntscContentLines;
    float activeTime = 9.40e-6 + clamp(tubeUv.x, 0.0, 1.0) *
        ntscActiveVideoUs * 1.0e-6;
    float rasterTime = frameStart + rasterLine /
        max(ntscLineRateHz, 1.0) + activeTime;

    vec4 supply = texture(aplTexture, vec2(0.5));
    float bPlus = supply.b > 0.25 ? supply.b : 1.0;
    float heater = supply.a > 0.25 ? supply.a : 1.0;
    bPlus *= 1.0 - max(bPlusRipple, 0.0) *
        sin(2.0 * PI * 120.0 * rasterTime);

    // The RGB output transistors and cathode capacitances form one more pole
    // after the jungle IC.  Convert the measured electrical bandwidth into a
    // screen-space Gaussian at the native System-M active-video duration.
    float samplesPerActiveLine = max(displayRect.z, 1.0);
    float sampleRateMHz = samplesPerActiveLine / 52.655;
    float cutoff = max(videoOutputBandwidthMHz, 0.05);
    float sigma = clamp(sampleRateMHz / (2.0 * PI * cutoff), 0.08, 3.0);
    vec2 texel = vec2(1.0 / max(resolution.x, 1.0), 0.0);
    vec3 voltage = vec3(0.0);
    float weightSum = 0.0;
    for (int tap = -4; tap <= 4; ++tap) {
        float x = float(tap);
        float weight = exp(-0.5 * x * x / max(sigma * sigma, 0.0001));
        voltage += max(texture(texture0, fragTexCoord + texel * x).rgb,
            vec3(0.0)) * weight;
        weightSum += weight;
    }
    voltage /= max(weightSum, 0.0001);

    // Cathode cutoff and the Child-Langmuir-like gun transfer are evaluated
    // in voltage/current space.  B+ headroom and space charge act before the
    // deflection and mask, rather than as a brightness filter afterward.
    vec3 cathodeOverdrive = max(voltage * videoGain * bPlus - videoCutoff,
        vec3(0.0));
    cathodeOverdrive /= max(cathodeDriveHeadroom, 0.05);
    vec3 current = pow(cathodeOverdrive, max(gunGamma, vec3(0.1))) * heater;
    current /= vec3(1.0) + max(spaceChargeCompression, 0.0) * current;

    float totalCurrent = luma(current);
    float overCurrent = max(totalCurrent - max(beamCurrentLimit, 0.01), 0.0);
    current /= 1.0 + max(beamCurrentCompression, 0.0) * overCurrent;

    finalColor = vec4(max(current, vec3(0.0)), 1.0) *
        vec4(colDiffuse.rgb * fragColor.rgb, colDiffuse.a * fragColor.a);
}
