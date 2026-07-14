#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform float ntscActiveVideoUs;
uniform float testPattern;

out vec4 finalColor;

const float PI = 3.14159265358979323846;

vec3 colorBars(vec2 uv) {
    const vec3 bars[8] = vec3[8](
        vec3(0.75),
        vec3(0.75, 0.75, 0.0),
        vec3(0.0, 0.75, 0.75),
        vec3(0.0, 0.75, 0.0),
        vec3(0.75, 0.0, 0.75),
        vec3(0.75, 0.0, 0.0),
        vec3(0.0, 0.0, 0.75),
        vec3(0.0)
    );
    int index = clamp(int(floor(uv.x * 8.0)), 0, 7);
    if (uv.y < 0.72) return bars[index];
    if (uv.y < 0.86) {
        if (uv.x < 0.25) return vec3(0.035);
        if (uv.x < 0.50) return vec3(0.075);
        if (uv.x < 0.75) return vec3(0.115);
        return vec3(0.0);
    }
    return mod(floor(uv.x * 32.0), 2.0) < 1.0 ? vec3(1.0) : vec3(0.0);
}

vec3 convergenceGrid(vec2 uv) {
    vec2 grid = abs(fract(uv * vec2(16.0, 12.0)) - 0.5);
    float line = 1.0 - smoothstep(0.015, 0.035, min(grid.x, grid.y));
    vec2 center = abs(uv - 0.5);
    float cross = 1.0 - smoothstep(0.002, 0.006, min(center.x, center.y));
    return vec3(max(line * 0.70, cross));
}

vec3 multiburst(vec2 uv) {
    int band = clamp(int(floor(uv.y * 6.0)), 0, 5);
    const float frequencies[6] = float[6](0.5, 1.0, 1.5, 2.0, 3.0, 4.0);
    float xTimeUs = uv.x * ntscActiveVideoUs;
    float wave = 0.5 + 0.45 * sin(2.0 * PI * frequencies[band] * xTimeUs);
    return vec3(wave);
}

vec3 zonePlate(vec2 uv) {
    vec2 centered = uv * 2.0 - 1.0;
    // The horizontal edge reaches about 4 MHz over 52.655 us. The vertical
    // chirp reveals scan-line/spot interaction and mask aliasing independently.
    float phase = PI * (105.0 * centered.x * centered.x +
        60.0 * centered.y * centered.y);
    return vec3(0.5 + 0.45 * cos(phase));
}

vec3 grayscaleRamp(vec2 uv) {
    if (uv.y < 0.60) return vec3(uv.x);
    if (uv.y < 0.82) return vec3(floor(uv.x * 16.0) / 15.0);
    if (uv.x < 0.20) return vec3(0.0);
    if (uv.x < 0.40) return vec3(0.035);
    if (uv.x < 0.60) return vec3(0.075);
    if (uv.x < 0.80) return vec3(0.115);
    return vec3(1.0);
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

    // RenderTexture coordinates are vertically inverted when sampled by the
    // following waveform pass. Keep diagnostic conventions (PLUGE at bottom,
    // multiburst low-to-high from top to bottom) in visible screen space.
    vec2 patternUv = vec2(tubeUv.x, 1.0 - tubeUv.y);
    vec3 color = texture(texture0, fragTexCoord).rgb;
    if (testPattern >= 0.5 && testPattern < 1.5) {
        color = colorBars(patternUv);
    } else if (testPattern < 2.5 && testPattern >= 1.5) {
        color = convergenceGrid(patternUv);
    } else if (testPattern < 3.5 && testPattern >= 2.5) {
        color = multiburst(patternUv);
    } else if (testPattern < 4.5 && testPattern >= 3.5) {
        color = vec3(step(0.5, patternUv.x));
    } else if (testPattern < 5.5 && testPattern >= 4.5) {
        color = zonePlate(patternUv);
    } else if (testPattern >= 5.5) {
        color = grayscaleRamp(patternUv);
    }
    finalColor = vec4(color, 1.0) * colDiffuse * fragColor;
}
