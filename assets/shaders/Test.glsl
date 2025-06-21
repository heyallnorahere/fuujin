#version 450

layout(push_constant) uniform PushConstants {
    vec4 Color;
} u_PushConstants;

#stage vertex
layout(location = 0) in vec3 i_Position;

layout(set = 0, binding = 0, std140) uniform MatrixUBO {
    mat4 Projection, View, Model;
} u_MatrixUBO;

void main() {
    mat4 mvp = u_MatrixUBO.Projection * u_MatrixUBO.View * u_MatrixUBO.Model;
    gl_Position = mvp * vec4(i_Position, 1.0);
}

#stage fragment
layout(location = 0) out vec4 o_Color;

void main() {
    //o_Color = vec4(0.7, 0.0, 0.8, 1.0);
    o_Color = u_PushConstants.Color;
}