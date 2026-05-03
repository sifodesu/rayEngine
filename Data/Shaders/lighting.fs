#version 330

#define MAX_LIGHTS 64

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 resolution;
uniform vec2 nativeResolution;
uniform vec4 displayRect;
uniform int lightCount;
uniform vec4 lights[MAX_LIGHTS];
uniform vec4 lightColors[MAX_LIGHTS];
uniform float ambientLight;
uniform float lightFalloff;

out vec4 finalColor;

vec2 uvToNativePx(vec2 uv) {
    vec2 screenPx = vec2(uv.x, 1.0 - uv.y) * resolution;
    return ((screenPx - displayRect.xy) / max(displayRect.zw, vec2(1.0))) * nativeResolution;
}

bool insideNativeFrame(vec2 nativePx) {
    return nativePx.x >= 0.0 && nativePx.y >= 0.0 &&
           nativePx.x < nativeResolution.x && nativePx.y < nativeResolution.y;
}

void main() {
    vec4 src = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec2 nativePx = floor(uvToNativePx(fragTexCoord)) + vec2(0.5);

    if (!insideNativeFrame(nativePx)) {
        finalColor = src;
        return;
    }

    vec3 light = vec3(clamp(ambientLight, 0.0, 2.0));
    float falloff = max(lightFalloff, 0.01);

    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (i >= lightCount) break;

        vec4 lightData = lights[i];
        float radius = max(lightData.z, 0.001);
        float intensity = max(lightData.w, 0.0);
        float dist01 = clamp(length(nativePx - lightData.xy) / radius, 0.0, 1.0);
        float attenuation = pow(1.0 - dist01, falloff);
        attenuation *= 1.0 - smoothstep(0.0, 1.0, dist01);
        light += lightColors[i].rgb * lightColors[i].a * intensity * attenuation;
    }

    finalColor = vec4(clamp(src.rgb * light, 0.0, 1.0), src.a);
}
