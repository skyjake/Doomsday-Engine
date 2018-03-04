#version 330 core

#include "common/gbuffer_out.glsl"
#include "common/material.glsl"
#include "common/tangentspace.glsl"

uniform vec4        uCameraPos; // world space

     DENG_VAR vec2  vUV;
     DENG_VAR vec3  vWSPos;
     DENG_VAR vec3  vWSTangent;
     DENG_VAR vec3  vWSBitangent;
     DENG_VAR vec3  vWSNormal;
flat DENG_VAR float vMaterial;
flat DENG_VAR uint  vFlags;

vec2 Gloom_Parallax(uint matIndex, vec2 texCoords, vec3 viewDir) {
    float heightScale = 0.1;
    float height = 1.0 - Gloom_FetchTexture(matIndex, Texture_NormalDisplacement, texCoords).a;
    vec2 p = viewDir.xy / viewDir.z * (height * heightScale);
    return texCoords - p;
}

void main(void) {
    uint matIndex = uint(vMaterial + 0.5);

    // Normal mapping.
    TangentSpace ts = TangentSpace(normalize(vWSTangent),
                                   normalize(vWSBitangent),
                                   normalize(vWSNormal));

    // vec4 normalDisp = Gloom_FetchTexture(matIndex, Texture_NormalDisplacement, vUV);
    // vec3 normal = Gloom_TangentMatrix(ts) * GBuffer_UnpackNormal(normalDisp);

    // float displacement = normalDisp.w; // 0 = back, 1 = front

    // GBuffer_SetFragDiffuse(vec4(vec3(displacement), 1.0));

    // Parallax mapping.

    // TODO : calculate these in the vertex shader
    mat3 TBN = transpose(Gloom_TangentMatrix(ts)); // world to tangent
    vec3 tsViewPos = TBN * uCameraPos.xyz;
    vec3 tsFragPos = TBN * vWSPos;

    vec3 viewDir  = normalize(tsViewPos - tsFragPos);
    vec2 texCoord = Gloom_Parallax(matIndex, vUV, viewDir);

    GBuffer_SetFragNormal(
        Gloom_TangentMatrix(ts) *
        GBuffer_UnpackNormal(Gloom_FetchTexture(matIndex, Texture_NormalDisplacement, texCoord)));
    GBuffer_SetFragDiffuse(Gloom_FetchTexture(matIndex, Texture_Diffuse, texCoord));
    GBuffer_SetFragEmissive(Gloom_FetchTexture(matIndex, Texture_Emissive, texCoord).rgb);
    GBuffer_SetFragSpecGloss(Gloom_FetchTexture(matIndex, Texture_SpecularGloss, texCoord));

    GBuffer_SetFragDiffuse(vec4(1.0)); // testing
}
