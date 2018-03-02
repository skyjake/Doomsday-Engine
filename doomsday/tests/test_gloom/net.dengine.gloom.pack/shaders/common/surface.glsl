#ifndef GLOOM_SURFACE_H
#define GLOOM_SURFACE_H

uniform sampler2D uPlanes;

DENG_ATTRIB vec4  aVertex;
DENG_ATTRIB vec4  aUV; // s, t, wallLength, rotation angle
DENG_ATTRIB vec3  aNormal;
DENG_ATTRIB vec3  aIndex0; // planes: geo, tex bottom, tex top
DENG_ATTRIB float aFlags;

struct Surface {
    uint  flags;
    vec4  vertex;
    vec3  normal;
    float wallLength;
    float botPlaneY;
    float topPlaneY;
    int   side;
    bool  isFrontSide;
};

float Gloom_FetchPlaneY(uint planeIndex) {
    uint dw = uint(textureSize(uPlanes, 0).x);
    return texelFetch(uPlanes, ivec2(planeIndex % dw, planeIndex / dw), 0).r;
}

Surface Gloom_LoadVertexSurface(void) {
    Surface surface;

    surface.flags = floatBitsToUint(aFlags);
    surface.vertex = aVertex;

    // Check for a plane offset.
    surface.vertex.y = Gloom_FetchPlaneY(floatBitsToUint(aIndex0.x));

    // Choose the side.
    surface.botPlaneY = Gloom_FetchPlaneY(floatBitsToUint(aIndex0.y));
    surface.topPlaneY = Gloom_FetchPlaneY(floatBitsToUint(aIndex0.z));
    surface.side = (surface.botPlaneY <= surface.topPlaneY)? 0 : 1;
    surface.isFrontSide = (surface.side == 0);

    surface.normal = (surface.isFrontSide? aNormal : -aNormal);
    surface.wallLength = aUV.z;

    return surface;
}

#endif // GLOOM_SURFACE_H
