#version 450
#ifndef DEPTH_ONLY
#define DEPTH_ONLY 0
#endif
layout(location=0) in vec3 position;
layout(location=2) out float v_depth;
#if !DEPTH_ONLY
layout(location=1) in vec4 color;
layout(location=0) out vec4 v_color;
#endif
layout(set=0,binding=1,std140) uniform VertexConstants { mat4 transform; } vertex_constants;
void main() { v_depth=position.z; gl_Position=vertex_constants.transform*vec4(position,1.0);
#if !DEPTH_ONLY
 v_color=color;
#endif
}
