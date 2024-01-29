#version 330 core

layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aTexCoord;

uniform int clientWidth, clientHeight, textureWidth, textureHeight;

out vec2 vTexCoord;

void main(){
    vec3 sPos = aPosition;
    sPos.x = 2.0 * (sPos.x / clientWidth) - 1.0;
    sPos.y = 2.0 * (sPos.y / clientHeight) - 1.0;
    vTexCoord.x = aTexCoord.x / textureWidth;
    vTexCoord.y = aTexCoord.y / textureHeight;
    gl_Position = vec4(aPosition,1.0);
}