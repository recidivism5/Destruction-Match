#version 330 core

layout(location=0) in vec2 aPosition;

out vec4 vPosition;

void main() {
    vPosition = vec4(aPosition,1.0,1.0);
    gl_Position = vPosition;
}