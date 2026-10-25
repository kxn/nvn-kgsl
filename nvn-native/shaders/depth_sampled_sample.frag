#version 450
// Authored noncompare native-depth qualification. No game identity.
layout(set=0,binding=1) uniform sampler2D source_image;
layout(set=0,binding=5,std140) uniform Coordinates { vec4 coordinates; };
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main(){float d=texture(source_image,uv*coordinates.xy+coordinates.zw).r;color=vec4(d,d*.5,d*.25,1);}
