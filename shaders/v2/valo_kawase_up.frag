#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D u_SourceTexture;

layout(std140, binding = 0) uniform buf {
    mat4 u_Matrix;
    vec2 u_HalfPixel;
    vec2 u_Offset;
};

void main() {
    vec2 uv = v_TexCoord;
    vec2 hp = u_HalfPixel * u_Offset;

    // Kernel de 8 amostras Dual-Kawase (Upsample)
    vec4 sum = vec4(0.0);

    // 4 amostras diagonais (peso 2.0 cada)
    sum += texture(u_SourceTexture, uv + vec2(-hp.x, -hp.y)) * 2.0;
    sum += texture(u_SourceTexture, uv + vec2( hp.x, -hp.y)) * 2.0;
    sum += texture(u_SourceTexture, uv + vec2(-hp.x,  hp.y)) * 2.0;
    sum += texture(u_SourceTexture, uv + vec2( hp.x,  hp.y)) * 2.0;

    // 4 amostras cardinais (peso 1.0 cada)
    sum += texture(u_SourceTexture, uv + vec2(-2.0 * hp.x, 0.0));
    sum += texture(u_SourceTexture, uv + vec2( 2.0 * hp.x, 0.0));
    sum += texture(u_SourceTexture, uv + vec2(0.0, -2.0 * hp.y));
    sum += texture(u_SourceTexture, uv + vec2(0.0,  2.0 * hp.y));

    fragColor = sum / 12.0;
}

