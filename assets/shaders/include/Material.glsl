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

vec4 MaterialAlbedo(in vec2 uv) {
    vec4 tex = texture(u_Albedo, uv);
    return u_Material.Albedo * tex;
}

vec4 MaterialSpecular(in vec2 uv) {
    vec4 tex = texture(u_Specular, uv);
    return u_Material.Specular * tex;
}

vec4 MaterialAmbient(in vec2 uv) {
    vec4 tex = texture(u_Ambient, uv);
    return u_Material.Ambient * tex;
}

float SampleShadowCubeMap(in Light light, in vec3 uvw) {
    float near = light.ShadowZRange[0];
    float far = light.ShadowZRange[1];
    float range = far - near;

    float shadowSample = texture(u_ShadowCubeMaps[light.ShadowIndex], uvw).r; 
    float closestDepth = shadowSample * range + near;

    return closestDepth;
}

// https://lisyarus.github.io/blog/posts/point-light-attenuation.html
float Attenuate(float distance, float radius, float falloff) {
    float s = distance / radius;
    if (s >= 1) {
        return 0;
    }

    float s2 = s * s;
    float oneMinusS2 = 1 - s2;
    return oneMinusS2 * oneMinusS2 / (1 + falloff * s);
}

float CalculatePointLightShadow(in Light light) {
    vec3 lightToFragment = in_Data.WorldPosition - light.Position;
    float currentDepth = length(lightToFragment);

    // todo: pass in through uniform buffer
    const float bias = 0.05;

    const int samples = 5;
    const float sampleOffset = 0.1;

    float step = sampleOffset * 2 / float(samples - 1);

    float shadow = 0;
    for (int u = 0; u < samples; u++) {
        float deltaX = u * step - sampleOffset;

        for (int v = 0; v < samples; v++) {
            float deltaY = v * step - sampleOffset;

            for (int w = 0; w < samples; w++) {
                float deltaZ = w * step - sampleOffset;

                vec3 delta = vec3(deltaX, deltaY, deltaZ);
                float closestDepth = SampleShadowCubeMap(light, lightToFragment + delta);

                if (currentDepth - bias > closestDepth) {
                    shadow += 1;
                }
            }
        }
    }

    return shadow / float(samples * samples * samples);
}

vec3 CalculateLightColor(int index, in vec3 normal, in vec3 materialAlbedo,
                         in vec3 materialSpecular, in vec3 materialAmbient) {
    Light light = u_Scene.Lights[index];

    vec3 lightToFragment;
    float shadow;

    switch (light.Type) {
    case POINT_LIGHT:
        lightToFragment = in_Data.WorldPosition - light.Position;
        shadow = CalculatePointLightShadow(light);

        break;
    default:
        return vec3(0);
    }

    // diffuse
    vec3 lightDirection = normalize(lightToFragment);
    float diffuseStrength = max(dot(normal, -lightDirection), 0);

    // specular
    vec3 fragmentToCamera = normalize(in_Data.CameraPosition - in_Data.WorldPosition);
    vec3 reflectedDirection = reflect(lightDirection, normal);
    vec3 halfwayDirection = normalize(reflectedDirection + fragmentToCamera);

    float cameraLightCoincidence = max(dot(normal, halfwayDirection), 0);
    float specularStrength = pow(cameraLightCoincidence, u_Material.Shininess);

    vec3 diffuse = materialAlbedo * light.Colors.Diffuse * diffuseStrength;
    vec3 specular = materialSpecular * light.Colors.Specular * specularStrength;
    vec3 ambient = materialAmbient * light.Colors.Ambient * 0.05;

    float distance = length(lightToFragment);
    float attenuation = Attenuate(distance, light.Attenuation.InfluenceRadius, light.Attenuation.Falloff);
    float lightIntensity = max(1 - shadow, 0) * attenuation;

    return lightIntensity * (diffuse + specular + ambient);
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
