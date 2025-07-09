#version 450

#stage vertex
#include "include/Renderer.glsl"
#include "include/Scene.glsl"

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_UV;
layout(location = 2) in vec3 in_Normal;
layout(location = 3) in vec3 in_Tangent;

layout(location = 4) in ivec4 in_BoneIDs;
layout(location = 5) in vec4 in_Weights;

layout(location = 0) out vec2 out_UV;
layout(location = 1) out vec3 out_WorldPosition;
layout(location = 2) out vec3 out_Normal;
layout(location = 3) out vec3 out_Tangent;
layout(location = 4) out vec3 out_Bitangent;

layout(set = 2, binding = 0, std140) uniform Bones {
    mat4 Transforms[128];
} u_Bones;

void main() {
    mat4 bindToModel = mat4(0.0);
    for (int i = 0; i < 4; i++) {
        int boneID = u_PushConstants.BoneOffset + in_BoneIDs[i];
        float weight = in_Weights[i];

        bindToModel += weight * u_Bones.Transforms[boneID];
    }

    mat4 modelToWorld = u_PushConstants.Model;
    mat4 bindToWorld = modelToWorld * bindToModel;

    mat4 viewProjection = GetViewProjection(u_PushConstants.CameraIndex);
    vec4 worldPosition = bindToWorld * vec4(in_Position, 1.0);
    gl_Position = viewProjection * worldPosition;

    mat3 normalMatrix = transpose(inverse(mat3(bindToWorld)));
    out_Normal = normalize(normalMatrix * in_Normal);
    out_Tangent = normalize(normalMatrix * in_Tangent);
    out_Bitangent = normalize(cross(out_Normal, out_Tangent));

    out_UV = in_UV;
    out_WorldPosition = worldPosition.xyz;
}

#stage fragment
#include "include/Material.glsl"

layout(location = 0) in vec2 in_UV;
layout(location = 1) in vec3 in_WorldPosition;
layout(location = 2) in vec3 in_Normal;
layout(location = 3) in vec3 in_Tangent;
layout(location = 4) in vec3 in_Bitangent;

layout(location = 0) out vec4 out_Color;

void main() {
    out_Color = MaterialOutputColor(in_Normal, in_Tangent, in_Bitangent, in_UV, in_WorldPosition);
}