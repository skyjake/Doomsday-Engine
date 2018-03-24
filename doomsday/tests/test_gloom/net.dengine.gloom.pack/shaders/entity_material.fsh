#version 330 core

#include "common/tangentspace.glsl"
#include "common/gbuffer_out.glsl"
#include "common/material.glsl"

in vec3 vWSNormal;
in vec3 vWSTangent;
in vec3 vWSBitangent;
in vec2 vUV;
flat in ivec4 vGotTexture;
flat in vec4 vInstanceColor;
flat in vec4 vTexBounds[4];

#define hasDiffuseTex()     (vGotTexture.x != 0)
#define hasNormalTex()      (vGotTexture.y != 0)
#define hasSpecularTex()    (vGotTexture.z != 0)
#define hasEmissiveTex()    (vGotTexture.w != 0)

void main(void) {
    // Calculate the final texcoords.
    vec2 uv[4];
    for (int i = 0; i < 4; ++i) {
        uv[i] = vTexBounds[i].xy + fract(vUV) * vTexBounds[i].zw;
    }

    vec4 diffuse;
    if (hasDiffuseTex()) {
        diffuse = texture(uTextureAtlas[Texture_Diffuse], uv[Texture_Diffuse]);
        if (diffuse.a < 0.5) discard;
    }
    else {
        diffuse = Material_DefaultTextureValue[Texture_Diffuse];
    }

    GBuffer_SetFragDiffuse(diffuse * vInstanceColor);
    GBuffer_SetFragEmissive(
        texture(uTextureAtlas[Texture_Emissive], uv[Texture_Emissive]).rgb);

    vec4 specGloss = texture(uTextureAtlas[Texture_SpecularGloss],
         uv[Texture_SpecularGloss]);
    GBuffer_SetFragSpecGloss(specGloss);

    // GBuffer_SetFragDiffuse(vec4(0.30, 0.30, 0.30, 1.0));
    // GBuffer_SetFragSpecGloss(vec4(0.5, 0.5, 0.5, 1.0));

    vec3 normal = vWSNormal;
#if 1
    if (hasNormalTex()) {
        // Rotate from tangent to world space.
        TangentSpace ts = TangentSpace(normalize(vWSTangent),
                                       normalize(vWSBitangent),
                                       normalize(vWSNormal));

        normal = Gloom_TangentMatrix(ts) * GBuffer_UnpackNormal(
                texture(uTextureAtlas[Texture_NormalDisplacement],
                        uv[Texture_NormalDisplacement]));
    }
#endif

    GBuffer_SetFragNormal(normal);
}
