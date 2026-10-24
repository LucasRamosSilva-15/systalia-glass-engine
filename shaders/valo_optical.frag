#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec2 v_Position;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 u_Matrix;
    vec4 u_Radii; // x = TL, y = TR, z = BL, w = BR
    vec2 u_WindowSize;
    vec2 u_ScreenResolution;
    float u_Slant;
    float u_Ior;
    float u_ChromaticDispersion;
    float u_CausticIntensity;
    float u_Opacity;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(binding = 1) uniform sampler2D u_BackdropTexture;

// Seleção analítica do raio de curvatura correspondente ao quadrante
float selectRadius(vec2 p, vec4 r) {
    if (p.x < 0.0) {
        return (p.y < 0.0) ? r.x : r.z; // Top-Left / Bottom-Left
    } else {
        return (p.y < 0.0) ? r.y : r.w; // Top-Right / Bottom-Right
    }
}

// Signed Distance Field com 4 raios e continuidade G2 (Superelipse de ordem 4)
float sdRoundedBox4G2(vec2 p, vec2 b, vec4 r) {
    float rad = selectRadius(p, r);
    rad = clamp(rad, 0.0, min(b.x, b.y) - 0.5);
    
    vec2 q = abs(p) - b + vec2(rad);
    vec2 q_pos = max(q, 0.0);
    
    // Equação da Superelipse G2: (x^4 + y^4)^(1/4) - r
    float cornerDist = pow(pow(q_pos.x, 4.0) + pow(q_pos.y, 4.0), 0.25) - rad;
    return (q.x > 0.0 && q.y > 0.0) ? cornerDist : (max(q.x, q.y) - rad);
}

// Avaliação combinada do SDF considerando o slant (inclinação lateral para trapézios/domos)
float evaluateSurfaceSDF(vec2 p, vec2 halfSize, vec4 radii, float slant) {
    vec2 p_mod = p;
    float slantFactor = 1.0 - (p.y / max(halfSize.y, 1.0)) * slant * 0.25;
    p_mod.x /= max(slantFactor, 0.05);
    float dist = sdRoundedBox4G2(p_mod, halfSize, radii);
    return dist * slantFactor;
}

// Cálculo das normais analíticas por gradiente diferencial subpixel
vec2 calculateNormal(vec2 p, vec2 halfSize, vec4 radii, float slant) {
    vec2 eps = vec2(0.5, 0.0);
    float dx = evaluateSurfaceSDF(p + eps.xy, halfSize, radii, slant) - evaluateSurfaceSDF(p - eps.xy, halfSize, radii, slant);
    float dy = evaluateSurfaceSDF(p + eps.yx, halfSize, radii, slant) - evaluateSurfaceSDF(p - eps.yx, halfSize, radii, slant);
    float len = length(vec2(dx, dy));
    return (len > 1e-5) ? vec2(dx, dy) / len : vec2(0.0, -1.0);
}

void main() {
    vec2 halfSize = u_WindowSize * 0.5;
    vec2 p = v_Position - halfSize;

    // 1. Recorte Analítico SDF G2 e Subpixel Anti-Aliasing
    float dist = evaluateSurfaceSDF(p, halfSize, u_Radii, u_Slant);
    float fw = max(fwidth(dist), 0.75);
    float alphaMask = 1.0 - smoothstep(-fw * 0.5, fw * 0.5, dist);

    if (alphaMask <= 0.001) {
        discard;
    }

    // 2. Normais Analíticas do Menisco de Borda (7.0px para evitar Rubber Ring)
    vec2 norm = calculateNormal(p, halfSize, u_Radii, u_Slant);
    float distFromEdge = max(-dist, 0.0);

    const float MENISCUS_MAX = 7.0;
    float meniscusFactor = clamp(1.0 - (distFromEdge / MENISCUS_MAX), 0.0, 1.0);
    float meniscusProfile = smoothstep(0.0, 1.0, meniscusFactor);

    // 3. Vetor de Refração Composto (Domo Convexo + Menisco Tangencial)
    vec2 p_norm = p / max(halfSize, vec2(1.0));
    vec2 bodyRefraction = p_norm * (u_Ior - 1.0) * 0.14;
    vec2 meniscusRefraction = norm * (meniscusProfile * (u_Ior - 1.0) * 0.10);
    vec2 refractionOffset = bodyRefraction + meniscusRefraction;

    // 4. Amostragem em Screen-Space com Dispersão Espectral de Cauchy
    vec2 res = max(u_ScreenResolution, vec2(1.0));
    vec2 screenUV = clamp(gl_FragCoord.xy / res, vec2(0.0), vec2(1.0));

    vec2 refrUV_R = clamp(screenUV + refractionOffset * (1.0 - u_ChromaticDispersion * 2.0), vec2(0.001), vec2(0.999));
    vec2 refrUV_G = clamp(screenUV + refractionOffset, vec2(0.001), vec2(0.999));
    vec2 refrUV_B = clamp(screenUV + refractionOffset * (1.0 + u_ChromaticDispersion * 2.0), vec2(0.001), vec2(0.999));

    float colR = texture(u_BackdropTexture, refrUV_R).r;
    float colG = texture(u_BackdropTexture, refrUV_G).g;
    float colB = texture(u_BackdropTexture, refrUV_B).b;
    vec3 refractedBackdrop = vec3(colR, colG, colB);

    // 5. Specular Rim Zenital Fino (1.1px no topo)
    float zenith = max(-norm.y, 0.0);
    float rimLine = exp(-pow((distFromEdge - 0.6) / 0.45, 2.0));
    float specularRim = clamp(rimLine * zenith, 0.0, 1.0);
    vec3 rimSpecularColor = clamp(vec3(1.0) * (specularRim * 0.95) + vec3(0.0, 0.898, 1.0) * (specularRim * 0.25), vec3(0.0), vec3(1.0));

    // 6. Feixe Cáustico Planar a 45°
    float diagCoord = clamp(dot(v_TexCoord, vec2(0.707106, 0.707106)), 0.0, 1.414);
    float causticBand = exp(-pow((diagCoord - 0.45) / 0.22, 2.0));
    vec3 causticTint = mix(vec3(0.0, 0.898, 1.0), vec3(1.0, 1.0, 1.0), 0.72);
    vec3 causticContribution = clamp(causticTint * (causticBand * u_CausticIntensity * 0.35), vec3(0.0), vec3(0.4));

    // 7. Base Abissal Translúcida VALO Glass (rgba(12, 28, 48))
    vec3 glassBase = vec3(0.047, 0.110, 0.188);
    vec3 finalColor = mix(refractedBackdrop, glassBase, clamp(0.22 * u_Opacity, 0.0, 1.0));
    finalColor += causticContribution;
    finalColor += rimSpecularColor;
    finalColor = clamp(finalColor, vec3(0.0), vec3(1.0));

    fragColor = vec4(finalColor, alphaMask * u_Opacity);
}
