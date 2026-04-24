#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDir;
uniform float ambient;

out vec4 finalColor;

void main() {
    vec4 texel = texture(texture0, fragTexCoord);
    vec3 n = normalize(fragNormal);
    vec3 l = normalize(-lightDir);
    float ndotl = max(dot(n, l), 0.0);

    float lit = clamp(ambient + ndotl * (1.0 - ambient), 0.0, 1.0);
    vec3 rgb = texel.rgb * colDiffuse.rgb * fragColor.rgb * lit;
    float alpha = texel.a * colDiffuse.a * fragColor.a;

    finalColor = vec4(rgb, alpha);
}
