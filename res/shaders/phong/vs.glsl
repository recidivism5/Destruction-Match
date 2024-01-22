#version 330 core

layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;

uniform mat4 uMVP;
uniform vec3 translation;

out vec3 vPosition;
out vec3 vNormal;
out vec2 vTexCoord;

void main(){
    gl_Position = uMVP * vec4(aPosition,1.0);
    vPosition = aPosition + translation;
    vNormal = aNormal;
    vTexCoord = aTexCoord;
}