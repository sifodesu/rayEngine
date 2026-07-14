#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D slowTexture;
uniform sampler2D mediumTexture;
uniform sampler2D currentEmissionTexture;
uniform sampler2D previousEmissionTexture;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform vec3 phosphorFastDecay;
uniform vec3 phosphorMediumDecay;
uniform vec3 phosphorSlowDecay;
uniform float phosphorMediumWeight;
uniform float phosphorSlowWeight;
uniform float phosphorSpread;
uniform float ntscLineRateHz;
uniform float ntscFrameRateHz;
uniform float ntscContentLines;
uniform float ntscContentStartLine;
uniform float ntscActiveVideoUs;
uniform float crtFramePhase;
uniform float crtFramesAdvanced;

out vec4 finalColor;

vec3 spreadExcitation(sampler2D image, vec2 uv) {
    vec2 texel = 1.0 / max(resolution, vec2(1.0));
    vec3 center = max(texture(image, uv).rgb, vec3(0.0));
    vec3 neighbours = (
        max(texture(image, uv + vec2( texel.x, 0.0)).rgb, vec3(0.0)) +
        max(texture(image, uv + vec2(-texel.x, 0.0)).rgb, vec3(0.0)) +
        max(texture(image, uv + vec2(0.0,  texel.y)).rgb, vec3(0.0)) +
        max(texture(image, uv + vec2(0.0, -texel.y)).rgb, vec3(0.0))
    ) * 0.25;
    float spread = clamp(max(phosphorSpread, 0.0) * 0.15, 0.0, 0.35);
    return mix(center, neighbours, spread);
}

vec3 synchronizedExposure(vec3 endpoint, vec3 excitation, vec3 tau,
        float framePeriod, float postStrikeTime) {
    vec3 cycleIntegral = endpoint * tau *
        (vec3(1.0) - exp(-vec3(framePeriod) / tau));
    cycleIntegral += excitation * framePeriod *
        (vec3(1.0) - exp(-vec3(postStrikeTime) / tau));
    return cycleIntegral / framePeriod;
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

    vec3 currentExcitation = spreadExcitation(
        currentEmissionTexture, fragTexCoord);
    vec3 previousExcitation = spreadExcitation(
        previousEmissionTexture, fragTexCoord);
    float framePeriod = 1.0 / max(ntscFrameRateHz, 1.0);
    float linePeriod = 1.0 / max(ntscLineRateHz, 1.0);
    float rasterLine = ntscContentStartLine + tubeUv.y * ntscContentLines;
    float activeTime = 9.40e-6 + tubeUv.x * ntscActiveVideoUs * 1.0e-6;
    float strikeOffset = rasterLine * linePeriod + activeTime;
    float endTime = crtFramePhase * framePeriod;
    float strikePhase = strikeOffset / framePeriod;
    float scanMix = crtFramesAdvanced > 1.5
        ? 1.0 : step(strikePhase, crtFramePhase);
    vec3 cycleExcitation = mix(
        previousExcitation, currentExcitation, scanMix);
    float timeToStrike = strikeOffset - endTime;
    if (timeToStrike <= 0.0000001) timeToStrike += framePeriod;
    float postStrikeTime = max(framePeriod - timeToStrike, 0.0);

    vec3 fast = synchronizedExposure(
        max(texture(texture0, fragTexCoord).rgb, vec3(0.0)),
        cycleExcitation,
        clamp(phosphorFastDecay, vec3(0.00025), vec3(0.250)),
        framePeriod,
        postStrikeTime
    );
    vec3 medium = synchronizedExposure(
        max(texture(mediumTexture, fragTexCoord).rgb, vec3(0.0)),
        cycleExcitation,
        clamp(phosphorMediumDecay, vec3(0.00025), vec3(0.250)),
        framePeriod,
        postStrikeTime
    );
    vec3 slow = synchronizedExposure(
        max(texture(slowTexture, fragTexCoord).rgb, vec3(0.0)),
        cycleExcitation,
        clamp(phosphorSlowDecay, vec3(0.00025), vec3(0.250)),
        framePeriod,
        postStrikeTime
    );

    float mediumWeight = max(phosphorMediumWeight, 0.0);
    float slowWeight = max(phosphorSlowWeight, 0.0);
    float totalTail = mediumWeight + slowWeight;
    if (totalTail > 1.0) {
        mediumWeight /= totalTail;
        slowWeight /= totalTail;
        totalTail = 1.0;
    }
    float fastWeight = 1.0 - totalTail;
    vec3 radiance = fast * fastWeight + medium * mediumWeight +
        slow * slowWeight;
    finalColor = vec4(max(radiance, vec3(0.0)), 1.0) *
        colDiffuse * fragColor;
}
