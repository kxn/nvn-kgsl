#version 450
#ifndef CASE
#define CASE 0
#endif
layout(location=0) in vec4 color;
layout(location=0) out vec4 result;
#if CASE >= 4
layout(std140,binding=15) uniform DynamicFS { vec4 values[1024]; } data;
#endif
void main() {
    result=color;
#if CASE >= 4
    result+=data.values[uint(gl_FragCoord.x)&255u];
#endif
}
