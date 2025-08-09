#include "InterStage.glsl"

layout (location = 0) in GeometryOut in_Data;

void main() {
    vec3 lightToFragment = in_Data.WorldPosition - in_Data.CameraPosition;
    gl_FragDepth = length(lightToFragment) / in_Data.ZRange[1];
}
