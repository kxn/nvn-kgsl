#version 450
// Authored raw-native shader, no Maxwell/NVN identity.
layout(location=0) in vec2 position;
layout(location=1) in float texel;
layout(location=0) out float coordinate;
void main(){gl_Position=vec4(position,0.5,1.0);coordinate=texel;}
