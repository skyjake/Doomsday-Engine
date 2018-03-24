#version 330 core

uniform mat4 uSkyMvpMatrix;
in vec4 aVertex;
out vec3 vModelPos;

void main(void) {
    gl_Position = uSkyMvpMatrix * aVertex;
    vModelPos = aVertex.xyz;
}
