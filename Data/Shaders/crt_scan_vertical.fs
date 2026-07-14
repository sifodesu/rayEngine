#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D aplTexture;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform float ntscActiveLines;
uniform float ntscContentLines;
uniform float beamMinWidth;
uniform float beamMaxWidth;
uniform float beamShape;
uniform float beamIntensityWeight;
uniform float beamScanlineStrength;
uniform float spotBloom;
uniform float dynamicFocus;
uniform float misconvergence;
uniform float focusEdgeSoftness;
uniform float astigmatism;

out vec4 finalColor;

float beamLuma(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float generalizedGaussianArea(float shape) {
    // Integral of exp(-abs(x/width)^shape) divided by width. This compact
    // approximation covers the adjustable beam-shape range without requiring
    // a gamma function in GLSL 330 (the exact value at shape 2 is sqrt(pi)).
    float nearGaussian = mix(2.0, 1.78,
        smoothstep(1.0, 2.0, shape));
    return mix(nearGaussian, 1.90,
        smoothstep(3.0, 8.0, shape));
}

vec3 sampleConverged(float uvX, float rasterY, vec2 centered,
                     float edgeAmount) {
    float screenY = displayRect.y + rasterY /
        max(ntscContentLines, 1.0) * displayRect.w;
    vec2 baseUv = vec2(uvX, screenY / max(resolution.y, 1.0));
    vec2 radial = centered / max(length(centered), 0.001);
    vec2 convergenceDirection = radial * vec2(1.0, 0.72) +
        vec2(0.20, -0.10);
    vec2 offset = convergenceDirection * max(misconvergence, 0.0) *
        edgeAmount / max(resolution, vec2(1.0));
    return vec3(
        texture(texture0, baseUv + offset).r,
        texture(texture0, baseUv).g,
        texture(texture0, baseUv - offset).b
    );
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 screenPx = uv * resolution;
    vec2 tubeUv = (screenPx - displayRect.xy) /
        max(displayRect.zw, vec2(1.0));
    bool inside = tubeUv.x >= 0.0 && tubeUv.y >= 0.0 &&
                  tubeUv.x < 1.0 && tubeUv.y < 1.0;
    if (!inside) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec2 centered = tubeUv * 2.0 - 1.0;
    float radius2 = dot(centered, centered);
    float edgeAmount = mix(0.10, 1.0, smoothstep(0.08, 1.35, radius2));
    float focusGrowth = 1.0 + max(focusEdgeSoftness, 0.0) * radius2;
    float yokeAstigmatism = 1.0 + max(astigmatism, 0.0) *
        abs(centered.x * centered.y) * 2.0;
    float rasterY = tubeUv.y * ntscContentLines;
    // The engine emits 192 source-bearing progressive scan positions. The
    // full 262-line electrical interval is preserved by the waveform stages.
    float baseLine = floor(rasterY) + 0.5;
    vec3 emission = vec3(0.0);

    float minimumWidth = max(min(beamMinWidth, beamMaxWidth), 0.05);
    float maximumWidth = max(max(beamMinWidth, beamMaxWidth), minimumWidth);
    float shape = max(beamShape, 1.0);
    float intensityPower = max(beamIntensityWeight, 0.001);
    vec4 supply = texture(aplTexture, vec2(0.5));
    float highVoltage = supply.g > 0.25 ? supply.g : 1.0;
    float voltageDefocus = 1.0 + 1.8 * max(1.0 - highVoltage, 0.0);

    for (int tap = -2; tap <= 2; ++tap) {
        float scanlineY = baseLine + float(tap);
        vec3 signal = sampleConverged(uv.x, scanlineY, centered, edgeAmount);
        float current = max(beamLuma(signal), 0.0);
        vec3 drive = pow(clamp(signal, vec3(0.0), vec3(1.0)),
            vec3(intensityPower));
        vec3 width = mix(vec3(minimumWidth), vec3(maximumWidth), drive) *
            focusGrowth * yokeAstigmatism * voltageDefocus;
        width *= 1.0 + max(spotBloom, 0.0) * sqrt(current);
        width *= 1.0 + max(dynamicFocus, 0.0) * radius2 *
            (0.25 + current);
        float distanceValue = abs(rasterY - scanlineY);
        vec3 weight = exp(-pow(vec3(distanceValue) / width, vec3(shape)));
        // Beam width redistributes a fixed electron current; it must not
        // destroy energy. The former unnormalised kernel reduced dark narrow
        // spots to roughly 35 percent of their intended line-average output.
        vec3 kernelArea = max(
            width * generalizedGaussianArea(shape), vec3(0.05));
        emission += signal * weight / kernelArea;
    }

    vec3 continuous = texture(texture0, uv).rgb;
    emission = mix(continuous, emission,
        clamp(beamScanlineStrength, 0.0, 1.0));
    finalColor = vec4(max(emission, vec3(0.0)), 1.0) *
        vec4(colDiffuse.rgb * fragColor.rgb, colDiffuse.a * fragColor.a);
}
