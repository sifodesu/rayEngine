#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 tubeColorMatrixR;
uniform vec3 tubeColorMatrixG;
uniform vec3 tubeColorMatrixB;

out vec4 finalColor;

void main() {
    vec3 phosphor = max(texture(texture0, fragTexCoord).rgb, vec3(0.0));
    vec3 observerRgb = vec3(
        dot(tubeColorMatrixR, phosphor),
        dot(tubeColorMatrixG, phosphor),
        dot(tubeColorMatrixB, phosphor)
    );
    finalColor = vec4(max(observerRgb, vec3(0.0)), 1.0) *
        colDiffuse * fragColor;
}
