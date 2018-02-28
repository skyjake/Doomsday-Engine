#version 330 core

uniform mat4 uMvpMatrix;
DENG_ATTRIB vec4 aVertex;

void main(void) {
    gl_Position = uMvpMatrix * aVertex;
}
