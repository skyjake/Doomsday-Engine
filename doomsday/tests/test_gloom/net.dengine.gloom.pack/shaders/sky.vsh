uniform mat4 uMvpMatrix;

DENG_ATTRIB vec4 aVertex;

DENG_VAR vec3 vModelPos;

void main(void) {
    gl_Position = uMvpMatrix * aVertex;
    vModelPos = aVertex.xyz;
}
