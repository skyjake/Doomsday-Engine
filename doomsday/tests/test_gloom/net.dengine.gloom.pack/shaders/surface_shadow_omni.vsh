#version 330 core

#include "common/surface.glsl"

uniform vec3 uLightOrigin; // world space

out vec2 gmUV;
out vec3 gmTSLightDir;
flat out uint gmMaterial;

void main(void) {
    Surface surface = Gloom_LoadVertexSurface();

    gl_Position = surface.vertex;
    gmMaterial  = surface.material;
    gmUV        = Gloom_MapSurfaceTexCoord(surface);

    // World to tangent space.
    mat3 tbn = transpose(Gloom_TangentMatrix(Gloom_SurfaceTangentSpace(surface)));
    vec3 tsLightPos = tbn * uLightOrigin;
    vec3 tsSurfPos  = tbn * surface.vertex.xyz;
    gmTSLightDir = tsLightPos - tsSurfPos;
}
