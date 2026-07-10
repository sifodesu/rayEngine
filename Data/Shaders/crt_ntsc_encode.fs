#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D prevTexture;
uniform vec4 colDiffuse;
uniform float time;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform float ntscSubcarrierMHz;
uniform float ntscFrameRateHz;
uniform float ntscActiveVideoUs;
uniform float ntscNoise;
uniform float ntscHum;
uniform float videoHistoryValid;

out vec4 finalColor;

const float PI = 3.14159265358979323846;

vec3 rgbToYiq(vec3 rgb) {
    return vec3(
        dot(rgb, vec3(0.299000,  0.587000,  0.114000)),
        dot(rgb, vec3(0.595716, -0.274453, -0.321263)),
        dot(rgb, vec3(0.211456, -0.522591,  0.311135))
    );
}

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec2 screenPx = fragTexCoord * resolution;
    vec2 tubeUv = (screenPx - displayRect.xy) /
        max(displayRect.zw, vec2(1.0));
    bool inside = tubeUv.x >= 0.0 && tubeUv.y >= 0.0 &&
                  tubeUv.x <= 1.0 && tubeUv.y <= 1.0;
    if (!inside) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Composite video is formed from gamma-coded R'G'B', before the tube's
    // electro-optical transfer function. 240p uses one 262.5-line field as a
    // frame; the half-line relationship creates the four-field NTSC sequence.
    vec3 currentVideo = texture(texture0, fragTexCoord).rgb;
    vec3 previousVideo = texture(prevTexture, fragTexCoord).rgb;
    vec3 encodedVideo = mix(currentVideo, previousVideo,
        clamp(videoHistoryValid, 0.0, 1.0));
    vec3 yiq = rgbToYiq(clamp(encodedVideo, 0.0, 1.0));
    float activeLine = floor(tubeUv.y * 240.0) + 21.0;
    float field = floor(time * ntscFrameRateHz);
    float xTimeUs = tubeUv.x * ntscActiveVideoUs;
    float phase = 2.0 * PI * ntscSubcarrierMHz * xTimeUs;
    phase += PI * activeLine + 0.5 * PI * mod(field, 4.0);

    float composite = yiq.x + yiq.y * cos(phase) + yiq.z * sin(phase);
    float mainsHum = sin(2.0 * PI * 60.0 * time + tubeUv.y * PI) * ntscHum;
    float randomNoise = (hash12(gl_FragCoord.xy + floor(time * 15734.264)) - 0.5) *
        2.0 * ntscNoise;
    composite += mainsHum + randomNoise;

    // Biasing keeps the real bipolar waveform intact if the half-float target
    // has to fall back to an unsigned render texture.
    float storedComposite = clamp((composite + 0.75) / 2.50, 0.0, 1.0);
    finalColor = vec4(storedComposite, storedComposite, storedComposite, 1.0) *
        vec4(colDiffuse.rgb * fragColor.rgb, colDiffuse.a * fragColor.a);
}
