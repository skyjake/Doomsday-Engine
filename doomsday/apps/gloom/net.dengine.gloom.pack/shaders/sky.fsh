#version 330 core

#include "common/gbuffer_out.glsl"

uniform samplerCube uEnvMap;
uniform vec3 uEnvIntensity;

in vec3 vModelPos;

void main(void) {
    vec4 color = textureLod(uEnvMap, vModelPos, 0);
    GBuffer_SetFragEmissive(uEnvIntensity * color.rgb * color.a);
}
