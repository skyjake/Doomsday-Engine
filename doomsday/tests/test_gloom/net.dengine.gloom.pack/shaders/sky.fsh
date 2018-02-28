#version 330 core

#include "common/gbuffer_out.glsl"

uniform samplerCube uEnvMap;
uniform vec3 uEmissiveIntensity;

DENG_VAR vec3 vModelPos;

void main(void) {
    vec4 color = textureLod(uEnvMap, vModelPos, 0);
    GBuffer_SetFragEmissive(uEmissiveIntensity * color.rgb * color.a);
}
