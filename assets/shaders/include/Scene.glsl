#define POINT_LIGHT 0

struct LightColors {
    vec3 Diffuse;
    vec3 Specular;
    vec3 Ambient;
};

struct LightAttenuation {
    float Quadratic;
    float Linear;
    float Constant;
};

struct Light {
    int Type;
    vec3 Position;

    LightColors Colors;
    LightAttenuation Attenuation;
};

struct Camera {
    vec3 Position;
    mat4 ViewProjection;
};

layout(set = 0, binding = 0, std140) uniform Scene {
    Camera Cameras[10];

    Light Lights[64];
    int LightCount;
} u_Scene;
