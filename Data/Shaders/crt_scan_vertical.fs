#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform vec2 nativeResolution;
uniform vec4 displayRect;
uniform float beamMinWidth;
uniform float beamMaxWidth;
uniform float beamShape;
uniform float beamIntensityWeight;
uniform float beamScanlineStrength;
uniform float misconvergence;
uniform float focusEdgeSoftness;
uniform float astigmatism;

out vec4 finalColor;

vec2 nativeToUv(vec2 nativePx) {
    vec2 screenPx = displayRect.xy +
        (nativePx / max(nativeResolution, vec2(1.0))) * displayRect.zw;
    return screenPx / max(resolution, vec2(1.0));
}

vec3 sampleConverged(float uvX, float nativeY, vec2 centered, float edgeAmount) {
    vec2 baseUv = vec2(uvX, nativeToUv(vec2(0.0, nativeY)).y);
    vec2 radial = centered / max(length(centered), 0.001);
    vec2 convergenceDirection = radial * vec2(1.0, 0.72) + vec2(0.20, -0.10);
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
    vec2 nativePx = ((screenPx - displayRect.xy) /
        max(displayRect.zw, vec2(1.0))) * nativeResolution;
    bool inside = nativePx.x >= 0.0 && nativePx.y >= 0.0 &&
                  nativePx.x < nativeResolution.x &&
                  nativePx.y < nativeResolution.y;
    if (!inside) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec2 centered = nativePx / max(nativeResolution, vec2(1.0)) * 2.0 - 1.0;
    float radius2 = dot(centered, centered);
    float edgeAmount = mix(0.10, 1.0, smoothstep(0.08, 1.35, radius2));
    float focusGrowth = 1.0 + max(focusEdgeSoftness, 0.0) * radius2;
    float yokeAstigmatism = 1.0 + max(astigmatism, 0.0) *
        abs(centered.x * centered.y) * 2.0;
    float baseLine = floor(nativePx.y) + 0.5;
    vec3 emission = vec3(0.0);

    float minimumWidth = max(min(beamMinWidth, beamMaxWidth), 0.05);
    float maximumWidth = max(max(beamMinWidth, beamMaxWidth), minimumWidth);
    float shape = max(beamShape, 1.0);
    float intensityPower = max(beamIntensityWeight, 0.001);

    for (int tap = -2; tap <= 2; ++tap) {
        float scanlineY = baseLine + float(tap);
        vec3 signal = sampleConverged(uv.x, scanlineY, centered, edgeAmount);
        vec3 drive = pow(clamp(signal, vec3(0.0), vec3(1.0)), vec3(intensityPower));
        vec3 width = mix(vec3(minimumWidth), vec3(maximumWidth), drive) *
            focusGrowth * yokeAstigmatism;
        float distanceValue = abs(nativePx.y - scanlineY);
        vec3 weight = exp(-pow(vec3(distanceValue) / width, vec3(shape)));
        emission += signal * weight;
    }

    vec3 continuous = texture(texture0, uv).rgb;
    emission = mix(
        continuous,
        emission,
        clamp(beamScanlineStrength, 0.0, 1.0)
    );
    finalColor = vec4(max(emission, vec3(0.0)), 1.0) *
        vec4(colDiffuse.rgb * fragColor.rgb, colDiffuse.a * fragColor.a);
}
