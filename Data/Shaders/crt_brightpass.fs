#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float bloomThreshold;

out vec4 finalColor;

void main() {
    vec3 color = texture(texture0, fragTexCoord).rgb;
    float peak = max(max(color.r, color.g), color.b);
    float threshold = max(bloomThreshold, 0.0);
    float knee = max(threshold * 0.35, 0.001);
    float soft = clamp((peak - threshold + knee) / (2.0 * knee), 0.0, 1.0);
    soft = soft * soft * (3.0 - 2.0 * soft);
    float contribution = max(peak - threshold, 0.0) + soft * knee;
    vec3 bright = color * contribution / max(peak, 0.0001);
    finalColor = vec4(bright, 1.0) * colDiffuse * fragColor;
}
