#version 450
// Authored qualification shader, never a substitute for a game stage.
layout(location=0) in vec2 position;
layout(location=1) in vec2 texel_coordinate;
layout(location=0) out vec2 coordinate;
void main() { gl_Position=vec4(position,0.5,1.0); coordinate=texel_coordinate; }
