uniform mat4 uMvpMatrix;

DENG_ATTRIB vec4 aVertex;
DENG_ATTRIB vec2 aUV0;
DENG_ATTRIB vec2 aUV1;
DENG_ATTRIB vec4 aBounds;
DENG_ATTRIB vec4 aColor;

DENG_VAR vec2 vUV;
DENG_VAR vec2 vTexSize;
DENG_VAR vec4 vColor;
DENG_VAR vec4 vBounds;

void main(void) {
    gl_Position = uMvpMatrix * aVertex;
    vUV = aUV0;
    vTexSize = aUV1;
    vBounds = aBounds;
    vColor = aColor;
}
