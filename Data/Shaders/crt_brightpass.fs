#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float bloomThreshold;

out vec4 finalColor;

void main() {
    // This pass renders at quarter resolution. Integrate the phosphor slots
    // before thresholding so one subpixel peak cannot alias into a bloom ray.
    vec2 texel = 1.0 / max(resolution, vec2(1.0));
    vec3 color = vec3(0.0);
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            float weight = (3.0 - abs(float(x))) *
                (3.0 - abs(float(y)));
            color += texture(texture0,
                fragTexCoord + vec2(float(x), float(y)) * texel).rgb * weight;
        }
    }
    color /= 81.0;
    float peak = max(max(color.r, color.g), color.b);
    float threshold = max(bloomThreshold, 0.0);
    float knee = max(threshold * 0.35, 0.001);
    float soft = clamp((peak - threshold + knee) / (2.0 * knee), 0.0, 1.0);
    soft = soft * soft * (3.0 - 2.0 * soft);
    float contribution = max(peak - threshold, 0.0) + soft * knee;
    vec3 bright = color * contribution / max(peak, 0.0001);
    finalColor = vec4(bright, 1.0) * colDiffuse * fragColor;
}
