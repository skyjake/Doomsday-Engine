#version 330 core

#include "common/defs.glsl"
#include "common/surface.glsl"

uniform mat4        uCameraMvpMatrix;
uniform sampler2D   uTexOffsets;
uniform float       uCurrentTime;

DENG_ATTRIB float   aTexture0; // front material
DENG_ATTRIB float   aTexture1; // back material
DENG_ATTRIB vec2    aIndex1; // tex offset (front, back)

     DENG_VAR vec2  vUV;
     DENG_VAR vec3  vWSPos;
     DENG_VAR vec3  vWSTangent;
     DENG_VAR vec3  vWSBitangent;
     DENG_VAR vec3  vWSNormal;
flat DENG_VAR float vMaterial;
flat DENG_VAR uint  vFlags;

vec4 fetchTexOffset(uint offsetIndex) {
    uint dw = uint(textureSize(uTexOffsets, 0).x);
    return texelFetch(uTexOffsets, ivec2(offsetIndex % dw, offsetIndex / dw), 0);
}

void main(void) {
    Surface surface = Gloom_LoadVertexSurface();

    gl_Position  = uCameraMvpMatrix * surface.vertex;
    vUV          = aUV.xy;
    vFlags       = surface.flags;
    vWSPos       = surface.vertex.xyz;
    vWSTangent   = surface.tangent;
    vWSBitangent = surface.bitangent;
    vWSNormal    = surface.normal;
    vMaterial    = floatBitsToUint(surface.isFrontSide? aTexture0 : aTexture1);

    // Generate texture coordinates.
    if (testFlag(surface.flags, Surface_WorldSpaceYToTexCoord)) {
        vUV.t = surface.vertex.y -
            (surface.isFrontSide ^^ testFlag(surface.flags, Surface_AnchorTopPlane)?
            surface.botPlaneY : surface.topPlaneY);
    }
    if (testFlag(surface.flags, Surface_WorldSpaceXZToTexCoords)) {
        vUV += aVertex.xz;
    }

    // Texture scrolling.
    if (testFlag(surface.flags, Surface_TextureOffset)) {
        vec4 texOffset = fetchTexOffset(
            floatBitsToUint(surface.isFrontSide? aIndex1.x : aIndex1.y));
        vUV += texOffset.xy + uCurrentTime * texOffset.zw;
    }

    // Texture rotation.
    if (aUV.w != 0.0) {
        float angle = radians(aUV.w);
        vUV = mat2(cos(angle), sin(angle), -sin(angle), cos(angle)) * vUV;
    }

    // Align with the left edge.
    if (!surface.isFrontSide) {
        vUV.s = surface.wallLength - vUV.s;
    }

    if (!testFlag(surface.flags, Surface_FlipTexCoordY)) {
        vUV.t = -vUV.t;
    }
}
