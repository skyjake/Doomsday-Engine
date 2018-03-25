#version 330 core

#include "common/gbuffer_out.glsl"
#include "common/material.glsl"
#include "common/tangentspace.glsl"

uniform mat4 uProjMatrix;

     in vec2  vUV;
     in vec4  vVSPos;
     in vec3  vTSViewPos;
     in vec3  vTSFragPos;
     in vec3  vWSTangent;
     in vec3  vWSBitangent;
     in vec3  vWSNormal;
flat in uint  vMaterial;
flat in uint  vFlags;

vec2 Gloom_Parallax(uint matIndex, vec2 texCoords, vec3 viewDir,
                    out float displacementDepth) {
    MaterialSampler matSamp = Gloom_Sampler(matIndex, Texture_NormalDisplacement);
    if (!matSamp.metrics.isValid) {
        displacementDepth = 0.0;
        return texCoords;
    }

    const float heightScale = 0.25;

#if 0
    // Basic Parallax Mapping
    float height = 1.0 - Gloom_FetchTexture(matIndex, Texture_NormalDisplacement, texCoords).a;
    vec2 p = viewDir.xy / viewDir.z * (height * heightScale);
    return texCoords - p;
#endif

    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers, abs(dot(Axis_Z, viewDir)));

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
    float d1 = curDepthMapValue;
    float d2 = 1.0 - Gloom_SampleMaterial(matSamp, prevTexCoords).a;
    float afterDepth = d1 - curLayerDepth;
    float beforeDepth = d2 - curLayerDepth + layerDepth;
    float weight = afterDepth / (afterDepth - beforeDepth);

    displacementDepth = mix(d1, d2, weight) * heightScale;

    return mix(curTexCoords, prevTexCoords, weight);
}

void main(void) {
    uint matIndex = vMaterial;

    // One-sided surfaces may lack one of the materials.
    if (matIndex == InvalidIndex) discard;

    // Normal mapping.
    TangentSpace ts = TangentSpace(normalize(vWSTangent),
                                   normalize(vWSBitangent),
                                   normalize(vWSNormal));

    // Parallax mapping.
    vec3 viewDir  = normalize(vTSViewPos - vTSFragPos);
    float displacementDepth;
    vec2 texCoord = Gloom_Parallax(matIndex, vUV, viewDir, displacementDepth);

    vec3 normal = GBuffer_UnpackNormal(Gloom_FetchTexture(matIndex,
        Texture_NormalDisplacement, texCoord));
    vec3 vsNormal = Gloom_TangentMatrix(ts) * normal;
    GBuffer_SetFragNormal(vsNormal);
    GBuffer_SetFragDiffuse(Gloom_FetchTexture(matIndex, Texture_Diffuse, texCoord));
    GBuffer_SetFragEmissive(Gloom_FetchTexture(matIndex, Texture_Emissive, texCoord).rgb);
    GBuffer_SetFragSpecGloss(Gloom_FetchTexture(matIndex, Texture_SpecularGloss, texCoord));

    // Write a displaced depth.
    if (displacementDepth < 1.0)
    {
        vec3 vsPos = vVSPos.xyz / vVSPos.w;
        vec3 vsDir = normalize(vsPos);

        vsPos += vsDir * displacementDepth;

        vec4 dispPos = uProjMatrix * vec4(vsPos, 1.0);
        float ndc = dispPos.z / dispPos.w;

        gl_FragDepth = 0.5 * ndc + 0.5;
    }

    // GBuffer_SetFragDiffuse(vec4(1.0)); // testing
}
