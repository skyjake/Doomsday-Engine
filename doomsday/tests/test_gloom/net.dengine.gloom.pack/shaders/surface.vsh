uniform highp mat4 uMvpMatrix;

DENG_ATTRIB highp vec4 aVertex;
DENG_ATTRIB highp vec2 aUV;
DENG_ATTRIB highp vec3 aNormal;
DENG_ATTRIB highp uint aTexture;

DENG_VAR highp vec2 vUV;
DENG_VAR highp vec3 vNormal;
flat DENG_VAR uint vTexture;

void main(void) {
    gl_Position = uMvpMatrix * aVertex;
    vUV = aUV;
    vNormal = aNormal;
    vTexture = aTexture;
}
