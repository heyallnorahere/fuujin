#include "Renderer.glsl"
#include "Scene.glsl"
#include "InterStage.glsl"

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_UV;
layout(location = 2) in vec3 in_Normal;
layout(location = 3) in vec3 in_Tangent;

layout(location = 0) out VertexOut out_Data;

void main() {
    gl_Position = u_PushConstants.Model * vec4(in_Position, 1.0);
    out_Data.UV = in_UV;

    mat3 normalMatrix = transpose(inverse(mat3(u_PushConstants.Model)));
    out_Data.Normal = normalize(normalMatrix * in_Normal);
    out_Data.Tangent = normalize(normalMatrix * in_Tangent);
    out_Data.Bitangent = normalize(cross(out_Data.Normal, out_Data.Tangent));
}
