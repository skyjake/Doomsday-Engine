uniform mat4 uMvpMatrix;

DENG_ATTRIB vec4 aVertex;
DENG_ATTRIB vec2 aUV;
DENG_ATTRIB vec3 aNormal;
DENG_ATTRIB uint aTexture;

     DENG_VAR vec2 vUV;
     DENG_VAR vec3 vNormal;
flat DENG_VAR uint vTexture;

void main(void) {
    gl_Position = uMvpMatrix * aVertex;
    vUV = aUV;
    vNormal = aNormal;
    vTexture = aTexture;
}
