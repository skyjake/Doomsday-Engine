//uniform mat4 uMvpMatrix;

DENG_ATTRIB vec4 aVertex;
DENG_ATTRIB vec2 aUV;

DENG_VAR vec2 vUV;

void main(void) {
    gl_Position = /*uMvpMatrix * */aVertex;
    vUV = aUV;
}
