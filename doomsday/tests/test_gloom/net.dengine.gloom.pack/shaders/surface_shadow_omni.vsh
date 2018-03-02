#version 330 core

#include "common/surface.glsl"

void main(void) {
    Surface surface = Gloom_LoadVertexSurface();
    gl_Position = surface.vertex;
}
