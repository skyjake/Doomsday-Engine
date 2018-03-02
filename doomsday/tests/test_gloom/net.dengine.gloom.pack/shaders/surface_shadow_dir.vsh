#version 330 core

#include "common/surface.glsl"

uniform mat4 uLightMatrix;
uniform vec3 uLightDir;

void main(void) {
    Surface surface = Gloom_LoadVertexSurface();
    gl_Position = uLightMatrix * surface.vertex;
    float dp = dot(surface.normal, uLightDir);
    if (dp > 0.0) {
        // Apply a slight bias to backfaces to avoid peter panning.
        //gl_Position.z *= 1.0 + 0.1*(1.0 - dp);
        gl_Position.z -= 0.004;
    }
}
