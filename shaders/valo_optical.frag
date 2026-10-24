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
        return (p.y < 0.0) ? r.x : r.z; // TL / BL
    } else {
        return (p.y < 0.0) ? r.y : r.w; // TR / BR
    }
}

// Signed Distance Field com 4 raios e continuidade G2 (Superelipse de ordem 4)
float sdRoundedBox4G2(vec2 p, vec2 b, vec4 r) {
    float rad = selectRadius(p, r);
    rad = clamp(rad, 0.0, min(b.x, b.y) - 0.5);
    
    vec2 q = abs(p) - b + vec2(rad);
    vec2 q_pos = max(q, 0.0);
    
    float cornerDist = pow(pow(q_pos.x, 4.0) + pow(q_pos.y, 4.0), 0.25) - rad;
    return (q.x > 0.0 && q.y > 0.0) ? cornerDist : (max(q.x, q.y) - rad);
}

// Avaliação analítica com inclinação lateral (slant)
float evaluateSurfaceSDF(vec2 p, vec2 halfSize, vec4 radii, float slant) {
    vec2 p_mod = p;
    float slantFactor = 1.0 - (p.y / max(halfSize.y, 1.0)) * slant * 0.25;
    p_mod.x /= max(slantFactor, 0.05);
    float dist = sdRoundedBox4G2(p_mod, halfSize, radii);
    return dist * slantFactor;
}

// Cálculo das normais analíticas através do gradiente subpixel
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

    // 1. Recorte analítico SDF G2 e Subpixel Anti-Aliasing
    float dist = evaluateSurfaceSDF(p, halfSize, u_Radii, u_Slant);
    float fw = max(fwidth(dist), 0.75);
    float alphaMask = 1.0 - smoothstep(-fw * 0.5, fw * 0.5, dist);

    if (alphaMask <= 0.001) {
        discard;
    }

    // 1. Geometria do Menisco (LENS_WIDTH = 28.0px)
    vec2 norm = calculateNormal(p, halfSize, u_Radii, u_Slant);
    const float LENS_WIDTH = 28.0;
    float edgeDepth = clamp(-dist / LENS_WIDTH, 0.0, 1.0);
    
    // Perfil de cúpula esférica da gota: z(d) = sqrt(1 - d^2)
    float domeProfile = sqrt(max(0.0, 1.0 - pow(edgeDepth, 2.0)));
    // Inclinação/slope da superfície 3D (tangente do menisco)
    float slope = (edgeDepth / max(domeProfile, 0.15)) * (u_Ior - 1.0) * 1.8;

    // Normal tridimensional da casca de cristal
    vec3 N3D = normalize(vec3(norm * slope, 1.0));
    vec3 viewDir = vec3(0.0, 0.0, -1.0);

    // Refração analítica tridimensional de Snell
    vec3 refrRay = refract(viewDir, N3D, 1.0 / max(u_Ior, 1.001));
    // Projeção do raio refratado no plano do backdrop
    vec2 displacementPixels = (refrRay.xy / max(-refrRay.z, 0.1)) * 32.0 * domeProfile;

    // Magnificação / Compressão da lente na borda (Efeito Lupa)
    vec2 texelSize = 1.0 / max(u_ScreenResolution, vec2(1.0));
    vec2 refrOffset = displacementPixels * texelSize;

    // 2. Dispersão Cromática de Cauchy
    vec2 screenUV = clamp(gl_FragCoord.xy / max(u_ScreenResolution, vec2(1.0)), vec2(0.0), vec2(1.0));
    float dispersionDelta = u_ChromaticDispersion * domeProfile * 40.0 * texelSize.x;

    vec2 uvR = clamp(screenUV + refrOffset + norm * dispersionDelta, vec2(0.002), vec2(0.998));
    vec2 uvG = clamp(screenUV + refrOffset,                           vec2(0.002), vec2(0.998));
    vec2 uvB = clamp(screenUV + refrOffset - norm * dispersionDelta, vec2(0.002), vec2(0.998));

    float colR = texture(u_BackdropTexture, uvR).r;
    float colG = texture(u_BackdropTexture, uvG).g;
    float colB = texture(u_BackdropTexture, uvB).b;
    vec3 refractedBackdrop = vec3(colR, colG, colB);

    // 3. Substrato de Vidro Cristalino (Pureza na Borda)
    vec3 glassBase = vec3(0.047, 0.110, 0.188); // #0C1C30
    float interiorTint = clamp(0.32 * u_Opacity, 0.0, 0.60) * pow(edgeDepth, 2.0);
    vec3 finalColor = mix(refractedBackdrop, glassBase, interiorTint);

    // 4. Bisel e Oclusão Ambiente
    vec2 lightDir = normalize(vec2(-0.707106, -0.707106));
    float lightFactor = dot(norm, lightDir);
    float bevelLight = clamp(lightFactor, 0.0, 1.0) * pow(domeProfile, 2.5);
    finalColor += vec3(0.9, 0.95, 1.0) * (bevelLight * 0.35);

    float bevelShadow = clamp(-lightFactor, 0.0, 1.0) * pow(domeProfile, 2.0);
    finalColor = mix(finalColor, finalColor * 0.50, bevelShadow * 0.50);

    // 5. Linha Especular Fina de Cristal (1.2px) - Sem barra grossa
    float outerRim = exp(-pow((dist + 0.8) / 0.55, 2.0));
    float zenithGlint = max(-norm.y, 0.0);
    finalColor += vec3(1.0, 1.0, 1.0) * (outerRim * zenithGlint * 0.95);

    // 6. Cáustica Planar
    float diagCoord = clamp(dot(v_TexCoord, vec2(0.707106, 0.707106)), 0.0, 1.414);
    float causticBand = exp(-pow((diagCoord - 0.45) / 0.22, 2.0));
    vec3 causticTint = mix(vec3(0.0, 0.898, 1.0), vec3(1.0, 1.0, 1.0), 0.75);
    finalColor += clamp(causticTint * (causticBand * u_CausticIntensity * 0.30), vec3(0.0), vec3(0.30));

    fragColor = vec4(clamp(finalColor, vec3(0.0), vec3(1.0)), alphaMask * u_Opacity);
}