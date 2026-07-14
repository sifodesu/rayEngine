#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D prevTexture;
uniform vec4 colDiffuse;
uniform float frameTime;
uniform float observerIntegration;

out vec4 finalColor;

void main() {
    vec3 instantaneous = max(texture(texture0, fragTexCoord).rgb, vec3(0.0));
    vec3 previousExposure = max(texture(prevTexture, fragTexCoord).rgb, vec3(0.0));
    float dt = max(frameTime, 0.0);
    float exposureTime = max(observerIntegration, 0.00025);
    float response = 1.0 - exp(-dt / exposureTime);
    vec3 observedRadiance = mix(previousExposure, instantaneous, response);
    finalColor = vec4(observedRadiance, 1.0) * colDiffuse * fragColor;
}
