#version 330 core

uniform float roughness;
uniform sampler2D uTex;

in vec3 vPosition;
in vec3 vNormal;
in vec2 vTexCoord;

layout (location = 0) out vec3 fPosition;
layout (location = 1) out vec3 fNormal;
layout (location = 2) out vec4 fAlbedoSpec;

void main(){
    fPosition = FragPos;
    fNormal = normalize(vNormal);
    gAlbedoSpec.rgb = texture(texture_diffuse1, TexCoords).rgb;
    gAlbedoSpec.a = texture(texture_specular1, TexCoords).r;
}