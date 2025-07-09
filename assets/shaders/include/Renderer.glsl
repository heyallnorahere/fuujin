layout(push_constant) uniform PushConstants {
    mat4 Model;
    int CameraIndex;
    int BoneOffset;
} u_PushConstants;