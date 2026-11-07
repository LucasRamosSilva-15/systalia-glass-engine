#version 450
layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec2 v_Position;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 u_Matrix;
    vec4 u_Radii;
    vec2 u_WindowSize;
    vec2 u_ScreenResolution;
    float u_Slant;
    float u_Ior;
    float u_ChromaticDispersion;
    float u_CausticIntensity;
    float u_SurfaceOpacity;
    float u_Time;
    float u_HasClientTexture;
    float u_TitleBarHeight;
    float u_SpecularStrokeWidth;
    float u_GeometryMode;
    float u_ShadowIntensity;
    float u_ShadowOffsetY;
    vec4 u_VolumetricTint;
};

float evaluateSignedDistance(vec2 p, vec2 size, vec4 radii, float mode) {
    vec2 halfSize = size * 0.5;
    float r = (p.x < 0.0) ? ((p.y < 0.0) ? radii.x : radii.z)
                          : ((p.y < 0.0) ? radii.y : radii.w);
    vec2 b = halfSize - vec2(r);
    vec2 q = abs(p) - b;
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec2 p = v_Position - (u_WindowSize * 0.5);
    float dist = evaluateSignedDistance(p, u_WindowSize, u_Radii, u_GeometryMode);
    float aa = fwidth(dist);
    aa = max(aa, 0.7);
    float coverage = 1.0 - smoothstep(-0.5 * aa, 0.5 * aa, dist);

    // Cápsula branca sobre fundo transparente (alpha pré-multiplicado)
    fragColor = vec4(vec3(1.0) * coverage, coverage);
}

