#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in float fragBlend;
in vec3 fragNormal;
in vec3 fragCamDir;
in vec3 fragLightDir;

uniform vec4 colDiffuse;
uniform vec4 frameColor;
uniform float diffuseBrightness;
uniform float specBrightness;
uniform float specPower;
uniform float fresnelBrightness;
uniform float reflectionScalar;

out vec4 finalColor;

void main() {
    vec3 n = normalize(fragNormal);
    vec3 v = normalize(fragCamDir);
    vec3 l = normalize(fragLightDir);
    vec3 h = normalize(v + l);

    float diffuse = max(dot(n, l), 0.0) * diffuseBrightness;
    float spec = pow(max(dot(n, h), 0.0), max(specPower, 0.001)) * specBrightness;
    float fresnel = pow(max(1.0 - dot(n, v), 0.0), 2.0) * fresnelBrightness;
    float reflection = smoothstep(0.05, 1.0, fragBlend) * reflectionScalar;

    vec3 base = frameColor.rgb;
    vec3 color = base + vec3(0.16, 0.13, 0.2) * diffuse;
    color += vec3(0.55, 0.52, 0.62) * spec;
    color += vec3(0.34, 0.28, 0.42) * fresnel;
    color += vec3(0.42, 0.36, 0.25) * reflection;
    finalColor = vec4(clamp(color, vec3(0.0), vec3(1.0)), frameColor.a) * colDiffuse * fragColor;
}
