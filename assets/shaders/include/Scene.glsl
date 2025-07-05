struct Camera {
    vec3 Position;
    mat4 Projection, View;
};

layout(set = 0, binding = 0, std140) uniform Scene {
    Camera Cameras[10];
} u_Scene;

mat4 GetViewProjection(int index) {
    Camera camera = u_Scene.Cameras[index];
    return camera.Projection * camera.View;
}