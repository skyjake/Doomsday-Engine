#version 330 core

#include "common/gbuffer_out.glsl"
#include "common/material.glsl"

     DENG_VAR vec2  vUV;
     DENG_VAR vec3  vNormal;
flat DENG_VAR float vMaterial;
flat DENG_VAR uint  vFlags;

void main(void) {
    uint matIndex = uint(vMaterial + 0.5);

    // GBuffer_SetFragMaterial(matIndex, vUV);
    GBuffer_SetFragDiffuse(Gloom_FetchTexture(matIndex, Texture_Diffuse, vUV));
    GBuffer_SetFragEmissive(Gloom_FetchTexture(matIndex, Texture_Emissive, vUV).rgb);
    GBuffer_SetFragSpecGloss(Gloom_FetchTexture(matIndex, Texture_SpecularGloss, vUV));

    // TODO: normal/parallax mapping

    GBuffer_SetFragNormal(vNormal);
}
