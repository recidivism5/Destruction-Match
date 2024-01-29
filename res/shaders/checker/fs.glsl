#version 330 core

uniform sampler2D uTex;

in vec2 vTexCoord;

out vec4 FragColor;

void main(){
    FragColor = texture(uTex,vTexCoord);
}