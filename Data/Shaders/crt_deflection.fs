#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D aplTexture;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform vec4 displayRect;
uniform float time;
uniform float crtFramePhase;
uniform float ntscLineRateHz;
uniform float ntscFrameRateHz;
uniform float ntscActiveLines;
uniform float ntscActiveStartLine;
uniform float ntscContentLines;
uniform float ntscContentStartLine;
uniform float ntscActiveVideoUs;
uniform float curvatureX;
uniform float curvatureY;
uniform float pincushion;
uniform float highVoltageBloom;
uniform float highVoltageSag;
uniform float highVoltageRipple;

out vec4 finalColor;

void main() {
    vec2 screenPx = fragTexCoord * resolution;
    vec2 tubeUv = (screenPx - displayRect.xy) /
        max(displayRect.zw, vec2(1.0));
    if (tubeUv.x < 0.0 || tubeUv.y < 0.0 ||
        tubeUv.x > 1.0 || tubeUv.y > 1.0) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec2 rasterP = tubeUv * 2.0 - 1.0;
    float radius2 = dot(rasterP, rasterP);
    vec2 curvature = max(vec2(curvatureX, curvatureY), vec2(0.0));
    // Normalize each radial polynomial by its value on the corresponding
    // raster edge. The old unnormalised mapping sent valid picture UVs beyond
    // the tube and applied a large unintended overscan. Borders now remain
    // anchored while interior lines retain a small residual geometry error.
    float normalizedX = (1.0 + curvature.x * radius2) /
        (1.0 + curvature.x * (1.0 + rasterP.y * rasterP.y));
    float normalizedY = (1.0 + curvature.y * radius2) /
        (1.0 + curvature.y * (1.0 + rasterP.x * rasterP.x));
    vec2 sourceP = rasterP * vec2(normalizedX, normalizedY);
    float residualPincushion = max(pincushion, 0.0);
    sourceP.x += residualPincushion * rasterP.x * rasterP.y * rasterP.y *
        max(1.0 - rasterP.x * rasterP.x, 0.0);
    sourceP.y += residualPincushion * rasterP.y * rasterP.x * rasterP.x *
        max(1.0 - rasterP.y * rasterP.y, 0.0);
    vec4 supply = texture(aplTexture, vec2(0.5));
    float hv = supply.g > 0.25 ? supply.g : 1.0;
    float framePeriod = 1.0 / max(ntscFrameRateHz, 1.0);
    float frameStart = time - crtFramePhase * framePeriod;
    float rasterLine = ntscContentStartLine + tubeUv.y * ntscContentLines;
    float activeTime = 9.40e-6 + tubeUv.x * ntscActiveVideoUs * 1.0e-6;
    float rasterTime = frameStart + rasterLine /
        max(ntscLineRateHz, 1.0) + activeTime;
    hv *= 1.0 - max(highVoltageRipple, 0.0) *
        sin(2.0 * 3.14159265358979323846 *
            (120.0 * rasterTime + 0.17));
    float electricalSag = clamp((1.0 - hv) /
        max(highVoltageSag, 0.001), 0.0, 1.5);
    float rasterExpansion = 1.0 + max(highVoltageBloom, 0.0) *
        electricalSag;
    sourceP /= rasterExpansion;

    vec2 sourceTubeUv = sourceP * 0.5 + 0.5;
    bool inside = sourceTubeUv.x >= 0.0 && sourceTubeUv.y >= 0.0 &&
                  sourceTubeUv.x <= 1.0 && sourceTubeUv.y <= 1.0;
    vec2 sourcePx = displayRect.xy + sourceTubeUv * displayRect.zw;
    vec2 sourceUv = sourcePx / max(resolution, vec2(1.0));
    vec3 signal = inside ? max(texture(texture0, sourceUv).rgb, vec3(0.0))
                         : vec3(0.0);
    finalColor = vec4(signal, 1.0) * colDiffuse * fragColor;
}
