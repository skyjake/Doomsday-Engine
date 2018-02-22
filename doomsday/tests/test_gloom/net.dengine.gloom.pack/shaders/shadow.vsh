uniform mat4 uLightMatrix;

DENG_ATTRIB vec4 aVertex;

void main(void) {
    gl_Position = uLightMatrix * aVertex;
}
