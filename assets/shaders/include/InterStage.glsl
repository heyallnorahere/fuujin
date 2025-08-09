struct VertexOut {
    vec2 UV;
    vec3 Normal, Tangent, Bitangent;
};

struct GeometryOut {
    vec3 WorldPosition;
    vec3 CameraPosition;
    vec2 ZRange;

    VertexOut VertexData;
};
