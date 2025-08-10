#include "InterStage.glsl"

layout (location = 0) in GeometryOut in_Data;

void main() {
    vec3 lightToFragment = in_Data.WorldPosition - in_Data.CameraPosition;
    float fragmentDistance = length(lightToFragment);

    float near = in_Data.ZRange[0];
    float far = in_Data.ZRange[1];
    float scale = far - near;
    
    gl_FragDepth = (fragmentDistance - near) / scale;
}
