#version 450

layout(push_constant) uniform PushConstants {
    vec4 Color;
} u_PushConstants;

#stage vertex
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_UV;

layout(location = 0) out vec3 out_Normal;
layout(location = 1) out vec2 out_UV;

layout(set = 0, binding = 0, std140) uniform TransformUBO {
    vec3 Translation;
} u_TransformUBO;

void main() {
    out_Normal = in_Normal;
    out_UV = in_UV;

    vec3 position = u_TransformUBO.Translation + in_Position;
    gl_Position = vec4(position, 1.0);
}

#stage fragment
layout(location = 1) in vec2 in_UV;

layout(location = 0) out vec4 out_Color;

layout(set = 0, binding = 1) uniform sampler2D u_Texture;

void main() {
    vec4 tex = texture(u_Texture, in_UV);
    out_Color = mix(u_PushConstants.Color, tex, 0.5);
}