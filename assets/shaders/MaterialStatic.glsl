#version 450
#include "include/Renderer.glsl"
#include "include/Material.glsl"
#include "include/Scene.glsl"

#stage vertex
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_UV;

layout(location = 0) out vec3 out_Normal;
layout(location = 1) out vec2 out_UV;
layout(location = 2) out vec3 out_WorldPosition;

void main() {
    mat4 viewProjection = GetViewProjection(u_PushConstants.CameraIndex);
    vec4 worldPosition = u_PushConstants.Model * vec4(in_Position, 1.0);
    gl_Position = viewProjection * worldPosition;

    mat3 normalMatrix = transpose(inverse(mat3(u_PushConstants.Model)));
    out_Normal = normalize(normalMatrix * in_Normal);
    out_UV = in_UV;
    out_WorldPosition = worldPosition.xyz;
}

#stage fragment
layout(location = 0) in vec3 in_Normal;
layout(location = 1) in vec2 in_UV;
layout(location = 2) in vec3 in_WorldPosition;

layout(location = 0) out vec4 out_Color;

void main() {
    vec3 normal = normalize(in_Normal);

    // no fancy lighting
    out_Color = MaterialAlbedo(in_UV);
}