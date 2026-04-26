#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D upsampleTexture;
uniform vec4 colDiffuse;
uniform float bloomIntensity;
uniform float bloomPower;

out vec4 finalColor;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 colorPow(vec3 color, float powerValue) {
    float actualLuma = max(luma(color), 0.0001);
    vec3 actualColor = color / actualLuma;
    return actualColor * pow(actualLuma, max(powerValue, 0.0001));
}

void main() {
    vec4 preBloom = texture(texture0, fragTexCoord);
    vec4 blurred = texture(upsampleTexture, fragTexCoord);
    vec3 color = preBloom.rgb + colorPow(blurred.rgb, bloomPower) * bloomIntensity;
    finalColor = vec4(clamp(color, 0.0, 1.0), preBloom.a) * colDiffuse * fragColor;
}
