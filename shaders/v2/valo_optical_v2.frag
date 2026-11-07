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
    float u_GeometryMode; // 0.0 = CircularL2, 1.0 = SquircleG2
    float u_ShadowIntensity;
    float u_ShadowOffsetY;
    vec4 u_VolumetricTint;
};

// --- Normalização de Raios sob CSS Backgrounds & Borders Level 3 ---
vec4 normalizeRadii(vec2 size, vec4 r) {
    float w = size.x;
    float h = size.y;
    float factor = 1.0;

    float sumTop = r.x + r.y;
    if (sumTop > w && sumTop > 0.0) {
        factor = min(factor, w / sumTop);
    }
    float sumBottom = r.z + r.w;
    if (sumBottom > w && sumBottom > 0.0) {
        factor = min(factor, w / sumBottom);
    }
    float sumLeft = r.x + r.z;
    if (sumLeft > h && sumLeft > 0.0) {
        factor = min(factor, h / sumLeft);
    }
    float sumRight = r.y + r.w;
    if (sumRight > h && sumRight > 0.0) {
        factor = min(factor, h / sumRight);
    }

    vec4 normR = r * factor;
    float maxR = min(w, h) * 0.5;
    return min(normR, vec4(maxR));
}

float selectRadius(vec2 p, vec4 r) {
    if (p.x < 0.0) {
        return (p.y < 0.0) ? r.x : r.z;
    } else {
        return (p.y < 0.0) ? r.y : r.w;
    }
}

// --- SDF Circular L2 (TopBar, Dock) ---
float sdCircularL2(vec2 p, vec2 halfSize, vec4 radii) {
    float rad = selectRadius(p, radii);
    vec2 q = abs(p) - halfSize + vec2(rad);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - rad;
}

// --- SDF Squircle G2 (Superelipse de Minkowski L4, Janelas) ---
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

    // 1. Avaliação do contorno geométrico analítico
    float dist = evaluateSurfaceSDF(p, halfSize, normRadii, u_GeometryMode, u_Slant);

    // 2. Antialiasing analítico derivado de derivadas de tela (fwidth)
    float aa = max(fwidth(dist), 0.75);
    float coverage = 1.0 - smoothstep(-0.5 * aa, 0.5 * aa, dist);

    // Descarte seguro estritamente fora da franja de antialiasing
    if (coverage <= 0.0001) {
        discard;
    }

    // 3. Contorno especular unificado derivado da MESMA SDF analítica
    float strokeWidth = max(u_SpecularStrokeWidth, 0.5);
    // Traço especular interno posicionado ao longo da borda
    float strokeDist = abs(dist + 0.5 * strokeWidth) - 0.5 * strokeWidth;
    float strokeCoverage = 1.0 - smoothstep(-0.5 * aa, 0.5 * aa, strokeDist);

    float normY = clamp(v_Position.y / max(u_WindowSize.y, 1.0), 0.0, 1.0);
    vec3 specularZenith = vec3(0.72, 0.90, 1.00); // Highlight ciano-branco eletroluminescente
    vec3 specularNadir  = vec3(0.18, 0.42, 0.62); // Reflexo suave ciano inferior
    vec3 strokeColor = mix(specularZenith, specularNadir, normY * normY);
    float strokeAlpha = strokeCoverage * 0.85;

    // 4. Preenchimento volumétrico oceânico VALO
    vec3 navyDeep = vec3(0.012, 0.071, 0.141);
    vec3 cyanGlass = vec3(0.024, 0.231, 0.427);
    vec3 baseVolumetric = mix(cyanGlass, navyDeep, normY);

    // Modulação pelo token de tonalização volumétrica
    if (u_VolumetricTint.a > 0.0) {
        baseVolumetric = mix(baseVolumetric, u_VolumetricTint.rgb, u_VolumetricTint.a);
    }

    // Reflexo difuso suave no topo (aurora top sheen)
    float sheen = exp(-normY * 5.0) * 0.22;
    vec3 fillColor = baseVolumetric + vec3(sheen);
    float fillAlpha = u_SurfaceOpacity;

    // 5. Composição de cor com alpha pré-multiplicado
    vec3 compositeRgb = mix(fillColor, strokeColor, strokeAlpha);
    float compositeAlpha = clamp(fillAlpha + strokeAlpha * (1.0 - fillAlpha), 0.0, 1.0) * coverage;

    // 6. Suporte aos Modos de Diagnóstico (Section 12)
    // Se u_ShadowOffsetY < -0.5, seu módulo codifica o passe de diagnóstico ativo
    int diagMode = (u_ShadowOffsetY < -0.5) ? int(-u_ShadowOffsetY + 0.5) : 0;

    if (diagMode == 1) {
        // Passe 1: Visualização da Distância SDF e Isolinhass
        float isolines = fract(abs(dist) * 0.1);
        vec3 sdfCol = (dist < 0.0) ? vec3(0.0, 0.45, 0.85) : vec3(0.85, 0.20, 0.10);
        vec3 vis = mix(sdfCol, vec3(1.0), step(0.88, isolines));
        fragColor = vec4(vis, 1.0);
    } else if (diagMode == 2) {
        // Passe 2: Máscara de Cobertura Antialiased
        fragColor = vec4(vec3(coverage), 1.0);
    } else if (diagMode == 3) {
        // Passe 3: Preenchimento Volumétrico Isolado
        float fa = fillAlpha * coverage;
        fragColor = vec4(fillColor * fa, fa);
    } else if (diagMode == 4) {
        // Passe 4: Contorno Especular Isolado
        float sa = strokeAlpha * coverage;
        fragColor = vec4(strokeColor * sa, sa);
    } else {
        // Passe 0 / 5: Composição Final com Alpha Pré-Multiplicado (rgb * alpha, alpha)
        fragColor = vec4(compositeRgb * compositeAlpha, compositeAlpha);
    }
}

