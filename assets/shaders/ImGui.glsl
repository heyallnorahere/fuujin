#version 450

struct VertexToFragment {
    vec2 UV;
    vec4 Color;
};

#stage vertex

layout(location = 0) in vec2 in_Position;
layout(location = 1) in vec2 in_UV;
layout(location = 2) in vec4 in_Color;

layout(location = 0) out VertexToFragment out_Data;

layout(push_constant) uniform PushConstants {
    vec2 Scale;
    vec2 Translation;
} u_PushConstants;

void main() {
    out_Data.UV = in_UV;
    out_Data.Color = in_Color;

    vec2 position = u_PushConstants.Scale * in_Position + u_PushConstants.Translation;
    gl_Position = vec4(position, 0, 1);
}

#stage fragment

layout(location = 0) in VertexToFragment in_Data;

layout(location = 0) out vec4 out_Color;

layout(set = 0, binding = 0) uniform sampler2D u_Texture;

void main() {
    out_Color = in_Data.Color * texture(u_Texture, in_Data.UV);
}