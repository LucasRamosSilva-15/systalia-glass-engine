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
    float u_InheritedOpacity; // offset 124: render-state opacity, uploaded by material
    float u_SpecularStrokeWidth;
    float u_GeometryMode;
    float u_ShadowIntensity;
    float u_ShadowOffsetY;
    vec4 u_VolumetricTint;
};

vec4 normalizeRadii(vec2 size, vec4 r) {
    float w = size.x;
    float h = size.y;
    float factor = 1.0;
    float sumTop = r.x + r.y;
    if (sumTop > w && sumTop > 0.0) factor = min(factor, w / sumTop);
    float sumBottom = r.z + r.w;
    if (sumBottom > w && sumBottom > 0.0) factor = min(factor, w / sumBottom);
    float sumLeft = r.x + r.z;
    if (sumLeft > h && sumLeft > 0.0) factor = min(factor, h / sumLeft);
    float sumRight = r.y + r.w;
    if (sumRight > h && sumRight > 0.0) factor = min(factor, h / sumRight);
    vec4 normR = r * factor;
    float maxR = min(w, h) * 0.5;
    return min(normR, vec4(maxR));
}

float selectRadius(vec2 p, vec4 r) {
    return (p.x < 0.0) ? ((p.y < 0.0) ? r.x : r.z) : ((p.y < 0.0) ? r.y : r.w);
}

float sdCircularL2(vec2 p, vec2 halfSize, vec4 radii) {
    float rad = selectRadius(p, radii);
    vec2 q = abs(p) - halfSize + vec2(rad);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - rad;
}

float sdSquircleG2(vec2 p, vec2 halfSize, vec4 radii) {
    float rad = selectRadius(p, radii);
    vec2 q = abs(p) - halfSize + vec2(rad);
    if (q.x > 0.0 && q.y > 0.0) {
        vec2 q2 = q * q;
        float l4 = q2.x * q2.x + q2.y * q2.y;
        return (l4 > 1e-12) ? (pow(l4, 0.25) - rad) : -rad;
    } else {
        return max(q.x, q.y) - rad;
    }
}

float evaluateSurfaceSDF(vec2 p, vec2 halfSize, vec4 radii, float mode, float slant) {
    vec2 pMod = p;
    float slantFactor = 1.0;
    if (abs(slant) > 1e-6) {
        slantFactor = 1.0 - (p.y / max(halfSize.y, 1.0)) * slant * 0.25;
        pMod.x /= max(slantFactor, 0.05);
    }
    float dist = (mode < 0.5)
        ? sdCircularL2(pMod, halfSize, radii)
        : sdSquircleG2(pMod, halfSize, radii);
    return dist * slantFactor;
}

void main() {
    vec2 halfSize = u_WindowSize * 0.5;
    vec2 p = v_Position - halfSize;
    vec4 normRadii = normalizeRadii(u_WindowSize, u_Radii);

    float dist = evaluateSurfaceSDF(p, halfSize, normRadii, u_GeometryMode, u_Slant);
    float aa = max(fwidth(dist), 0.75);
    float coverage = 1.0 - smoothstep(-0.5 * aa, 0.5 * aa, dist);
    if (coverage <= 0.0001) discard;

    vec2 eps = vec2(1.0, 0.0);
    vec2 grad = vec2(
        evaluateSurfaceSDF(p + eps.xy, halfSize, normRadii, u_GeometryMode, u_Slant) -
        evaluateSurfaceSDF(p - eps.xy, halfSize, normRadii, u_GeometryMode, u_Slant),
        evaluateSurfaceSDF(p + eps.yx, halfSize, normRadii, u_GeometryMode, u_Slant) -
        evaluateSurfaceSDF(p - eps.yx, halfSize, normRadii, u_GeometryMode, u_Slant)
    ) * 0.5;
    vec2 normal = (length(grad) > 1e-5) ? normalize(grad) : vec2(0.0);

    // First-order Euclidean correction of the L4 field; leave silhouette/AA unchanged.
    float physicalDepth = max(-dist, 0.0) / max(length(grad), 0.05);
    float meniscus = 1.0 - smoothstep(0.0, 24.0, physicalDepth);

    // Reflexo prismático espectral calibrado
    vec2 lightIncidence = vec2(0.8572, -0.5150);
    float specDot = max(dot(-normal, lightIncidence), 0.0);
    float specBroad = specDot;
    float rimGleam = pow(meniscus, 2.0) * 0.22;
    float specFactor = (0.12 + specBroad * 0.12 + rimGleam * 0.18) * meniscus;

    vec3 valoCyan   = vec3(0.000, 0.937, 1.000);
    vec3 valoIce    = vec3(0.960, 1.000, 1.000);
    vec3 valoAmber  = vec3(0.973, 0.769, 0.482);
    vec3 valoViolet = vec3(0.620, 0.467, 1.000);

    float anglePhase = atan(normal.y, normal.x) / 3.14159265 * 0.5 + 0.5;
    vec3 spectralGradient = mix(valoCyan, valoAmber, smoothstep(0.25, 0.55, anglePhase));
    spectralGradient = mix(spectralGradient, valoViolet, smoothstep(0.55, 0.85, anglePhase));
    vec3 reflectionColor = mix(valoIce, spectralGradient, 0.40);
    float strokeWidth = max(u_SpecularStrokeWidth, 0.5);
    float strokeDist = abs(dist + 0.5 * strokeWidth) - 0.5 * strokeWidth;
    float strokeAlpha = (1.0 - smoothstep(-0.5 * aa, 0.5 * aa, strokeDist)) * 0.85;
    specFactor *= 1.0 - strokeAlpha; // same occlusion by the stroke as H

    // Premultiplied source-over diagnostic; on black RGB is the isolated energy.
    float alpha = specFactor * coverage * clamp(u_InheritedOpacity, 0.0, 1.0);
    fragColor = vec4(reflectionColor * alpha, alpha);
}
