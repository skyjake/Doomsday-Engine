#include 'flags.glsl'

uniform mat4 uMvpMatrix;

DENG_ATTRIB vec4    aVertex;
DENG_ATTRIB vec2    aUV;
DENG_ATTRIB vec3    aNormal;
DENG_ATTRIB float   aTexture;
DENG_ATTRIB float   aFlags;

     DENG_VAR vec2  vUV;
     DENG_VAR vec3  vNormal;
flat DENG_VAR float vTexture;
flat DENG_VAR uint  vFlags;

void main(void) {
    gl_Position = uMvpMatrix * aVertex;
    vUV = aUV;
    vNormal = aNormal;
    vTexture = aTexture;
    vFlags = floatBitsToUint(aFlags);

    if (testFlag(vFlags, Surface_WorldSpaceXZToTexCoords)) {
        vUV += aVertex.xz;
    }
}
