#version 330 core

uniform sampler2D uTex;

in vec2 vTexCoord;
in vec3 vColor;

out vec4 FragColor;

void main(){
    vec4 sample = texture(uTex,vTexCoord);
    FragColor = vec4(vColor * sample.rgb, sample.a);
}