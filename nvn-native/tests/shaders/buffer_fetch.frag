#version 450
// TYPE supplied explicitly by the normal producer, not runtime guessing.
#ifndef TYPE
#define TYPE 0
#endif
#if TYPE == 0
layout(set=0,binding=1) uniform samplerBuffer image_buffer;
#elif TYPE == 1
layout(set=0,binding=1) uniform usamplerBuffer image_buffer;
#else
layout(set=0,binding=1) uniform isamplerBuffer image_buffer;
#endif
layout(set=0,binding=2,std140) uniform Parameters{vec4 scale;vec4 bias;ivec4 offset;ivec4 reserved;}control;
layout(location=0) in float coordinate;
layout(location=0) out vec4 color;
void main(){color=vec4(texelFetch(image_buffer,int(coordinate)+control.offset.x))*control.scale+control.bias;}
