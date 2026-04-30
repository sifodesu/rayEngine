#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D prevTexture;
uniform vec4 colDiffuse;

uniform float frameTime;
uniform vec2 resolution;
uniform vec2 nativeResolution;
uniform vec4 displayRect;
uniform float persistence;
uniform float phosphorTrail;
uniform vec3 phosphorDecay;
uniform float phosphorSpread;

out vec4 finalColor;

float maxChannel(vec3 color) {
    return max(max(color.r, color.g), color.b);
}

vec4 sampleSafe(sampler2D tex, vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) {
        return vec4(0.0);
    }
    return texture(tex, uv);
}

vec3 toLinear(vec3 color) {
    return pow(max(color, vec3(0.0)), vec3(2.2));
}

vec3 toGamma(vec3 color) {
    return pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 nativeStep = max((displayRect.zw / max(nativeResolution, vec2(1.0))) / max(resolution, vec2(1.0)),
                          1.0 / max(resolution, vec2(1.0)));

    vec3 current = toLinear(sampleSafe(texture0, uv).rgb);
    float currentPeak = maxChannel(current);
    float beamDrive = smoothstep(0.012, 0.70, currentPeak);
    vec3 excitation = current * mix(0.46, 1.08, beamDrive);

    vec2 spread = nativeStep * max(phosphorSpread, 0.0);
    vec3 previous = toLinear(sampleSafe(prevTexture, uv).rgb) * 0.50;
    float weightSum = 0.50;

    if (phosphorSpread > 0.001) {
        previous += toLinear(sampleSafe(prevTexture, uv + vec2( spread.x, 0.0)).rgb) * 0.115;
        previous += toLinear(sampleSafe(prevTexture, uv + vec2(-spread.x, 0.0)).rgb) * 0.115;
        previous += toLinear(sampleSafe(prevTexture, uv + vec2(0.0,  spread.y)).rgb) * 0.085;
        previous += toLinear(sampleSafe(prevTexture, uv + vec2(0.0, -spread.y)).rgb) * 0.085;
        previous += toLinear(sampleSafe(prevTexture, uv + vec2( spread.x,  spread.y)).rgb) * 0.045;
        previous += toLinear(sampleSafe(prevTexture, uv + vec2(-spread.x, -spread.y)).rgb) * 0.045;
        previous += toLinear(sampleSafe(prevTexture, uv + vec2( spread.x, -spread.y)).rgb) * 0.045;
        previous += toLinear(sampleSafe(prevTexture, uv + vec2(-spread.x,  spread.y)).rgb) * 0.045;
        weightSum += 0.585;
    }
    previous /= weightSum;

    float dt = clamp(frameTime, 1.0 / 240.0, 1.0 / 24.0);
    vec3 decayTimes = clamp(phosphorDecay, vec3(0.00025), vec3(0.400));
    vec3 decay = pow(vec3(0.1), vec3(dt) / decayTimes);

    float previousPeak = maxChannel(previous);
    float lowLightQuench = mix(0.82, 1.0, smoothstep(0.018, 0.32, previousPeak));
    float historyEnabled = step(0.0001, max(phosphorTrail, persistence));
    vec3 effectiveDecay = decay * lowLightQuench * historyEnabled;
    vec3 phosphor = previous * effectiveDecay + excitation * (vec3(1.0) - effectiveDecay);

    finalColor = vec4(toGamma(clamp(phosphor, vec3(0.0), vec3(1.0))), 1.0) * colDiffuse * fragColor;
}
