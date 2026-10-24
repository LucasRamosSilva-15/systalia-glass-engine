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

// Seleção de raio por quadrante
float selectRadius(vec2 p, vec4 r) {
    if (p.x < 0.0) {
        return (p.y < 0.0) ? r.x : r.z;
    } else {
        return (p.y < 0.0) ? r.y : r.w;
    }
}

// SDF G2 com superelipse de 4ª ordem
float sdRoundedBox4G2(vec2 p, vec2 b, vec4 r) {
    float rad = selectRadius(p, r);
    rad = clamp(rad, 0.0, min(b.x, b.y) - 0.5);
    vec2 q = abs(p) - b + vec2(rad);
    vec2 q_pos = max(q, 0.0);
    float cornerDist = pow(pow(q_pos.x, 4.0) + pow(q_pos.y, 4.0), 0.25) - rad;
    return (q.x > 0.0 && q.y > 0.0) ? cornerDist : (max(q.x, q.y) - rad);
}

// Avaliação analítica com slant
float evaluateSurfaceSDF(vec2 p, vec2 halfSize, vec4 radii, float slant) {
    vec2 p_mod = p;
    float slantFactor = 1.0 - (p.y / max(halfSize.y, 1.0)) * slant * 0.25;
    p_mod.x /= max(slantFactor, 0.05);
    float dist = sdRoundedBox4G2(p_mod, halfSize, radii);
    return dist * slantFactor;
}

// Gradiente subpixel para normais
vec2 calculateNormal(vec2 p, vec2 halfSize, vec4 radii, float slant) {
    vec2 eps = vec2(0.5, 0.0);
    float dx = evaluateSurfaceSDF(p + eps.xy, halfSize, radii, slant) - evaluateSurfaceSDF(p - eps.xy, halfSize, radii, slant);
    float dy = evaluateSurfaceSDF(p + eps.yx, halfSize, radii, slant) - evaluateSurfaceSDF(p - eps.yx, halfSize, radii, slant);
    float len = length(vec2(dx, dy));
    return (len > 1e-5) ? vec2(dx, dy) / len : vec2(0.0, -1.0);
}

// Paleta espectral contínua para dispersão de arco-íris (Rainbow Flare)
vec3 spectralRainbow(float t) {
    t = clamp(t, 0.0, 1.0);
    // Transição espectral suave: Vermelho -> Amarelo -> Verde -> Ciano -> Azul -> Magenta
    vec3 c = clamp(abs(mod(t * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return c;
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

    // 2. Geometria de Menisco de Gota d'Água (Tensão Superficial de 32px)
    vec2 norm = calculateNormal(p, halfSize, u_Radii, u_Slant);
    const float LENS_WIDTH = 32.0;
    float edgeDepth = clamp(-dist / LENS_WIDTH, 0.0, 1.0);
    
    // Perfil volumétrico convexo de gota fluida: curvatura senoidal suave
    float waterDropProfile = pow(sin((1.0 - edgeDepth) * 1.5707963), 1.7);

    // 3. Vetor de Refração Convexa Tridimensional (Puxando a luz circundante para dentro)
    vec2 texelSize = 1.0 / max(u_ScreenResolution, vec2(1.0));
    float refrStrength = waterDropProfile * (u_Ior - 1.0) * 70.0;
    vec2 refrOffset = norm * refrStrength * texelSize;

    // 4. Coordenadas de Amostragem Screen-Space
    vec2 screenUV = clamp(gl_FragCoord.xy / max(u_ScreenResolution, vec2(1.0)), vec2(0.0), vec2(1.0));

    // Amostragem refratada dos canais RGB com defasagem de Cauchy
    float dispersionBase = u_ChromaticDispersion * waterDropProfile * 55.0 * texelSize.x;
    vec2 uvR = clamp(screenUV + refrOffset + norm * dispersionBase, vec2(0.002), vec2(0.998));
    vec2 uvG = clamp(screenUV + refrOffset,                         vec2(0.002), vec2(0.998));
    vec2 uvB = clamp(screenUV + refrOffset - norm * dispersionBase, vec2(0.002), vec2(0.998));

    float colR = texture(u_BackdropTexture, uvR).r;
    float colG = texture(u_BackdropTexture, uvG).g;
    float colB = texture(u_BackdropTexture, uvB).b;
    vec3 waterBackdrop = vec3(colR, colG, colB);

    // 5. Decomposição Espectral de Arco-Íris (Rainbow Optical Flare nas bordas e cantos)
    // Ativação intensa na borda e cantos quando a luz é refratada em ângulos oblíquos
    float cornerStress = clamp(length(abs(p) / max(halfSize, vec2(1.0))) - 0.75, 0.0, 1.0);
    float flareFactor = waterDropProfile * (0.6 * u_ChromaticDispersion + 0.4 * cornerStress);
    // Ângulo do feixe de decomposição prismática
    float spectralPhase = fract(dot(norm, vec2(0.707, -0.707)) * 1.8 + v_TexCoord.x * 0.5);
    vec3 rainbowTint = spectralRainbow(spectralPhase);
    waterBackdrop = mix(waterBackdrop, waterBackdrop * 0.4 + rainbowTint * 0.9, clamp(flareFactor * 1.6, 0.0, 0.85));

    // 6. Substrato de Água Pura Translúcida (VALO Oceanic Tint)
    // Saturação da água cristalina sem lama escura:
    float luma = dot(waterBackdrop, vec3(0.2126, 0.7152, 0.0722));
    vec3 saturatedWater = mix(vec3(luma), waterBackdrop, 1.35);

    // Tonalização abissal cristalina suave (RGBA: 0, 160, 255 com densidade controlada)
    vec3 oceanicCyanTint = vec3(0.02, 0.25, 0.45);
    // A água é límpida! Apenas 15% a 25% de tonalização interna para preservar nitidez:
    vec3 finalColor = mix(saturatedWater, oceanicCyanTint, clamp(0.20 * u_Opacity * pow(edgeDepth, 0.8), 0.0, 0.40));

    // 7. Iluminação de Volume da Gota (Bisel Suave + Brilho Especular Orgânico)
    vec2 lightDir = normalize(vec2(-0.707106, -0.707106)); // Luz zenital vinda de cima-esquerda
    float lightDot = dot(norm, lightDir);
    
    // Brilho volumétrico da curvatura superior-esquerda (não é uma linha dura, é o volume da gota refletindo)
    float dropVolumeGleam = clamp(lightDot, 0.0, 1.0) * pow(waterDropProfile, 1.8);
    finalColor += vec3(0.75, 0.95, 1.0) * (dropVolumeGleam * 0.45);

    // Fio de luz do menisco na quina externa (1.2px)
    float fineRim = exp(-pow((dist + 1.0) / 0.7, 2.0));
    finalColor += vec3(1.0, 1.0, 1.0) * (fineRim * max(-norm.y, 0.0) * 0.80);

    // 8. Saída Final com Alpha Blending Contínuo
    fragColor = vec4(clamp(finalColor, vec3(0.0), vec3(1.0)), alphaMask * u_Opacity);
}