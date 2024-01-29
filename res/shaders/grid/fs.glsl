#version 330 core

uniform vec3 uCamPos;
uniform samplerCube uSkybox;
uniform float uAmbient;
uniform float uReflectivity;

in vec3 vPosition;

out vec4 FragColor;

void main(){
    vec3 color = vec3(1.0,0.0,0.0);
    vec3 vNormal = vec3(0.0,0.0,1.0);

    vec3 lightDir = normalize(vec3(1.0,-2.0,1.0));
    float light = (max(dot(vNormal,-lightDir),0.0) + uAmbient);

    vec3 ray = normalize(vPosition-uCamPos);
    FragColor = vec4(mix(color*light,texture(uSkybox,reflect(ray,vNormal)).rgb,uReflectivity),1.0);
}