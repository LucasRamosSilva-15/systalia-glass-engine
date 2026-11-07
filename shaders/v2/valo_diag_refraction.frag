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

vec2 evaluateNormal(vec2 p, vec2 size, vec4 radii, float mode) {
    vec2 eps = vec2(0.5, 0.0);
    float d = evaluateSignedDistance(p, size, radii, mode);
    vec2 grad = vec2(
        evaluateSignedDistance(p + eps.xy, size, radii, mode) - evaluateSignedDistance(p - eps.xy, size, radii, mode),
        evaluateSignedDistance(p + eps.yx, size, radii, mode) - evaluateSignedDistance(p - eps.yx, size, radii, mode)
    );
    float len = length(grad);
    return (len > 0.0001) ? (grad / len) : vec2(0.0, 1.0);
}

void main() {
    vec2 p = v_Position - (u_WindowSize * 0.5);
    float dist = evaluateSignedDistance(p, u_WindowSize, u_Radii, u_GeometryMode);
    float aa = fwidth(dist);
    aa = max(aa, 0.7);
    float coverage = 1.0 - smoothstep(-0.5 * aa, 0.5 * aa, dist);

    vec2 normal = evaluateNormal(p, u_WindowSize, u_Radii, u_GeometryMode);
    float meniscusDist = clamp(-dist / 24.0, 0.0, 1.0);
    float meniscus = (dist < 0.0) ? sin(meniscusDist * 3.14159265) : 0.0;

    float etaBase = max(u_Ior - 1.0, 0.0) * 0.08;
    vec2 refractionVector = normal * etaBase * meniscus;

    // Normalizado no intervalo [0, 1] e pré-multiplicado
    vec3 col = clamp(vec3(abs(refractionVector) * 200.0, meniscus), 0.0, 1.0);
    fragColor = vec4(col * coverage, coverage);
}
