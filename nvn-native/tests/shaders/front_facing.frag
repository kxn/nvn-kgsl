#version 450
#ifndef CASE
#define CASE 0
#endif
layout(location=0)out vec4 color;
void main(){
#if CASE==0
 color=gl_FrontFacing?vec4(1,0,0,1):vec4(0,1,0,1);
#elif CASE==1
 color=vec4(gl_FragCoord.xy*.01,gl_FrontFacing?1:0,1);
#elif CASE==2
 color=vec4(1,0,0,1); // No live FrontFacing; identical old native recipe.
#elif CASE==3
 color=gl_FrontFacing?vec4(1):vec4(0);gl_FragDepth=.5;
#else
 color=vec4(float(gl_SampleID),gl_FrontFacing?1:0,0,1);
#endif
}
