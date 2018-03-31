#version 330 core

#include "common/gbuffer.glsl"
#include "common/material.glsl"
#include "common/tangentspace.glsl"

uniform mat4 uProjMatrix;

     in vec2  vUV;
     in vec4  vVSPos;
     in vec3  vTSViewDir;
     in vec3  vWSTangent;
     in vec3  vWSBitangent;
     in vec3  vWSNormal;
flat in uint  vMaterial;
flat in uint  vFlags;

void main(void) {
    uint matIndex = vMaterial;

    // One-sided surfaces may lack one of the materials.
    if (matIndex == InvalidIndex) discard;

    // Normal mapping.
    TangentSpace ts = TangentSpace(normalize(vWSTangent),
                                   normalize(vWSBitangent),
                                   normalize(vWSNormal));

    // Parallax mapping.
    float displacementDepth;
    vec3 viewDir  = normalize(vTSViewDir);
    vec2 texCoord = Gloom_Parallax(matIndex, vUV, viewDir, displacementDepth);

    vec3 normal = GBuffer_UnpackNormal(
        Gloom_FetchTexture(matIndex, Texture_NormalDisplacement, texCoord));
    vec3 vsNormal = Gloom_TangentMatrix(ts) * normal;

    vec4 diffuse = Gloom_FetchTexture(matIndex, Texture_Diffuse, texCoord);
    vec3 emissive = Gloom_FetchTexture(matIndex, Texture_Emissive, texCoord).rgb;
    vec4 specGloss = Gloom_FetchTexture(matIndex, Texture_SpecularGloss, texCoord);

    out_FragColor = vec4(normal, 0.5);

    // Write a displaced depth.  // TODO: Add a routine for doing this.
    if (displacementDepth > 0.0) {
        vec3 vsPos = vVSPos.xyz / vVSPos.w;
        vsPos += normalize(vsPos) * displacementDepth / abs(dot(Axis_Z, viewDir));
        vec4 dispPos = uProjMatrix * vec4(vsPos, 1.0);
        float ndc = dispPos.z / dispPos.w;
        gl_FragDepth = 0.5 * ndc + 0.5;
    }
    else {
        gl_FragDepth = gl_FragCoord.z;
    }
}
