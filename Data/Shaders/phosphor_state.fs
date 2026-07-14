#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D prevTexture;
uniform sampler2D previousEmissionTexture;
uniform vec4 colDiffuse;
uniform float time;
uniform float frameTime;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform vec3 stateDecay;
uniform float phosphorSpread;
uniform float ntscLineRateHz;
uniform float ntscFrameRateHz;
uniform float ntscActiveLines;
uniform float ntscActiveStartLine;
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
    // Lateral light/electron spread occurs when energy is deposited; stored
    // phosphor energy never diffuses to neighbouring dots after the fact.
    float spread = clamp(max(phosphorSpread, 0.0) * 0.15, 0.0, 0.35);
    return mix(center, neighbours, spread);
}

// Sum an arbitrary consecutive range of beam strikes without a frame-count
// loop. Each strike injects excitation*T/tau and decays exponentially.
void accumulateStrikes(vec3 excitation, float firstFrame, float lastFrame,
        float strikeOffset, float endTime, float framePeriod, vec3 tau,
        out vec3 endpoint) {
    endpoint = vec3(0.0);
    if (lastFrame < firstFrame) return;

    float count = floor(lastFrame - firstFrame + 1.5);
    float newestStrike = lastFrame * framePeriod + strikeOffset;
    vec3 newestDecay = exp(-vec3(max(endTime - newestStrike, 0.0)) / tau);
    vec3 perFrameDecay = exp(-vec3(framePeriod) / tau);
    vec3 geometric = (vec3(1.0) - pow(perFrameDecay, vec3(count))) /
        max(vec3(1.0) - perFrameDecay, vec3(1.0e-8));
    endpoint = excitation * vec3(framePeriod) / tau *
        newestDecay * geometric;
}

void main() {
    vec2 screenPx = fragTexCoord * resolution;
    vec2 tubeUv = (screenPx - displayRect.xy) /
        max(displayRect.zw, vec2(1.0));
    bool inside = tubeUv.x >= 0.0 && tubeUv.y >= 0.0 &&
                  tubeUv.x <= 1.0 && tubeUv.y <= 1.0;
    float dt = max(frameTime, 0.000001);
    vec3 tau = clamp(stateDecay, vec3(0.00025), vec3(0.250));
    vec3 excitation = inside
        ? spreadExcitation(texture0, fragTexCoord)
        : vec3(0.0);
    vec3 previousExcitation = inside
        ? spreadExcitation(previousEmissionTexture, fragTexCoord)
        : vec3(0.0);
    float framePeriod = 1.0 / max(ntscFrameRateHz, 1.0);
    float linePeriod = 1.0 / max(ntscLineRateHz, 1.0);
    float rasterLine = ntscContentStartLine + tubeUv.y * ntscContentLines;
    float activeTime = 9.40e-6 + tubeUv.x * ntscActiveVideoUs * 1.0e-6;
    float strikeOffset = rasterLine * linePeriod + activeTime;

    // Work in a synthetic frame whose current vertical interval is frame 0.
    // This avoids ever-growing float time while preserving every strike in an
    // arbitrarily long (manager-capped one-hour) host interval.
    float endTime = crtFramePhase * framePeriod;
    float startTime = endTime - dt;
    float firstStrike = floor((startTime - strikeOffset) / framePeriod) + 1.0;
    float lastStrike = floor((endTime - strikeOffset) / framePeriod);

    // When frames were skipped only the previous and latest complete source
    // images exist.  The pending image becomes authoritative at the first
    // crossed vertical boundary, matching the manager's latch policy.
    float firstCurrentFrame = 1.0 - max(crtFramesAdvanced, 1.0);
    float lastPreviousFrame = min(lastStrike, firstCurrentFrame - 1.0);
    float firstNewFrame = max(firstStrike, firstCurrentFrame);

    vec3 oldEndpoint;
    vec3 newEndpoint;
    accumulateStrikes(previousExcitation, firstStrike, lastPreviousFrame,
        strikeOffset, endTime, framePeriod, tau, oldEndpoint);
    accumulateStrikes(excitation, firstNewFrame, lastStrike,
        strikeOffset, endTime, framePeriod, tau, newEndpoint);

    vec3 initial = max(texture(prevTexture, fragTexCoord).rgb, vec3(0.0));
    vec3 intervalDecay = exp(-vec3(dt) / tau);
    vec3 endpoint = initial * intervalDecay + oldEndpoint + newEndpoint;
    // Only the physical endpoint is persistent. The synchronized exposure is
    // reconstructed once in crt_phosphor_combine.fs, avoiding six duplicate
    // 4K render targets without changing the temporal solution.
    finalColor = vec4(max(endpoint, vec3(0.0)), 1.0) *
        colDiffuse * fragColor;
}
