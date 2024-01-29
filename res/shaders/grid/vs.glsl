#version 330 core

layout(location=0) in vec2 aPosition;

uniform mat4 uVP;

out vec3 vPosition;

void main(){
    gl_Position = uVP * vec4(aPosition,0.0,1.0);
    vPosition = vec3(aPosition,0.0);
}