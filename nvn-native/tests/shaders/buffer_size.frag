#version 450
layout(set=0,binding=1) uniform samplerBuffer image_buffer;
layout(location=0) in float coordinate;
layout(location=0) out vec4 color;
void main(){int n=textureSize(image_buffer);n+=int(coordinate)&1;color=vec4(float(n&255),float((n>>8)&255),float((n>>16)&255),255.0)/255.0;}
