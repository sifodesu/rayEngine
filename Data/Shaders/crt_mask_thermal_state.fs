#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D prevTexture;
uniform vec4 colDiffuse;
uniform float frameTime;
uniform float maskHeating;
uniform float maskThermalTau;
uniform float maskThermalDiffusion;

out vec4 finalColor;

float luma(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    ivec2 stateSize = textureSize(prevTexture, 0);
    vec2 stateTexel = 1.0 / max(vec2(stateSize), vec2(1.0));
    vec2 sourceTexel = 1.0 / max(vec2(textureSize(texture0, 0)), vec2(1.0));

    float localLoad = 0.0;
    float sourceWeight = 0.0;
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            float weight = exp(-0.35 * float(x * x + y * y));
            localLoad += luma(max(texture(texture0, fragTexCoord +
                sourceTexel * vec2(x, y) * 3.0).rgb, vec3(0.0))) * weight;
            sourceWeight += weight;
        }
    }
    localLoad /= max(sourceWeight, 0.0001);

    float center = max(texture(prevTexture, fragTexCoord).r, 0.0);
    float neighbours = (
        texture(prevTexture, fragTexCoord + vec2(stateTexel.x, 0.0)).r +
        texture(prevTexture, fragTexCoord - vec2(stateTexel.x, 0.0)).r +
        texture(prevTexture, fragTexCoord + vec2(0.0, stateTexel.y)).r +
        texture(prevTexture, fragTexCoord - vec2(0.0, stateTexel.y)).r
    ) * 0.25;

    float dt = max(frameTime, 0.0);
    float diffusion = 1.0 - exp(-max(maskThermalDiffusion, 0.0) * dt);
    float diffused = mix(center, neighbours, clamp(diffusion, 0.0, 0.24));
    float thermalResponse = 1.0 - exp(-dt / max(maskThermalTau, 0.01));
    float targetTemperature = max(maskHeating, 0.0) * localLoad;
    float temperature = mix(diffused, targetTemperature, thermalResponse);

    finalColor = vec4(max(temperature, 0.0), localLoad, 0.0, 1.0) *
        colDiffuse * fragColor;
}
