#include 'flags.glsl'

uniform mat4        uMvpMatrix;
uniform sampler2D   uPlanes;
uniform sampler2D   uTexOffsets;
uniform float       uCurrentTime;

DENG_ATTRIB vec4    aVertex;
DENG_ATTRIB vec3    aUV;
DENG_ATTRIB vec3    aNormal;
DENG_ATTRIB float   aTexture0; // front texture
DENG_ATTRIB float   aTexture1; // back texture
DENG_ATTRIB float   aIndex0; // geoPlane
DENG_ATTRIB float   aIndex1; // texPlane bottom
DENG_ATTRIB float   aIndex2; // texPlane top
DENG_ATTRIB float   aIndex3; // texOffset
DENG_ATTRIB float   aFlags;

     DENG_VAR vec2  vUV;
     DENG_VAR vec3  vNormal;
flat DENG_VAR float vTexture;
flat DENG_VAR uint  vFlags;

float fetchPlaneY(uint planeIndex) {
    uint dw = uint(textureSize(uPlanes, 0).x);
    return texelFetch(uPlanes, ivec2(planeIndex % dw, planeIndex / dw), 0).r;
}

vec4 fetchTexOffset(uint offsetIndex) {
    uint dw = uint(textureSize(uTexOffsets, 0).x);
    return texelFetch(uTexOffsets, ivec2(offsetIndex % dw, offsetIndex / dw), 0);
}

void main(void) {
    vFlags = floatBitsToUint(aFlags);

    //float geoDeltaY;
    vec4 vertex = aVertex;

    /* Check for a plane offset. */ {
        float planeY = fetchPlaneY(floatBitsToUint(aIndex0));
        //geoDeltaY = planeY - vertex.y;
        vertex.y = planeY;
    }
    gl_Position = uMvpMatrix * vertex;

    vUV = aUV.xy;
    float wallLength = aUV.z;

    // Choose the side.
    float botPlaneY = fetchPlaneY(floatBitsToUint(aIndex1));
    float topPlaneY = fetchPlaneY(floatBitsToUint(aIndex2));
    int   side = (botPlaneY <= topPlaneY)? 0 : 1;
    bool  isFrontSide = (side == 0);

    if (testFlag(vFlags, Surface_WorldSpaceYToTexCoord)) {
        vUV.t = vertex.y - (isFrontSide ^^ testFlag(vFlags, Surface_AnchorTopPlane)?
            botPlaneY : topPlaneY);
    }

    vNormal  = (isFrontSide? aNormal : -aNormal);
    vTexture = floatBitsToUint(isFrontSide? aTexture0 : aTexture1);

    if (testFlag(vFlags, Surface_WorldSpaceXZToTexCoords)) {
        vUV += aVertex.xz;
    }

    /* Texture scrolling. */ {
        vec4 texOffset = fetchTexOffset(floatBitsToUint(aIndex3));
        vUV += texOffset.xy + uCurrentTime * texOffset.zw;
    }

    if (!isFrontSide) {
        vUV.s = wallLength - vUV.s;
    }
    if (!testFlag(vFlags, Surface_FlipTexCoordY)) {
        vUV.t = -vUV.t;
    }
}
