#version 450
#ifndef CASE
#define CASE 0
#endif
#if CASE==3
#define FB 15
#else
#define FB 1
#endif
layout(location=0)in vec4 value;
layout(location=0)out vec4 color;
layout(set=0,binding=FB,std430)buffer FragmentStorage{uint data[];}fs;
void main(){
 uint index=uint(gl_FragCoord.x)&7u;
 float y=float(fs.data[index]&255u)/255.0;
#if CASE==1 || CASE==3
 fs.data[index+16u]=uint(gl_FragCoord.x);
#endif
#if CASE==2
 y+=float(fs.data.length())/4096.0;
#endif
 color=value+vec4(0.0,y,0.0,0.0);
}
