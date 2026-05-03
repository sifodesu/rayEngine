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
uniform float stillReflectionShiftAmplitude;
uniform float stillRippleSlowScale;
uniform float stillRippleFastScale;
uniform float stillRippleSlowSpeed;
uniform float stillRippleFastSpeed;
uniform float stillRippleSlowWeight;
uniform float stillRippleFastWeight;
uniform float stillReflectionLineOffset;
uniform float waterfallShiftAmplitude;
uniform float waterfallSegmentHeight;
uniform float waterfallFlowSpeed;
uniform float waterfallLineSpacing;
uniform float waterfallLineWidth;
uniform float waterfallLineLength;
uniform float waterfallLinePeriod;
uniform float waterfallLineIntensity;
uniform float waterfallLineRandomSeed;

out vec4 finalColor;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec4 sampleSafe(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) {
        return vec4(0.0);
    }
    return texture(texture0, uv);
}

bool uvInside(vec2 uv) {
    return uv.x >= 0.0 && uv.y >= 0.0 && uv.x <= 1.0 && uv.y <= 1.0;
}

vec2 uvToNativePx(vec2 uv) {
    vec2 screenPx = vec2(uv.x, 1.0 - uv.y) * resolution;
    return ((screenPx - displayRect.xy) / max(displayRect.zw, vec2(1.0))) * nativeResolution;
}

vec2 nativePixelCenter(vec2 nativePx) {
    return floor(nativePx) + vec2(0.5);
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
    vec2 snappedNativePx = nativePixelCenter(nativePx);
    vec2 screenPx = displayRect.xy + (snappedNativePx / max(nativeResolution, vec2(1.0))) * displayRect.zw;
    vec2 screenUv = screenPx / max(resolution, vec2(1.0));
    return vec2(screenUv.x, 1.0 - screenUv.y);
}

vec2 worldToUv(vec2 world) {
    return nativePxToUv(worldToNativePx(world));
}

bool insideWaterRect(vec2 local) {
    return local.x >= 0.0 && local.y >= 0.0 && local.x < waterRect.z && local.y < waterRect.w;
}

float snapPixel(float value) {
    return sign(value) * floor(abs(value) + 0.5);
}

float verticalPixelLineShift(vec2 local, float amplitude, float segmentHeight, float speed) {
    float line = floor(local.x);
    float segmentSize = max(segmentHeight, 1.0);
    float flow = local.y - time * abs(speed);
    float segment = floor(flow / segmentSize);
    float segmentPhase = fract(flow / segmentSize);
    float a = hash12(vec2(line, segment));
    float b = hash12(vec2(line, segment + 1.0));
    float n = mix(a, b, smoothstep(0.0, 1.0, segmentPhase));
    return snapPixel((n * 2.0 - 1.0) * amplitude);
}

float horizontalReflectionLineShift(vec2 local) {
    float line = floor(local.y);
    float slow = sin(line * stillRippleSlowScale + time * stillRippleSlowSpeed);
    float fast = sin(line * stillRippleFastScale + time * stillRippleFastSpeed);
    return snapPixel((slow * stillRippleSlowWeight + fast * stillRippleFastWeight) * stillReflectionShiftAmplitude);
}

vec3 stillWater(vec2 uv, vec2 world, vec2 local) {
    float reflectedLine = floor(local.y) + stillReflectionLineOffset;
    float shift = horizontalReflectionLineShift(local);

    vec2 reflectedWorld = vec2(world.x - shift, waterRect.y - reflectedLine);
    vec2 reflectedUv = worldToUv(reflectedWorld);

    if (!uvInside(reflectedUv) || reflectedWorld.y >= waterRect.y) {
        return texture(texture0, uv).rgb;
    }

    return texture(texture0, reflectedUv).rgb;
}

float fallingWhiteLines(vec2 local) {
    float spacing = max(waterfallLineSpacing, 1.0);
    float width = max(waterfallLineWidth, 0.5);
    float period = max(waterfallLinePeriod, 1.0);
    float length = clamp(waterfallLineLength, 1.0, period);
    float lane = floor(local.x / spacing);
    float laneRandom = hash12(vec2(lane, waterfallLineRandomSeed));
    float laneX = fract(local.x / spacing) * spacing;
    float lineCenter = spacing * 0.5 + (laneRandom - 0.5) * max(spacing - width, 0.0) * 0.35;
    float xMask = 1.0 - smoothstep(width * 0.5, width * 0.5 + 0.5, abs(laneX - lineCenter));

    float dashY = fract((local.y - time * abs(waterfallFlowSpeed) + laneRandom * period) / period) * period;
    float yMask = smoothstep(0.0, 1.0, dashY) * (1.0 - smoothstep(max(length - 1.0, 0.0), length, dashY));
    return clamp(xMask * yMask * mix(0.55, 1.0, laneRandom) * waterfallLineIntensity, 0.0, 1.0);
}

vec3 waterfall(vec2 uv, vec2 local) {
    float shift = verticalPixelLineShift(local, waterfallShiftAmplitude, waterfallSegmentHeight, waterfallFlowSpeed);
    vec3 refracted = sampleSafe(uv + nativeDeltaToUv(vec2(shift, 0.0))).rgb;
    float lines = fallingWhiteLines(local);
    return mix(refracted, vec3(1.0), lines);
}

void main() {
    vec2 rawUv = fragTexCoord;
    vec2 nativePx = nativePixelCenter(uvToNativePx(rawUv));
    vec2 uv = nativePxToUv(nativePx);
    vec2 world = nativeToWorld(nativePx);
    vec2 local = world - waterRect.xy;

    if (!insideWaterRect(local)) {
        finalColor = texture(texture0, rawUv) * colDiffuse * fragColor;
        return;
    }

    vec3 color = waterKind > 0.5 ? waterfall(uv, local) : stillWater(uv, world, local);

    finalColor = vec4(clamp(color, 0.0, 1.0), 1.0) * colDiffuse * fragColor;
}
