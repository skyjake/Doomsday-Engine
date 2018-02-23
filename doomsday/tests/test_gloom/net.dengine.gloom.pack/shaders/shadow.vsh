#include "common/planes.glsl"

uniform mat4 uLightMatrix;

void main(void) {
    Surface surface = Gloom_LoadVertexSurface();
    gl_Position = uLightMatrix * surface.vertex;
}
