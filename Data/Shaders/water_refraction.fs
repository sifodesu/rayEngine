#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform float time;
uniform vec2 resolution;
uniform vec2 nativeResolution;
uniform vec4 displayRect;
uniform vec4 waterRect;
uniform vec2 cameraTarget;
uniform vec2 cameraOffset;
uniform float cameraZoom;
uniform float waterKind;

out vec4 finalColor;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

vec4 sampleSafe(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) {
        return vec4(0.0);
    }
    return texture(texture0, uv);
}

vec2 uvToNativePx(vec2 uv) {
    vec2 screenPx = vec2(uv.x, 1.0 - uv.y) * resolution;
    return ((screenPx - displayRect.xy) / max(displayRect.zw, vec2(1.0))) * nativeResolution;
}

vec2 nativeDeltaToUv(vec2 deltaPx) {
    vec2 screenDelta = (deltaPx / max(nativeResolution, vec2(1.0))) * displayRect.zw;
    return vec2(screenDelta.x / max(resolution.x, 1.0), -screenDelta.y / max(resolution.y, 1.0));
}

vec2 nativeToWorld(vec2 nativePx) {
    return (nativePx - cameraOffset) / max(cameraZoom, 0.001) + cameraTarget;
}

vec2 worldToNativePx(vec2 world) {
    return (world - cameraTarget) * cameraZoom + cameraOffset;
}

vec2 nativePxToUv(vec2 nativePx) {
    vec2 screenPx = displayRect.xy + (nativePx / max(nativeResolution, vec2(1.0))) * displayRect.zw;
    vec2 screenUv = screenPx / max(resolution, vec2(1.0));
    return vec2(screenUv.x, 1.0 - screenUv.y);
}

vec2 worldToUv(vec2 world) {
    return nativePxToUv(worldToNativePx(world));
}

float snapPixel(float value) {
    return sign(value) * floor(abs(value) + 0.5);
}

float verticalPixelLineShift(vec2 local, float amplitude, float segmentHeight, float speed) {
    float line = floor(local.x);
    float flow = local.y + time * speed;
    float segment = floor(flow / max(segmentHeight, 1.0));
    float segmentPhase = fract(flow / max(segmentHeight, 1.0));
    float a = hash12(vec2(line, segment));
    float b = hash12(vec2(line, segment + 1.0));
    float n = mix(a, b, smoothstep(0.0, 1.0, segmentPhase));
    float bend = sin(flow * 0.21 + a * 6.2831853) * 0.55;
    return snapPixel((n * 2.0 - 1.0) * amplitude + bend);
}

float horizontalReflectionLineShift(vec2 local, float amplitude) {
    float line = floor(local.y);
    float slow = sin(line * 0.43 + time * 2.1);
    float fast = sin(line * 0.91 - time * 4.4);
    return snapPixel((slow * 0.70 + fast * 0.30) * amplitude);
}

vec3 stillWater(vec2 uv, vec2 world, vec2 local) {
    float reflectedLine = floor(local.y) + 1.0;
    float shift = horizontalReflectionLineShift(local, 4.0);

    vec2 reflectedWorld = vec2(world.x - shift, waterRect.y - reflectedLine);
    return sampleSafe(worldToUv(reflectedWorld)).rgb;
}

vec3 waterfall(vec2 uv, vec2 world, vec2 local) {
    float shift = verticalPixelLineShift(local, 4.0, 7.0, -86.0);
    vec3 refracted = sampleSafe(uv + nativeDeltaToUv(vec2(shift, 0.0))).rgb;

    float height = max(waterRect.w, 1.0);
    float bottom = smoothstep(0.68, 1.0, clamp(local.y / height, 0.0, 1.0));
    float lane = floor(local.x / 3.0);
    float laneRandom = hash12(vec2(lane, 13.7));
    float laneCenter = fract(local.x / 3.0) - 0.5;
    float laneMask = smoothstep(0.28, 0.04, abs(laneCenter));
    float dashPeriod = 11.0 + laneRandom * 9.0;
    float dashPhase = fract((local.y - time * (78.0 + laneRandom * 38.0) + laneRandom * dashPeriod) / dashPeriod);
    float fallingDash = laneMask * smoothstep(0.02, 0.10, dashPhase) * (1.0 - smoothstep(0.28, 0.52, dashPhase));
    float thread = smoothstep(0.70, 0.96, hash12(vec2(floor(local.x), floor(local.y * 0.18 - time * 12.0))));
    float foam = thread * 0.10 + fallingDash * 0.62 + bottom * 0.22;

    vec3 color = refracted * vec3(0.70, 0.92, 1.08) + vec3(0.0, 0.030, 0.055);
    color += foam * vec3(0.17, 0.25, 0.29);
    return color;
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 nativePx = uvToNativePx(uv);
    vec2 world = nativeToWorld(nativePx);
    vec2 local = world - waterRect.xy;

    vec3 color = waterKind > 0.5 ? waterfall(uv, world, local) : stillWater(uv, world, local);

    finalColor = vec4(clamp(color, 0.0, 1.0), 1.0) * colDiffuse * fragColor;
}
