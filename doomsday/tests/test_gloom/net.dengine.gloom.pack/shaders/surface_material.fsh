#version 330 core

#include "common/gbuffer_out.glsl"
#include "common/material.glsl"

     DENG_VAR vec2  vUV;
     DENG_VAR vec3  vNormal;
flat DENG_VAR float vMaterial;
flat DENG_VAR uint  vFlags;

void main(void) {
    uint matIndex = uint(vMaterial + 0.5);

    // Mask transparent texels.
    // vec4 color = Gloom_FetchTexture(matIndex, Texture_Diffuse, vUV);
    // if (color.a < 0.005) {
    //     discard;
    // }

    GBuffer_SetFragMaterial(matIndex, vUV);
    GBuffer_SetFragNormal(vNormal);
}
