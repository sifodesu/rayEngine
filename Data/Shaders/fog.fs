#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform float time;
uniform vec2 resolution;
uniform vec2 nativeResolution;
uniform vec4 displayRect;
uniform vec4 fogRect;
uniform vec2 cameraTarget;
uniform vec2 cameraOffset;
uniform float cameraZoom;
uniform vec3 fogColor;
uniform float fogOpacity;
uniform float fogScale;
uniform float fogSpeedX;
uniform float fogSpeedY;
uniform float fogContrast;
uniform float fogSoftness;

out vec4 finalColor;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 rotate2(vec2 p, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c) * p;
}

float ellipseBlob(vec2 p, vec2 center, vec2 radius, float angle) {
    vec2 q = rotate2(p - center, -angle) / max(radius, vec2(0.001));
    return 1.0 - dot(q, q);
}

float tongueBlob(vec2 p, vec2 center, vec2 halfSize, float angle, float bend) {
    vec2 q = rotate2(p - center, -angle);
    q.y += sin(q.x * 3.2 + bend) * halfSize.y * 0.18;

    float along = abs(q.x) / max(halfSize.x, 0.001);
    float side = abs(q.y) / max(halfSize.y, 0.001);
    float taperedSide = side + max(along - 0.48, 0.0) * 0.22;
    return 1.0 - max(along, taperedSide);
}

float edgeCut(vec2 p, vec2 center, float angle, vec2 cell) {
    vec2 q = rotate2(p - center, -angle);
    vec2 notchCell = floor(q * vec2(5.0, 18.0) + cell * vec2(11.7, 3.9));
    return step(0.72, hash12(notchCell));
}

float cloudBlob(vec2 p, vec2 cell) {
    float spawn = hash12(cell + vec2(19.1, 73.7));
    if (spawn < 0.18) {
        return -100.0;
    }

    vec2 center = cell + vec2(0.5);
    center += vec2(hash12(cell + vec2(4.7, 15.2)), hash12(cell + vec2(35.3, 9.8))) * 0.42 - vec2(0.21);

    float angle = (hash12(cell + vec2(61.4, 8.2)) - 0.5) * 0.32;
    float width = mix(1.28, 2.10, hash12(cell + vec2(2.3, 91.4)));
    float height = mix(0.20, 0.34, hash12(cell + vec2(88.8, 6.1)));
    float taper = mix(0.62, 0.92, hash12(cell + vec2(17.8, 41.0)));
    float bend = hash12(cell + vec2(77.2, 54.1)) * 6.28318;

    vec2 left = rotate2(vec2(-width * 0.58, height * 0.05), angle);
    vec2 farLeft = rotate2(vec2(-width * 0.98, -height * 0.06), angle);
    vec2 right = rotate2(vec2(width * 0.58, -height * 0.02), angle);
    vec2 farRight = rotate2(vec2(width * 1.02, height * 0.04), angle);
    vec2 top = rotate2(vec2(width * 0.08, -height * 0.42), angle);

    float blob = tongueBlob(p, center, vec2(width * 0.86, height * 0.82), angle, bend);
    blob = max(blob, tongueBlob(p, center + left, vec2(width * 0.48, height * 0.62), angle + 0.03, bend + 1.7));
    blob = max(blob, tongueBlob(p, center + farLeft, vec2(width * 0.36, height * taper), angle - 0.04, bend + 3.1));
    blob = max(blob, tongueBlob(p, center + right, vec2(width * 0.52, height * 0.66), angle - 0.02, bend + 2.4));
    blob = max(blob, tongueBlob(p, center + farRight, vec2(width * 0.34, height * taper), angle + 0.05, bend + 4.2));
    blob = max(blob, ellipseBlob(p, center + top, vec2(width * 0.32, height * 0.44), angle));

    float cut = edgeCut(p, center, angle, cell);
    float edgeOnly = smoothstep(-0.24, 0.24, blob) * (1.0 - smoothstep(0.34, 0.70, blob));
    blob -= cut * edgeOnly * 0.18;
    return blob;
}

vec2 uvToNativePx(vec2 uv) {
    vec2 screenPx = vec2(uv.x, 1.0 - uv.y) * resolution;
    return ((screenPx - displayRect.xy) / max(displayRect.zw, vec2(1.0))) * nativeResolution;
}

vec2 nativeToWorld(vec2 nativePx) {
    return (nativePx - cameraOffset) / max(cameraZoom, 0.001) + cameraTarget;
}

float rectEdgeMask(vec2 local) {
    if (local.x < 0.0 || local.y < 0.0 || local.x > fogRect.z || local.y > fogRect.w) {
        return 0.0;
    }

    float edgeDist = min(min(local.x, local.y), min(fogRect.z - local.x, fogRect.w - local.y));
    return smoothstep(0.0, max(fogSoftness, 0.001), edgeDist);
}

void main() {
    vec4 base = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec2 nativePx = floor(uvToNativePx(fragTexCoord)) + vec2(0.5);
    vec2 world = nativeToWorld(nativePx);
    vec2 local = world - fogRect.xy;
    float edgeMask = rectEdgeMask(local);

    if (edgeMask <= 0.0 || fogOpacity <= 0.0) {
        finalColor = base;
        return;
    }

    vec2 wind = vec2(fogSpeedX, fogSpeedY) * time;
    vec2 p = (world + wind) * max(fogScale, 0.0001);

    vec2 cell = floor(p);
    float blob = -100.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -2; x <= 2; ++x) {
            blob = max(blob, cloudBlob(p, cell + vec2(float(x), float(y))));
        }
    }

    float threshold = mix(0.10, -0.18, clamp(fogContrast, 0.0, 1.0));
    float opacityLevel = 0.0;
    opacityLevel += step(threshold, blob);
    opacityLevel += step(threshold + 0.30, blob);
    opacityLevel += step(threshold + 0.62, blob);
    opacityLevel /= 3.0;

    float alpha = clamp(opacityLevel * fogOpacity * edgeMask, 0.0, 1.0);
    vec3 color = mix(base.rgb, fogColor, alpha);
    finalColor = vec4(color, base.a);
}
