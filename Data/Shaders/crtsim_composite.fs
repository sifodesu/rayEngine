#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D prevCompositeTexture;
uniform sampler2D ntscArtifactTexture;
uniform vec4 colDiffuse;

uniform vec2 resolution;
uniform vec2 nativeResolution;
uniform vec4 displayRect;
uniform float sharpness;
uniform vec3 persistenceRgb;
uniform float bleed;
uniform float ntscArtifacts;
uniform float ntscLerp;

out vec4 finalColor;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec4 sampleCurrent(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return vec4(0.0);
    return texture(texture0, uv);
}

vec4 samplePrevious(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return vec4(0.0);
    return texture(prevCompositeTexture, uv);
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 nativeStep = max((displayRect.zw / max(nativeResolution, vec2(1.0))) / max(resolution, vec2(1.0)),
                          1.0 / max(resolution, vec2(1.0)));

    vec2 sourcePx = uv * resolution;
    vec2 nativePx = ((sourcePx - displayRect.xy) / max(displayRect.zw, vec2(1.0))) * nativeResolution;
    vec2 artifactUv = nativePx / vec2(256.0, 224.0);
    vec3 artifactA = texture(ntscArtifactTexture, artifactUv).rgb;
    vec3 artifactB = texture(ntscArtifactTexture, artifactUv + vec2(0.0, 1.0 / 224.0)).rgb;
    vec3 artifact = mix(artifactA, artifactB, clamp(ntscLerp, 0.0, 1.0));

    vec4 left = sampleCurrent(uv - vec2(nativeStep.x, 0.0));
    vec4 local = sampleCurrent(uv);
    vec4 right = sampleCurrent(uv + vec2(nativeStep.x, 0.0));

    vec3 tunedArtifact = artifact * clamp(ntscArtifacts, 0.0, 1.0);
    local.rgb = clamp(local.rgb + ((left.rgb - local.rgb) + (right.rgb - local.rgb)) * tunedArtifact, 0.0, 1.0);

    float localLuma = luma(local.rgb);
    float offset = 0.0;
    vec4 neighborLeft = sampleCurrent(uv - vec2(nativeStep.x * 1.0, 0.0));
    vec4 neighborRight = sampleCurrent(uv + vec2(nativeStep.x * 1.0, 0.0));
    offset += ((localLuma - luma(neighborLeft.rgb)) + (localLuma - luma(neighborRight.rgb))) * 1.0;
    neighborLeft = sampleCurrent(uv - vec2(nativeStep.x * 2.0, 0.0));
    neighborRight = sampleCurrent(uv + vec2(nativeStep.x * 2.0, 0.0));
    offset += ((localLuma - luma(neighborLeft.rgb)) + (localLuma - luma(neighborRight.rgb))) * -0.3162277;
    neighborLeft = sampleCurrent(uv - vec2(nativeStep.x * 3.0, 0.0));
    neighborRight = sampleCurrent(uv + vec2(nativeStep.x * 3.0, 0.0));
    offset += ((localLuma - luma(neighborLeft.rgb)) + (localLuma - luma(neighborRight.rgb))) * 0.1;
    local.rgb = clamp(local.rgb + offset * sharpness * mix(vec3(1.0), artifact, clamp(ntscArtifacts, 0.0, 1.0)), 0.0, 1.0);

    float temporalBleed = clamp(bleed, 0.0, 1.0);
    vec3 prevLeft = samplePrevious(uv - vec2(nativeStep.x, 0.0)).rgb;
    vec3 prevLocal = samplePrevious(uv).rgb;
    vec3 prevRight = samplePrevious(uv + vec2(nativeStep.x, 0.0)).rgb;
    vec3 persisted = (prevLocal + (prevLeft + prevRight) * temporalBleed) / (1.0 + temporalBleed * 2.0);
    local.rgb = max(local.rgb, persisted * clamp(persistenceRgb, vec3(0.0), vec3(1.0)));

    finalColor = local * colDiffuse * fragColor;
}
