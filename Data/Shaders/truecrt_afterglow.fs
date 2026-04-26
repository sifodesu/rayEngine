#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D prevPhosphorTexture;
uniform vec4 colDiffuse;

uniform float deltaTime;
uniform vec3 afterglowRgb;
uniform float afterglowThreshold;

out vec4 finalColor;

void main() {
    vec4 current = texture(texture0, fragTexCoord);
    vec3 previous = texture(prevPhosphorTexture, fragTexCoord).rgb;
    vec3 decay = vec3(0.0);
    decay.r = afterglowRgb.r > 0.0001 ? exp(-deltaTime / afterglowRgb.r) : 0.0;
    decay.g = afterglowRgb.g > 0.0001 ? exp(-deltaTime / afterglowRgb.g) : 0.0;
    decay.b = afterglowRgb.b > 0.0001 ? exp(-deltaTime / afterglowRgb.b) : 0.0;
    vec3 tail = max(previous - vec3(afterglowThreshold * deltaTime), vec3(0.0)) * decay;
    vec3 saturationBrake = 1.0 - clamp(current.rgb, vec3(0.0), vec3(1.0)) * 0.35;
    vec3 color = current.rgb + tail * saturationBrake;
    finalColor = vec4(clamp(color, vec3(0.0), vec3(8.0)), current.a) * colDiffuse * fragColor;
}
