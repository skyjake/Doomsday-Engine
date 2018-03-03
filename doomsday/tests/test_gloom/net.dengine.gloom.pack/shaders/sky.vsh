#version 330 core

uniform mat4 uSkyMvpMatrix;
DENG_ATTRIB vec4 aVertex;
DENG_VAR vec3 vModelPos;

void main(void) {
    gl_Position = uSkyMvpMatrix * aVertex;
    vModelPos = aVertex.xyz;
}
