#version 330 core

uniform float shininess;
uniform sampler2D uTex;
uniform vec3 cameraPos;

struct Light {
    vec3 position;
    vec3 color;
};
uniform int numLights;
uniform Light lights[16];

in vec3 vPosition;
in vec3 vNormal;
in vec2 vTexCoord;

out vec4 FragColor;

void main(){
    vec3 baseColor = texture(uTex,vTexCoord).rgb;
    vec3 toCam = normalize(cameraPos - vPosition);
    vec3 result = vec3(0.0);
    for(int i = 0; i < numLights; i++){
        vec3 toLight = normalize(lights[i].position - vPosition);
        float diffuse = max(dot(toLight, vNormal), 0.0);
        float specular = pow(max(dot(reflect(-toLight, vNormal), toCam), 0.0), shininess);
        result += (diffuse + specular + 0.25) * lights[i].color * baseColor;
    }
    FragColor = vec4(result, 1.0);
}