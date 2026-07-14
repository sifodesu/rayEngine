#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D bloomTexture;
uniform sampler2D wideBloomTexture;
uniform sampler2D aplTexture;
uniform vec4 colDiffuse;
uniform float time;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform float ntscLineRateHz;
uniform float ntscFrameRateHz;
uniform float ntscActiveLines;
uniform float ntscActiveStartLine;
uniform float ntscContentLines;
uniform float ntscContentStartLine;
uniform float ntscActiveVideoUs;
uniform float crtFramePhase;
uniform float outputGamma;
uniform float tubePeakNits;
uniform float hostPeakNits;
uniform float referenceWhiteRadiance;
uniform float bloomThreshold;
uniform float bloomIntensity;
uniform float wideBloomIntensity;
uniform float halation;
uniform float highVoltageSag;
uniform float cornerRadius;
uniform float vignette;
uniform float glassTransmission;
uniform vec3 glassTint;
uniform float glassDispersion;
uniform float glassRefractiveIndex;
uniform float glassThicknessMm;
uniform vec3 glassAbsorption;
uniform float internalReflection;
uniform float ambientIlluminance;
uniform float faceplateCurvatureX;
uniform float faceplateCurvatureY;
uniform float reflection;
uniform float blackLevel;
uniform float brightness;
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

