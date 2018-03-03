#version 330 core

#include "common/gbuffer_out.glsl"

uniform sampler2D uDiffuseAtlas;

DENG_VAR vec2 vUV;
DENG_VAR vec4 vInstanceColor;
DENG_VAR vec3 vNormal;

void main(void) {
    vec4 color = texture(uDiffuseAtlas, vUV);
    if (color.a < 0.5) discard;
    out_FragColor = color * vInstanceColor;
    GBuffer_SetFragNormal(vNormal);
}
