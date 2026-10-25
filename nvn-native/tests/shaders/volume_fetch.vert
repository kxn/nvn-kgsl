#version 450
// Authored native qualification shader; no Maxwell/game envelope.
layout(location=0) in vec2 position;
layout(location=1) in vec3 texel_coordinate;
layout(location=0) out vec3 coordinate;
void main(){gl_Position=vec4(position,0.5,1.0);coordinate=texel_coordinate;}
