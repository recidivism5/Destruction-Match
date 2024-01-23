#version 330 core

layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;

uniform mat4 uMVP;
uniform mat4 uMTW;

out vec3 vPosition;
out vec3 vNormal;
out vec2 vTexCoord;

void main(){
    gl_Position = uMVP * vec4(aPosition,1.0);
    vPosition = (uMTW * vec4(aPosition,1.0)).xyz;
    vNormal = (uMTW * vec4(aNormal,0.0)).xyz;
    vTexCoord = aTexCoord;
}