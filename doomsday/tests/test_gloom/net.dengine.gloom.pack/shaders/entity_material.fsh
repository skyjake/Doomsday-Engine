#version 330 core

#include "common/gbuffer_out.glsl"
#include "common/material.glsl"

DENG_VAR vec2 vUV[4];
DENG_VAR vec4 vInstanceColor;
DENG_VAR vec3 vNormal;

void main(void) {
    vec4 color = texture(uTextureAtlas[Texture_Diffuse], vUV[Texture_Diffuse]);
    if (color.a < 0.5) discard;

    GBuffer_SetFragDiffuse(color * vInstanceColor);
    GBuffer_SetFragEmissive(
        texture(uTextureAtlas[Texture_Emissive], vUV[Texture_Emissive]).rgb);
    GBuffer_SetFragSpecGloss(
        texture(uTextureAtlas[Texture_SpecularGloss], vUV[Texture_SpecularGloss]));
    // TODO: normal mapping
    GBuffer_SetFragNormal(vNormal);
}
