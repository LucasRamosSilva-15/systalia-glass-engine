#version 450
layout(location = 0) in vec2 aVertex;
layout(location = 1) in vec2 aTexCoord;
layout(location = 0) out vec2 v_TexCoord;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

void main() {
    v_TexCoord = aTexCoord;
    gl_Position = qt_Matrix * vec4(aVertex, 0.0, 1.0);
}

