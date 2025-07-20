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

vec3 CalculateLightColor(int index, vec3 normal, vec3 materialAlbedo, vec3 materialSpecular,
                         vec3 materialAmbient) {
    Light light = u_Scene.Lights[index];

    vec3 lightToFragment;
    switch (light.Type) {
    case POINT_LIGHT:
        lightToFragment = in_Data.WorldPosition - light.Position;
        break;
    default:
        lightToFragment = vec3(0);
        break;
    }

    vec3 lightDirection = -normalize(lightToFragment);
    float diffuseStrength = dot(normal, lightDirection);

    // todo: specular lighting
    float specularStrength = 0;

    vec3 diffuse = materialAlbedo * light.Colors.Diffuse * diffuseStrength;
    vec3 specular = materialSpecular * light.Colors.Specular * specularStrength;
    vec3 ambient = materialAmbient * light.Colors.Ambient * 0.05;

    float distance = length(lightToFragment);
    float attenuationQuadratic = light.Attenuation.Quadratic * distance * distance;
    float attenuationLinear = light.Attenuation.Linear * distance;
    float attenuationConstant = light.Attenuation.Constant;

    float attenuation = 1 / (attenuationQuadratic + attenuationLinear + attenuationConstant);
    return attenuation * (diffuse + specular + ambient);
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

    vec4 albedo = MaterialAlbedo(in_Data.VertexData.UV);
    vec4 specular = MaterialSpecular(in_Data.VertexData.UV);
    vec4 ambient = MaterialAlbedo(in_Data.VertexData.UV);

    vec3 fragmentColor = vec3(0);
    for (int i = 0; i < u_Scene.LightCount; i++) {
        fragmentColor += CalculateLightColor(i, normal, albedo.rgb, specular.rgb, ambient.rgb);
    }

    float alpha = albedo.a + specular.a + ambient.a;
    out_Color = vec4(fragmentColor, alpha);
}
