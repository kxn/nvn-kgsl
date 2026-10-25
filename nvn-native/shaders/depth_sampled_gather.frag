#version 450
// Authored noncompare native-depth qualification. No game identity.
layout(set=0,binding=1) uniform sampler2D source_image;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main(){color=textureGather(source_image,uv,0);}
