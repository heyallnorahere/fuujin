#include "Renderer.glsl"
#include "Scene.glsl"
#include "InterStage.glsl"

layout(location = 0) in GeometryOut in_Data;

layout(location = 0) out vec4 out_Color;

layout(set = 1, binding = 0, std140) uniform Material {
    vec4 Albedo, Specular, Ambient;
    float Shininess;
    bool HasNormalMap;
} u_Material;

layout(set = 1, binding = 1) uniform sampler2D u_Albedo;
layout(set = 1, binding = 2) uniform sampler2D u_Specular;
layout(set = 1, binding = 3) uniform sampler2D u_Ambient;
layout(set = 1, binding = 4) uniform sampler2D u_Normal;

// MaterialXXX(UV) just returns the corresponding material aspect color
// no lighting shenanigans

vec4 MaterialAlbedo(vec2 uv) {
    vec4 tex = texture(u_Albedo, uv);
    return u_Material.Albedo * tex;
}

vec4 MaterialSpecular(vec2 uv) {
    vec4 tex = texture(u_Specular, uv);
    return u_Material.Specular * tex;
}

vec4 MaterialAmbient(vec2 uv) {
    vec4 tex = texture(u_Ambient, uv);
    return u_Material.Ambient * tex;
}

void main() {
    vec3 nnorm = normalize(in_Data.VertexData.Normal);

    vec3 normal;
    if (u_Material.HasNormalMap) {
        vec3 tangent = normalize(in_Data.VertexData.Tangent);
        vec3 bitangent = normalize(in_Data.VertexData.Bitangent);

        mat3 tbn = mat3(tangent, bitangent, nnorm);
        vec3 normalSample = texture(u_Normal, in_Data.VertexData.UV).rgb;

        normal = normalize(tbn * normalSample);
    } else {
        normal = nnorm;
    }

    // no fancy lighting
    out_Color = MaterialAlbedo(in_Data.VertexData.UV);
}
