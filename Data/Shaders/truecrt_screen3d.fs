#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragCamDir;
in vec3 fragLightDir;

uniform sampler2D compFrameTexture;
uniform vec4 colDiffuse;
uniform float sampleOverscan;
uniform vec2 uvScalar;
uniform vec2 uvOffset;
uniform float glassDiffusion;
uniform float halation;
uniform float reflectionScalar;
uniform float fresnelBrightness;
uniform float brightness;
uniform float saturation;

out vec4 finalColor;

float lumaOf(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec4 sampleSafe(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return vec4(0.0);
    return texture(compFrameTexture, uv);
}

void main() {
    vec2 uv = fragTexCoord * uvScalar + uvOffset;
    uv = (uv - vec2(0.5)) / max(sampleOverscan, 0.001) + vec2(0.5);
    vec4 source = sampleSafe(uv);

    vec3 n = normalize(fragNormal);
    vec3 v = normalize(fragCamDir);
    vec3 l = normalize(fragLightDir);
    float fresnel = pow(max(1.0 - dot(n, v), 0.0), 2.0);
    float spec = pow(max(dot(reflect(-l, n), v), 0.0), 36.0);
    vec3 glass = vec3(0.18, 0.13, 0.22) * (fresnel * fresnelBrightness + spec * reflectionScalar);

    vec3 color = source.rgb;
    float gray = lumaOf(color);
    color = mix(vec3(gray), color, saturation);
    color += color * halation * smoothstep(0.25, 1.6, gray) * 0.22;
    color += glass * glassDiffusion * 0.45;
    color *= brightness;
    finalColor = vec4(max(color, vec3(0.0)), source.a) * colDiffuse * fragColor;
}
