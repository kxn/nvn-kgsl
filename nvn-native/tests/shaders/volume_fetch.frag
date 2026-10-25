#version 450
// Exact integer XYZ/lod hardware fetch, no filtering or runtime resource guesses.
layout(set=0,binding=1) uniform sampler3D image3d;
layout(set=0,binding=2,std140) uniform Parameters{ivec4 p;ivec4 reserved[3];}control;
layout(location=0) in vec3 coordinate;
layout(location=0) out vec4 color;
void main(){color=texelFetch(image3d,ivec3(ivec2(coordinate.xy),control.p.y),control.p.x);}
