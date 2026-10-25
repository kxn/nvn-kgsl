#version 450
// True3D textureSize result includes mip-relative depth. No guest-CB mapping.
layout(set=0,binding=1) uniform sampler3D image3d;
layout(set=0,binding=2,std140) uniform Parameters{ivec4 p;ivec4 reserved[3];}control;
layout(location=0) in vec3 coordinate;
layout(location=0) out vec4 color;
// X parity makes the ordinary varying path observable alongside size/depth.
void main(){ivec3 size=textureSize(image3d,control.p.x);size.x+=int(coordinate.x)&1;color=vec4(size,255)/255.;}
