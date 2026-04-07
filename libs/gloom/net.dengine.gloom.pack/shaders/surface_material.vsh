#version 330 core

#include "common/surface.glsl"
#include "common/tangentspace.glsl"

uniform  mat4  uCameraMvpMatrix;
uniform  mat4  uModelViewMatrix;

     out vec2  vUV;
     out vec4  vVSPos;
     out vec3  vTSViewDir;
     out vec3  vWSTangent;
     out vec3  vWSBitangent;
     out vec3  vWSNormal;
flat out uint  vMaterial;
flat out uint  vFlags;

void main(void) {
    Surface surface = Gloom_LoadVertexSurface();

    gl_Position  = uCameraMvpMatrix * surface.vertex;
    vVSPos       = uModelViewMatrix * surface.vertex;
    vUV          = Gloom_MapSurfaceTexCoord(surface);
    vFlags       = surface.flags;
    vWSTangent   = surface.tangent;
    vWSBitangent = surface.bitangent;
    vWSNormal    = surface.normal;
    vMaterial    = surface.material;

    // Tangent space.
    vec3 tsViewPos, tsFragPos;
    Gloom_TangentSpaceFragment(surface, tsViewPos, tsFragPos);
    vTSViewDir = tsViewPos - tsFragPos;
}
