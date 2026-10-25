#version 450
// Fetch true hardware BC descriptor without sampler/filter state. One selected
// mip/layer is sufficient for the first closed GPU qualifier; BC storage/mip
// addressing is supplied by original FDL rather than authored shader logic.
layout(set=0,binding=1) uniform sampler2D compressed_image;
layout(location=0) in vec2 coordinate;
layout(location=0) out vec4 color;
void main() { color=texelFetch(compressed_image,ivec2(coordinate),0); }
