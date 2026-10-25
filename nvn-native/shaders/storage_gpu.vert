#version 450
#ifndef CASE
#define CASE 0
#endif
#if CASE==3
#define VB 15
#else
#define VB 1
#endif
layout(location=0)in vec2 position;
layout(set=0,binding=VB,std430)buffer VertexStorage{uint data[];}vs;
layout(location=0)out vec4 value;
void main(){
 uint index=uint(gl_VertexIndex);
 float x=float(vs.data[index]&255u)/255.0;
#if CASE==1 || CASE==3
 vs.data[index+8u]=vs.data[index]^0x55u;
#endif
#if CASE==2
 x+=float(vs.data.length())/4096.0;
#endif
 gl_Position=vec4(position,0.0,1.0);value=vec4(x,0.25,0.5,1.0);
}
