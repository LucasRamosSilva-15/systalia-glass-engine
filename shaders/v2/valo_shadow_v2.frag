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
    if (mode > 0.5 && q.x > 0.0 && q.y > 0.0) {
        vec2 q2 = q * q;
        float l4 = q2.x * q2.x + q2.y * q2.y;
        return (l4 > 1e-12) ? (pow(l4, 0.25) - r) : -r;
    }
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    // Posição local em relação ao centro da cápsula (0, 0 no centro)
    vec2 pGlass = v_Position - (u_WindowSize * 0.5);

    // Posição relativa ao centro da sombra deslocada em Y
    float offsetY = (u_ShadowOffsetY > 0.0) ? u_ShadowOffsetY : 12.0;
    vec2 pShadow = pGlass - vec2(0.0, offsetY);

    float dShadow = evaluateSignedDistance(pShadow, u_WindowSize, u_Radii, u_GeometryMode);
    float dGlass  = evaluateSignedDistance(pGlass,  u_WindowSize, u_Radii, u_GeometryMode);

    // Atenuação espacial suave contínua: sigma = 20px
    float sigma = 20.0;
    float shadowFactor = 1.0 - smoothstep(-0.5, sigma, dShadow);

    // Oclusão sob o corpo de vidro para que a sombra seja puramente EXTERNA (princípio Lucid)
    // Se u_Slant > 0.5 (modo isolado G), renderiza sombra completa sem recorte
    float externalFactor = (u_Slant > 0.5) ? 1.0 : smoothstep(-2.0, 1.0, dGlass);

    float opacity = (u_ShadowIntensity > 0.0) ? u_ShadowIntensity : 0.35;
    vec3 shadowRgb = vec3(0.0, 0.051, 0.129); // #000D21

    float alpha = opacity * shadowFactor * externalFactor;
    fragColor = vec4(shadowRgb * alpha, alpha);
}

