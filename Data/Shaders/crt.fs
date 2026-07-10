#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D bloomTexture;
uniform sampler2D wideBloomTexture;
uniform vec4 colDiffuse;
uniform float time;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform float ntscFrameRateHz;
uniform float outputGamma;
uniform float bloomIntensity;
uniform float wideBloomIntensity;
uniform float halation;
uniform float curvatureX;
uniform float curvatureY;
uniform float pincushion;
uniform float highVoltageBloom;
uniform float overscan;
uniform float cornerRadius;
uniform float vignette;
uniform float glassTransmission;
uniform vec3 glassTint;
uniform float reflection;
uniform float blackLevel;
uniform float brightness;
uniform float saturation;
uniform float flicker;
uniform float noise;

out vec4 finalColor;

const float PI = 3.14159265358979323846;

float luma(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float sdRoundBox(vec2 p, vec2 b, float radius) {
    vec2 q = abs(p) - b + vec2(radius);
    return length(max(q, vec2(0.0))) +
        min(max(q.x, q.y), 0.0) - radius;
}

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 sourceUvFromTube(vec2 tubeP) {
    float radius2 = dot(tubeP, tubeP);
    vec2 sourceP = tubeP *
        (vec2(1.0) + vec2(max(curvatureX, 0.0), max(curvatureY, 0.0)) * radius2);
    sourceP.x *= 1.0 + max(pincushion, 0.0) * tubeP.y * tubeP.y;
    sourceP.y *= 1.0 + max(pincushion, 0.0) * tubeP.x * tubeP.x;
    sourceP /= max(overscan, 0.25);

    vec2 probeTubeUv = sourceP * 0.5 + 0.5;
    vec2 probePx = displayRect.xy + probeTubeUv * displayRect.zw;
    vec2 probeUv = probePx / max(resolution, vec2(1.0));
    float localDrive = luma(max(texture(texture0, probeUv).rgb, vec3(0.0)));

    // Beam current loads the 22 kV supply and reduces deflection/focus. The
    // small local expansion is the visible high-voltage bloom of bright areas.
    float hvExpansion = 1.0 + max(highVoltageBloom, 0.0) *
        smoothstep(0.45, 1.45, localDrive);
    sourceP /= hvExpansion;
    vec2 sourceTubeUv = sourceP * 0.5 + 0.5;
    vec2 sourcePx = displayRect.xy + sourceTubeUv * displayRect.zw;
    return sourcePx / max(resolution, vec2(1.0));
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 screenPx = uv * resolution;
    vec2 tubeUv = (screenPx - displayRect.xy) /
        max(displayRect.zw, vec2(1.0));
    vec2 tubeP = tubeUv * 2.0 - 1.0;
    float radius2 = dot(tubeP, tubeP);
    vec2 sourceUv = sourceUvFromTube(tubeP);
    vec2 sourceTubeUv = (sourceUv * resolution - displayRect.xy) /
        max(displayRect.zw, vec2(1.0));
    bool insideSource = sourceTubeUv.x >= 0.0 && sourceTubeUv.y >= 0.0 &&
                        sourceTubeUv.x <= 1.0 && sourceTubeUv.y <= 1.0;

    float rounded = max(cornerRadius, 0.001);
    float tubeDistance = sdRoundBox(tubeP, vec2(1.0 - rounded), rounded);
    float edgeAa = max(fwidth(tubeDistance) * 1.5, 0.001);
    float tubeMask = 1.0 - smoothstep(-edgeAa, edgeAa, tubeDistance);
    if (!insideSource) tubeMask = 0.0;

    vec3 emission = insideSource
        ? max(texture(texture0, sourceUv).rgb, vec3(0.0))
        : vec3(0.0);
    vec3 localBloom = insideSource
        ? max(texture(bloomTexture, sourceUv).rgb, vec3(0.0))
        : vec3(0.0);
    vec3 wideBloom = insideSource
        ? max(texture(wideBloomTexture, sourceUv).rgb, vec3(0.0))
        : vec3(0.0);

    vec3 color = emission;
    color += localBloom * max(bloomIntensity, 0.0);
    color += wideBloom * max(wideBloomIntensity, 0.0);
    color += luma(wideBloom) * vec3(1.00, 0.43, 0.20) * max(halation, 0.0);

    float gray = luma(color);
    color = mix(vec3(gray), color, max(saturation, 0.0));
    color *= max(glassTransmission, 0.0) * max(glassTint, vec3(0.0));
    float glassVignette = 1.0 - max(vignette, 0.0) *
        smoothstep(0.16, 1.52, radius2);
    color *= max(glassVignette, 0.0);

    // The A34JLN60X uses a dark-tint bonded faceplate. Reflections remain
    // visible over black, while emitted light is attenuated by the glass.
    vec2 reflectionP = (tubeP - vec2(-0.30, -0.38)) / vec2(0.88, 0.36);
    float glassHighlight = exp(-dot(reflectionP, reflectionP) * 3.2);
    color += vec3(0.70, 0.80, 0.92) * glassHighlight *
        max(reflection, 0.0) * (1.0 - clamp(gray, 0.0, 1.0));

    float supplyRipple = 1.0 - max(flicker, 0.0) *
        (0.5 + 0.5 * sin(2.0 * PI * ntscFrameRateHz * time));
    color *= supplyRipple;
    float noiseValue = hash12(gl_FragCoord.xy + floor(time * 120.0)) - 0.5;
    color += noiseValue * max(noise, 0.0);
    color += vec3(max(blackLevel, 0.0));

    // Simulated scene-referred tube radiance is retained in half float until
    // this eye/camera response and the transfer function of the host display.
    vec3 mapped = vec3(1.0) - exp(-max(color, vec3(0.0)) *
        max(brightness, 0.0) * 1.35);
    mapped = pow(clamp(mapped, vec3(0.0), vec3(1.0)),
        vec3(1.0 / max(outputGamma, 0.1)));
    mapped *= tubeMask;

    finalColor = vec4(
        mapped * colDiffuse.rgb * fragColor.rgb,
        tubeMask * colDiffuse.a * fragColor.a
    );
}
