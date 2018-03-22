#version 330 core

#include "common/gbuffer_out.glsl"
#include "common/material.glsl"
#include "common/tangentspace.glsl"

     DENG_VAR vec2  vUV;
     DENG_VAR vec3  vTSViewPos;
     DENG_VAR vec3  vTSFragPos;
     DENG_VAR vec3  vWSTangent;
     DENG_VAR vec3  vWSBitangent;
     DENG_VAR vec3  vWSNormal;
flat DENG_VAR uint  vMaterial;
flat DENG_VAR uint  vFlags;

vec2 Gloom_Parallax(uint matIndex, vec2 texCoords, vec3 viewDir) {
    MaterialSampler matSamp = Gloom_Sampler(matIndex, Texture_NormalDisplacement);
    if (!matSamp.metrics.isValid) return texCoords;

    const float heightScale = 0.25;

#if 0
    // Basic Parallax Mapping
    float height = 1.0 - Gloom_FetchTexture(matIndex, Texture_NormalDisplacement, texCoords).a;
    vec2 p = viewDir.xy / viewDir.z * (height * heightScale);
    return texCoords - p;
#endif

    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));

    float layerDepth = 1.0 / numLayers;
    float curLayerDepth = 0.0;
    vec2 P = viewDir.xy * heightScale;
    vec2 deltaTexCoords = P / numLayers;

    vec2 curTexCoords = texCoords;
    float curDepthMapValue = 1.0 - Gloom_SampleMaterial(matSamp, curTexCoords).a;

    while (curLayerDepth < curDepthMapValue) {
        curTexCoords -= deltaTexCoords;
        curDepthMapValue = 1.0 - Gloom_SampleMaterial(matSamp, curTexCoords).a;
        curLayerDepth += layerDepth;
    }

    // Interpolate for more accurate collision point.
    vec2 prevTexCoords = curTexCoords + deltaTexCoords;
    float afterDepth = curDepthMapValue - curLayerDepth;
    float beforeDepth = (1.0 - Gloom_SampleMaterial(matSamp, prevTexCoords).a) -
        curLayerDepth + layerDepth;
    float weight = afterDepth / (afterDepth - beforeDepth);
    return mix(curTexCoords, prevTexCoords, weight);
}

void main(void) {
    uint matIndex = vMaterial; //uint(vMaterial + 0.5);

    // One-sided surfaces may lack one of the materials.
    if (matIndex == InvalidIndex) discard;

    // Normal mapping.
    TangentSpace ts = TangentSpace(normalize(vWSTangent),
                                   normalize(vWSBitangent),
                                   normalize(vWSNormal));

    // Parallax mapping.
    vec3 viewDir  = normalize(vTSViewPos - vTSFragPos);
    vec2 texCoord = Gloom_Parallax(matIndex, vUV, viewDir);

    vec3 normal = GBuffer_UnpackNormal(Gloom_FetchTexture(matIndex,
        Texture_NormalDisplacement, texCoord));
    GBuffer_SetFragNormal(Gloom_TangentMatrix(ts) * normal);
    GBuffer_SetFragDiffuse(Gloom_FetchTexture(matIndex, Texture_Diffuse, texCoord));
    GBuffer_SetFragEmissive(Gloom_FetchTexture(matIndex, Texture_Emissive, texCoord).rgb);
    GBuffer_SetFragSpecGloss(Gloom_FetchTexture(matIndex, Texture_SpecularGloss, texCoord));

    // GBuffer_SetFragDiffuse(vec4(1.0)); // testing
}
