#version 450
#extension GL_ARB_shader_draw_parameters : require
#ifndef CASE
#define CASE 0
#endif
#if CASE >= 4
layout(std140,binding=1) uniform DynamicVS { vec4 values[1024]; } data;
#endif
layout(location=0) in vec2 position;
layout(location=0) out vec4 color;
void main() {
    uint v=uint(gl_VertexIndex);
    vec4 result=vec4(0.0);
#if CASE == 0 || CASE == 3 || CASE == 5
    result.x=float(gl_BaseInstanceARB);
#endif
#if CASE == 1 || CASE == 3 || CASE == 5
    result.y=float(gl_BaseVertexARB);
#endif
#if CASE == 2 || CASE == 3 || CASE == 5
    result.z=float(gl_DrawIDARB);
#endif
#if CASE >= 4
    result+=data.values[v&255u];
#endif
    color=result;
    gl_Position=vec4(position,0.0,1.0);
}
