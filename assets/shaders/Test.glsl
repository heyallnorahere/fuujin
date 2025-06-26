#version 450

layout(push_constant) uniform PushConstants {
    vec4 Color;
} u_PushConstants;

#stage vertex
layout(location = 0) in vec3 i_Position;
layout(location = 1) in vec2 i_UV;

layout(location = 0) out vec2 o_UV;

layout(set = 0, binding = 0, std140) uniform TransformUBO {
    vec3 Translation;
} u_TransformUBO;

void main() {
    vec3 position = u_TransformUBO.Translation + i_Position;
    gl_Position = vec4(position, 1.0);
}

#stage fragment
layout(location = 0) in vec2 i_UV;

layout(location = 0) out vec4 o_Color;

layout(set = 0, binding = 1) uniform sampler2D u_Texture;

void main() {
    vec4 tex = texture(u_Texture, i_UV);
    o_Color = mix(u_PushConstants.Color, tex, 0.5);
}