#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec2 v_Position;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_BackdropTexture;
layout(binding = 2) uniform sampler2D u_ClientTexture;

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

    // 3. Normal analítica por diferenças finitas centrais normalizadas
    vec2 eps = vec2(1.0, 0.0);
    vec2 grad = vec2(
        evaluateSurfaceSDF(p + eps.xy, halfSize, normRadii, u_GeometryMode, u_Slant) -
        evaluateSurfaceSDF(p - eps.xy, halfSize, normRadii, u_GeometryMode, u_Slant),
        evaluateSurfaceSDF(p + eps.yx, halfSize, normRadii, u_GeometryMode, u_Slant) -
        evaluateSurfaceSDF(p - eps.yx, halfSize, normRadii, u_GeometryMode, u_Slant)
    ) * 0.5;
    vec2 normal = (length(grad) > 1e-5) ? normalize(grad) : vec2(0.0);

    // 4. Menisco óptico (curvatura da borda até ~24px)
    float meniscusWidth = 24.0;
    float meniscusDist = clamp(-dist / meniscusWidth, 0.0, 1.0);
    float meniscus = (dist < 0.0) ? sin(meniscusDist * 3.14159265) : 0.0;

    // 5. Refração e Dispersão Cromática Cauchy
    vec2 screenUV = (u_ScreenResolution.x > 0.0 && u_ScreenResolution.y > 0.0)
        ? (gl_FragCoord.xy / u_ScreenResolution)
        : v_TexCoord;

    float iorDelta = max(u_Ior - 1.0, 0.0);
    vec2 refractionVector = normal * (iorDelta * 0.08) * meniscus;
    float dispersion = u_ChromaticDispersion * 0.025 * meniscus;

    vec2 uvR = screenUV + refractionVector * (1.0 + dispersion);
    vec2 uvG = screenUV + refractionVector;
    vec2 uvB = screenUV + refractionVector * (1.0 - dispersion);

    float rBackdrop = texture(u_BackdropTexture, uvR).r;
    float gBackdrop = texture(u_BackdropTexture, uvG).g;
    float bBackdrop = texture(u_BackdropTexture, uvB).b;
    vec3 refractedBackdrop = vec3(rBackdrop, gBackdrop, bBackdrop);

    // 6. Reflexos Prismáticos Espectrais (ângulo nominal -31°)
    vec2 lightIncidence = vec2(0.8572, -0.5150); // cos(-31°), sin(-31°)
    float specDot = max(dot(-normal, lightIncidence), 0.0);
    float specSharp = pow(specDot, 12.0);

    vec3 valoCyan   = vec3(0.000, 0.937, 1.000); // #00EFFF
    vec3 valoIce    = vec3(0.960, 1.000, 1.000); // #F5FFFF
    vec3 valoAmber  = vec3(0.973, 0.769, 0.482); // #F8C47B
    vec3 valoViolet = vec3(0.620, 0.467, 1.000); // #9E77FF

    float anglePhase = atan(normal.y, normal.x) / 3.14159265 * 0.5 + 0.5;
    vec3 spectralGradient = mix(valoCyan, valoAmber, smoothstep(0.25, 0.55, anglePhase));
    spectralGradient = mix(spectralGradient, valoViolet, smoothstep(0.55, 0.85, anglePhase));
    vec3 spectralReflection = mix(valoIce, spectralGradient, 0.45) * (specSharp * 1.5 * meniscus);

    // 7. Cáusticas procedurais de baixa intensidade
    vec3 causticLight = vec3(0.0);
    if (u_CausticIntensity > 0.001) {
        float c1 = sin(p.x * 0.06 + u_Time * 1.25) * cos(p.y * 0.06 + u_Time * 0.95);
        float c2 = sin(p.x * 0.11 - u_Time * 0.80 + p.y * 0.04) * cos(p.y * 0.09 + u_Time * 1.10);
        float causticWave = pow(max(0.0, c1 + c2), 2.5);
        float interiorMask = smoothstep(-2.0, -18.0, dist);
        causticLight = vec3(0.00, 0.88, 0.98) * (causticWave * u_CausticIntensity * 0.22 * interiorMask);
    }

    // 8. Contorno especular unificado
    float strokeWidth = max(u_SpecularStrokeWidth, 0.5);
    float strokeDist = abs(dist + 0.5 * strokeWidth) - 0.5 * strokeWidth;
    float strokeCoverage = 1.0 - smoothstep(-0.5 * aa, 0.5 * aa, strokeDist);

    float normY = clamp(v_Position.y / max(u_WindowSize.y, 1.0), 0.0, 1.0);
    vec3 specularZenith = vec3(0.72, 0.90, 1.00);
    vec3 specularNadir  = vec3(0.18, 0.42, 0.62);
    vec3 strokeColor = mix(specularZenith, specularNadir, normY * normY);
    float strokeAlpha = strokeCoverage * 0.85;

    // 9. Preenchimento volumétrico oceânico VALO
    vec3 navyDeep = vec3(0.012, 0.071, 0.141);
    vec3 cyanGlass = vec3(0.024, 0.231, 0.427);
    vec3 baseVolumetric = mix(cyanGlass, navyDeep, normY);

    if (u_VolumetricTint.a > 0.0) {
        baseVolumetric = mix(baseVolumetric, u_VolumetricTint.rgb, u_VolumetricTint.a);
    }

    float sheen = exp(-normY * 5.0) * 0.22;
    vec3 glassBody = mix(refractedBackdrop, baseVolumetric + vec3(sheen), u_SurfaceOpacity);
    glassBody += causticLight;
    glassBody += spectralReflection;

    // 10. Integração com textura do cliente se presente (100% nítida)
    if (u_HasClientTexture > 0.5) {
        vec4 clientSample = texture(u_ClientTexture, v_TexCoord);
        glassBody = mix(glassBody, clientSample.rgb, clientSample.a);
    }

    // 11. Composição de cor com contorno e alpha pré-multiplicado
    vec3 compositeRgb = mix(glassBody, strokeColor, strokeAlpha);
    float fillAlpha = max(u_SurfaceOpacity, 0.15);
    float compositeAlpha = clamp(fillAlpha + strokeAlpha * (1.0 - fillAlpha), 0.0, 1.0) * coverage;

    // 12. Suporte aos 8 Modos de Diagnóstico
    int diagMode = (u_ShadowOffsetY < -0.5) ? int(-u_ShadowOffsetY + 0.5) : 0;

    if (diagMode == 1 || diagMode == 3) {
        // Modo A (1 / 3): Campo Geométrico SDF e Normais Analíticas
        fragColor = vec4((normal * 0.5 + 0.5) * coverage, 0.0, coverage);
    } else if (diagMode == 2) {
        // Modo B (2): Máscara Analítica (Coverage L2 / G2)
        fragColor = vec4(vec3(coverage), coverage);
    } else if (diagMode == 8) {
        // Modo C (8): Preenchimento Volumétrico Oceânico Isolado
        fragColor = vec4(baseVolumetric * coverage * max(u_SurfaceOpacity, 0.4), coverage);
    } else if (diagMode == 4) {
        // Modo D (4): Vetores de Refração e Menisco Óptico
        fragColor = vec4(abs(refractionVector) * 250.0, meniscus * coverage, coverage);
    } else if (diagMode == 5) {
        // Modo E (5): Blur Dual-Kawase GPU Filtrado Isolado
        vec4 blurredSample = texture(u_BackdropTexture, screenUV);
        fragColor = vec4(blurredSample.rgb * coverage, coverage);
    } else if (diagMode == 6) {
        // Modo F (6): Contorno Especular Isolado de 1.25px
        fragColor = vec4(strokeColor * strokeAlpha, strokeAlpha);
    } else if (diagMode == 7) {
        // Modo 7: Reflexos Prismáticos e Cáusticas Isoladas
        fragColor = vec4((spectralReflection + causticLight) * coverage, coverage);
    } else {
        // Modo H (0): Composição Final Completa com Alpha Pré-Multiplicado
        fragColor = vec4(compositeRgb * compositeAlpha, compositeAlpha);
    }
}
