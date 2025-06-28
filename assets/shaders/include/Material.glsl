layout(set = 0, binding = 0, std140) uniform Material {
    vec4 Albedo, Specular, Ambient;
    float Shininess;
    bool HasNormalMap;
} u_Material;

layout(set = 0, binding = 1) uniform sampler2D u_Albedo;
layout(set = 0, binding = 2) uniform sampler2D u_Specular;
layout(set = 0, binding = 3) uniform sampler2D u_Ambient;

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