#version 450

#stage vertex
layout(location = 0) in vec3 in_Position;

void main() {
    gl_Position = vec4(in_Position, 1.0);
}

#stage fragment
layout(location = 0) out vec4 out_Color;

void main() {
    out_Color = vec4(0.7, 0.0, 0.8, 1.0);
}