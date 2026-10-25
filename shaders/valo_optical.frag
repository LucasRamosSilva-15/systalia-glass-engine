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
    float u_Opacity;
    float u_Time;
    float u_HasClientTexture;
    float u_TitleBarHeight;
};

layout(binding = 1) uniform sampler2D u_BackdropTexture;
layout(binding = 2) uniform sampler2D u_ClientTexture;

float selectRadius(vec2 p, vec4 r) {
    if (p.x < 0.0) return (p.y < 0.0) ? r.x : r.z;
    else return (p.y < 0.0) ? r.y : r.w;
}

float sdRoundedBox4G2(vec2 p, vec2 b, vec4 r) {
    float rad = selectRadius(p, r);
    rad = clamp(rad, 0.0, max(0.0, min(b.x, b.y) - 0.5));
    vec2 q = abs(p) - b + vec2(rad);
    vec2 q_pos = max(q, 0.0);
    float cornerDist = pow(pow(q_pos.x, 4.0) + pow(q_pos.y, 4.0), 0.25) - rad;
    return (q.x > 0.0 && q.y > 0.0) ? cornerDist : (max(q.x, q.y) - rad);
}

float evaluateSurfaceSDF(vec2 p, vec2 halfSize, vec4 radii, float slant) {
    vec2 p_mod = p;
    float slantFactor = 1.0 - (p.y / max(halfSize.y, 1.0)) * slant * 0.25;
    p_mod.x /= max(slantFactor, 0.05);
    float dist = sdRoundedBox4G2(p_mod, halfSize, radii);
    return dist * slantFactor;
}

vec2 calculateNormal(vec2 p, vec2 halfSize, vec4 radii, float slant) {
    vec2 eps = vec2(0.5, 0.0);
    float dx = evaluateSurfaceSDF(p + eps.xy, halfSize, radii, slant) - evaluateSurfaceSDF(p - eps.xy, halfSize, radii, slant);
    float dy = evaluateSurfaceSDF(p + eps.yx, halfSize, radii, slant) - evaluateSurfaceSDF(p - eps.yx, halfSize, radii, slant);
    float len = length(vec2(dx, dy));
    return (len > 1e-5) ? vec2(dx, dy) / len : vec2(0.0, -1.0);
}

// Cáustica analítica de água (raios subaquáticos ondulantes)
float waterCaustics(vec2 uv, float t) {
    vec2 p = uv * 8.0;
    float w1 = sin(p.x * 1.2 + p.y * 0.8 + t * 0.8);
    float w2 = sin(p.x * 0.9 - p.y * 1.3 - t * 0.6);
    float w3 = cos(p.x * 1.5 + p.y * 1.5 + t * 0.4);
    float c = (w1 + w2 + w3) * 0.3333;
    return pow(max(0.0, c * 0.5 + 0.5), 2.8);
}

