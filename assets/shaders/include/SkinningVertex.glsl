#include "Renderer.glsl"
#include "Scene.glsl"
#include "InterStage.glsl"

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_UV;
layout(location = 2) in vec3 in_Normal;
layout(location = 3) in vec3 in_Tangent;

layout(location = 4) in ivec4 in_BoneIDs;
layout(location = 5) in vec4 in_Weights;

layout(location = 0) out VertexOut out_Data;

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

    gl_Position = bindToWorld * vec4(in_Position, 1.0);
    out_Data.UV = in_UV;

    mat3 normalMatrix = transpose(inverse(mat3(bindToWorld)));
    out_Data.Normal = normalize(normalMatrix * in_Normal);
    out_Data.Tangent = normalize(normalMatrix * in_Tangent);
    out_Data.Bitangent = normalize(cross(out_Data.Normal, out_Data.Tangent));
}
