#version 450

layout(push_constant) uniform PushConstants {
    vec4 Color;
} u_PushConstants;

#stage vertex
layout(location = 0) in vec3 i_Position;

void main() {
    gl_Position = vec4(i_Position, 1.0);
}

#stage fragment
layout(location = 0) out vec4 o_Color;

void main() {
    //o_Color = vec4(0.7, 0.0, 0.8, 1.0);
    o_Color = u_PushConstants.Color;
}