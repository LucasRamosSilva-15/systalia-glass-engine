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

    // Kernel de 5 amostras Dual-Kawase (Downsample)
    vec4 sum = texture(u_SourceTexture, uv) * 4.0;
    sum += texture(u_SourceTexture, uv + vec2(-hp.x, -hp.y));
    sum += texture(u_SourceTexture, uv + vec2( hp.x, -hp.y));
    sum += texture(u_SourceTexture, uv + vec2(-hp.x,  hp.y));
    sum += texture(u_SourceTexture, uv + vec2( hp.x,  hp.y));

    fragColor = sum * 0.125;
}

