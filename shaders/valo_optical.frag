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

void main() {
    vec2 halfSize = u_WindowSize * 0.5;
    vec2 p = v_Position - halfSize;

    // 1. Cálculo da distância analítica exata à casca externa
    float dist = evaluateSurfaceSDF(p, halfSize, u_Radii, u_Slant);

    // 2. Anti-Aliasing Subpixel Analítico derivado do gradiente de variação
    float fw = max(fwidth(dist), 0.75);
    float alphaMask = 1.0 - smoothstep(-fw * 0.5, fw * 0.5, dist);

    // 3. Rejeição e Descarte Imediato fora da geometria
    if (alphaMask <= 0.001) {
        discard;
    }

    // 4. Amostragem em Screen-Space do Backdrop Framebuffer (gl_FragCoord)
    vec2 res = max(u_ScreenResolution, vec2(1.0));
    vec2 screenUV = clamp(gl_FragCoord.xy / res, vec2(0.001), vec2(0.999));
    vec4 backdropColor = texture(u_BackdropTexture, screenUV);

    // 5. Mistura com cor base abissal VALO Glass (#0C1C30)
    vec3 glassTint = vec3(0.047, 0.110, 0.188);
    vec3 finalColor = mix(backdropColor.rgb, glassTint, clamp(0.35 * u_Opacity, 0.0, 1.0));

    // 6. Linha sutil de chanfro interno na borda (1.5px) para validação visual do recorte G2
    float borderHighlight = smoothstep(0.0, 1.5, abs(dist + 0.8));
    vec3 colorWithEdge = mix(vec3(0.0, 0.898, 1.0), finalColor, borderHighlight);

    fragColor = vec4(colorWithEdge, alphaMask * u_Opacity);
}