vec3 brightContribution(vec3 color) {
    float peak = max(max(color.r, color.g), color.b);
    float threshold = max(bloomThreshold, 0.0);
    float knee = max(threshold * 0.35, 0.001);
    float soft = clamp((peak - threshold + knee) /
        (2.0 * knee), 0.0, 1.0);
    soft = soft * soft * (3.0 - 2.0 * soft);
    float contribution = max(peak - threshold, 0.0) + soft * knee;
    return color * contribution / max(peak, 0.0001);
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 screenPx = uv * resolution;
    vec2 tubeUv = (screenPx - displayRect.xy) /
        max(displayRect.zw, vec2(1.0));
    vec2 tubeP = tubeUv * 2.0 - 1.0;
    float radius2 = dot(tubeP, tubeP);
    float refractiveIndex = max(glassRefractiveIndex, 1.0001);
    vec3 surfaceNormal = normalize(vec3(
        -max(faceplateCurvatureX, 0.0) * tubeP.x * 2.0,
        -max(faceplateCurvatureY, 0.0) * tubeP.y * 2.0,
        1.0));
    float cosOutside = clamp(surfaceNormal.z, 0.05, 1.0);
    float sinInside2 = max(1.0 - cosOutside * cosOutside, 0.0) /
        (refractiveIndex * refractiveIndex);
    float cosInside = sqrt(max(1.0 - sinInside2, 0.0025));
    float opticalPathMm = max(glassThicknessMm, 0.0) / cosInside;
    float f0 = pow((refractiveIndex - 1.0) /
        (refractiveIndex + 1.0), 2.0);
    float fresnel = f0 + (1.0 - f0) * pow(1.0 - cosOutside, 5.0);
    vec2 faceplateCurvature = max(vec2(
        faceplateCurvatureX, faceplateCurvatureY), vec2(0.0));
    // Refraction must not perform a second unbounded barrel transform after
    // deflection. Anchor all four phosphor-plane edges to the glass aperture
    // and retain only the interior optical displacement.
    float opticalScaleX = (1.0 + faceplateCurvature.x * radius2) /
        (1.0 + faceplateCurvature.x * (1.0 + tubeP.y * tubeP.y));
    float opticalScaleY = (1.0 + faceplateCurvature.y * radius2) /
        (1.0 + faceplateCurvature.y * (1.0 + tubeP.x * tubeP.x));
    vec2 opticalP = tubeP * vec2(opticalScaleX, opticalScaleY);
    // Thick curved glass laterally displaces the apparent phosphor plane.
    // The scale converts millimetres into the normalized 13-inch faceplate.
    float edgeTaper = max(1.0 - tubeP.x * tubeP.x, 0.0) *
        max(1.0 - tubeP.y * tubeP.y, 0.0);
    opticalP += surfaceNormal.xy * (1.0 - 1.0 / refractiveIndex) *
        max(glassThicknessMm, 0.0) * 0.0009 * edgeTaper;
    vec2 sourceTubeUv = opticalP * 0.5 + 0.5;
    vec2 sourcePx = displayRect.xy + sourceTubeUv * displayRect.zw;
    vec2 sourceUv = sourcePx / max(resolution, vec2(1.0));
    bool insideSource = sourceTubeUv.x >= 0.0 && sourceTubeUv.y >= 0.0 &&
                        sourceTubeUv.x <= 1.0 && sourceTubeUv.y <= 1.0;

    float rounded = max(cornerRadius, 0.001);
    // sdRoundBox already subtracts the corner radius internally. Passing
    // (1-radius) here used to subtract it twice and shrink every straight
    // tube edge by roughly 10.5 percent.
    float tubeDistance = sdRoundBox(tubeP, vec2(1.0), rounded);
    float edgeAa = max(fwidth(tubeDistance) * 1.5, 0.001);
    float tubeMask = 1.0 - smoothstep(-edgeAa, edgeAa, tubeDistance);
    if (!insideSource) tubeMask = 0.0;

    vec2 radialDirection = length(tubeP) > 0.0001
        ? normalize(tubeP) : vec2(0.0);
    vec2 dispersionOffset = radialDirection * max(glassDispersion, 0.0) /
        max(resolution, vec2(1.0));
    vec3 emission = insideSource ? max(vec3(
        texture(texture0, sourceUv + dispersionOffset).r,
        texture(texture0, sourceUv).g,
        texture(texture0, sourceUv - dispersionOffset).b
    ), vec3(0.0)) : vec3(0.0);
    vec3 localBloom = insideSource
        ? max(texture(bloomTexture, sourceUv).rgb, vec3(0.0))
        : vec3(0.0);
    vec3 wideBloom = insideSource
        ? max(texture(wideBloomTexture, sourceUv).rgb, vec3(0.0))
        : vec3(0.0);

    float localScatter = max(bloomIntensity, 0.0);
    float wideScatter = max(wideBloomIntensity, 0.0);
    float halationScatter = max(halation, 0.0);
    float reflectionScatter = max(internalReflection, 0.0) * fresnel;
    vec3 scatteredSeed = brightContribution(emission);
    float removedFraction = clamp(localScatter + wideScatter +
        halationScatter + reflectionScatter, 0.0, 0.95);
    // Bloom, halation and internal return redistribute the bright component;
    // remove the scattered energy from the direct ray before adding its PSFs.
    vec3 color = max(emission - scatteredSeed * removedFraction, vec3(0.0));
    color += localBloom * localScatter;
    color += wideBloom * wideScatter;
    color += luma(wideBloom) * vec3(1.00, 0.43, 0.20) * halationScatter;
    color += wideBloom * reflectionScatter;
    vec4 supply = texture(aplTexture, vec2(0.5));
    float highVoltage = supply.g > 0.25 ? supply.g : 1.0;
    color *= sqrt(max(highVoltage, 0.0));

    float gray = luma(color);
    vec3 beerLambert = exp(-max(glassAbsorption, vec3(0.0)) *
        opticalPathMm);
    color *= max(glassTransmission, 0.0) * max(glassTint, vec3(0.0)) *
        beerLambert * (1.0 - fresnel);
    float glassVignette = 1.0 - max(vignette, 0.0) *
        smoothstep(0.16, 1.52, radius2);
    color *= max(glassVignette, 0.0);

    // The A34JLN60X uses a dark-tint bonded faceplate. Reflections remain
    // visible over black, while emitted light is attenuated by the glass.
    vec2 reflectionP = (tubeP - vec2(-0.30, -0.38)) / vec2(0.88, 0.36);
    float glassHighlight = exp(-dot(reflectionP, reflectionP) * 3.2);
    float ambientRadiance = max(ambientIlluminance, 0.0) / 100.0;
    color += vec3(0.70, 0.80, 0.92) *
        (glassHighlight * max(reflection, 0.0) + ambientRadiance * fresnel) *
        (1.0 - clamp(gray, 0.0, 1.0));

    float framePeriod = 1.0 / max(ntscFrameRateHz, 1.0);
    float frameStart = time - crtFramePhase * framePeriod;
    float rasterLine = ntscContentStartLine +
        clamp(tubeUv.y, 0.0, 1.0) * ntscContentLines;
    float activeTime = 9.40e-6 + clamp(tubeUv.x, 0.0, 1.0) *
        ntscActiveVideoUs * 1.0e-6;
    float rasterTime = frameStart + rasterLine /
        max(ntscLineRateHz, 1.0) + activeTime;
    float supplyRipple = 1.0 - max(flicker, 0.0) *
        sin(2.0 * PI * (120.0 * rasterTime + 0.37));
    color *= supplyRipple;
    float noiseValue = hash12(gl_FragCoord.xy + floor(time * 120.0)) - 0.5;
    color += noiseValue * max(noise, 0.0);
    color += vec3(max(blackLevel, 0.0));

    // tubePeakNits is a front-of-glass photometer measurement. The simulated
    // current, mask, phosphor and faceplate operate in arbitrary internal
    // radiance units, whose mean reference-white response is recorded in
    // referenceWhiteRadiance. Dividing by it calibrates those units once;
    // without this term their losses were counted again after tubePeakNits
    // and a nominal 100-nit white was displayed at only about 64 nits.
    // Relative APL sag, mask modulation, glass angle and vignette remain in
    // color. Values above the host capability clip physically instead of
    // passing through an arbitrary filmic curve.
    float nitScale = max(tubePeakNits, 1.0) /
        max(hostPeakNits, 1.0) * max(brightness, 0.0) /
        max(referenceWhiteRadiance, 0.01);
    vec3 mapped = clamp(max(color, vec3(0.0)) * nitScale,
        vec3(0.0), vec3(1.0));
    mapped = pow(clamp(mapped, vec3(0.0), vec3(1.0)),
        vec3(1.0 / max(outputGamma, 0.1)));
    mapped *= tubeMask;

    finalColor = vec4(
        mapped * colDiffuse.rgb * fragColor.rgb,
        tubeMask * colDiffuse.a * fragColor.a
    );
}
