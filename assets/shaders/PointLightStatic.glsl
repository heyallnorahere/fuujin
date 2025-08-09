#version 460

#stage vertex
#include "include/StaticVertex.glsl"

#stage geometry
#include "include/MultiCameraGeometry.glsl"

#stage fragment
#include "include/PointLightDepth.glsl"
