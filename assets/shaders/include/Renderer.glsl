layout(push_constant) uniform PushConstants {
    mat4 Model;
    int FirstCamera, CameraCount;
    int BoneOffset;
} u_PushConstants;
