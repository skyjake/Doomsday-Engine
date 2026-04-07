DE_ATTRIB vec4 aVertex;
DE_ATTRIB vec2 aUV;

out vec2 vUV;

void main(void) {
    gl_Position = aVertex;
    vUV = aUV;
}
