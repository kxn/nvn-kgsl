#version 450
// Implicit SAMPLE derivatives are real NIR facts, not an explicit-LOD tag.
layout(set=0,binding=1) uniform sampler2D source_image;
layout(set=0,binding=5) uniform Coordinates { vec4 scale_bias; } coordinates;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main() {
    vec2 p=uv*coordinates.scale_bias.xy+coordinates.scale_bias.zw;
    color=texture(source_image,p);
}
