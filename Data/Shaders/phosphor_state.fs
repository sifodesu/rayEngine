#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D prevTexture;
uniform vec4 colDiffuse;
uniform float time;
uniform float frameTime;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform float ntscFrameRateHz;
uniform vec3 stateDecay;
uniform float phosphorSpread;

out vec4 finalColor;

vec3 previousState(vec2 uv) {
    vec2 spread = vec2(max(phosphorSpread, 0.0)) /
        max(resolution, vec2(1.0));
    vec3 state = texture(prevTexture, uv).rgb * 0.62;
    state += texture(prevTexture, uv + vec2( spread.x, 0.0)).rgb * 0.08;
    state += texture(prevTexture, uv + vec2(-spread.x, 0.0)).rgb * 0.08;
    state += texture(prevTexture, uv + vec2(0.0,  spread.y)).rgb * 0.06;
    state += texture(prevTexture, uv + vec2(0.0, -spread.y)).rgb * 0.06;
    state += texture(prevTexture, uv + vec2( spread.x,  spread.y)).rgb * 0.025;
    state += texture(prevTexture, uv + vec2(-spread.x, -spread.y)).rgb * 0.025;
    state += texture(prevTexture, uv + vec2( spread.x, -spread.y)).rgb * 0.025;
    state += texture(prevTexture, uv + vec2(-spread.x,  spread.y)).rgb * 0.025;
    return state;
}

bool rasterCrossed(float previousPhase, float currentPhase, float linePhase,
                   float sweptFraction) {
    if (sweptFraction >= 1.0) return true;
    if (currentPhase >= previousPhase) {
        return linePhase > previousPhase && linePhase <= currentPhase;
    }
    return linePhase > previousPhase || linePhase <= currentPhase;
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 screenPx = uv * resolution;
    vec2 tubeUv = (screenPx - displayRect.xy) /
        max(displayRect.zw, vec2(1.0));
    bool inside = tubeUv.x >= 0.0 && tubeUv.y >= 0.0 &&
                  tubeUv.x <= 1.0 && tubeUv.y <= 1.0;

    float dt = clamp(frameTime, 1.0 / 240.0, 1.0 / 24.0);
    vec3 tau = clamp(stateDecay, vec3(0.00025), vec3(0.250));
    vec3 state = previousState(uv) * exp(-vec3(dt) / tau);

    if (inside) {
        // 192 active picture lines occupy the active part of a 262.5-line 240p
        // field. During vertical blanking no phosphor is being excited.
        float activeStart = 21.0 / 262.5;
        float activeSpan = 240.0 / 262.5;
        float linePhase = activeStart + tubeUv.y * activeSpan;
        float currentPhase = fract(time * ntscFrameRateHz);
        float previousPhase = fract((time - dt) * ntscFrameRateHz);
        float swept = dt * ntscFrameRateHz;

        if (rasterCrossed(previousPhase, currentPhase, linePhase, swept)) {
            float age = mod(currentPhase - linePhase, 1.0) /
                max(ntscFrameRateHz, 1.0);
            vec3 excitation = max(texture(texture0, uv).rgb, vec3(0.0));
            state += excitation * exp(-vec3(age) / tau);
        }
    }

    finalColor = vec4(max(state, vec3(0.0)), 1.0) * colDiffuse * fragColor;
}
