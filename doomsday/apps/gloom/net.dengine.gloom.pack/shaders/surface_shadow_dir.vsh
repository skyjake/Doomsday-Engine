#version 330 core

#include "common/surface.glsl"

uniform mat4 uLightMatrix;
uniform vec3 uLightDir;

out vec2 vUV;
out vec3 vTSLightDir;
flat out uint vMaterial;

void main(void) {
    Surface surface = Gloom_LoadVertexSurface();
    float dp = dot(surface.normal, uLightDir);

    float thickness = 1.0 * 0.2;

    // Fixing shadow gaps.
    // if (dp > 0.0) { // Backfacing?
        if (surface.normal.y == 0.0) {
        //float adj = (1.0 - dp * 0.75);
        //surface.vertex.xz -= 0.3 * surface.normal.xz * adj;


            // if (testFlag(surface.flags, Surface_LeftEdge)) {
            //     surface.vertex.xyz -= 0.5 * surface.tangent;
            // }
            // else if (testFlag(surface.flags, Surface_RightEdge)) {
            //     surface.vertex.xyz += 0.5 * surface.tangent;
            // }

            if (surface.vertex.y == surface.topPlaneY) {
                surface.vertex.y += thickness;
            }
            else if (surface.vertex.y == surface.botPlaneY) {
                surface.vertex.y -= thickness;
            }
        }
        else {
            // surface.vertex.y -= surface.normal.y;
        }

        // if (dp > 0.0) {

            // surface.vertex.xyz -= uLightDir * 0.5;
        // }

        // surface.vertex.xyz -= uLightDir * 0.5;

        surface.vertex.xz += thickness * surface.expander;
    // }

    gl_Position = uLightMatrix * surface.vertex;

    if (dp > 0.0) {
        // Apply a slight bias to backfaces to avoid peter panning.
        // gl_Position.z *= 1.0 + 0.1*(1.0 - dp);
        gl_Position.z -= 0.05;
    }
    // else {
    //     // gl_Position.z += 0.004;
    // }

    vUV = Gloom_MapSurfaceTexCoord(surface);
    vTSLightDir = Gloom_TangentMatrix(Gloom_SurfaceTangentSpace(surface)) * uLightDir;
    vMaterial = surface.material;
}