void main() {
    vec2 halfSize = u_WindowSize * 0.5;
    vec2 p = v_Position - halfSize;

    // 1. Recorte Analítico SDF G2 e Subpixel Anti-Aliasing
    float dist = evaluateSurfaceSDF(p, halfSize, u_Radii, u_Slant);
    float fw = max(fwidth(dist), 0.75);
    float alphaMask = 1.0 - smoothstep(-fw * 0.5, fw * 0.5, dist);
    if (alphaMask <= 0.001) discard;

    // 2. Menisco de Tensão Superficial (Gota de 28px)
    vec2 norm = calculateNormal(p, halfSize, u_Radii, u_Slant);
    const float LENS_WIDTH = 28.0;
    float edgeDepth = clamp(-dist / LENS_WIDTH, 0.0, 1.0);
    float meniscus = pow(sin((1.0 - edgeDepth) * 1.5707963), 1.6);

    // 3. Refração Suave
    vec2 texelSize = 1.0 / max(u_ScreenResolution, vec2(1.0));
    float refrStrength = meniscus * (u_Ior - 1.0) * 45.0;
    vec2 refrOffset = norm * refrStrength * texelSize;

    // 4. Amostragem em Screen-Space com Dispersão de Cauchy Fluida (sem moiré)
    vec2 screenUV = clamp(gl_FragCoord.xy / max(u_ScreenResolution, vec2(1.0)), vec2(0.0), vec2(1.0));
    float disp = u_ChromaticDispersion * meniscus * 30.0 * texelSize.x;

    float colR = texture(u_BackdropTexture, clamp(screenUV + refrOffset + norm * disp, vec2(0.002), vec2(0.998))).r;
    float colG = texture(u_BackdropTexture, clamp(screenUV + refrOffset,               vec2(0.002), vec2(0.998))).g;
    float colB = texture(u_BackdropTexture, clamp(screenUV + refrOffset - norm * disp, vec2(0.002), vec2(0.998))).b;
    vec3 waterBackdrop = vec3(colR, colG, colB);

    // 5. Brilho Prismático Espectral Suave (Foco nos cantos inferiores, sem estrias)
    float cornerGlow = pow(clamp(-norm.y * 0.5 + norm.x * 0.5 + 0.5, 0.0, 1.0), 3.0) * meniscus;
    vec3 prismColor = mix(vec3(0.0, 0.9, 1.0), vec3(1.0, 0.3, 0.8), cornerGlow);
    waterBackdrop += prismColor * (cornerGlow * u_ChromaticDispersion * 1.2);

    // 6. Clareza Cristalina e Tonalização Oceânica Leve
    float luma = dot(waterBackdrop, vec3(0.2126, 0.7152, 0.0722));
    vec3 vibrantWater = mix(vec3(luma), waterBackdrop, 1.20);
    vec3 oceanicTint = vec3(0.03, 0.18, 0.32);
    vec3 finalColor = mix(vibrantWater, oceanicTint, clamp(0.18 * u_Opacity * edgeDepth, 0.0, 0.35));

    // 7. Cáusticas Subaquáticas Animadas (Veios de sol)
    float caustics = waterCaustics(v_TexCoord, u_Time);
    vec3 causticGlow = vec3(0.3, 0.85, 1.0) * (caustics * u_CausticIntensity * 0.45);
    finalColor += causticGlow;

    // 8. Iluminação da Borda de Gota (Bisel + Fio Zenital)
    float zenith = max(-norm.y, 0.0);
    float rimLine = exp(-pow((dist + 1.0) / 0.75, 2.0));
    finalColor += vec3(0.9, 0.98, 1.0) * (rimLine * zenith * 0.90);

    // Borda iluminada suave ao redor de todo o chassi
    float softEdgeGleam = pow(meniscus, 2.2) * 0.20;
    finalColor += vec3(0.0, 0.8, 1.0) * softEdgeGleam;

    // A textura Wayland entra na mesma passada óptica e fica restrita ao chassi interno.
    float clientCoverage = 0.0;
    if (u_HasClientTexture > 0.5) {
        const float CLIENT_INSET = 8.0;
        vec2 clientSize = u_WindowSize - vec2(2.0 * CLIENT_INSET,
                                               u_TitleBarHeight + 2.0 * CLIENT_INSET);
        if (clientSize.x > 0.0 && clientSize.y > 0.0) {
            vec2 clientOrigin = vec2(CLIENT_INSET, u_TitleBarHeight + CLIENT_INSET);
            vec2 clientPosition = v_Position - clientOrigin;
            vec2 clientHalfSize = clientSize * 0.5;
            vec2 clientPoint = clientPosition - clientHalfSize;
            vec4 clientRadii = vec4(0.0, 0.0,
                                    max(0.0, u_Radii.z - CLIENT_INSET),
                                    max(0.0, u_Radii.w - CLIENT_INSET));
            float clientDistance = evaluateSurfaceSDF(clientPoint, clientHalfSize,
                                                      clientRadii, u_Slant);
            float clientFw = max(fwidth(clientDistance), 0.75);
            float clientMask = 1.0 - smoothstep(-clientFw * 0.5,
                                                clientFw * 0.5, clientDistance);
            vec2 clientUV = clamp(clientPosition / clientSize, vec2(0.0), vec2(1.0));
            vec4 clientPixel = texture(u_ClientTexture, clientUV);
            clientCoverage = clientMask * clientPixel.a;
            finalColor = mix(finalColor, clientPixel.rgb, clientCoverage);
        }
    }

    float finalAlpha = max(alphaMask * u_Opacity, clientCoverage);
    fragColor = vec4(clamp(finalColor, vec3(0.0), vec3(1.0)), finalAlpha);
}
