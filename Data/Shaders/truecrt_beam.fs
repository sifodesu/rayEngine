#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 resolution;
uniform vec2 nativeResolution;
uniform vec4 displayRect;
uniform float beamWidthX;
uniform float beamWidthY;
uniform float beamFocus;
uniform float beamBloom;
uniform float beamScanlineStrength;
uniform float edgeDefocus;
uniform vec3 convergenceR;
uniform vec3 convergenceG;
uniform vec3 convergenceB;

out vec4 finalColor;

float lumaOf(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec4 sampleSafe(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return vec4(0.0);
    return texture(texture0, uv);
}

float gaussian(float x, float sigma) {
    return exp(-(x * x) / max(2.0 * sigma * sigma, 0.00001));
}

float apertureScanline(vec2 uv, vec3 color) {
    vec2 px = uv * resolution;
    float pitch = displayRect.w / max(nativeResolution.y, 1.0);
    float nativeY = (px.y - displayRect.y) / max(pitch, 1.0);
    float phase = fract(nativeY);
    float dist = abs(phase - 0.5) * 2.0;
    float bright = clamp(lumaOf(color), 0.0, 1.0);
    float beam = mix(0.34, 0.88, pow(bright, 0.45));
    float line = exp(-(dist * dist) / max(beam * beam, 0.0001));
    return mix(1.0, line, clamp(beamScanlineStrength, 0.0, 1.0));
}

vec3 sampleConverged(vec2 uv) {
    vec2 px = 1.0 / max(resolution, vec2(1.0));
    return vec3(
        sampleSafe(uv + convergenceR.xy * px).r,
        sampleSafe(uv + convergenceG.xy * px).g,
        sampleSafe(uv + convergenceB.xy * px).b
    );
}

void main() {
    vec2 uv = fragTexCoord;
    vec3 center = sampleConverged(uv);
    float alpha = sampleSafe(uv).a;
    float neutral = step(max(beamWidthX, beamWidthY), 0.001) * step(beamBloom, 0.001) * step(beamScanlineStrength, 0.001);
    if (neutral > 0.5) {
        finalColor = vec4(center, alpha) * colDiffuse * fragColor;
        return;
    }

    vec2 centered = uv * 2.0 - 1.0;
    float edge = smoothstep(0.25, 1.35, dot(centered, centered)) * edgeDefocus;
    float brightness = lumaOf(center);
    float bloomWidth = beamBloom * smoothstep(0.08, 1.3, brightness);
    float sx = max(beamWidthX * (1.0 + bloomWidth + edge) / max(beamFocus, 0.05), 0.001);
    float sy = max(beamWidthY * (1.0 + bloomWidth * 0.7 + edge) / max(beamFocus, 0.05), 0.001);

    vec2 px = 1.0 / max(resolution, vec2(1.0));
    vec3 sum = vec3(0.0);
    float weight = 0.0;
    for (int y = -3; y <= 3; ++y) {
        for (int x = -3; x <= 3; ++x) {
            vec2 o = vec2(float(x), float(y));
            float w = gaussian(o.x, sx) * gaussian(o.y, sy);
            sum += sampleConverged(uv + o * px) * w;
            weight += w;
        }
    }

    vec3 color = sum / max(weight, 0.0001);
    color *= apertureScanline(uv, color);
    color += center * brightness * beamBloom * 0.18;
    finalColor = vec4(max(color, vec3(0.0)), alpha) * colDiffuse * fragColor;
}
