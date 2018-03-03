#version 330 core

#include "common/gbuffer_out.glsl"

uniform samplerCube uEnvMap;
uniform vec3 uEnvIntensity;

DENG_VAR vec3 vModelPos;

void main(void) {
    vec4 color = textureLod(uEnvMap, vModelPos, 0);
    out_FragColor = vec4(uEnvIntensity * color.rgb * color.a, 1.0);
}
