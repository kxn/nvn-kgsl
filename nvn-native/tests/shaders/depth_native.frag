#version 450
#ifndef DEPTH_ONLY
#define DEPTH_ONLY 0
#endif
#if !DEPTH_ONLY
layout(location=0) in vec4 v_color;
layout(location=0) out vec4 out_color;
#endif
layout(location=2) in float v_depth;
layout(set=0,binding=2,std140) uniform FragmentConstants { vec4 depth; } fragment_constants;
void main() {
#if !DEPTH_ONLY
 out_color=v_color;
#endif
 gl_FragDepth=fragment_constants.depth.x+v_depth;
}
