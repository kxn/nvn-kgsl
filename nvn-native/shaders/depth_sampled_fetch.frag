#version 450
// Authored noncompare native-depth qualification. No game identity.
layout(set=0,binding=1) uniform sampler2D source_image;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main(){ivec2 p=ivec2(uv*vec2(textureSize(source_image,0)));float d=texelFetch(source_image,p,0).r;color=vec4(d,d*.5,d*.25,1);}
