#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D slowTexture;
uniform vec4 colDiffuse;
uniform float phosphorSlowWeight;

out vec4 finalColor;

void main() {
    vec3 fastState = max(texture(texture0, fragTexCoord).rgb, vec3(0.0));
    vec3 slowState = max(texture(slowTexture, fragTexCoord).rgb, vec3(0.0));
    vec3 radiance = fastState + slowState * max(phosphorSlowWeight, 0.0);
    finalColor = vec4(radiance, 1.0) * colDiffuse * fragColor;
}
