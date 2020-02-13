#ifndef GLOOM_SURFACE_H
#define GLOOM_SURFACE_H

#include "defs.glsl"
#include "tangentspace.glsl"
#include "time.glsl"

uniform samplerBuffer uPlanes;
uniform samplerBuffer uTexOffsets;
uniform vec4          uCameraPos; // world space

in vec4  aVertex;
in vec4  aUV; // s, t, wallLength, rotation angle
in vec2  aDirection; // expander
in vec3  aNormal;
in vec3  aTangent;
in vec3  aIndex0; // planes: geo, tex bottom, tex top
in float aFlags;
in float aTexture0; // front material
in float aTexture1; // back material
in vec2  aIndex1; // tex offset (front, back)

struct Surface {
    uint  flags;
    vec4  vertex;
    vec3  tangent;
    vec3  bitangent;
    vec3  normal;
    vec2  expander;
    float wallLength;
    float botPlaneY;
    float topPlaneY;
    int   side;
    bool  isFrontSide;
    uint  material;
};

vec4 Gloom_FetchTexOffset(uint offsetIndex) {
    return texelFetch(uTexOffsets, int(offsetIndex));
}

float Gloom_FetchPlaneY(uint planeIndex) {
    vec4 mov = texelFetch(uPlanes, int(planeIndex));
    float target    = mov.x;
    float initial   = mov.y;
    float startTime = mov.z;
    float speed     = mov.w;
    float y = initial + (uCurrentTime - startTime) * speed;
    if (speed > 0.0) return min(y, target);
    if (speed < 0.0) return max(y, target);
    return y;
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

    surface.expander = aDirection; //(surface.isFrontSide? aDirection : -aDirection);

    surface.normal     = (surface.isFrontSide? -aNormal  : aNormal);
    surface.tangent    = (surface.isFrontSide? -aTangent : aTangent);
    surface.bitangent  = cross(surface.tangent, surface.normal);
    surface.wallLength = aUV.z;

    surface.material = floatBitsToUint(surface.isFrontSide? aTexture0 : aTexture1);

    return surface;
}

TangentSpace Gloom_SurfaceTangentSpace(const Surface surface) {
    return TangentSpace(surface.tangent, surface.bitangent, surface.normal);
}

void Gloom_TangentSpaceFragment(Surface surface, out vec3 tsViewPos, out vec3 tsFragPos) {
    mat3 tbn = transpose(Gloom_TangentMatrix(Gloom_SurfaceTangentSpace(surface)));
    tsViewPos = tbn * uCameraPos.xyz;
    tsFragPos = tbn * surface.vertex.xyz;
}

vec2 Gloom_MapSurfaceTexCoord(Surface surface) {
    vec2 uv = aUV.xy;

    // Generate texture coordinates.
    if (testFlag(surface.flags, Surface_WorldSpaceYToTexCoord)) {
        uv.t = surface.vertex.y -
            (surface.isFrontSide ^^ testFlag(surface.flags, Surface_AnchorTopPlane) ?
            surface.botPlaneY : surface.topPlaneY);
    }
    if (testFlag(surface.flags, Surface_WorldSpaceXZToTexCoords)) {
        uv += aVertex.xz;
    }

    // Texture scrolling.
    if (testFlag(surface.flags, Surface_TextureOffset)) {
        vec4 texOffset = Gloom_FetchTexOffset(
            floatBitsToUint(surface.isFrontSide ? aIndex1.x : aIndex1.y));
        uv += texOffset.xy + uCurrentTime * texOffset.zw;
    }

    // Texture rotation.
    if (aUV.w != 0.0) {
        float angle = radians(aUV.w);
        uv = mat2(cos(angle), sin(angle), -sin(angle), cos(angle)) * uv;
    }

    // Align with the left edge.
    if (!surface.isFrontSide) {
        uv.s = surface.wallLength - uv.s;
    }

    if (!testFlag(surface.flags, Surface_FlipTexCoordY)) {
        uv.t = -uv.t;
    }

    return uv;
}

#endif // GLOOM_SURFACE_H
