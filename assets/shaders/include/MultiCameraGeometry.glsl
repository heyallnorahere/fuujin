#include "Renderer.glsl"
#include "Scene.glsl"
#include "InterStage.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in VertexOut in_Data[];

layout(location = 0) out GeometryOut out_Data;

void main() {
    for (int i = 0; i < u_PushConstants.CameraCount; i++) {
        if (u_PushConstants.CameraCount > 1) {
            gl_Layer = i;
        }

        int cameraIndex = i + u_PushConstants.FirstCamera;
        Camera camera = u_Scene.Cameras[cameraIndex];

        for (int j = 0; j < 3; j++) {
            vec4 worldPosition = gl_in[j].gl_Position;
            gl_Position = camera.ViewProjection * worldPosition;

            out_Data.WorldPosition = worldPosition.xyz;
            out_Data.CameraPosition = camera.Position;
            out_Data.VertexData = in_Data[j];

            EmitVertex();
        }

        EndPrimitive();
    }
}
