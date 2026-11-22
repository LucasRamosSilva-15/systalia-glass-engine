#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
// Independent ShaderEffect UBO, not SystaliaSurfacePropsV2.
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float animationTime;
    float intensity;
    float quality;
    vec2 surfaceSize;
    vec4 radii;
    float geometryMode;
    float slant;
    float seedOffset;
};
float random(float n) { return fract(sin(n * 127.1 + 311.7) * 43758.5453); }
void main() {
    if (quality < 0.5 || intensity <= 0.0) { fragColor = vec4(0); return; }
    vec2 halfSize = surfaceSize * 0.5;
    vec2 p = qt_TexCoord0 * surfaceSize - halfSize;
    float scaleX = max(1.0 - p.y / max(halfSize.y, 1.0) * slant * 0.25, 0.05);
    vec2 shapeP = vec2(p.x / scaleX, p.y);
    float factor = min(1.0, surfaceSize.x / max(max(radii.x+radii.y, radii.z+radii.w), 0.001));
    factor = min(factor, surfaceSize.y / max(max(radii.x+radii.z, radii.y+radii.w), 0.001));
    vec4 r = min(radii * factor, vec4(min(halfSize.x, halfSize.y)));
    float radius = shapeP.x < 0.0 ? (shapeP.y < 0.0 ? r.x : r.z) : (shapeP.y < 0.0 ? r.y : r.w);
    vec2 q = abs(shapeP) - halfSize + radius;
    vec2 positive = max(q, vec2(0));
    float roundedLength = geometryMode < 0.5 ? length(positive)
        : pow(dot(positive * positive, positive * positive), 0.25);
    float distanceToEdge = (min(max(q.x, q.y), 0.0) + roundedLength - radius) * scaleX;
    float mask = 1.0 - smoothstep(-10.0, -4.0, distanceToEdge);
    float light = 0.0;
    // No tiled cells or wrapping: six distinct, bounded, slowly drifting paths.
    for (int i = 0; i < 6; ++i) {
        if (quality < 1.5 && i >= 3) break;
        float seed = float(i) + 1.0 + seedOffset;
        vec2 origin = vec2(random(seed), random(seed + 19.0));
        vec2 drift = vec2(sin(animationTime * (0.035 + random(seed+31.0)*0.025) + seed*2.3),
                          cos(animationTime * (0.027 + random(seed+43.0)*0.021) + seed*3.1));
        vec2 center = (vec2(0.12) + origin * 0.76 + drift * 0.06) * surfaceSize;
        float d = length(qt_TexCoord0 * surfaceSize - center);
        float coreRadius = 1.0 + random(seed + 7.0) * 0.8;
        float core = 1.0 - smoothstep(coreRadius * 0.35, coreRadius + 0.8, d);
        float halo = exp(-d*d / 12.0) * 0.16;
        light += (core * 0.72 + halo) * (0.65 + random(seed + 61.0) * 0.35);
    }
    float alpha = clamp(light * intensity * mask, 0.0, 0.65) * qt_Opacity;
    fragColor = vec4(vec3(0.12, 0.74, 1.0) * alpha, alpha);
}
